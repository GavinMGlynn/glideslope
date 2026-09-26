#include "sim/departure.hpp"

#include "sim/figures.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <stdexcept>

namespace glideslope::sim {
namespace {

constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double feet_per_metre = 3.280839895013123;
constexpr double steps_per_second = 120.0;
// Below this the rudder is too soft to hold the nose, and the brakes help.
constexpr double rudder_bites_kts = 60.0;
// How far past the rotation speed a rotation begun there may leave the
// runway before it is begun sooner, knots.
constexpr double lift_off_after_kts = 3.0;
// The least a take-off climbs to the screen height, degrees of flight path.
constexpr double climb_floor_deg = 3.0;
// A model's pitch trim for take-off, where its flight manual gives one.
constexpr const char* takeoff_trim_property = "fcs/pitch-trim-takeoff-norm";
// How fast the take-off trim is taken off once she is flying, of its travel a
// second.
constexpr double takeoff_trim_washout_per_s = 0.1;
// Her nose this far above the attitude she stands at, as she leaves the
// ground, is a rotation, degrees.
constexpr double self_rotated_deg = 2.0;
// How far short of the attitude her tail strikes at the nose is held on the
// wheels, degrees.
constexpr double strike_margin_deg = 2.0;

double metres_per_degree_latitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111132.92 - 559.82 * std::cos(2.0 * lat) + 1.175 * std::cos(4.0 * lat) -
           0.0023 * std::cos(6.0 * lat);
}

double metres_per_degree_longitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111412.84 * std::cos(lat) - 93.5 * std::cos(3.0 * lat) +
           0.118 * std::cos(5.0 * lat);
}

// A figure by the flight that measures it, or null.
const FigureSpec* by_flight(const PublishedFigures& figures, const std::string& flight) {
    for (const FigureSpec& spec : figures.figures) {
        if (spec.flight == flight) {
            return &spec;
        }
    }
    return nullptr;
}

// How long the rotation takes: the nose raised from where she sits to the
// take-off attitude at a pilot's four degrees a second.
double rotation_lead_s(double pitch_deg) {
    return std::max(0.0, (10.0 - pitch_deg) / 4.0);
}

// What the aeroplane weighed for a figure: its loading's total, or the
// file's first loading's where it names none.
double weighed_lbs(const PublishedFigures& figures, const FigureSpec& spec) {
    const auto at = figures.loadings.find(spec.loading);
    return at != figures.loadings.end() ? at->second.total_lbs : figures.total_lbs;
}

double condition(const FigureSpec& spec, const std::string& name, double missing) {
    const auto at = spec.conditions.find(name);
    return at == spec.conditions.end() ? missing : at->second;
}

} // namespace

DepartureSpeeds departure_speeds(const std::filesystem::path& data,
                                 const std::string& model) {
    const PublishedFigures figures =
        read_published_figures(data / "figures" / (model + ".xml"));

    DepartureSpeeds speeds;
    const FigureSpec* climb = by_flight(figures, "climb_rate");
    if (climb == nullptr) {
        throw std::runtime_error(model +
                                 " publishes no rate of climb, so there is no "
                                 "speed to climb away at");
    }
    speeds.climb_kts = condition(*climb, "speed_kcas", 0.0);
    if (speeds.climb_kts <= 0.0) {
        throw std::runtime_error(model +
                                 " publishes a rate of climb but not the speed "
                                 "it was measured at");
    }

    speeds.initial_climb_kts = speeds.climb_kts;

    // A flying boat's published water take-off gives the run's attitude, the
    // speed it is raised from and the flap it was flown with - at the loading
    // a figure naming none is flown at, which is the standard boat.
    for (const FigureSpec& spec : figures.figures) {
        if (spec.flight == "water_takeoff" && spec.loading == figures.first_loading &&
            condition(spec, "rotate_kcas", 0.0) > 0.0) {
            speeds.rotate_kts = condition(spec, "rotate_kcas", 0.0);
            speeds.rotate_is_published = true;
            speeds.running_pitch_deg = condition(spec, "running_pitch_deg", 8.0);
            const double flap_deg = condition(spec, "flaps_deg", 0.0);
            speeds.flap = figures.flaps_full_deg > 0.0
                              ? std::clamp(flap_deg / figures.flaps_full_deg, 0.0, 1.0)
                              : 0.0;
            return speeds;
        }
    }

    // A published take-off field length names the flap it is flown with. The
    // stall at that flap, where the aeroplane has one, is what it rotates
    // from, and it climbs away at V2 and ten.
    if (const FigureSpec* field = by_flight(figures, "takeoff_field_length");
        field != nullptr) {
        const double field_flap_deg = condition(*field, "flaps_deg", 0.0);
        for (const FigureSpec& spec : figures.figures) {
            if (spec.flight == "stall_speed" &&
                std::abs(condition(spec, "flaps_deg", -1.0) - field_flap_deg) < 0.5) {
                speeds.rotate_kts = 1.15 * spec.published;
                speeds.rotate_is_published = false;
                speeds.initial_climb_kts = 1.2 * spec.published + 10.0;
                speeds.reference_lbs = weighed_lbs(figures, spec);
                speeds.flap = figures.flaps_full_deg > 0.0
                                  ? std::clamp(field_flap_deg / figures.flaps_full_deg,
                                               0.0, 1.0)
                                  : 0.0;
                return speeds;
            }
        }
    }

    // The take-off roll, where it has one, gives both the lift-off speed and
    // the flap it was flown with.
    if (const FigureSpec* roll = by_flight(figures, "takeoff_ground_roll");
        roll != nullptr && condition(*roll, "lift_off_kcas", 0.0) > 0.0) {
        speeds.rotate_kts = condition(*roll, "lift_off_kcas", 0.0);
        speeds.rotate_is_published = !roll->measured;
        speeds.reference_lbs = weighed_lbs(figures, *roll);
        const double flap_deg = condition(*roll, "flaps_deg", 0.0);
        speeds.flap = figures.flaps_full_deg > 0.0
                          ? std::clamp(flap_deg / figures.flaps_full_deg, 0.0, 1.0)
                          : 0.0;
        return speeds;
    }

    // Otherwise from the stall, by the usual relation. The flaps-up stall is
    // the one to use: an aeroplane with no published take-off roll has no
    // published take-off flap either, so it leaves the ground clean.
    const FigureSpec* cleanest = nullptr;
    double least_flap_deg = 0.0;
    for (const FigureSpec& spec : figures.figures) {
        if (spec.flight != "stall_speed") {
            continue;
        }
        const double flap_deg = condition(spec, "flaps_deg", 0.0);
        if (cleanest == nullptr || flap_deg < least_flap_deg) {
            cleanest = &spec;
            least_flap_deg = flap_deg;
        }
    }
    if (cleanest == nullptr) {
        throw std::runtime_error(model +
                                 " publishes neither a take-off roll nor a "
                                 "stall speed, so there is nothing to work a "
                                 "rotation speed from");
    }
    speeds.rotate_kts = 1.15 * cleanest->published;
    speeds.rotate_is_published = false;
    speeds.reference_lbs = weighed_lbs(figures, *cleanest);
    speeds.flap = 0.0;
    return speeds;
}

Departure::Departure(const Aircraft& aircraft, const Runway& runway,
                     const DepartureSpeeds& speeds, double to_ft)
    : a_(aircraft), runway_(runway), speeds_(speeds), to_ft_(to_ft) {
    measure();
    standing_m_ = above_m_;
    standing_pitch_deg_ = a_.state().pitch_deg;
    read_the_gear();
    tail_pitch_deg_ = standing_pitch_deg_;
    // **Her speeds for what she weighs.** The figures give them at the
    // weight their loading names; a speed that holds her up goes as the
    // square root of her weight. At its model's own loading the B-2A
    // weighs 327,000 lb against the 177,160 its rotation speed was taken
    // at, and asked to fly at the lighter aeroplane's speed it was hauled
    // on to its tail at 110 knots and left the runway at 148. On the water
    // a published water take-off is flown as published.
    if (speeds_.reference_lbs > 0.0 && speeds_.running_pitch_deg <= 0.0) {
        const double scale = std::sqrt(a_.property("inertia/weight-lbs") / speeds_.reference_lbs);
        speeds_.rotate_kts *= scale;
        speeds_.initial_climb_kts *= scale;
    }
}

// **What she stands on, worked from her model's own contacts**, as they are
// placed - not from which of them touch, since she is started level and
// settles on to her tail or her nose after. Her main wheels are the lowest
// contacts off the centreline. Pivoting on them, she falls the way her
// centre of gravity lies until the first centreline contact that way meets
// the ground: that is her nose wheel, or her tail wheel, and the angle it
// takes is the attitude she stands at. The A320's model makes its tail skid
// and wing tips wheels as well, and counting every wheel took it for a
// tail-wheel aeroplane. JSBSim's structural x runs aft and z up, in inches.
void Departure::read_the_gear() {
    struct Point {
        double x, y, z;
    };
    std::vector<Point> points;
    for (int unit = 0; unit < 64; ++unit) {
        for (const char* kind : {"gear/unit[", "contact/unit["}) {
            const std::string at = kind + std::to_string(unit) + "]/";
            if (a_.has_property(at + "WOW")) {
                points.push_back({a_.property(at + "x-position"), a_.property(at + "y-position"),
                                  a_.property(at + "z-position")});
            }
        }
    }
    double main_z = 1e9;
    for (const Point& p : points) {
        if (std::abs(p.y) > 1.0) {
            main_z = std::min(main_z, p.z);
        }
    }
    double main_x = -1e9;
    for (const Point& p : points) {
        if (std::abs(p.y) > 1.0 && p.z < main_z + 1.0) {
            main_x = std::max(main_x, p.x);
        }
    }
    if (main_x < -1e8) {
        return;
    }
    // The pitch, nose up positive, at which a point meets the ground
    // pivoting on the main wheels.
    const auto meets = [&](const Point& p) {
        return std::atan((p.z - main_z) / (p.x - main_x)) * degrees;
    };
    const bool tail_down = a_.property("inertia/cg-x-in") > main_x;
    const Point* stands_on = nullptr;
    for (const Point& p : points) {
        if (std::abs(p.y) <= 1.0 && std::abs(p.x - main_x) > 1.0 &&
            (p.x > main_x) == tail_down &&
            (stands_on == nullptr || std::abs(meets(p)) < std::abs(meets(*stands_on)))) {
            stands_on = &p;
        }
    }
    if (stands_on == nullptr) {
        return;
    }
    tail_wheel_ = tail_down;
    standing_pitch_deg_ = meets(*stands_on);
    // **The attitude her tail strikes at**: the lowest, pivoting on her
    // main wheels, at which anything behind them meets the ground - a tail
    // skid, a tail cone, a nacelle. None, for a tail-wheel aeroplane, whose
    // tail is on the ground already.
    if (!tail_wheel_) {
        for (const Point& p : points) {
            if (p.x > main_x + 1.0) {
                strike_pitch_deg_ = std::min(strike_pitch_deg_, meets(p));
            }
        }
    }
}

void Departure::measure() {
    const AircraftState s = a_.state();
    const double north_m = (s.latitude_deg - runway_.threshold_lat_deg) *
                           metres_per_degree_latitude(runway_.threshold_lat_deg);
    const double east_m = (s.longitude_deg - runway_.threshold_lon_deg) *
                          metres_per_degree_longitude(runway_.threshold_lat_deg);
    const double heading = runway_.heading_deg / degrees;
    along_m_ = east_m * std::sin(heading) + north_m * std::cos(heading);
    across_m_ = east_m * std::cos(heading) - north_m * std::sin(heading);
    above_m_ = (s.altitude_ft - runway_.elevation_ft) / feet_per_metre;
}

Controls Departure::fly() {
    measure();
    const AircraftState s = a_.state();
    const double kcas = s.airspeed_kts;

    Controls c;
    c.gear = 1.0;
    c.mixture = 1.0;
    c.propeller = 1.0;
    c.flaps = speeds_.flap;
    // **Trimmed for take-off**, where the aeroplane's model says what that
    // is: the Learjet 35A's flight manual sets its stabilizer for take-off by
    // the centre of gravity (figure 2-2), and without it the elevator alone
    // could not lift her nose wheel until twenty knots past her rotation
    // speed. An aeroplane with no such setting is left at none.
    //
    // **And the setting is taken off again once she is off the ground**, a
    // tenth of its travel a second, and carried by the elevator instead,
    // whose authority the Learjet's is near: left on, it was handed to the
    // autopilot with the rest of the controls and kept for the whole
    // flight, and at 350 knots the elevator held her at 0.94 of its travel
    // nose down against it.
    if (a_.has_property(takeoff_trim_property)) {
        if (!unstuck_) {
            takeoff_trim_ = a_.property(takeoff_trim_property);
        } else {
            const double step = std::clamp(takeoff_trim_, -takeoff_trim_washout_per_s / steps_per_second,
                                           takeoff_trim_washout_per_s / steps_per_second);
            takeoff_trim_ -= step;
            pitch_trim_ += step;
        }
        c.pitch_trim = takeoff_trim_;
    }

    // A hull in the water is on the ground, as far as a take-off goes: it has
    // no weight on any wheel, and taken for airborne it would be flown by the
    // airborne law from a standstill.
    const bool on_ground = a_.property("gear/wow") > 0.5 || a_.in_water();
    const bool on_water = speeds_.running_pitch_deg > 0.0;
    if (!unstuck_ && !on_ground && (above_m_ - standing_m_) * feet_per_metre > 5.0) {
        unstuck_ = true;
        unstuck_along_m_ = along_m_;
    }
    // **Rotated off the runway, or bounced off it.** As the wheels leave,
    // the stick as it was - whoever held it, this autopilot or a pilot
    // rotating her early; JSBSim's command is nose down positive - says
    // which. The climb below is held off the runway only if she was rotated.
    //
    // **Or by herself**: her nose up off the attitude she stands at as she
    // leaves is a rotation, whoever or whatever made it. The Learjet 35A at
    // its model's own loading sits aft of the last row of its flight
    // manual's take-off trim, and on that trim lifted her own nose on the
    // roll; taken for a hop, she was not held off the runway, and was flown
    // back on to it sinking at 1,784 ft/min.
    if (was_on_ground_ && !on_ground && !rotated_off_) {
        rotated_off_ = rotation_begun_ || -a_.property("fcs/elevator-cmd-norm") > 0.2 ||
                       s.pitch_deg > standing_pitch_deg_ + self_rotated_deg;
    }
    if (was_on_ground_ && !on_ground) {
        left_at_pitch_deg_ = s.pitch_deg;
    }
    was_on_ground_ = on_ground;
    // **Rotated by a pilot, the rotation is carried on from there.** Back
    // stick on the roll that this autopilot did not put there - a pilot
    // rotating her early - begins the rotation stage, so that the stick she
    // is handed back with is held and the nose not put back down on to the
    // runway: in the roll stage the stick goes to neutral, and a J-3 Cub
    // pulled off at 30 knots and let go ran on to 39 before she left. Not
    // put there is a fifth of its travel further back than it last asked
    // for, since it holds a tail-wheel aeroplane's tail up with the stick.
    const bool pilot_rotated = stage_ == Stage::roll && on_ground &&
                               -a_.property("fcs/elevator-cmd-norm") > last_elevator_ + 0.2;

    if (stage_ != Stage::done && above_m_ * feet_per_metre >= to_ft_) {
        stage_ = Stage::done;
    } else if (unstuck_) {
        stage_ = Stage::climb;
        // The flaps come up once the aeroplane is safely climbing away.
        if (above_m_ * feet_per_metre > 200.0) {
            c.flaps = 0.0;
        }
    } else if (kcas >= speeds_.rotate_kts || (!on_water && pilot_rotated) ||
               (!on_water && kcas + std::max(0.0, accel_ktps_ * rotation_lead_s(s.pitch_deg) -
                                                      lift_off_after_kts) >=
                                 speeds_.rotate_kts)) {
        // **The rotation begins as early as it takes**, so that she leaves
        // the ground within lift_off_after_kts of the speed she should leave
        // it at. `rotate_kts` is that speed - a published lift-off speed, or
        // a seventh above the stall - and a rotation begun there leaves the
        // ground as far past it as she accelerates while the nose comes up:
        // for an airliner or a light aeroplane a few knots, which is left as
        // it was, but an F-15C gains thirteen knots a second, and rotated
        // from 174 knots it was off at 201. The F-15's own flight manual
        // brings the stick back at 120 knots for a take-off at 141 (T.O.
        // 1F-15A-1, section II and figure A3-6). On the water the hull is
        // held on the step until the speed itself, as its published take-off
        // is flown.
        stage_ = Stage::rotate;
        if (rotation_began_kts_ == 0.0) {
            rotation_began_kts_ = kcas;
        }
    }
    // How fast she is gaining speed, smoothed over a second.
    if (last_kcas_ > 0.0) {
        const double now = (kcas - last_kcas_) * steps_per_second;
        accel_ktps_ += (now - accel_ktps_) / steps_per_second;
    }
    last_kcas_ = kcas;

    // **The throttle goes fully open over three seconds** and stays there:
    // an engine slammed open swings a tail-wheel aeroplane off the runway.
    //
    // **Fully open is 0.99: military power, not afterburner.** JSBSim lights
    // an afterburning turbine's reheat above 0.99, and the F-15's own flight
    // manual (T.O. 1F-15A-1, section II, "Takeoff") gives MIL as a normal
    // take-off. In afterburner the F-15C, light and clean, gained 24 knots
    // a second and was off the ground eighty knots past its rotation speed
    // whatever was done with the stick. To any other engine 0.99 is full
    // throttle, less a hundredth.
    constexpr double full = 0.99;
    throttle_ = std::min(throttle_ + 1.0 / (3.0 * steps_per_second), full);
    c.throttle = throttle_;

    // --- the nose, down the centreline ------------------------------------
    //
    // **On the ground is the roll, whatever the stage said** - the mirror of
    // the rollout in `sim/lander.hpp`, and for the same reason: the wheels
    // decide, not the height and not the stage. An aeroplane that has bounced
    // is not flying. The Mosquito bounces at about 97 knots, which latched
    // `unstuck_` and handed her to the airborne law - bank to hold a heading
    // - while she was still on the runway at 115 knots. She rolled on for
    // fourteen seconds with no steering on the wheels at all and swung 45
    // degrees off the centreline, with the rudder sitting at a tenth of its
    // travel because the airborne law only had a little sideslip to answer.
    //
    // `unstuck_` itself is left alone: where she first came off is where the
    // ground roll ends, and the published take-off distances are measured
    // from it.
    if (stage_ == Stage::roll || stage_ == Stage::rotate || on_ground) {
        // On the ground the rudder and the nosewheel are one control, and
        // below the speed at which the rudder bites the brakes help it.
        //
        // **On open water there is no centreline, only a heading**, and a
        // hull answers its rudder slowly: steering back to a line as a
        // runway asks, the Short S.23 weaved twenty degrees either side of
        // her heading and never came off the water.
        const double want = on_water ? 0.0 : std::clamp(-across_m_ * 2.0, -15.0, 15.0);
        const double error =
            std::remainder(runway_.heading_deg + want - s.heading_deg, 360.0);
        const double r_degps = s.r_radps * degrees;
        // `turn` is positive to swing the nose right. **The model's rudder
        // command yaws the nose left for a positive value**
        // (sim/test_pilot.cpp), so the rudder takes the opposite sign; the
        // brake is on the side being turned towards.
        const double turn = on_water
                                ? std::clamp(0.05 * error - 0.60 * r_degps, -1.0, 1.0)
                                : std::clamp(0.10 * error - 0.30 * r_degps, -1.0, 1.0);
        c.rudder = -turn;
        if (kcas < rudder_bites_kts) {
            c.left_brake = std::max(-turn, 0.0) * 0.5;
            c.right_brake = std::max(turn, 0.0) * 0.5;
        }
        c.aileron = std::clamp(-0.02 * s.roll_deg, -1.0, 1.0);
    } else {
        // Flying: wings level on the runway heading.
        const double error = std::remainder(runway_.heading_deg - s.heading_deg, 360.0);
        const double want_bank = std::clamp(error * 1.2, -20.0, 20.0);
        const double p_degps = s.p_radps * degrees;
        c.aileron =
            std::clamp(0.035 * (want_bank - s.roll_deg) - 0.02 * p_degps, -1.0, 1.0);
        const double beta_deg = a_.property("aero/beta-deg");
        c.rudder = std::clamp(-0.05 * beta_deg, -1.0, 1.0);
    }

    // --- the elevator ------------------------------------------------------
    double want_pitch = 0.0;
    if (on_water && (stage_ == Stage::roll || stage_ == Stage::rotate)) {
        // **On the water the hull is held at its running attitude** - over
        // the hump and on to the step, where too low an attitude or too high
        // sets off porpoising (FAA-H-8083-23, chapter 4) - and three degrees
        // higher from the rotation speed until it is clear.
        want_pitch = speeds_.running_pitch_deg + (stage_ == Stage::rotate ? 3.0 : 0.0);
        // The climb begins from the attitude she left the water at.
        rotate_pitch_ = want_pitch;
    } else if (stage_ == Stage::roll && !(tail_wheel_ && kcas >= 0.5 * speeds_.rotate_kts)) {
        // The stick is held where the aeroplane sits: a tail-wheel aeroplane
        // wants its tail down until it has the speed to lift it.
        c.elevator = 0.0;
        last_elevator_ = c.elevator;
        return c;
    } else if (stage_ == Stage::roll) {
        // **A tail-wheel aeroplane's tail comes up on the roll** - from half
        // her rotation speed, two degrees a second, until she rolls level on
        // her main wheels - and she is rotated from there. Left on three
        // points, the Mosquito sat at twelve degrees of incidence, close to
        // her stall, and flew herself off at 90 knots before her rotation
        // speed, which pulling the stick back could only make later: she was
        // already at the attitude that lifts most.
        tail_pitch_deg_ = std::max(tail_pitch_deg_ - 2.0 / steps_per_second, 0.0);
        want_pitch = tail_pitch_deg_;
        rotate_pitch_ = s.pitch_deg;
    } else if (stage_ == Stage::rotate) {
        // **The nose comes up at a pilot's rate**, not at once, and stops at
        // a take-off attitude the aeroplane can carry - **from the attitude
        // she sits at**, not from level. An F-15C sits two degrees nose up,
        // and an attitude asked for from nothing was below her own: the
        // elevator went nose down at the rotation speed, and she ran on for
        // another twenty-five knots before it had come back up through
        // neutral.
        if (!rotation_begun_) {
            rotation_begun_ = true;
            rotate_pitch_ = std::max(rotate_pitch_, s.pitch_deg);
            // **The push that held a tail up is let go as she is rotated.**
            // Wound into the trim on the roll and left there, it carried the
            // Mosquito's nose from fifteen degrees to below the horizon as
            // she climbed away, and flew her back on to the runway.
            pitch_trim_ = std::max(pitch_trim_, 0.0);
        }
        rotate_pitch_ = std::min(rotate_pitch_ + 4.0 / steps_per_second, 10.0);
        want_pitch = on_ground ? rotate_pitch_ : std::max(rotate_pitch_, left_at_pitch_deg_);
        // **While the nose will not come, the stick goes further back** -
        // half its travel a second, as the F-15's flight manual has the
        // stick brought back (T.O. 1F-15A-1, figure A3-6: one half aft stick
        // over one second). The attitude law alone asks a twentieth of the
        // travel for each degree the nose is short, and its trim winds in
        // at three hundredths a second: an aeroplane heavy on its nose
        // wheel, which needs the stick well back to lift it, was held on
        // the runway by it thirty knots past its rotation speed. **And
        // eased off again as the nose comes**, a quarter of the travel a
        // second: left in, it carried an airliner's nose on past twenty
        // degrees once she had lifted it, and down again through the
        // horizon as the law took it out. **Held while she is off the
        // ground** too, before she has climbed clear of it, as it is on it:
        // a PA-28 lifted off at 55 knots in ground effect, the pull went,
        // and she settled back on to the runway.
        // **The stick is taken over where a pilot held it**: further back
        // than this autopilot last put it is a pilot's pull, and it becomes
        // the pull the law eases off from. Let go all at once, a PA-28
        // hauled off at 49 knots dropped her nose from fifteen degrees to
        // six and settled back on to the runway.
        const double held = -a_.property("fcs/elevator-cmd-norm");
        if (held > last_elevator_ + 0.05) {
            pull_ = std::min(pull_ + held - last_elevator_, 1.0);
        }
        const double lagging_deg = want_pitch - s.pitch_deg;
        if (lagging_deg > 1.0 && s.q_radps * degrees < 4.0 &&
            s.pitch_deg < strike_pitch_deg_ - strike_margin_deg - 1.0) {
            pull_ = std::min(pull_ + 0.5 / steps_per_second, 1.0);
        } else if (lagging_deg <= 1.0) {
            pull_ = std::max(pull_ - 0.25 / steps_per_second, 0.0);
        }
    } else {
        // Climbing: the attitude that holds the best climb speed - half a
        // degree of nose for each knot fast, and a slow trim that takes out
        // what is left. **It was the trim alone**, at 2.4 degrees a second
        // for each knot, and an integral with nothing to damp it feeds the
        // phugoid: a J-3 Cub at its figures' weight swung between 3 degrees
        // nose down and 18 up every eight seconds, and met the crosswind
        // turn at the top of a zoom with the speed falling away, stalled in
        // it and mushed seven hundred feet into the ground.
        //
        // **The speed asked of her builds from the one she left the ground
        // at** to the speed she climbs out at - as fast as she was gaining
        // speed on the runway, and all of it within twenty seconds at most:
        // she is accelerated in the climb, as she is flown, not asked for
        // all of it at once. Asked for all of it at once, 13 knots short, an
        // A380 off the runway at 162 knots was pitched from nineteen degrees
        // to five below the horizon and flown back on to it at 185, and the
        // Mosquito, off at 103 and asked for 148, was put back on the runway
        // and left it again at 120. Asked for it no faster than she had been
        // gaining speed, a B-2A on part throttle, which gains it slowly and
        // has 110 knots to gain, was still short of its climbing speed at the
        // top of the lesson's climb. On the water she is asked for all of
        // it, as her published take-off is flown. **And short of it, the
        // trim is wound on a quarter as fast** as it is when she is fast.
        //
        // (Until 2026-09-26 the speed asked began at the climbing speed
        // itself, so none of this was flown.)
        if (climb_target_kts_ == 0.0) {
            climb_target_kts_ =
                on_water ? speeds_.initial_climb_kts : std::min(kcas, speeds_.initial_climb_kts);
            climb_gain_ktps_ =
                std::max({accel_ktps_, 1.0, (speeds_.initial_climb_kts - climb_target_kts_) / 20.0});
        }
        climb_target_kts_ = std::min(climb_target_kts_ + climb_gain_ktps_ / steps_per_second,
                                     speeds_.initial_climb_kts);
        const double fast_by = kcas - climb_target_kts_;
        const double ki = fast_by < 0.0 ? 0.05 : 0.2;
        rotate_pitch_ = std::clamp(rotate_pitch_ + ki * fast_by / steps_per_second,
                                   0.0, 15.0);
        want_pitch = std::clamp(rotate_pitch_ + 0.5 * fast_by, 0.0, 15.0);
        // **A take-off climbs: it never goes back down to the runway to
        // gain speed.** A landplane leaves the ground at its rotation speed,
        // below the speed it climbs out at - a Cessna 182 twenty-five knots
        // below it - and the law above asks for the nose down to gain the
        // rest. It got it: the 182 climbed to sixteen feet, was pitched down
        // to level and settled back on to the runway, and ran on to 88 knots
        // before it left it again; the Mosquito to 158, the B-2A to 241. So
        // the nose is never put below the attitude that climbs - her
        // incidence and climb_floor_deg more - until she is 35 ft up, the
        // screen height a take-off is measured to, nor below the one that
        // holds her level after it; she accelerates climbing, as she is
        // flown.
        //
        // **Once she has been rotated** - by this autopilot, or by a pilot
        // with the stick back rotating her early. An aeroplane that hops off
        // the runway on the roll, as the Mosquito did at 90 knots from three
        // points before her tail was raised on it, is not flying, and is let
        // back down on to it.
        if (!on_water && rotated_off_) {
            const bool screen = (above_m_ - standing_m_) * feet_per_metre < 35.0;
            want_pitch = std::max(want_pitch, a_.property("aero/alpha-deg") +
                                                  (screen ? climb_floor_deg : 0.0));
        }
        // **Off the water, she is held off it.** A flying boat unsticks forty
        // knots below her climbing speed, and the law above answered that by
        // putting the nose down to gain it: the S.23 left the water at 77
        // knots, was pitched from nine degrees to three and went straight
        // back in. Below fifty feet she keeps her running attitude and
        // accelerates in ground effect, as a flying boat is flown off.
        if (on_water && above_m_ * feet_per_metre < 50.0) {
            want_pitch = std::max(want_pitch, speeds_.running_pitch_deg);
        }
    }
    // Never past the incidence the aeroplane's own tables cover: JSBSim
    // asserts rather than extrapolating. **Past it, the nose comes down by
    // as much as she is past it**, not merely no further up: holding the
    // attitude left a PA-28 hauled off early at fifteen degrees of
    // incidence, and the climb's floor above - her incidence and three
    // degrees - took her on up to thirty.
    if (a_.property("aero/alpha-deg") > 12.0) {
        want_pitch = std::min(want_pitch, s.pitch_deg - (a_.property("aero/alpha-deg") - 12.0));
    }
    if (!on_water) {
        // **On her wheels, the nose is held short of the attitude her tail
        // strikes at**, and the stick is not pulled further there. The B-2A
        // at its model's own weight was pulled to 14.1 degrees on its
        // wheels and struck its airframe.
        if (on_ground) {
            want_pitch = std::min(want_pitch, strike_pitch_deg_ - strike_margin_deg);
            if (s.pitch_deg > strike_pitch_deg_ - strike_margin_deg) {
                pull_ = std::max(pull_ - 1.0 / steps_per_second, 0.0);
            }
        } else if ((above_m_ - standing_m_) * feet_per_metre < 35.0) {
            // **Off the ground but not yet clear of it, the nose is never
            // pushed below the attitude she stands at** - level, for a
            // tail-wheel aeroplane, which stands on her tail - whatever
            // asks: letting a hop back down, or the incidence above. Unbound,
            // they flew the Learjet 35A back on to the runway nose first.
            want_pitch = std::max(want_pitch, tail_wheel_ ? 0.0 : standing_pitch_deg_);
        }
    }

    const double pitch_error = want_pitch - s.pitch_deg;
    const double q_degps = s.q_radps * degrees;
    if (stage_ != Stage::rotate) {
        pitch_trim_ = std::clamp(pitch_trim_ + 0.03 * pitch_error / steps_per_second, -0.8, 0.8);
        // **Out of the rotation, the pull is eased off only as the nose
        // comes up to where it is wanted**, and held while it is short of
        // it. Taken out at its own rate whatever the nose was doing, while
        // the trim winds in at three hundredths a second, the stick went
        // forward faster than the trim followed: a PA-28 off at 55 knots had
        // her nose put down from eight degrees to four and was back on the
        // runway, leaving it for good at 63.
        if (pitch_error < 1.0) {
            pull_ = std::max(pull_ - 1.0 / steps_per_second, 0.0);
        }
    }
    c.elevator =
        std::clamp(0.05 * pitch_error - 0.05 * q_degps + pitch_trim_ + pull_, -1.0, 1.0);
    last_elevator_ = c.elevator;
    return c;
}

} // namespace glideslope::sim
