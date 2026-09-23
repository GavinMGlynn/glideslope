#include "sim/lander.hpp"

#include "sim/figures.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace glideslope::sim {
namespace {

constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double feet_per_metre = 3.280839895013123;
constexpr double steps_per_second = 120.0;

// **The approach is a local problem.** Five miles of runway centreline does
// not need great-circle geometry: the metres in a degree of latitude and of
// longitude at the threshold are enough, and are right to better than a metre
// over that distance. The simulation links no world library, and this is why
// it does not have to.
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

} // namespace

ApproachSpeeds approach_speeds(const std::filesystem::path& data,
                               const std::string& model) {
    const PublishedFigures figures =
        read_published_figures(data / "figures" / (model + ".xml"));
    // The landing configuration is the most flap the aeroplane publishes a
    // stall speed at - all of it for an aeroplane with flaps, none for one
    // without.
    const FigureSpec* landing = nullptr;
    double most_flap_deg = -1.0;
    for (const FigureSpec& spec : figures.figures) {
        if (spec.flight != "stall_speed") {
            continue;
        }
        const auto flap = spec.conditions.find("flaps_deg");
        const double flap_deg =
            flap == spec.conditions.end() ? 0.0 : flap->second;
        if (flap_deg > most_flap_deg) {
            most_flap_deg = flap_deg;
            landing = &spec;
        }
    }
    if (landing == nullptr) {
        throw std::runtime_error(model +
                                 " publishes no stall speed, so there is no "
                                 "reference speed to fly an approach at");
    }
    ApproachSpeeds speeds;
    // Vref, by the usual convention: a third above the stall in the landing
    // configuration.
    speeds.vref_kts = 1.3 * landing->published;
    // A faster approach comes down faster on the same glidepath, so it needs
    // longer to round out: the flare starts a quarter of a foot up for every
    // knot of approach speed, which is fifteen feet for a Cessna and ten for
    // a Cub.
    speeds.flare_ft = std::clamp(0.25 * speeds.vref_kts, 8.0, 30.0);
    speeds.flap = figures.flaps_full_deg > 0.0
                      ? std::clamp(most_flap_deg / figures.flaps_full_deg, 0.0, 1.0)
                      : 0.0;
    return speeds;
}

Lander::Lander(const Aircraft& aircraft, const Runway& runway,
               const ApproachSpeeds& speeds, double glidepath_deg)
    : a_(aircraft), runway_(runway), speeds_(speeds),
      glidepath_rad_(glidepath_deg / degrees) {
    measure();
    throttle_ = 0.5;
}

void Lander::measure() {
    const AircraftState s = a_.state();
    const double north_m = (s.latitude_deg - runway_.threshold_lat_deg) *
                           metres_per_degree_latitude(runway_.threshold_lat_deg);
    const double east_m = (s.longitude_deg - runway_.threshold_lon_deg) *
                          metres_per_degree_longitude(runway_.threshold_lat_deg);
    const double heading = runway_.heading_deg / degrees;
    // Along the landing direction, and to the right of it.
    const double past_m = east_m * std::sin(heading) + north_m * std::cos(heading);
    along_m_ = -past_m;
    across_m_ = east_m * std::cos(heading) - north_m * std::sin(heading);
    above_m_ = (s.altitude_ft - runway_.elevation_ft) / feet_per_metre;
}

Controls Lander::fly() {
    measure();
    const AircraftState s = a_.state();
    const double kcas = s.airspeed_kts;

    Controls c;
    c.gear = 1.0;
    c.flaps = speeds_.flap;
    c.mixture = 1.0;
    c.propeller = 1.0;

    // **On the ground is the rollout, whatever the stage said.** The wheels
    // are what decides it, not the height: an aeroplane that has touched has
    // landed.
    const bool on_ground = a_.property("gear/wow") > 0.5;
    if (on_ground && !touched_) {
        touched_ = true;
        touchdown_sink_fpm_ = -s.climb_rate_fpm;
        touchdown_across_m_ = across_m_;
        touchdown_along_m_ = -along_m_;
    }
    if (touched_) {
        // **Stopped is over the ground, not through the air.** An aeroplane
        // standing still in a ten-knot wind still reads ten knots of
        // airspeed, so asking the airspeed would mean it never stopped.
        stage_ = std::abs(a_.property("velocities/vg-fps")) < 1.0 ? Stage::stopped
                                                                 : Stage::rollout;
    } else if (above_m_ * feet_per_metre <= speeds_.flare_ft && along_m_ < 400.0) {
        stage_ = Stage::flare;
    }

    // --- where the nose points -------------------------------------------
    //
    // The centreline is held by turning towards it: an offset asks for a
    // track correction, limited so that a long way out it flies an intercept
    // rather than an aerobatic turn. The rudder keeps the ball in the middle
    // while flying, and holds the runway heading once the wheels are down.
    const double heading_error_to_runway =
        std::remainder(runway_.heading_deg - s.heading_deg, 360.0);
    if (stage_ == Stage::rollout || stage_ == Stage::stopped) {
        // Nosewheel steering and rudder are one control: the nose is held on
        // the centreline, which is the heading corrected by where it is.
        const double want = std::clamp(-across_m_ * 2.0, -20.0, 20.0);
        const double error = std::remainder(runway_.heading_deg + want - s.heading_deg, 360.0);
        const double r_degps = s.r_radps * degrees;
        // **The model's rudder command yaws the nose left for a positive
        // value** (sim/test_pilot.cpp), so holding the centreline takes the
        // opposite sign from the correction wanted.
        c.rudder = -std::clamp(0.10 * error - 0.30 * r_degps, -1.0, 1.0);
        c.aileron = std::clamp(-0.02 * s.roll_deg, -1.0, 1.0);
        c.throttle = 0.0;
        // **The stick comes back and stays back.** On a tailwheel aeroplane
        // that is what holds the tail down; brakes applied with the stick
        // forward put a Cub on its nose, which is exactly what happened
        // before this was written - the pitch went to -71 degrees and JSBSim
        // asserted on its own aerodynamic tables. On a nosewheel aeroplane
        // the same stick merely keeps the nosewheel light, so one rule fits
        // both.
        c.elevator = 1.0;
        // The brakes come on as the aeroplane slows, not the moment it
        // touches, and come off altogether if the nose is going down anyway.
        const double vg_kts = a_.property("velocities/vg-fps") / 1.68781;
        double brake = 0.5 * std::clamp((0.9 * speeds_.vref_kts - vg_kts) /
                                            std::max(1.0, speeds_.vref_kts),
                                        0.0, 1.0);
        if (s.pitch_deg < -4.0) {
            brake = 0.0;
        }
        c.left_brake = brake;
        c.right_brake = brake;
        return c;
    }

    // Flying: bank towards the centreline. The integral is what removes a
    // steady drift - a crosswind, or a propeller's slipstream - which a
    // proportional term alone answers with a standing offset.
    across_trim_ = std::clamp(across_trim_ - across_m_ * 0.02 / steps_per_second,
                              -20.0, 20.0);
    const double track_correction =
        std::clamp(-across_m_ * 0.6 + across_trim_, -30.0, 30.0);
    const double want_heading = runway_.heading_deg + track_correction;
    const double heading_error = std::remainder(want_heading - s.heading_deg, 360.0);
    const double want_bank = std::clamp(heading_error * 1.2, -25.0, 25.0);
    const double p_degps = s.p_radps * degrees;
    c.aileron = std::clamp(0.035 * (want_bank - s.roll_deg) - 0.02 * p_degps, -1.0, 1.0);

    const double beta_deg = a_.property("aero/beta-deg");
    rudder_trim_ = std::clamp(rudder_trim_ - 0.02 * beta_deg / steps_per_second, -0.5, 0.5);
    c.rudder = std::clamp(-0.05 * beta_deg + rudder_trim_, -1.0, 1.0);
    (void)heading_error_to_runway;

    // --- the path, on the elevator ----------------------------------------
    double want_pitch = 0.0;
    if (stage_ == Stage::flare) {
        // **The flare is a sink that decays with height**, not a fixed
        // attitude: the wheels should be going down at forty feet a minute as
        // they arrive, whatever the aeroplane weighs and however fast its
        // approach was. Commanding the sink rather than the attitude is what
        // makes one law fit a Cub and a Skylane.
        c.throttle = 0.0;
        const double high_ft = std::max(0.0, above_m_ * feet_per_metre);
        const double want_fpm =
            -(40.0 + 150.0 * high_ft / std::max(1.0, speeds_.flare_ft));
        const double fpm_error = want_fpm - s.climb_rate_fpm;
        // **The nose comes up through the flare, and no faster than a pilot
        // would raise it.** Without the rate limit a large sink asks for a
        // large attitude at once, and the aeroplane is flown off the end of
        // its own aerodynamic tables - JSBSim asserts rather than
        // extrapolating, so an unlimited flare ends the flight.
        const double want_flare = s.pitch_deg + 0.020 * fpm_error;
        flare_pitch_ = std::clamp(
            std::max(flare_pitch_, std::min(want_flare, flare_pitch_ + 6.0 / steps_per_second)),
            -4.0, 10.0);
        // **And no further than the wing will carry.** A flare held on past
        // the stall is not a landing, and an aeroplane flown past the
        // incidence its aerodynamic tables cover ends the flight: JSBSim
        // asserts rather than extrapolating. So once the incidence is high
        // the nose stops coming up, whatever the sink still asks for.
        if (a_.property("aero/alpha-deg") > 12.0) {
            flare_pitch_ = std::min(flare_pitch_, s.pitch_deg);
        }
        want_pitch = flare_pitch_;
    } else {
        // The glidepath, from the threshold: how high the aeroplane should be
        // where it is, and the sink that holds it there.
        const double want_above_m =
            std::max(0.0, along_m_ + speeds_.aim_m) * std::tan(glidepath_rad_);
        const double high_m = above_m_ - want_above_m;
        const double ground_fpm = s.airspeed_kts * 101.269 * std::tan(glidepath_rad_);
        const double want_fpm = std::clamp(-ground_fpm - high_m * 40.0, -1200.0, 300.0);
        const double fpm_error = want_fpm - s.climb_rate_fpm;
        // **The attitude the path needs, plus a correction for being off
        // it.** It was the correction alone, 0.006 degrees a foot a minute,
        // so an aeroplane that needs its nose up on the glidepath could only
        // get it by sinking faster than the path: twelve degrees, which the
        // F-35B needs at its reference speed, took 2,000 ft/min of error. A
        // light aeroplane flies the path near level and never showed it. The
        // attitude is learnt, slowly, while the correction is not at a limit,
        // and the limit is fifteen degrees, not ten.
        if (!path_pitch_set_) {
            path_pitch_ = s.pitch_deg;
            path_pitch_set_ = true;
        }
        want_pitch = path_pitch_ + 0.006 * fpm_error;
        if (want_pitch > -8.0 && want_pitch < 15.0) {
            path_pitch_ = std::clamp(path_pitch_ + 0.0006 * fpm_error / steps_per_second,
                                     -8.0, 15.0);
        }
        want_pitch = std::clamp(want_pitch, -8.0, 15.0);
        flare_pitch_ = s.pitch_deg;

        // The speed, on the throttle - **and the path too, once the nose has
        // run out.** Pitch for the path and power for the speed is right for
        // an aeroplane on the front of its drag curve, which is where a
        // reference speed is meant to put it. The F-35B at its reference speed
        // is not: six knots fast, the throttle closed; it sank; the nose came
        // up to the ten degrees it is allowed and to twenty degrees of alpha,
        // which is all drag, and it sank at 43 ft/s onto the runway with the
        // throttle still shut because the speed was still on target. On the
        // back of the curve a pilot flies the path with the power. So a sink
        // more than 300 ft/min faster than the glidepath asks for opens the
        // throttle in proportion, and stops it winding back while it lasts.
        // An approach that is on its path never comes near the margin.
        const double speed_error = speeds_.vref_kts - kcas;
        const double sinking_fpm = std::max(0.0, fpm_error - 300.0);
        throttle_ = std::clamp(
            throttle_ + (speed_error * 0.004 * 60.0 + sinking_fpm * 0.0002) / steps_per_second,
            0.0, 1.0);
        c.throttle = std::clamp(throttle_ + speed_error * 0.01 + sinking_fpm * 0.0005, 0.0, 1.0);
    }

    const double pitch_error = want_pitch - s.pitch_deg;
    const double q_degps = s.q_radps * degrees;
    pitch_trim_ = std::clamp(pitch_trim_ + 0.02 * pitch_error / steps_per_second, -0.8, 0.8);
    c.elevator = std::clamp(0.05 * pitch_error - 0.05 * q_degps + pitch_trim_, -1.0, 1.0);
    return c;
}

} // namespace glideslope::sim
