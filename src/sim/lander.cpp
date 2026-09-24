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
    // are what decides it, not the height.
    // A hull in the water has touched, as surely as wheels on a runway: a
    // flying boat alighting has no weight on any wheel.
    const bool on_ground = a_.property("gear/wow") > 0.5 || a_.in_water();
    if (on_ground && !touched_) {
        touched_ = true;
        touchdown_pitch_deg_ = s.pitch_deg;
        touchdown_above_m_ = above_m_;
        touchdown_sink_fpm_ = -s.climb_rate_fpm;
        touchdown_across_m_ = across_m_;
        touchdown_along_m_ = -along_m_;
    }
    // **But a bounce is flown, not rolled out.** Back in the air after
    // touching - more than a foot above where the wheels met the runway, and
    // no weight on them - she is flown with the flare's law again until she
    // settles, as the FAA's Airplane Flying Handbook (FAA-H-8083-3C, chapter
    // 9) has a bounce recovered: a landing attitude held and the sink let
    // decay. The rollout's stick, flown in the air, stalled a bounced Cub on
    // to its back.
    const bool bounced =
        touched_ && !on_ground && above_m_ > touchdown_above_m_ + 1.0 / feet_per_metre;
    if (bounced) {
        stage_ = Stage::flare;
    } else if (touched_) {
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
    // On the ground the nosewheel, the rudder and the brakes hold the
    // centreline; in the air L1 guidance does, below, and the rudder keeps
    // the ball in the middle.
    const double heading_error_to_runway =
        std::remainder(runway_.heading_deg - s.heading_deg, 360.0);
    if (stage_ == Stage::rollout || stage_ == Stage::stopped) {
        // Nosewheel steering and rudder are one control: the nose is held on
        // the centreline, which is the heading corrected by where it is.
        // **Gently while she is fast.** Twenty degrees off the runway's
        // heading to regain the centreline is a swerve at eighty knots: a
        // Mosquito touching down 26 metres off it was turned twenty degrees
        // at 89 knots and ground-looped. Five degrees at the reference speed,
        // the full twenty by half of it.
        const double rolling_kts = a_.property("velocities/vg-fps") / 1.68781;
        const double fast = std::clamp(
            (rolling_kts - 0.5 * speeds_.vref_kts) / std::max(1.0, 0.5 * speeds_.vref_kts), 0.0,
            1.0);
        const double most_deg = 20.0 - 15.0 * fast;
        const double want = std::clamp(-across_m_ * 2.0, -most_deg, most_deg);
        const double error = std::remainder(runway_.heading_deg + want - s.heading_deg, 360.0);
        const double r_degps = s.r_radps * degrees;
        // **The model's rudder command yaws the nose left for a positive
        // value** (sim/test_pilot.cpp), so holding the centreline takes the
        // opposite sign from the correction wanted.
        c.rudder = -std::clamp(0.10 * error - 0.30 * r_degps, -1.0, 1.0);
        c.aileron = std::clamp(-0.02 * s.roll_deg, -1.0, 1.0);
        c.throttle = 0.0;
        // **The stick comes back as she slows, and then stays back.** On a
        // tailwheel aeroplane that is what holds the tail down; brakes
        // applied with the stick forward put a Cub on its nose - the pitch
        // went to -71 degrees and JSBSim asserted on its own aerodynamic
        // tables. On a nosewheel aeroplane the same stick merely keeps the
        // nosewheel light, so one rule fits both.
        //
        // **But not while she can still fly.** The stick went fully back the
        // moment the wheels touched, and at the speed she touches at that
        // flies her off again: every light aeroplane bounced, the C172P
        // fifty feet in calm air, and came down nose first - the Cub on its
        // back. So while the speed is there the attitude she touched at is
        // held, and the stick is eased back as the speed goes, all the way
        // back by half the reference speed, where no wing will carry her.
        const double vg_kts = a_.property("velocities/vg-fps") / 1.68781;
        const double flying = std::clamp(
            (vg_kts - 0.5 * speeds_.vref_kts) / std::max(1.0, 0.4 * speeds_.vref_kts), 0.0,
            1.0);
        // **With a little forward pressure while she is fast**, as the FAA's
        // Airplane Flying Handbook (FAA-H-8083-3C, chapter 14) lands a
        // tailwheel aeroplane on its main wheels: holding the attitude she
        // touched at, the Mosquito - touching at 116 knots, a reference of
        // 123 - was still flying, and bounced seventy feet. Two degrees below
        // it at speed, easing to the attitude itself as she slows.
        const double hold_error = touchdown_pitch_deg_ - 2.0 * flying - s.pitch_deg;
        const double hold = std::clamp(
            0.05 * hold_error - 0.05 * s.q_radps * degrees + pitch_trim_, -1.0, 1.0);
        c.elevator = flying * hold + (1.0 - flying) * 1.0;
        // **The brakes hold a deceleration, as an autobrake does** - the 737's
        // are 4, 5, 7.2 and 14 feet a second squared at settings 1, 2, 3 and
        // MAX. They were a pressure that
        // grew as she slowed - half at most, and only near a stop - which
        // stopped a Cessna and let a 787 roll off the far end of a 3,000
        // metre runway. They come on once she is below nine tenths of the
        // reference speed, not the moment she touches, and come off
        // altogether if the nose is going down anyway.
        const double vg_fps = a_.property("velocities/vg-fps");
        if (last_vg_fps_ >= 0.0) {
            const double slowing = (last_vg_fps_ - vg_fps) * steps_per_second;
            decel_fps2_ += (slowing - decel_fps2_) / (0.5 * steps_per_second);
        }
        last_vg_fps_ = vg_fps;
        // **Set for the runway, as a pilot sets it**: the deceleration that
        // stops her with 300 metres to spare, from where and how fast she
        // touched, and never less than autobrake 2 nor more than MAX's 14.
        // At autobrake 2 an F-15C touching at 196 knots needs 3.3 km.
        if (autobrake_fps2_ <= 0.0) {
            const double left_ft =
                std::max(1.0, runway_.length_m - touchdown_along_m_ - 300.0) * feet_per_metre;
            autobrake_fps2_ = std::clamp(vg_fps * vg_fps / (2.0 * left_ft), 5.0, 14.0);
        }
        const double autobrake_fps2 = autobrake_fps2_;
        if (vg_kts < 0.9 * speeds_.vref_kts) {
            brake_ = std::clamp(brake_ + 0.1 * (autobrake_fps2 - decel_fps2_) / steps_per_second,
                                0.0, 1.0);
        }
        if (s.pitch_deg < -4.0) {
            brake_ = 0.0;
        }
        const double brake = brake_;
        // **And the brakes steer when the rudder is not enough.** A
        // castoring tailwheel steers nothing, and a Mosquito's rudder at full
        // travel could not hold a swing that grew from four to eighteen
        // degrees a second as she slowed: she ground-looped. Her Pilot's
        // Notes give the answer - differential brakes worked from the rudder
        // bar, the lever held and the pedal releasing the other wheel - and
        // any aeroplane with toe brakes is held straight the same way. Past
        // half its travel the rudder brings in the brake on the side the
        // nose is wanted, up to six tenths, and lets the other off.
        // (A positive rudder yaws the nose left, as above.)
        const double steer = std::clamp((std::abs(c.rudder) - 0.5) / 0.5, 0.0, 1.0);
        const double inside = std::clamp(brake + 0.6 * steer, 0.0, 1.0);
        const double outside = brake * (1.0 - steer);
        c.left_brake = c.rudder > 0.0 ? inside : outside;
        c.right_brake = c.rudder > 0.0 ? outside : inside;
        return c;
    }

    // **Flying: the centreline by L1 guidance** - Park, Deyst and How's
    // nonlinear guidance law ("A New Nonlinear Guidance Logic for Trajectory
    // Tracking", AIAA 2004-4900), as the ArduPilot and PX4 autopilots fly
    // lines with it. The aeroplane steers for a point on the centreline a
    // distance L1 ahead, which grows with its speed, and asks for the lateral
    // acceleration 4 zeta^2 V^2 / L1 sin(eta) that turns it on to the line
    // with damping zeta. It works in the ground track, so a steady wind is
    // flown out without an integral.
    //
    // **What it replaced** steered by heading, 0.6 degrees of it for each
    // metre off the line, and wound an integral up while far off it: right
    // for a Cessna and wrong for a 787 at 148 knots, whose turn is a
    // kilometre and a half across. Handed the approach from a circuit, two
    // miles to the side, it S-turned 370 metres either side of the
    // centreline all the way down and stopped 84 metres off the runway;
    // retuning it moved the error about without removing it.
    constexpr double zeta = 0.75;      // damping
    // The L1 period, thirty seconds: the law's own stability analysis puts
    // the limit where the roll response is slow beside L1/V, and at twenty -
    // L1/V of 4.8 seconds - a 787's roll lagged its commands and the
    // S-turns grew to 28 degrees of bank either way.
    constexpr double period_s = 30.0;
    const double h = runway_.heading_deg / degrees;
    const double v_north = a_.property("velocities/v-north-fps") / feet_per_metre;
    const double v_east = a_.property("velocities/v-east-fps") / feet_per_metre;
    const double along_v = v_east * std::sin(h) + v_north * std::cos(h);
    const double across_v = v_east * std::cos(h) - v_north * std::sin(h);
    const double ground_mps = std::max(1.0, std::hypot(along_v, across_v));
    // **And never inside the turn the aeroplane can fly.** At the bank
    // limit a 787 at 148 knots turns on a radius of 1,260 metres; steering
    // for a point 544 metres ahead it could not reach it, held the limit and
    // overshot, 300 metres each side of the centreline down to the runway.
    // L1 is at least that radius. For a light aeroplane the period's L1 is
    // already the longer - a Cessna's radius is about 210 metres against 222
    // - so it is flown as before; half as much again again broke their
    // landings, too slow to settle on the line in two miles.
    constexpr double most_bank_deg = 25.0;
    const double turn_radius_m =
        ground_mps * ground_mps / (9.80665 * std::tan(most_bank_deg / degrees));
    const double l1_m =
        std::max(zeta * period_s * ground_mps / 3.141592653589793, turn_radius_m);
    // eta: the angle from the velocity to the point L1 ahead on the line.
    // Right of the line (across_m_ > 0), or heading right of it, is a turn to
    // the left, so both terms count against it; the capture is limited to
    // forty-five degrees, as the law's authors limit it.
    const double offset_angle = std::asin(std::clamp(across_m_ / l1_m, -0.7071, 0.7071));
    const double track_angle = std::atan2(across_v, along_v);
    const double eta = std::clamp(-(offset_angle + track_angle), -1.5708, 1.5708);
    const double lateral_mps2 = 4.0 * zeta * zeta * ground_mps * ground_mps / l1_m *
                                std::sin(eta);
    // (A steady offset is not wound out here: the aileron's own trim, below,
    // takes the steady bank that caused it. An integral on the offset, as
    // there was, set a slow F-15C swinging 130 metres either side of the
    // centreline all the way down final.)
    const double want_bank = std::clamp(
        std::atan(lateral_mps2 / 9.80665) * degrees, -most_bank_deg,
        most_bank_deg);
    const double p_degps = s.p_radps * degrees;
    // **With a trim for what holds a steady bank against the aileron**: the
    // change of torque as a Mosquito's throttles close at the flare held her
    // in three degrees of bank against a proportional aileron, and she
    // drifted twenty metres in the float.
    aileron_trim_ = std::clamp(aileron_trim_ + 0.03 * (want_bank - s.roll_deg) / steps_per_second,
                               -0.3, 0.3);
    c.aileron = std::clamp(0.035 * (want_bank - s.roll_deg) - 0.02 * p_degps + aileron_trim_,
                           -1.0, 1.0);

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
        //
        // **The power comes off through the round out, not at its start**:
        // the FAA's Airplane Flying Handbook (FAA-H-8083-3C, chapter 9)
        // reduces it gradually to idle. Shut in one step, a Mosquito's
        // throttles took the torque of both propellers off at once and
        // rolled her three degrees; closed over two seconds from where they
        // were, they do not.
        c.throttle = std::max(0.0, last_throttle_ - 0.5 / steps_per_second);
        last_throttle_ = c.throttle;
        const double high_ft = std::max(0.0, above_m_ * feet_per_metre);
        const double want_fpm =
            -(40.0 + 150.0 * high_ft / std::max(1.0, speeds_.flare_ft));
        const double fpm_error = want_fpm - s.climb_rate_fpm;
        // **The nose comes up through the flare, and no faster than a pilot
        // would raise it.** Without the rate limit a large sink asks for a
        // large attitude at once, and the aeroplane is flown off the end of
        // its own aerodynamic tables - JSBSim asserts rather than
        // extrapolating, so an unlimited flare ends the flight.
        //
        // **As much nose as the path needs, and no more.** The change of
        // attitude a change of sink needs is the change of flight path, the
        // sink over the speed: at 43 knots 500 ft/min is 6.6 degrees of path,
        // at 123 knots 2.3. It was 0.02 degrees a foot a minute for every
        // aeroplane - one and a half times the Cub's path - and asked a
        // Mosquito at 116 knots for thirteen degrees in two and a half
        // seconds: she climbed at 950 ft/min to seventy feet, ran out of
        // speed, and fell on to the runway at 2,000. One and a half times
        // the path is kept, for every speed; the FAA's Airplane Flying
        // Handbook (FAA-H-8083-3C, chapter 9) has the round out's rate
        // proportional to the rate of closure with the ground.
        const double path_fpm = std::max(1.0, kcas) * 101.269;
        const double want_flare =
            s.pitch_deg + 1.5 * std::atan(fpm_error / path_fpm) * degrees;
        flare_pitch_ = std::clamp(
            std::max(flare_pitch_, std::min(want_flare, flare_pitch_ + 6.0 / steps_per_second)),
            -4.0, 10.0);
        // **And no further than the wing will carry.** A flare held on past
        // the stall is not a landing, and an aeroplane flown past the
        // incidence its aerodynamic tables cover ends the flight: JSBSim
        // asserts rather than extrapolating. So once the incidence is high
        // the nose stops coming up, whatever the sink still asks for.
        //
        // **Nor while she is climbing.** A round out that has gone up rather
        // than level is a balloon, and the handbook's answer is to relax the
        // back pressure or hold it, never to push: the nose is held where it
        // is and she is let settle.
        if (a_.property("aero/alpha-deg") > 12.0 || s.climb_rate_fpm > 0.0) {
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
        // **Mostly in proportion, a little by the integral.** The integral
        // was 0.24 of the throttle a second for each knot, and set a
        // Mosquito's throttles swinging from shut to full every six seconds
        // down final, the speed with them.
        throttle_ = std::clamp(
            throttle_ + (speed_error * 0.02 + sinking_fpm * 0.0002) / steps_per_second, 0.0,
            1.0);
        // **And the speed's trend as well as its error**, as an autothrottle
        // flies speed with acceleration fed back: power comes on while the
        // speed is still falling towards the reference, not after it has
        // gone under it. Without it an aeroplane still slowing from base on
        // short final - the Mosquito, from 133 knots - had its throttle
        // wound shut and then opened it in a lunge.
        if (last_kcas_ > 0.0) {
            const double rate = (kcas - last_kcas_) * steps_per_second;
            kcas_rate_ += (rate - kcas_rate_) / (0.5 * steps_per_second); // half a second
        }
        last_kcas_ = kcas;
        c.throttle = std::clamp(throttle_ + speed_error * 0.05 - kcas_rate_ * 0.1 +
                                    sinking_fpm * 0.0005,
                                0.0, 1.0);
        // **Near the ground the power comes on only a little at a time.**
        // Below twice the flare height the throttle may close as fast as it
        // likes and open by half its travel a second: chasing a speed
        // a few knots low there took a Mosquito's throttles from shut to
        // full and back in two seconds at forty feet, and the swing both her
        // propellers turning one way put on carried her 26 metres off the
        // centreline by the time she touched. A pilot's hand makes small
        // corrections over the threshold, not a burst. (Closing it outright
        // there left her eight knots slow crossing the threshold.)
        if (above_m_ * feet_per_metre <= 2.0 * speeds_.flare_ft) {
            c.throttle = std::min(c.throttle, last_throttle_ + 0.5 / steps_per_second);
        }
        last_throttle_ = c.throttle;
    }

    const double pitch_error = want_pitch - s.pitch_deg;
    const double q_degps = s.q_radps * degrees;
    pitch_trim_ = std::clamp(pitch_trim_ + 0.02 * pitch_error / steps_per_second, -0.8, 0.8);
    c.elevator = std::clamp(0.05 * pitch_error - 0.05 * q_degps + pitch_trim_, -1.0, 1.0);
    return c;
}

} // namespace glideslope::sim
