#include "sim/departure.hpp"

#include "sim/figures.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace glideslope::sim {
namespace {

constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double feet_per_metre = 3.280839895013123;
constexpr double steps_per_second = 120.0;
// Below this the rudder is too soft to hold the nose, and the brakes help.
constexpr double rudder_bites_kts = 60.0;

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

    // The take-off roll, where it has one, gives both the lift-off speed and
    // the flap it was flown with.
    if (const FigureSpec* roll = by_flight(figures, "takeoff_ground_roll");
        roll != nullptr && condition(*roll, "lift_off_kcas", 0.0) > 0.0) {
        speeds.rotate_kts = condition(*roll, "lift_off_kcas", 0.0);
        speeds.rotate_is_published = true;
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
    speeds.flap = 0.0;
    return speeds;
}

Departure::Departure(const Aircraft& aircraft, const Runway& runway,
                     const DepartureSpeeds& speeds, double to_ft)
    : a_(aircraft), runway_(runway), speeds_(speeds), to_ft_(to_ft) {
    measure();
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

    const bool on_ground = a_.property("gear/wow") > 0.5;
    if (!unstuck_ && !on_ground && above_m_ * feet_per_metre > 5.0) {
        unstuck_ = true;
        unstuck_along_m_ = along_m_;
    }

    if (stage_ != Stage::done && above_m_ * feet_per_metre >= to_ft_) {
        stage_ = Stage::done;
    } else if (unstuck_) {
        stage_ = Stage::climb;
        // The flaps come up once the aeroplane is safely climbing away.
        if (above_m_ * feet_per_metre > 200.0) {
            c.flaps = 0.0;
        }
    } else if (kcas >= speeds_.rotate_kts) {
        stage_ = Stage::rotate;
    }

    // **The throttle goes fully open over three seconds** and stays there:
    // an engine slammed open swings a tail-wheel aeroplane off the runway.
    throttle_ = std::min(throttle_ + 1.0 / (3.0 * steps_per_second), 1.0);
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
        const double want = std::clamp(-across_m_ * 2.0, -15.0, 15.0);
        const double error =
            std::remainder(runway_.heading_deg + want - s.heading_deg, 360.0);
        const double r_degps = s.r_radps * degrees;
        // `turn` is positive to swing the nose right. **The model's rudder
        // command yaws the nose left for a positive value**
        // (sim/test_pilot.cpp), so the rudder takes the opposite sign; the
        // brake is on the side being turned towards.
        const double turn = std::clamp(0.10 * error - 0.30 * r_degps, -1.0, 1.0);
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
    if (stage_ == Stage::roll) {
        // The stick is held where the aeroplane sits: a tail-wheel aeroplane
        // wants its tail down until it has the speed to lift it.
        c.elevator = 0.0;
        return c;
    }
    if (stage_ == Stage::rotate) {
        // **The nose comes up at a pilot's rate**, not at once, and stops at
        // a take-off attitude the aeroplane can carry.
        rotate_pitch_ = std::min(rotate_pitch_ + 4.0 / steps_per_second, 10.0);
        want_pitch = rotate_pitch_;
    } else {
        // Climbing: the attitude that holds the best climb speed - half a
        // degree of nose for each knot fast, and a slow trim that takes out
        // what is left. **It was the trim alone**, at 2.4 degrees a second
        // for each knot, and an integral with nothing to damp it feeds the
        // phugoid: a J-3 Cub at its figures' weight swung between 3 degrees
        // nose down and 18 up every eight seconds, and met the crosswind
        // turn at the top of a zoom with the speed falling away, stalled in
        // it and mushed seven hundred feet into the ground.
        const double fast_by = kcas - speeds_.climb_kts;
        rotate_pitch_ = std::clamp(rotate_pitch_ + 0.2 * fast_by / steps_per_second,
                                   0.0, 15.0);
        want_pitch = std::clamp(rotate_pitch_ + 0.5 * fast_by, 0.0, 15.0);
    }
    // Never past the incidence the aeroplane's own tables cover: JSBSim
    // asserts rather than extrapolating.
    if (a_.property("aero/alpha-deg") > 12.0) {
        want_pitch = std::min(want_pitch, s.pitch_deg);
    }

    const double pitch_error = want_pitch - s.pitch_deg;
    const double q_degps = s.q_radps * degrees;
    pitch_trim_ = std::clamp(pitch_trim_ + 0.03 * pitch_error / steps_per_second, -0.8, 0.8);
    c.elevator = std::clamp(0.05 * pitch_error - 0.05 * q_degps + pitch_trim_, -1.0, 1.0);
    return c;
}

} // namespace glideslope::sim
