#pragma once

// An autopilot that flies an approach and lands.
//
// **This is not the cruise autopilot** (sim/autopilot.hpp), which holds a
// heading, an altitude and a speed, never touches the flaps, the gear or the
// brakes, and holds no path at all. Here the elevator flies the glidepath and
// the throttle holds the speed, as an autopilot and an autothrottle do, and
// the flaps, the gear and the brakes are worked as the stages need them.
//
// It flies four stages, in order:
//
//   approach   down the extended centreline and a glidepath to the threshold,
//              the flaps and gear put where the landing needs them, the speed
//              held at the reference speed
//   flare      from the flare height: the power closed and the nose raised to
//              arrest the sink, so that the wheels meet the ground gently
//   rollout    on the ground: the nose held on the centreline, the brakes on
//   stopped    still
//
// **The runway is a datum, not a database.** A threshold, an elevation, a
// heading and a length are all it is; where those come from is the caller's.
//
// Nothing here draws, and nothing here reads the DEM: the ground it lands on
// is the runway's own elevation, which is what the aircraft's terrain must
// agree with.

#include "sim/aircraft.hpp"

#include <filesystem>
#include <string>

namespace glideslope::sim {

// Where to land: the landing threshold, and the runway from it.
struct Runway {
    std::string name;
    double threshold_lat_deg = 0.0;
    double threshold_lon_deg = 0.0;
    double elevation_ft = 0.0; // the threshold's, above sea level
    double heading_deg = 0.0;  // true, the direction of landing
    double length_m = 1500.0;  // from the threshold onwards
};

// How this aeroplane is flown down an approach. `vref_kts` is the speed over
// the threshold - by convention 1.3 times the stall speed in the landing
// configuration, which `approach_speeds` works out from the aircraft's own
// published figures.
struct ApproachSpeeds {
    double vref_kts = 60.0;
    double flap = 1.0;     // the landing flap setting, 0 to 1
    double flare_ft = 15.0; // height above the threshold to begin the flare
    // **The glidepath aims past the threshold, not at it.** An approach flown
    // at the threshold puts the flare before it and the wheels on the grass;
    // aiming three hundred metres down the runway puts the aeroplane about
    // fifty feet up as it crosses, which is where an aeroplane should be.
    double aim_m = 300.0;
};

// The speeds for an aircraft, from `data`/figures/MODEL.xml: the lowest
// published stall speed in the landing configuration, times 1.3. Throws
// std::runtime_error where the aircraft publishes no stall speed, because a
// reference speed guessed is a reference speed that means nothing.
ApproachSpeeds approach_speeds(const std::filesystem::path& data,
                               const std::string& model);

class Lander {
public:
    enum class Stage { approach, flare, rollout, stopped };

    Lander(const Aircraft& aircraft, const Runway& runway, const ApproachSpeeds& speeds,
           double glidepath_deg = 3.0);

    // One 120 Hz step's controls. Call once a step, as the aircraft is now.
    Controls fly();

    Stage stage() const { return stage_; }

    // Where the aeroplane is with respect to the runway, as the last `fly`
    // saw it.
    //
    // `along_m` is positive before the threshold and negative past it;
    // `across_m` is positive right of the centreline looking along the
    // landing direction; `above_m` is above the threshold's elevation.
    double along_m() const { return along_m_; }
    double across_m() const { return across_m_; }
    double above_m() const { return above_m_; }

    // The sink at the moment the wheels first touched, feet a minute, or 0
    // before that.
    double touchdown_sink_fpm() const { return touchdown_sink_fpm_; }
    // Where it touched down: across the centreline, and along from the
    // threshold. Meaningless before touchdown.
    double touchdown_across_m() const { return touchdown_across_m_; }
    double touchdown_along_m() const { return touchdown_along_m_; }

private:
    const Aircraft& a_;
    Runway runway_;
    ApproachSpeeds speeds_;
    double glidepath_rad_ = 0.0;
    Stage stage_ = Stage::approach;

    double along_m_ = 0.0;
    double across_m_ = 0.0;
    double above_m_ = 0.0;

    double touchdown_sink_fpm_ = 0.0;
    double touchdown_across_m_ = 0.0;
    double touchdown_along_m_ = 0.0;

    // The loops' own state, advanced one step a call.
    double pitch_trim_ = 0.0;
    double throttle_ = 0.0;
    double rudder_trim_ = 0.0;
    // The steady drift off the centreline that a wind, or a propeller's
    // slipstream, puts on: without it the aeroplane flies a track parallel
    // to the runway rather than down it.
    double across_trim_ = 0.0;
    double flare_pitch_ = 0.0;
    bool touched_ = false;

    void measure();
};

} // namespace glideslope::sim
