#include "sim/figures.hpp"

#include "sim/fixed_step.hpp"

#include <input_output/FGXMLElement.h>
#include <input_output/FGXMLFileRead.h>
#include <simgear/misc/sg_path.hxx>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);
constexpr double g_fps2 = 32.174;

int steps(double seconds) {
    return static_cast<int>(
        std::lround(seconds * static_cast<double>(steps_per_second)));
}

double clamp_unit(double x) {
    return std::clamp(x, -1.0, 1.0);
}

// ---------------------------------------------------------------------------
// The test pilot.
//
// Just enough flying to hold the conditions a handbook figure was measured in:
// a pitch attitude, and above it a speed or an altitude; a bank angle; and the
// ball in the middle. It is not the product's autopilot (Phase 4) and is not
// meant to fly like a person - it is meant to hold one condition steadily so
// that one number can be read off.
// ---------------------------------------------------------------------------
class TestPilot {
public:
    explicit TestPilot(const Aircraft& aircraft) : a_(aircraft) {}

    // Elevator for a pitch attitude, with an integral that finds the trim.
    double pitch_to(double theta_deg) {
        const double error = theta_deg - a_.property("attitude/theta-deg");
        const double q_degps =
            a_.property("velocities/q-rad_sec") * 180.0 / std::numbers::pi;
        trim_ = std::clamp(trim_ + 0.02 * error * dt, -1.0, 1.0);
        return clamp_unit(trim_ + 0.05 * error - 0.03 * q_degps);
    }

    // A pitch attitude that brings calibrated airspeed to `kcas`: nose up when
    // fast. Slow, so that it settles rather than chases the phugoid.
    double pitch_for_speed(double kcas) {
        const double error = a_.property("velocities/vc-kts") - kcas;
        speed_integral_ = std::clamp(speed_integral_ + error * dt, -1000.0, 1000.0);
        return std::clamp(0.6 * error + 0.03 * speed_integral_, -15.0, 20.0);
    }

    // A pitch attitude that brings the aircraft to `altitude_ft` and holds it.
    double pitch_for_altitude(double altitude_ft) {
        const double error = altitude_ft - a_.property("position/h-sl-ft");
        const double climb_fps = a_.property("velocities/h-dot-fps");
        altitude_integral_ =
            std::clamp(altitude_integral_ + error * dt, -3000.0, 3000.0);
        return std::clamp(0.01 * error - 0.08 * climb_fps + 0.0005 * altitude_integral_,
                          -10.0, 15.0);
    }

    // Aileron for a bank angle.
    double roll_to(double phi_deg) const {
        const double phi = a_.property("attitude/phi-deg");
        const double p_degps =
            a_.property("velocities/p-rad_sec") * 180.0 / std::numbers::pi;
        return clamp_unit(0.04 * (phi_deg - phi) - 0.02 * p_degps);
    }

    // Rudder against sideslip, with an integral that finds the rudder a steady
    // turn needs. Without the integral the aircraft slipped half a degree in a
    // 30-degree turn and turned 2% slower than its bank demanded.
    double coordinate() {
        const double beta = a_.property("aero/beta-deg");
        rudder_integral_ = std::clamp(rudder_integral_ - 0.05 * beta * dt, -1.0, 1.0);
        return clamp_unit(-0.1 * beta + rudder_integral_);
    }

private:
    const Aircraft& a_;
    double trim_ = 0.0;
    double speed_integral_ = 0.0;
    double altitude_integral_ = 0.0;
    double rudder_integral_ = 0.0;
};

double condition(const FigureSpec& spec, const std::string& key) {
    const auto it = spec.conditions.find(key);
    if (it == spec.conditions.end()) {
        throw std::runtime_error("figure " + spec.name + " needs a " + key +
                                 " attribute");
    }
    return it->second;
}

// An aircraft loaded as the figures file says, and put somewhere.
struct Flight {
    Aircraft aircraft;
    TestPilot pilot;

    Flight(const std::filesystem::path& root, const PublishedFigures& figures,
           const InitialConditions& ic)
        : aircraft(root, figures.model), pilot(aircraft) {
        aircraft.load(figures.loading);
        aircraft.initialize(ic);
        const double weight = aircraft.property("inertia/weight-lbs");
        if (std::abs(weight - figures.total_lbs) > 1.0) {
            std::ostringstream m;
            m << figures.model << " loaded as its figures file says weighs " << weight
              << " lb, not the " << figures.total_lbs << " lb the file states";
            throw std::runtime_error(m.str());
        }
    }

    void fly(const Controls& c) {
        aircraft.set_controls(c);
        aircraft.step();
    }
};

InitialConditions on_the_runway() {
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    return ic;
}

// Airborne, over terrain far enough below that it plays no part.
InitialConditions airborne(double altitude_ft, double kcas, bool engine_running) {
    InitialConditions ic = on_the_runway();
    ic.altitude_ft = altitude_ft;
    ic.terrain_elevation_ft = std::min(0.0, altitude_ft) - 3000.0;
    ic.airspeed_kts = kcas;
    ic.engine_running = engine_running;
    return ic;
}

double flaps_command(double flaps_deg) {
    return flaps_deg / 30.0; // the model's flaps travel 0 to 30 degrees over 0 to 1
}

// ---------------------------------------------------------------------------
// The flights, one per figure.
// ---------------------------------------------------------------------------

// Brakes on, full throttle, mixture leaned in steps of 0.05; the highest RPM any
// mixture reaches after fifteen seconds.
double static_rpm(const std::filesystem::path& root, const PublishedFigures& figures,
                  const FigureSpec&) {
    Flight f(root, figures, on_the_runway());
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
    Flight f(root, figures, on_the_runway());
    Controls c;
    c.throttle = 1.0;
    c.flaps = flaps_command(condition(spec, "flaps_deg"));
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

// Full throttle, flaps up, holding the climb speed; the average climb rate over
// forty seconds that pass through sea level, after thirty to settle.
double climb_rate(const std::filesystem::path& root, const PublishedFigures& figures,
                  const FigureSpec& spec) {
    const double kcas = condition(spec, "speed_kcas");
    Flight f(root, figures, airborne(-600.0, kcas, true));
    Controls c;
    c.throttle = 1.0;
    const auto hold = [&] {
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

// Level at the cruise altitude: first full throttle with the mixture leaned in
// steps to find the richest setting giving the most RPM, then the throttle
// holding the cruise RPM; the average true airspeed over the last fifty
// seconds of three minutes at that RPM.
double cruise_speed(const std::filesystem::path& root, const PublishedFigures& figures,
                    const FigureSpec& spec) {
    const double altitude = condition(spec, "altitude_ft");
    const double rpm_target = condition(spec, "rpm");
    Flight f(root, figures, airborne(altitude, 110.0, true));
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
    Flight f(root, figures, airborne(6500.0, kcas, false));
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

// Power off, wings level, flaps set; from 70 KCAS the target speed falls at one
// knot a second, the handbook's rate for a stall, and the pilot raises the
// nose to follow it until the elevator can raise it no further. The lowest
// calibrated airspeed reached.
double stall_speed(const std::filesystem::path& root, const PublishedFigures& figures,
                   const FigureSpec& spec) {
    Flight f(root, figures, airborne(5000.0, 70.0, true));
    Controls c;
    c.throttle = 0.0;
    c.flaps = flaps_command(condition(spec, "flaps_deg"));
    double slowest = 1e9;
    for (int i = 0; i < steps(80); ++i) {
        const double t = i * dt;
        const double target = t < 10.0 ? 70.0 : 70.0 - (t - 10.0);
        c.elevator = f.pilot.pitch_to(f.pilot.pitch_for_speed(target));
        c.aileron = f.pilot.roll_to(0.0);
        c.rudder = f.pilot.coordinate();
        f.fly(c);
        if (t >= 10.0) {
            slowest = std::min(slowest, f.aircraft.property("velocities/vc-kts"));
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
    Flight f(root, figures, airborne(altitude, 100.0, true));
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

using FlightFn = std::function<double(const std::filesystem::path&,
                                      const PublishedFigures&, const FigureSpec&)>;

const std::vector<std::pair<std::string, FlightFn>>& flights() {
    static const std::vector<std::pair<std::string, FlightFn>> all = {
        {"static_rpm", static_rpm},
        {"takeoff_ground_roll", takeoff_ground_roll},
        {"climb_rate", climb_rate},
        {"cruise_speed", cruise_speed},
        {"glide_ratio", glide_ratio},
        {"stall_speed_flaps_up", stall_speed},
        {"stall_speed_flaps_10", stall_speed},
        {"stall_speed_flaps_30", stall_speed},
        {"turn_rate", turn_rate},
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
    if (JSBSim::Element* source = root->FindElement("source")) {
        out.source = collapse_whitespace(source->GetDataLine());
    }

    JSBSim::Element* loading = root->FindElement("loading");
    if (loading == nullptr) {
        throw std::runtime_error(file.string() + " has no <loading>");
    }
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

    for (JSBSim::Element* e = root->FindElement("figure"); e != nullptr;
         e = root->FindNextElement("figure")) {
        FigureSpec spec;
        spec.name = e->GetAttributeValue("name");
        spec.unit = e->GetAttributeValue("unit");
        if (flight_for(spec.name) == nullptr) {
            throw std::runtime_error(file.string() + " names figure '" + spec.name +
                                     "', which no flight exists for");
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

        for (const char* key : {"flaps_deg", "lift_off_kcas", "speed_kcas",
                                "altitude_ft", "rpm", "bank_deg"}) {
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
    const FlightFn* fn = flight_for(spec.name);
    if (fn == nullptr) {
        throw std::runtime_error("no flight exists for figure " + spec.name);
    }
    FigureResult result;
    result.spec = &spec;
    result.measured = (*fn)(jsbsim_root, figures, spec);
    return result;
}

} // namespace glideslope::sim
