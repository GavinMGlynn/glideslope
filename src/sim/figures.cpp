#include "sim/figures.hpp"

#include "sim/fixed_step.hpp"
#include "sim/test_pilot.hpp"

#include <input_output/FGXMLElement.h>
#include <input_output/FGXMLFileRead.h>
#include <simgear/misc/sg_path.hxx>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);
constexpr double g_fps2 = 32.174;
constexpr double knots_per_mph = 1.0 / 1.150779;

int steps(double seconds) {
    return static_cast<int>(
        std::lround(seconds * static_cast<double>(steps_per_second)));
}

double condition(const FigureSpec& spec, const std::string& key) {
    const auto it = spec.conditions.find(key);
    if (it == spec.conditions.end()) {
        throw std::runtime_error("figure " + spec.name + " needs a " + key +
                                 " attribute");
    }
    return it->second;
}

double condition_or(const FigureSpec& spec, const std::string& key, double otherwise) {
    const auto it = spec.conditions.find(key);
    return it == spec.conditions.end() ? otherwise : it->second;
}

// An aircraft loaded as the figures file says for the figure, and put
// somewhere, its gear held where the start put it.
struct Flight {
    Aircraft aircraft;
    TestPilot pilot;
    double gear;

    Flight(const std::filesystem::path& root, const PublishedFigures& figures,
           const FigureSpec& spec, const InitialConditions& ic)
        : aircraft(root, figures.model), pilot(aircraft), gear(ic.gear) {
        const auto it = figures.loadings.find(spec.loading);
        if (it == figures.loadings.end()) {
            throw std::runtime_error("figure " + spec.name + " names no loading " +
                                     spec.loading);
        }
        aircraft.load(it->second.loading);
        aircraft.initialize(ic);
        // A figure measured at a weight, whatever the engines burn.
        aircraft.freeze_fuel(condition_or(spec, "fuel_frozen", 0.0) > 0.0);
        const double weight = aircraft.property("inertia/weight-lbs");
        if (std::abs(weight - it->second.total_lbs) > 1.0) {
            std::ostringstream m;
            m << figures.model << " loaded as its figures file's loading "
              << spec.loading << " says weighs " << weight << " lb, not the "
              << it->second.total_lbs << " lb the file states";
            throw std::runtime_error(m.str());
        }
    }

    void fly(Controls c) {
        c.gear = gear;
        aircraft.set_controls(c);
        aircraft.step();
    }

    double engine(int i, const std::string& what) const {
        return aircraft.property("propulsion/engine[" + std::to_string(i) + "]/" + what);
    }

    double heading_error(double heading_deg) const {
        return std::remainder(heading_deg - aircraft.property("attitude/psi-deg"), 360.0);
    }
};

InitialConditions on_the_runway() {
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    return ic;
}

// Airborne, over terrain far enough below that it plays no part, with any
// retractable gear up.
InitialConditions airborne(double altitude_ft, double kcas, bool engine_running) {
    InitialConditions ic = on_the_runway();
    ic.altitude_ft = altitude_ft;
    ic.terrain_elevation_ft = std::min(0.0, altitude_ft) - 3000.0;
    ic.airspeed_kts = kcas;
    ic.engine_running = engine_running;
    ic.gear = 0.0;
    return ic;
}

double flaps_command(const PublishedFigures& figures, double flaps_deg) {
    if (flaps_deg == 0.0) {
        return 0.0;
    }
    if (figures.flaps_full_deg <= 0.0) {
        throw std::runtime_error("the figure asks for " + std::to_string(flaps_deg) +
                                 " degrees of flap, and the aircraft has none");
    }
    return flaps_deg / figures.flaps_full_deg;
}

// The standard atmosphere's density ratio at a pressure altitude.
double density_ratio(double altitude_ft) {
    return std::pow(1.0 - 6.8756e-6 * altitude_ft, 4.2559);
}

// The boost gauge: lb/sq in above the standard atmosphere at sea level.
double boost_psi(const Flight& f, int engine) {
    return f.engine(engine, "map-inhg") * 0.4911541 - 14.6959;
}

// A pilot setting power: the throttles to a boost read on one engine's gauge,
// and the propeller lever to an rpm, each by a slow integral, as a hand on the
// levers watching the gauges would. A boost the engine cannot give - above its
// rated boost, or above its full-throttle height - leaves the throttles open.
class Power {
public:
    Power(double boost, double rpm, int engine)
        : boost_(boost), rpm_(rpm), engine_(engine) {}

    void set(const Flight& f, Controls& c) {
        const double boost = boost_psi(f, engine_);
        throttle_ = std::clamp(throttle_ + 0.03 * (boost_ - boost) * dt, 0.0, 1.0);
        lever_ = std::clamp(lever_ + 0.001 * (rpm_ - f.engine(engine_, "engine-rpm")) * dt,
                            0.0, 1.0);
        c.throttle = throttle_;
        c.propeller = lever_;
        if (gear_change_drop_ > 0.0) {
            high_gear_ = high_gear_ || (throttle_ >= 0.999 && boost < boost_ - gear_change_drop_);
            c.supercharger = high_gear_ ? 1.0 : 0.0;
        }
    }

    void aim(double boost) {
        boost_ = boost;
    }

    // A two-speed supercharger climbed in low gear until, the throttles open,
    // the boost has fallen `drop` below what is wanted; then automatic - the
    // Mosquito's Pilot's Notes' "when the maximum obtainable boost has fallen
    // to +7 lb./sq. in., change to AUTO", climbing at +9.
    void change_gear_when_boost_falls(double drop) {
        gear_change_drop_ = drop;
    }

private:
    double boost_;
    double rpm_;
    int engine_;
    double throttle_ = 0.8;
    double lever_ = 1.0;
    double gear_change_drop_ = 0.0;
    bool high_gear_ = false;
};

// ---------------------------------------------------------------------------
// The flights, one per kind of figure.
// ---------------------------------------------------------------------------

// Brakes on, full throttle, mixture leaned in steps of 0.05; the highest RPM any
// mixture reaches after fifteen seconds.
double static_rpm(const std::filesystem::path& root, const PublishedFigures& figures,
                  const FigureSpec& spec) {
    Flight f(root, figures, spec, on_the_runway());
    double best = 0.0;
    for (int m = 20; m >= 11; --m) {
        Controls c;
        c.throttle = 1.0;
        c.mixture = m * 0.05;
        c.left_brake = c.right_brake = 1.0;
        for (int i = 0; i < steps(15); ++i) {
            f.fly(c);
        }
        best = std::max(best, f.aircraft.property("propulsion/engine[0]/engine-rpm"));
    }
    return best;
}

// Flaps set and full throttle against the brakes, then released; the ground
// distance covered until no wheel is on the ground, rotating at the lift-off
// speed.
double takeoff_ground_roll(const std::filesystem::path& root,
                           const PublishedFigures& figures, const FigureSpec& spec) {
    Flight f(root, figures, spec, on_the_runway());
    Controls c;
    c.throttle = 1.0;
    c.flaps = flaps_command(figures, condition(spec, "flaps_deg"));
    c.left_brake = c.right_brake = 1.0;
    for (int i = 0; i < steps(8); ++i) {
        f.fly(c);
    }
    const double flaps_deg = f.aircraft.property("fcs/flap-pos-deg");
    if (std::abs(flaps_deg - condition(spec, "flaps_deg")) > 0.5) {
        throw std::runtime_error("the flaps reached " + std::to_string(flaps_deg) +
                                 " degrees");
    }

    c.left_brake = c.right_brake = 0.0;
    const double lift_off = condition(spec, "lift_off_kcas");
    double ground_ft = 0.0;
    for (int i = 0; i < steps(60); ++i) {
        c.elevator = f.aircraft.property("velocities/vc-kts") >= lift_off ? 0.6 : 0.0;
        f.fly(c);
        ground_ft += f.aircraft.property("velocities/vg-fps") * dt;
        if (f.aircraft.property("gear/wow") < 0.5) {
            return ground_ft;
        }
    }
    throw std::runtime_error("still on the ground after a minute");
}

// Flaps set on the brakes; then the brakes off and the throttles opened to the
// boost over three seconds, steering by rudder and, below 60 knots,
// differential brake - the tail wheel castors, and does not steer. The tail is
// raised from 60 knots to a take-off attitude of a degree nose up, and at the
// lift-off speed the aircraft is rotated to 12 degrees - its attitude sitting
// on its tail - and held there as it climbs away. The ground distance covered
// until it is 50 ft higher than it stood.
double takeoff_distance(const std::filesystem::path& root,
                        const PublishedFigures& figures, const FigureSpec& spec) {
    Flight f(root, figures, spec, on_the_runway());
    const double heading = on_the_runway().heading_deg;
    const double lift_off = condition(spec, "lift_off_kcas");
    const double boost = condition(spec, "boost_psi");
    Power power(0.0, condition(spec, "rpm"), 0);
    Controls c;
    c.flaps = flaps_command(figures, condition(spec, "flaps_deg"));
    c.left_brake = c.right_brake = 1.0;
    for (int i = 0; i < steps(12); ++i) {
        f.fly(c);
    }
    const double standing_ft = f.aircraft.property("position/h-agl-ft");
    double ground_ft = 0.0;
    // On the ground the stick holds an attitude by itself, with no integral
    // to wind up against the wheels; once the aircraft flies, a fresh pilot.
    std::optional<TestPilot> airborne_pilot;
    for (int i = 0; i < steps(90); ++i) {
        power.aim(std::min(i * dt / 3.0, 1.0) * boost);
        power.set(f, c);
        const double kcas = f.aircraft.property("velocities/vc-kts");
        const double theta = f.aircraft.property("attitude/theta-deg");
        const double q_degps = f.aircraft.property("velocities/q-rad_sec") * 57.29578;
        c.rudder = f.pilot.steer_to(heading);
        const double r_degps = f.aircraft.property("velocities/r-rad_sec") * 57.29578;
        const double turn =
            std::clamp(0.1 * f.heading_error(heading) - 0.3 * r_degps, -1.0, 1.0);
        c.left_brake = kcas < 60.0 ? std::max(-turn, 0.0) : 0.0;
        c.right_brake = kcas < 60.0 ? std::max(turn, 0.0) : 0.0;
        if (kcas >= lift_off && !airborne_pilot) {
            airborne_pilot.emplace(f.aircraft);
        }
        if (airborne_pilot) {
            c.elevator = airborne_pilot->pitch_to(12.0);
            c.aileron = airborne_pilot->roll_to(0.0);
        } else {
            const double attitude = kcas >= 60.0 ? 1.0 : theta;
            c.elevator = std::clamp(0.1 * (attitude - theta) - 0.05 * q_degps, -1.0, 1.0);
        }
        f.fly(c);
        ground_ft += f.aircraft.property("velocities/vg-fps") * dt;
        if (f.aircraft.property("position/h-agl-ft") - standing_ft >= 50.0) {
            return ground_ft;
        }
    }
    throw std::runtime_error("not 50 ft up after a minute and a half");
}

// The tendency to swing on the take-off run, as the pilot meets it opening the
// throttles: on the brakes, tail down, the port engine held at the boost and
// the starboard one `lead` below it, and the yawing moment the propellers and
// the air put on the aircraft about the vertical once both have settled.
double swing_moment(const std::filesystem::path& root, const PublishedFigures& figures,
                    const FigureSpec& spec, double lead) {
    Flight f(root, figures, spec, on_the_runway());
    const double boost = condition(spec, "boost_psi");
    Power port(boost, condition(spec, "rpm"), 0);
    Power starboard(boost - lead, condition(spec, "rpm"), 1);
    Controls c;
    c.left_brake = c.right_brake = 1.0;
    for (int i = 0; i < steps(20); ++i) {
        Controls p;
        Controls s;
        port.set(f, p);
        starboard.set(f, s);
        c.throttle = 0.0;
        c.throttle_offset = {p.throttle, s.throttle};
        c.propeller = p.propeller;
        f.fly(c);
    }
    // The body's roll and yaw moments, the nose up at its angle on the ground:
    // about the vertical, positive turning the nose to starboard.
    const double theta = f.aircraft.property("attitude/theta-rad");
    const double roll = f.aircraft.property("moments/l-aero-lbsft") +
                        f.aircraft.property("moments/l-prop-lbsft");
    const double yaw = f.aircraft.property("moments/n-aero-lbsft") +
                       f.aircraft.property("moments/n-prop-lbsft");
    return yaw * std::cos(theta) - roll * std::sin(theta);
}

// How far ahead of the starboard throttle the port one must be, in lb/sq in
// of boost, for the aircraft to have no tendency to swing at the stated boost:
// positive for a swing to port that the port throttle checks. The moment is
// linear in the lead, so two leads find it.
double takeoff_swing(const std::filesystem::path& root, const PublishedFigures& figures,
                     const FigureSpec& spec) {
    constexpr double trial = 2.0;
    const double level = swing_moment(root, figures, spec, 0.0);
    const double led = swing_moment(root, figures, spec, trial);
    if (std::abs(led - level) < 1.0) {
        throw std::runtime_error("the lead made no difference to the swing");
    }
    return -level * trial / (led - level);
}

// Full throttle, flaps up, holding the climb speed; the average climb rate over
// forty seconds that pass through the altitude - sea level if none is given -
// after thirty to settle. The engines at the stated boost and rpm, if any; a
// two-speed supercharger held in low gear up to `fs_gear_above_ft`, if stated,
// and automatic above it; the radiator shutters open if `radiators_open`.
double climb_rate(const std::filesystem::path& root, const PublishedFigures& figures,
                  const FigureSpec& spec) {
    const double kcas = condition(spec, "speed_kcas");
    const bool at_altitude = spec.conditions.count("altitude_ft") != 0;
    const double altitude = condition_or(spec, "altitude_ft", 0.0);
    // Started so that, at the published rate, the measurement is centred on
    // the altitude; the Cessna's from 600 ft below sea level.
    const double start =
        at_altitude ? altitude - spec.published * (30.0 + 20.0) / 60.0 : -600.0;
    Flight f(root, figures, spec, airborne(start, kcas, true));
    const bool powered = spec.conditions.count("boost_psi") != 0;
    Power power(condition_or(spec, "boost_psi", 0.0), condition_or(spec, "rpm", 0.0), 0);
    const double fs_above = condition_or(spec, "fs_gear_above_ft", 0.0);
    Controls c;
    c.throttle = 1.0;
    c.cooling_flaps.fill(condition_or(spec, "radiators_open", 0.0));
    const auto hold = [&] {
        if (powered) {
            power.set(f, c);
        }
        c.supercharger = f.aircraft.property("position/h-sl-ft") > fs_above ? 1.0 : 0.0;
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(kcas));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
    };
    for (int i = 0; i < steps(30); ++i) {
        hold();
    }
    const double start_ft = f.aircraft.property("position/h-sl-ft");
    for (int i = 0; i < steps(40); ++i) {
        hold();
    }
    return (f.aircraft.property("position/h-sl-ft") - start_ft) / 40.0 * 60.0;
}

// From sea level at the climb speed, climbing as climb_rate does, a two-speed
// supercharger changing gear when its boost has fallen `gear_change_boost_drop`
// below the climbing boost; the minutes taken to reach the altitude.
double time_to_altitude(const std::filesystem::path& root,
                        const PublishedFigures& figures, const FigureSpec& spec) {
    const double kcas = condition(spec, "speed_kcas");
    const double altitude = condition(spec, "altitude_ft");
    Flight f(root, figures, spec, airborne(0.0, kcas, true));
    Power power(condition(spec, "boost_psi"), condition(spec, "rpm"), 0);
    power.change_gear_when_boost_falls(condition(spec, "gear_change_boost_drop"));
    Controls c;
    c.cooling_flaps.fill(condition_or(spec, "radiators_open", 0.0));
    for (int i = 0; i < steps(3600); ++i) {
        power.set(f, c);
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(kcas));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (f.aircraft.property("position/h-sl-ft") >= altitude) {
            return i * dt / 60.0;
        }
    }
    throw std::runtime_error("not at the altitude after an hour");
}

// The power a figure wants from its engines, as a boost gauge's lb/sq in: its
// boost, or its manifold pressure in inches of mercury, or - given neither -
// full throttle.
double wanted_boost(const FigureSpec& spec) {
    if (spec.conditions.count("boost_psi") != 0) {
        return condition(spec, "boost_psi");
    }
    if (spec.conditions.count("manifold_inhg") != 0) {
        return condition(spec, "manifold_inhg") * 0.4911541 - 14.6959;
    }
    return std::numeric_limits<double>::infinity();
}

// Leans the mixture in steps of 0.05, flying `fly` for eight seconds at each,
// until the power has fallen past its peak, and leaves it at the richest that
// made within half a percent of the most power - best power, as a pilot leans
// by the gauges, and never so lean that the engine stops.
template <class Fly>
void lean_for_best_power(const Flight& f, Controls& c, const Fly& fly) {
    std::vector<std::pair<double, double>> tried;
    double most = 0.0;
    for (int m = 20; m >= 8; --m) {
        c.mixture = m * 0.05;
        for (int i = 0; i < steps(8); ++i) {
            fly();
        }
        double hp = 0.0;
        for (int e = 0; e < f.aircraft.figures().engines; ++e) {
            hp += f.engine(e, "power-hp");
        }
        tried.emplace_back(c.mixture, hp);
        most = std::max(most, hp);
        if (hp < 0.97 * most) {
            break;
        }
    }
    double best = 0.0;
    for (const auto& [mixture, hp] : tried) {
        best = std::max(best, hp);
    }
    for (const auto& [mixture, hp] : tried) {
        if (hp >= 0.995 * best) {
            c.mixture = mixture;
            return;
        }
    }
}

// Level at the altitude, the engines at the figure's power - a boost, a
// manifold pressure or full throttle - and rpm, gear and flaps up, the mixture
// first leaned for best power if `lean` is given; the average true airspeed
// over the last minute of four, in knots if the figure's unit is KTAS and in
// mph, as the Mosquito's trials give it, otherwise.
double level_speed(const std::filesystem::path& root, const PublishedFigures& figures,
                   const FigureSpec& spec) {
    const double altitude = condition(spec, "altitude_ft");
    const bool knots = spec.unit.rfind("KTAS", 0) == 0;
    // Started at the published speed, to settle sooner.
    const double published_kts = knots ? spec.published : spec.published * knots_per_mph;
    Flight f(root, figures, spec,
             airborne(altitude, published_kts * std::sqrt(density_ratio(altitude)), true));
    Power power(wanted_boost(spec), condition(spec, "rpm"), 0);
    Controls c;
    const auto level = [&] {
        power.set(f, c);
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_altitude(altitude));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
    };
    if (condition_or(spec, "lean", 0.0) > 0.0) {
        lean_for_best_power(f, c, level);
    }
    double kts_sum = 0.0;
    const int total = steps(240);
    const int measured = steps(60);
    for (int i = 0; i < total; ++i) {
        level();
        if (i >= total - measured) {
            kts_sum += f.aircraft.property("velocities/vtrue-kts");
        }
    }
    const double kts = kts_sum / measured;
    return knots ? kts : kts / knots_per_mph;
}

// Level at the cruise altitude: first full throttle with the mixture leaned in
// steps to find the richest setting giving the most RPM - leaning no further
// once the RPM has fallen 3% below the most, which an engine already at its
// best when full rich, as the Cub's is, does not survive - then the throttle
// holding the cruise RPM; the average true airspeed over the last fifty
// seconds of three minutes at that RPM.
double cruise_speed(const std::filesystem::path& root, const PublishedFigures& figures,
                    const FigureSpec& spec) {
    const double altitude = condition(spec, "altitude_ft");
    const double rpm_target = condition(spec, "rpm");
    Flight f(root, figures, spec, airborne(altitude, 110.0, true));
    Controls c;
    const auto level = [&] {
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_altitude(altitude));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
    };

    c.throttle = 1.0;
    double best_rpm = 0.0;
    double best_mixture = 1.0;
    for (int m = 20; m >= 10; --m) {
        c.mixture = m * 0.05;
        for (int i = 0; i < steps(8); ++i) {
            level();
        }
        const double rpm = f.aircraft.property("propulsion/engine[0]/engine-rpm");
        if (rpm > best_rpm + 1.0) {
            best_rpm = rpm;
            best_mixture = c.mixture;
        } else if (rpm < 0.97 * best_rpm) {
            break;
        }
    }
    c.mixture = best_mixture;

    double integral = 0.0;
    double ktas_sum = 0.0;
    const int total = steps(180);
    const int measured = steps(50);
    for (int i = 0; i < total; ++i) {
        const double error =
            rpm_target - f.aircraft.property("propulsion/engine[0]/engine-rpm");
        integral = std::clamp(integral + error * dt, -5000.0, 5000.0);
        c.throttle = std::clamp(0.5 + 0.0005 * error + 0.0002 * integral, 0.0, 1.0);
        level();
        if (i >= total - measured) {
            ktas_sum += f.aircraft.property("velocities/vtrue-kts");
        }
    }
    const double rpm = f.aircraft.property("propulsion/engine[0]/engine-rpm");
    if (std::abs(rpm - rpm_target) > 10.0) {
        throw std::runtime_error("could not hold " + std::to_string(rpm_target) +
                                 " RPM; the engine turned " + std::to_string(rpm));
    }
    return ktas_sum / measured;
}

// Engine stopped (mixture cut off, propeller windmilling), flaps up, holding
// the glide speed; ground distance over height lost across three minutes,
// after one to settle.
double glide_ratio(const std::filesystem::path& root, const PublishedFigures& figures,
                   const FigureSpec& spec) {
    const double kcas = condition(spec, "speed_kcas");
    Flight f(root, figures, spec, airborne(6500.0, kcas, false));
    Controls c;
    c.throttle = 0.0;
    c.mixture = 0.0;
    double ground_ft = 0.0;
    double start_ft = 0.0;
    for (int i = 0; i < steps(240); ++i) {
        if (i == steps(60)) {
            start_ft = f.aircraft.property("position/h-sl-ft");
            ground_ft = 0.0;
        }
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(kcas));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        ground_ft += f.aircraft.property("velocities/vg-fps") * dt;
    }
    return ground_ft / (start_ft - f.aircraft.property("position/h-sl-ft"));
}

// Power off, wings level, flaps and any retractable gear set; from the entry
// speed - 70 KCAS unless the figure gives one - the target speed falls at one
// knot a second, the handbook's rate for a stall, and the pilot raises the
// nose to follow it until the elevator can raise it no further. The lowest
// calibrated airspeed reached before the stalled aircraft gathers speed again
// - 3 knots above its lowest - so that a dive and zoom after the stall, with
// the stick still held back, are not counted.
double stall_speed(const std::filesystem::path& root, const PublishedFigures& figures,
                   const FigureSpec& spec) {
    const double entry = condition_or(spec, "entry_kcas", 70.0);
    InitialConditions ic = airborne(5000.0, entry, true);
    ic.gear = condition_or(spec, "gear", 0.0);
    Flight f(root, figures, spec, ic);
    Controls c;
    c.throttle = 0.0;
    c.flaps = flaps_command(figures, condition(spec, "flaps_deg"));
    double slowest = 1e9;
    // Long enough for the target to fall from any entry speed to nothing.
    for (int i = 0; i < steps(10.0 + entry); ++i) {
        const double t = i * dt;
        const double target = t < 10.0 ? entry : entry - (t - 10.0);
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(target));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (t >= 10.0) {
            const double kcas = f.aircraft.property("velocities/vc-kts");
            slowest = std::min(slowest, kcas);
            if (kcas > slowest + 3.0) {
                break;
            }
        }
    }
    return slowest;
}

// A level turn at the stated bank; the measured turn rate as a percentage of
// g tan(bank) / true airspeed, both averaged over thirty seconds after a minute
// to settle.
double turn_rate(const std::filesystem::path& root, const PublishedFigures& figures,
                 const FigureSpec& spec) {
    const double altitude = condition(spec, "altitude_ft");
    const double bank = condition(spec, "bank_deg");
    Flight f(root, figures, spec, airborne(altitude, 100.0, true));
    Controls c;
    c.throttle = 0.85;
    double rate_sum = 0.0;
    double expected_sum = 0.0;
    for (int i = 0; i < steps(90); ++i) {
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_altitude(altitude));
        c.aileron = f.pilot.roll_to(bank);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (i >= steps(60)) {
            const double phi = f.aircraft.property("attitude/phi-rad");
            rate_sum += f.aircraft.property("velocities/psidot-rad_sec");
            expected_sum +=
                g_fps2 * std::tan(phi) / f.aircraft.property("velocities/vtrue-fps");
        }
    }
    return rate_sum / expected_sum * 100.0;
}

// Both engines at the boost and rpm, level at 1,000 ft and 200 knots; then the
// port engine fails and windmills. The live engine keeps the boost; the rudder
// holds the heading and the ailerons 5 degrees of bank toward the live engine,
// each with an integral; the nose follows a target speed falling a knot a
// second from ten seconds after the failure. The safety speed - the lowest at
// which the aircraft can be held straight - is where holding the heading first
// takes the rudder's full travel for a second together.
double safety_speed(const std::filesystem::path& root, const PublishedFigures& figures,
                    const FigureSpec& spec) {
    constexpr double entry = 200.0;
    constexpr double fail_at = 20.0;
    Flight f(root, figures, spec, airborne(1000.0, entry, true));
    const double heading = on_the_runway().heading_deg;
    Power power(condition(spec, "boost_psi"), condition(spec, "rpm"), 1);
    Controls c;
    double rudder_integral = 0.0;
    double aileron_integral = 0.0;
    int at_full_rudder = 0;
    for (int i = 0; i < steps(fail_at + 130.0); ++i) {
        const double t = i * dt;
        if (i == steps(fail_at)) {
            f.aircraft.fail_engine(0, false);
        }
        power.set(f, c);
        const double target = t < fail_at + 10.0 ? entry : entry - (t - fail_at - 10.0);
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(target));
        const double bank = t < fail_at ? 0.0 : 5.0;
        const double bank_error = bank - f.aircraft.property("attitude/phi-deg");
        const double p_degps = f.aircraft.property("velocities/p-rad_sec") * 57.29578;
        aileron_integral = std::clamp(aileron_integral + 0.02 * bank_error * dt, -1.0, 1.0);
        c.aileron =
            std::clamp(0.04 * bank_error - 0.02 * p_degps + aileron_integral, -1.0, 1.0);
        // The model's rudder yaws the nose left for positive values.
        const double error = f.heading_error(heading);
        const double r_degps = f.aircraft.property("velocities/r-rad_sec") * 57.29578;
        rudder_integral = std::clamp(rudder_integral - 0.05 * error * dt, -1.0, 1.0);
        c.rudder =
            std::clamp(-(0.1 * error - 0.2 * r_degps) + rudder_integral, -1.0, 1.0);
        f.fly(c);
        const bool full = std::abs(c.rudder) >= 0.999;
        at_full_rudder = t > fail_at + 10.0 && full ? at_full_rudder + 1 : 0;
        if (at_full_rudder >= steps(1.0)) {
            return f.aircraft.property("velocities/vc-kts");
        }
    }
    throw std::runtime_error("the heading was still held without full rudder at " +
                             std::to_string(entry - 120.0) + " knots");
}

// The port propeller feathered and its radiator shut, the other engine at the
// boost and rpm with its radiator open and its supercharger changing gear as
// time_to_altitude's does, gear and flaps up, the ball in the middle and the
// heading held with bank, at the speed: the rate of climb at heights 2,000 ft
// apart, and the height at which it falls to nothing, between the two that
// bracket it.
double single_engine_ceiling(const std::filesystem::path& root,
                             const PublishedFigures& figures, const FigureSpec& spec) {
    const double kcas = condition(spec, "speed_kcas");
    const double heading = on_the_runway().heading_deg;
    const auto climb_at = [&](double altitude) {
        Flight f(root, figures, spec, airborne(altitude - 500.0, kcas, true));
        f.aircraft.fail_engine(0, true);
        Power power(condition(spec, "boost_psi"), condition(spec, "rpm"), 1);
        power.change_gear_when_boost_falls(condition(spec, "gear_change_boost_drop"));
        Controls c;
        c.cooling_flaps = {0.0, 1.0};
        double start_ft = 0.0;
        for (int i = 0; i < steps(100); ++i) {
            if (i == steps(40)) {
                start_ft = f.aircraft.property("position/h-sl-ft");
            }
            power.set(f, c);
            c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(kcas));
            c.aileron = f.pilot.roll_to(std::clamp(f.heading_error(heading), -10.0, 10.0));
            c.rudder = f.pilot.coordinate();
            f.fly(c);
        }
        return f.aircraft.property("position/h-sl-ft") - start_ft;
    };
    double below = 0.0;
    double below_rate = climb_at(below);
    if (below_rate <= 0.0) {
        return 0.0;
    }
    for (double above = 2000.0; above <= 30000.0; above += 2000.0) {
        const double rate = climb_at(above);
        if (rate <= 0.0) {
            return below + (above - below) * below_rate / (below_rate - rate);
        }
        below = above;
        below_rate = rate;
    }
    throw std::runtime_error("still climbing on one engine at 30,000 ft");
}

// ---------------------------------------------------------------------------
// Jets: the figures an airliner's airport-planning document and type
// certificate give - the take-off field length FAR 25 defines, the climb its
// certification basis demands with an engine out, and its speed and ceiling
// at altitude.

// A pitot tube's impact pressure over the static pressure at a Mach number:
// isentropic below Mach 1, and past it behind the normal shock the tube
// stands in, by Rayleigh's pitot formula.
double impact_over_static(double mach) {
    if (mach <= 1.0) {
        return std::pow(1.0 + 0.2 * mach * mach, 3.5) - 1.0;
    }
    const double m2 = mach * mach;
    return std::pow(1.2 * m2, 3.5) * std::pow(6.0 / (7.0 * m2 - 1.0), 2.5) - 1.0;
}

// The calibrated airspeed of a Mach number at a pressure altitude, in the
// standard atmosphere: the sea-level speed whose impact pressure is the same.
double kcas_for_mach(double mach, double altitude_ft) {
    const double delta =
        altitude_ft < 36089.0 ? std::pow(1.0 - 6.8756e-6 * altitude_ft, 5.2559)
                              : 0.22336 * std::exp(-4.80634e-5 * (altitude_ft - 36089.0));
    const double impact_over_sea_level = impact_over_static(mach) * delta;
    double low = 0.0;
    double high = 5.0;
    for (int i = 0; i < 60; ++i) {
        const double middle = (low + high) / 2.0;
        (impact_over_static(middle) < impact_over_sea_level ? low : high) = middle;
    }
    constexpr double sea_level_sound_kts = 661.4786;
    return sea_level_sound_kts * (low + high) / 2.0;
}

// FAR 25.121(b)'s second-segment climb at a V2: at the take-off flap, gear
// up, one engine failed and the rest at take-off thrust, out of ground
// effect; the gradient, in percent, over thirty seconds after forty to settle
// - the time the flaps take to run out.
double climb_gradient_at(const std::filesystem::path& root, const PublishedFigures& figures,
                         const FigureSpec& spec, double v2) {
    Flight f(root, figures, spec, airborne(1000.0, v2, true));
    f.aircraft.fail_engine(0, false);
    Controls c;
    c.throttle = 1.0;
    c.flaps = flaps_command(figures, condition(spec, "flaps_deg"));
    double start_ft = 0.0;
    double ground_ft = 0.0;
    for (int i = 0; i < steps(70); ++i) {
        if (i == steps(40)) {
            start_ft = f.aircraft.property("position/h-sl-ft");
            ground_ft = 0.0;
        }
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(v2));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        ground_ft += f.aircraft.property("velocities/vg-fps") * dt;
    }
    return (f.aircraft.property("position/h-sl-ft") - start_ft) / ground_ft * 100.0;
}

// The second-segment gradient FAR 25.121(b) demands: 2.4% of a twin, 2.7% of
// a three-engined aircraft and 3.0% of a four-engined one.
double required_gradient(int engines) {
    return engines <= 2 ? 2.4 : engines == 3 ? 2.7 : 3.0;
}

// The take-off speeds, from the aircraft's own stall at the take-off flap and
// weight, gear up, as stall_speed flies it. V2 is 1.2 times it, the margin
// these types were certificated to (FAR 25.107(b) before amendment 25-94),
// unless the aircraft cannot climb at FAR 25.121(b)'s gradient with an engine
// out there: then, as a flight manual's "improved climb" does, V2 rises in
// steps of 2% of the stall speed until it can, as far as 1.4 times it. VR is
// 5% below V2, as these types' take-off speeds put it. `gradient` is the
// climb at the V2 chosen - or, if none up to 1.4 times the stall climbs as
// FAR 25 demands, the best of them.
struct TakeoffSpeeds {
    double vs = 0.0;
    double vr = 0.0;
    double v2 = 0.0;
    double gradient = 0.0;
};

TakeoffSpeeds takeoff_speeds(const std::filesystem::path& root,
                             const PublishedFigures& figures, const FigureSpec& spec) {
    TakeoffSpeeds v;
    v.vs = stall_speed(root, figures, spec);
    const double required =
        required_gradient(Aircraft(root, figures.model).figures().engines);
    v.gradient = -std::numeric_limits<double>::infinity();
    for (double factor = 1.2; factor <= 1.4001; factor += 0.02) {
        const double v2 = factor * v.vs;
        const double gradient = climb_gradient_at(root, figures, spec, v2);
        if (gradient > v.gradient) {
            v.gradient = gradient;
            v.v2 = v2;
        }
        if (gradient >= required) {
            break;
        }
    }
    v.vr = 0.95 * v.v2;
    return v;
}

// One take-off from brake release: take-off thrust set on the brakes with the
// flaps out, then the brakes off, the runway held with rudder and nosewheel.
// If `failure_kcas` is reached, engine 0 - an outboard one - fails there.
// Going on, the aircraft is rotated at VR at 3 degrees a second to the
// take-off attitude, `takeoff_pitch_deg` (15 unless the figure says), or 3
// degrees less with an engine out, and held there; the ground distance to 35
// ft above where it stood. Stopping, a second after the failure - V1 - the
// brakes go on, a second later the throttles close and a second after that
// the speedbrakes go out, the allowances FAR 25.109 has made between the
// pilot's actions since amendment 25-42; the ground distance to a stop.
double takeoff_run(const std::filesystem::path& root, const PublishedFigures& figures,
                   const FigureSpec& spec, const TakeoffSpeeds& v, double failure_kcas,
                   bool stop) {
    Flight f(root, figures, spec, on_the_runway());
    const double heading = on_the_runway().heading_deg;
    const double pitch_limit = condition_or(spec, "takeoff_pitch_deg", 15.0);
    Controls c;
    c.flaps = flaps_command(figures, condition(spec, "flaps_deg"));
    c.throttle = 1.0;
    c.left_brake = c.right_brake = 1.0;
    for (int i = 0; i < steps(20); ++i) {
        c.rudder = f.pilot.steer_to(heading);
        f.fly(c);
    }
    const double standing_ft = f.aircraft.property("position/h-agl-ft");
    c.left_brake = c.right_brake = 0.0;
    double ground_ft = 0.0;
    std::optional<double> failed_at;
    bool stopping = false;
    bool rotating = false;
    double pitch_target = f.aircraft.property("attitude/theta-deg");
    double steering_integral = 0.0;
    for (int i = 0; i < steps(240); ++i) {
        const double t = i * dt;
        const double kcas = f.aircraft.property("velocities/vc-kts");
        if (!failed_at && kcas >= failure_kcas) {
            f.aircraft.fail_engine(0, false);
            failed_at = t;
        }
        if (stop && failed_at && t >= *failed_at + 1.0) {
            stopping = true;
        }
        if (stopping) {
            c.left_brake = c.right_brake = 1.0;
            if (t >= *failed_at + 2.0) {
                c.throttle = 0.0;
            }
            if (t >= *failed_at + 3.0) {
                c.speedbrake = 1.0;
            }
            c.elevator = 0.0;
        } else {
            if (kcas >= v.vr) {
                rotating = true;
            }
            if (rotating) {
                const double attitude = failed_at ? pitch_limit - 3.0 : pitch_limit;
                pitch_target = std::min(pitch_target + 3.0 * dt, attitude);
                const double theta = f.aircraft.property("attitude/theta-deg");
                const double q_degps =
                    f.aircraft.property("velocities/q-rad_sec") * 57.29578;
                c.elevator = std::clamp(0.2 * (pitch_target - theta) - 0.1 * q_degps,
                                        -1.0, 1.0);
            }
            c.aileron = f.pilot.roll_to(0.0);
        }
        // The runway, and in the air the heading, held with rudder; its
        // integral carries a failed engine's yaw.
        const double error = f.heading_error(heading);
        const double r_degps = f.aircraft.property("velocities/r-rad_sec") * 57.29578;
        steering_integral = std::clamp(steering_integral - 0.05 * error * dt, -1.0, 1.0);
        c.rudder = std::clamp(-(0.1 * error - 0.1 * r_degps) + steering_integral, -1.0, 1.0);
        f.fly(c);
        ground_ft += f.aircraft.property("velocities/vg-fps") * dt;
        if (stopping && f.aircraft.property("velocities/vg-fps") < 1.0) {
            return ground_ft;
        }
        if (!stopping && f.aircraft.property("position/h-agl-ft") - standing_ft >= 35.0) {
            return ground_ft;
        }
    }
    throw std::runtime_error(stop ? "not stopped after four minutes"
                                  : "not 35 ft up after four minutes");
}

// FAR 25's take-off field length, which an airport-planning document's
// take-off runway length is: the longer of 115% of the distance to 35 ft with
// every engine, and the balanced field - the distance at which, with an
// engine failing a second before V1, going on to 35 ft and stopping take the
// same ground. The failure speed is found by halving the interval between
// 60% of VR and VR; if going on after a failure at VR still takes longer than
// stopping, that distance governs.
double takeoff_field_length(const std::filesystem::path& root,
                            const PublishedFigures& figures, const FigureSpec& spec) {
    const TakeoffSpeeds v = takeoff_speeds(root, figures, spec);
    constexpr double never = std::numeric_limits<double>::infinity();
    const double every_engine = takeoff_run(root, figures, spec, v, never, false);
    const auto go = [&](double failure) {
        return takeoff_run(root, figures, spec, v, failure, false);
    };
    const auto stop = [&](double failure) {
        return takeoff_run(root, figures, spec, v, failure, true);
    };
    double low = 0.6 * v.vr;
    double high = v.vr;
    double balanced = 0.0;
    const double go_high = go(high);
    if (go_high >= stop(high)) {
        balanced = go_high;
    } else {
        for (int i = 0; i < 10; ++i) {
            const double middle = (low + high) / 2.0;
            const double going = go(middle);
            const double stopping = stop(middle);
            balanced = (going + stopping) / 2.0;
            (going > stopping ? low : high) = middle;
        }
    }
    return std::max(1.15 * every_engine, balanced);
}

// FAR 25.121(b)'s second-segment climb, in percent, at the V2 the take-off
// is flown at - raised, if need be, as takeoff_speeds says.
double climb_gradient_one_engine(const std::filesystem::path& root,
                                 const PublishedFigures& figures,
                                 const FigureSpec& spec) {
    return takeoff_speeds(root, figures, spec).gradient;
}

// Level at an altitude at full throttle - or the `throttle` given, 0.99
// being military power in an aircraft whose afterburner lights above it -
// flaps and gear up, from the Mach given; the Mach it settles at, averaged
// over the last minute of ten.
double level_mach_at(const std::filesystem::path& root, const PublishedFigures& figures,
                     const FigureSpec& spec, double altitude) {
    Flight f(root, figures, spec,
             airborne(altitude, kcas_for_mach(condition(spec, "mach"), altitude), true));
    Controls c;
    c.throttle = condition_or(spec, "throttle", 1.0);
    double mach_sum = 0.0;
    const int total = steps(600);
    const int measured = steps(60);
    for (int i = 0; i < total; ++i) {
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_altitude(altitude));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (i >= total - measured) {
            mach_sum += f.aircraft.property("velocities/mach");
        }
    }
    return mach_sum / measured;
}

double level_mach(const std::filesystem::path& root, const PublishedFigures& figures,
                  const FigureSpec& spec) {
    return level_mach_at(root, figures, spec, condition(spec, "altitude_ft"));
}

// The level Mach at the best altitude: level_mach at every 2,000 ft from
// `altitude_ft` to `altitude_to_ft`, the greatest.
double best_level_mach(const std::filesystem::path& root, const PublishedFigures& figures,
                       const FigureSpec& spec) {
    double best = 0.0;
    for (double altitude = condition(spec, "altitude_ft");
         altitude <= condition(spec, "altitude_to_ft"); altitude += 2000.0) {
        best = std::max(best, level_mach_at(root, figures, spec, altitude));
    }
    return best;
}

// Climbing at full throttle and the Mach given, flaps and gear up, from 1,000
// ft below the altitude; the rate of climb, in ft/min, over the forty seconds
// after thirty to settle.
double climb_at_altitude(const std::filesystem::path& root,
                         const PublishedFigures& figures, const FigureSpec& spec) {
    const double altitude = condition(spec, "altitude_ft");
    const double mach = condition(spec, "mach");
    Flight f(root, figures, spec,
             airborne(altitude - 1000.0, kcas_for_mach(mach, altitude - 1000.0), true));
    Controls c;
    c.throttle = 1.0;
    double start_ft = 0.0;
    for (int i = 0; i < steps(70); ++i) {
        if (i == steps(30)) {
            start_ft = f.aircraft.property("position/h-sl-ft");
        }
        const double here = f.aircraft.property("position/h-sl-ft");
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(kcas_for_mach(mach, here)));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
    }
    return (f.aircraft.property("position/h-sl-ft") - start_ft) / 40.0 * 60.0;
}

// The energy rate, in ft/min, at a load factor: at the altitude and Mach
// given and the `throttle` (full unless given), flaps and gear up, the
// elevator holding the load factor, with an integral, and the bank the one at
// which the lift's upright part holds the altitude; the energy height gained,
// kinetic and potential, over six seconds after six to settle. If the load
// factor is not reached - the wing or the elevator has no more - there is no
// such turn, and it is minus infinity.
double energy_rate_at_load_factor(const std::filesystem::path& root,
                                  const PublishedFigures& figures, const FigureSpec& spec,
                                  double load_factor) {
    const double altitude = condition(spec, "altitude_ft");
    Flight f(root, figures, spec,
             airborne(altitude, kcas_for_mach(condition(spec, "mach"), altitude), true));
    Controls c;
    c.throttle = condition_or(spec, "throttle", 1.0);
    constexpr double radians = 3.14159265358979323846 / 180.0;
    double elevator_integral = 0.0;
    double start_ft = 0.0;
    double nz_sum = 0.0;
    const auto energy_height = [&] {
        const double v = f.aircraft.property("velocities/vtrue-fps");
        return f.aircraft.property("position/h-sl-ft") + v * v / (2.0 * g_fps2);
    };
    const int settle = steps(6);
    const int measured = steps(6);
    for (int i = 0; i < settle + measured; ++i) {
        const double theta = f.aircraft.property("attitude/theta-rad");
        const double error_ft = altitude - f.aircraft.property("position/h-sl-ft");
        const double climb_fps = f.aircraft.property("velocities/h-dot-fps");
        const double level =
            std::acos(std::clamp(std::cos(theta) / load_factor, -1.0, 1.0)) / radians;
        const double bank = std::clamp(level - 0.02 * error_ft + 0.2 * climb_fps, 0.0, 88.0);
        const double nz_error = load_factor - f.aircraft.property("accelerations/Nz");
        elevator_integral = std::clamp(elevator_integral + 0.1 * nz_error * dt, -1.0, 1.0);
        const double q_degps = f.aircraft.property("velocities/q-rad_sec") / radians;
        c.elevator =
            std::clamp(elevator_integral + 0.05 * nz_error - 0.02 * q_degps, -1.0, 1.0);
        c.aileron = f.pilot.roll_to(bank);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (i == settle - 1) {
            start_ft = energy_height();
        }
        if (i >= settle) {
            nz_sum += f.aircraft.property("accelerations/Nz");
        }
    }
    if (nz_sum / measured < load_factor - 0.1) {
        return -std::numeric_limits<double>::infinity();
    }
    return (energy_height() - start_ft) / (measured * dt) * 60.0;
}

// The sustained turn: the greatest load factor at which, level at the
// altitude and Mach given, the aircraft neither gains energy nor loses it -
// the load factor a manoeuvrability chart plots - found by halving between 1
// and 9 g; the turn rate that load factor gives, in degrees a second, g times
// the root of n squared less one over the true airspeed.
double sustained_turn_rate(const std::filesystem::path& root,
                           const PublishedFigures& figures, const FigureSpec& spec) {
    double low = 1.0;
    double high = 9.0;
    if (energy_rate_at_load_factor(root, figures, spec, low) < 0.0) {
        throw std::runtime_error("not holding the Mach level, let alone turning");
    }
    for (int i = 0; i < 10; ++i) {
        const double middle = (low + high) / 2.0;
        (energy_rate_at_load_factor(root, figures, spec, middle) >= 0.0 ? low : high) = middle;
    }
    const double n = (low + high) / 2.0;
    // The true airspeed of the Mach in the standard atmosphere.
    const double altitude = condition(spec, "altitude_ft");
    const double theta = altitude < 36089.0 ? 1.0 - 6.8756e-6 * altitude : 0.75189;
    constexpr double sea_level_sound_fps = 1116.45;
    const double v = condition(spec, "mach") * sea_level_sound_fps * std::sqrt(theta);
    return g_fps2 * std::sqrt(n * n - 1.0) / v / 3.14159265358979323846 * 180.0;
}

// A level acceleration at the altitude - the flight test's way to measure
// energy: held level at the `throttle` (full unless given) from `from` knots,
// calibrated, until `to` or the aircraft stops gaining speed, the specific
// excess power at each speed, V dV/dt / g, over a second at a time; each is
// the rate at which the aircraft could climb at that speed. The greatest, in
// ft/min, once the engines have spooled up, five seconds, and the aircraft is
// level at 1 g: it starts at no incidence, and until its wing carries its
// weight its drag is less than in level flight.
double best_excess_power(const std::filesystem::path& root, const PublishedFigures& figures,
                         const FigureSpec& spec, double altitude, double from, double to,
                         double throttle) {
    Flight f(root, figures, spec, airborne(altitude, from, true));
    Controls c;
    c.throttle = throttle;
    double best = -std::numeric_limits<double>::infinity();
    double last_v = f.aircraft.property("velocities/vtrue-fps");
    double last_h = f.aircraft.property("position/h-sl-ft");
    const int window = steps(1.0);
    for (int i = 1; i <= steps(600); ++i) {
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_altitude(altitude));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (i % window != 0) {
            continue;
        }
        const double v = f.aircraft.property("velocities/vtrue-fps");
        const double h = f.aircraft.property("position/h-sl-ft");
        // The energy height gained, kinetic and potential, in the second.
        const double ps = (v * (v - last_v) / g_fps2 + (h - last_h)) * 60.0;
        if (i >= steps(5.0) && std::abs(f.aircraft.property("accelerations/Nz") - 1.0) < 0.05) {
            best = std::max(best, ps);
            if (v <= last_v || f.aircraft.property("velocities/vc-kts") >= to) {
                break;
            }
        }
        last_v = v;
        last_h = h;
    }
    return best;
}

// The greatest rate of climb at the altitude (sea level unless given): the
// best specific excess power of a level acceleration from `from_kcas` to
// `to_kcas` at the `throttle`, in ft/min.
double max_climb_rate(const std::filesystem::path& root, const PublishedFigures& figures,
                      const FigureSpec& spec) {
    return best_excess_power(root, figures, spec, condition_or(spec, "altitude_ft", 0.0),
                             condition(spec, "from_kcas"), condition(spec, "to_kcas"),
                             condition_or(spec, "throttle", 1.0));
}

// The altitude at which the greatest rate of climb, as max_climb_rate finds
// it, falls to `rate_fpm` - 100 ft/min for a service ceiling, 500 for a
// combat ceiling - found by halving between 20,000 and 80,000 ft; the level
// accelerations run from Mach `mach` to 2.5.
double service_ceiling(const std::filesystem::path& root, const PublishedFigures& figures,
                       const FigureSpec& spec) {
    const double rate = condition(spec, "rate_fpm");
    const double throttle = condition_or(spec, "throttle", 1.0);
    const double mach = condition(spec, "mach");
    const auto climb = [&](double altitude) {
        return best_excess_power(root, figures, spec, altitude, kcas_for_mach(mach, altitude),
                                 kcas_for_mach(2.5, altitude), throttle);
    };
    double low = 20000.0;
    double high = 80000.0;
    if (climb(low) < rate) {
        throw std::runtime_error("not climbing at " + std::to_string(rate) + " ft/min at " +
                                 std::to_string(low) + " ft");
    }
    for (int i = 0; i < 10; ++i) {
        const double middle = (low + high) / 2.0;
        (climb(middle) >= rate ? low : high) = middle;
    }
    return (low + high) / 2.0;
}

// Level at the altitude at full throttle, from Mach `mach` to Mach `mach_to`:
// the seconds it takes.
double acceleration_time(const std::filesystem::path& root, const PublishedFigures& figures,
                         const FigureSpec& spec) {
    const double altitude = condition(spec, "altitude_ft");
    const double to = condition(spec, "mach_to");
    Flight f(root, figures, spec,
             airborne(altitude, kcas_for_mach(condition(spec, "mach"), altitude), true));
    Controls c;
    c.throttle = condition_or(spec, "throttle", 1.0);
    for (int i = 0; i < steps(600); ++i) {
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_altitude(altitude));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (f.aircraft.property("velocities/mach") >= to) {
            return i * dt;
        }
    }
    throw std::runtime_error("not at Mach " + std::to_string(to) + " after ten minutes");
}

// Nautical miles flown per pound of fuel, level at the altitude and Mach
// given, the throttle holding the Mach, with the figure's loading's tanks at
// `fraction` of what it gives them: over two minutes after one to settle, the
// ground covered over the fuel burned - little enough that the weight hardly
// changes.
double specific_range(const std::filesystem::path& root, const PublishedFigures& figures,
                      const FigureSpec& spec, double fraction) {
    PublishedFigures lighter = figures;
    FigureLoading& loading = lighter.loadings.at(spec.loading);
    for (auto& [tank, lbs] : loading.loading.tank_lbs) {
        loading.total_lbs -= lbs * (1.0 - fraction);
        lbs *= fraction;
    }
    const double altitude = condition(spec, "altitude_ft");
    const double mach = condition(spec, "mach");
    Flight f(root, lighter, spec, airborne(altitude, kcas_for_mach(mach, altitude), true));
    Controls c;
    c.throttle = 0.7;
    double start_lbs = 0.0;
    double ground_ft = 0.0;
    for (int i = 0; i < steps(180); ++i) {
        c.throttle = std::clamp(
            c.throttle + 2.0 * (mach - f.aircraft.property("velocities/mach")) * dt, 0.0, 0.99);
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_altitude(altitude));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (i == steps(60)) {
            start_lbs = f.aircraft.property("propulsion/total-fuel-lbs");
        }
        if (i >= steps(60)) {
            ground_ft += f.aircraft.property("velocities/vg-fps") * dt;
        }
    }
    const double burned = start_lbs - f.aircraft.property("propulsion/total-fuel-lbs");
    if (burned <= 0.0) {
        throw std::runtime_error("no fuel burned in the cruise");
    }
    return ground_ft / 6076.12 / burned;
}

// The range, in nautical miles, on the fuel in the figure's loading, level
// at the altitude and Mach given: the specific range at full, half and nearly
// empty tanks - a twentieth - integrated over the fuel by Simpson's rule, the
// last twentieth counted at the nearly empty tanks' rate. As the fuel burns the
// aircraft lightens and flies further on each pound. No fuel is kept back.
double cruise_range(const std::filesystem::path& root, const PublishedFigures& figures,
                    const FigureSpec& spec) {
    const FigureLoading& loading = figures.loadings.at(spec.loading);
    double fuel = 0.0;
    for (const auto& [tank, lbs] : loading.loading.tank_lbs) {
        fuel += lbs;
    }
    const double full = specific_range(root, figures, spec, 1.0);
    const double half = specific_range(root, figures, spec, 0.525);
    const double empty = specific_range(root, figures, spec, 0.05);
    return 0.95 * fuel / 6.0 * (full + 4.0 * half + empty) + 0.05 * fuel * empty;
}

using FlightFn = std::function<double(const std::filesystem::path&,
                                      const PublishedFigures&, const FigureSpec&)>;

const std::vector<std::pair<std::string, FlightFn>>& flights() {
    static const std::vector<std::pair<std::string, FlightFn>> all = {
        {"static_rpm", static_rpm},
        {"takeoff_ground_roll", takeoff_ground_roll},
        {"takeoff_distance", takeoff_distance},
        {"takeoff_swing", takeoff_swing},
        {"climb_rate", climb_rate},
        {"time_to_altitude", time_to_altitude},
        {"level_speed", level_speed},
        {"cruise_speed", cruise_speed},
        {"glide_ratio", glide_ratio},
        {"stall_speed", stall_speed},
        {"turn_rate", turn_rate},
        {"safety_speed", safety_speed},
        {"single_engine_ceiling", single_engine_ceiling},
        {"takeoff_field_length", takeoff_field_length},
        {"climb_gradient_one_engine", climb_gradient_one_engine},
        {"level_mach", level_mach},
        {"best_level_mach", best_level_mach},
        {"climb_at_altitude", climb_at_altitude},
        {"sustained_turn_rate", sustained_turn_rate},
        {"max_climb_rate", max_climb_rate},
        {"service_ceiling", service_ceiling},
        {"acceleration_time", acceleration_time},
        {"cruise_range", cruise_range},
    };
    return all;
}

const FlightFn* flight_for(const std::string& name) {
    for (const auto& [n, fn] : flights()) {
        if (n == name) {
            return &fn;
        }
    }
    return nullptr;
}

std::string collapse_whitespace(const std::string& text) {
    std::istringstream in(text);
    std::string word;
    std::string out;
    while (in >> word) {
        out += (out.empty() ? "" : " ") + word;
    }
    return out;
}

FigureLoading read_loading(JSBSim::Element* loading) {
    FigureLoading out;
    out.total_lbs = loading->GetAttributeValueAsNumber("total_lbs");
    for (JSBSim::Element* e = loading->FindElement("pointmass"); e != nullptr;
         e = loading->FindNextElement("pointmass")) {
        out.loading
            .pointmass_lbs[static_cast<int>(e->GetAttributeValueAsNumber("index"))] =
            e->GetAttributeValueAsNumber("lbs");
    }
    for (JSBSim::Element* e = loading->FindElement("tank"); e != nullptr;
         e = loading->FindNextElement("tank")) {
        out.loading.tank_lbs[static_cast<int>(e->GetAttributeValueAsNumber("index"))] =
            e->GetAttributeValueAsNumber("lbs");
    }
    return out;
}

} // namespace

std::vector<std::string> known_figures() {
    std::vector<std::string> names;
    for (const auto& [name, fn] : flights()) {
        names.push_back(name);
    }
    return names;
}

PublishedFigures read_published_figures(const std::filesystem::path& file) {
    JSBSim::FGXMLFileRead reader;
    const std::u8string utf8 = file.u8string();
    JSBSim::Element* root = reader.LoadXMLDocument(
        SGPath::fromUtf8(std::string(utf8.begin(), utf8.end())), false);
    if (root == nullptr || root->GetName() != "published_figures") {
        throw std::runtime_error("no <published_figures> in " + file.string());
    }

    PublishedFigures out;
    out.model = root->GetAttributeValue("aircraft");
    if (!root->HasAttribute("flaps_full_deg")) {
        throw std::runtime_error(file.string() +
                                 " does not give flaps_full_deg, the flaps' travel");
    }
    out.flaps_full_deg = root->GetAttributeValueAsNumber("flaps_full_deg");
    if (JSBSim::Element* source = root->FindElement("source")) {
        out.source = collapse_whitespace(source->GetDataLine());
    }

    std::string first_loading;
    for (JSBSim::Element* e = root->FindElement("loading"); e != nullptr;
         e = root->FindNextElement("loading")) {
        const std::string name = e->GetAttributeValue("name");
        if (out.loadings.count(name) != 0) {
            throw std::runtime_error(file.string() + " has two loadings named '" +
                                     name + "'");
        }
        out.loadings[name] = read_loading(e);
        if (out.loadings.size() == 1) {
            first_loading = name;
            out.total_lbs = out.loadings[name].total_lbs;
            out.loading = out.loadings[name].loading;
        }
    }
    if (out.loadings.empty()) {
        throw std::runtime_error(file.string() + " has no <loading>");
    }

    for (JSBSim::Element* e = root->FindElement("figure"); e != nullptr;
         e = root->FindNextElement("figure")) {
        FigureSpec spec;
        spec.name = e->GetAttributeValue("name");
        spec.flight = e->HasAttribute("flight") ? e->GetAttributeValue("flight") : spec.name;
        spec.loading =
            e->HasAttribute("loading") ? e->GetAttributeValue("loading") : first_loading;
        spec.unit = e->GetAttributeValue("unit");
        if (flight_for(spec.flight) == nullptr) {
            throw std::runtime_error(file.string() + " measures figure '" + spec.name +
                                     "' by flight '" + spec.flight +
                                     "', which does not exist");
        }
        if (out.loadings.count(spec.loading) == 0) {
            throw std::runtime_error(file.string() + " flies figure '" + spec.name +
                                     "' at loading '" + spec.loading +
                                     "', which it does not have");
        }
        std::string text;
        for (unsigned i = 0; i < e->GetNumDataLines(); ++i) {
            text += e->GetDataLine(i) + " ";
        }
        spec.source = collapse_whitespace(text);

        if (e->HasAttribute("min") && e->HasAttribute("max")) {
            spec.low = e->GetAttributeValueAsNumber("min");
            spec.high = e->GetAttributeValueAsNumber("max");
            spec.published = (spec.low + spec.high) / 2.0;
        } else if (e->HasAttribute("value") && e->HasAttribute("tolerance_percent")) {
            spec.published = e->GetAttributeValueAsNumber("value");
            const double t = std::abs(spec.published) *
                             e->GetAttributeValueAsNumber("tolerance_percent") / 100.0;
            spec.low = spec.published - t;
            spec.high = spec.published + t;
        } else if (e->HasAttribute("value") && e->HasAttribute("tolerance")) {
            spec.published = e->GetAttributeValueAsNumber("value");
            spec.low = spec.published - e->GetAttributeValueAsNumber("tolerance");
            spec.high = spec.published + e->GetAttributeValueAsNumber("tolerance");
        } else {
            throw std::runtime_error(
                "figure " + spec.name + " in " + file.string() +
                " gives neither min and max nor a value and a tolerance");
        }

        for (const char* key :
             {"flaps_deg", "lift_off_kcas", "speed_kcas", "altitude_ft", "rpm",
              "bank_deg", "boost_psi", "gear", "entry_kcas", "fs_gear_above_ft",
              "radiators_open", "gear_change_boost_drop", "manifold_inhg", "lean",
              "takeoff_pitch_deg", "mach", "throttle", "from_kcas", "to_kcas", "rate_fpm",
              "mach_to", "fuel_frozen", "altitude_to_ft"}) {
            if (e->HasAttribute(key)) {
                spec.conditions[key] = e->GetAttributeValueAsNumber(key);
            }
        }
        out.figures.push_back(std::move(spec));
    }
    return out;
}

FigureResult fly_figure(const std::filesystem::path& jsbsim_root,
                        const PublishedFigures& figures, const FigureSpec& spec) {
    const FlightFn* fn = flight_for(spec.flight);
    if (fn == nullptr) {
        throw std::runtime_error("no flight exists for figure " + spec.name);
    }
    FigureResult result;
    result.spec = &spec;
    result.measured = (*fn)(jsbsim_root, figures, spec);
    return result;
}

} // namespace glideslope::sim
