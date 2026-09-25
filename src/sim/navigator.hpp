#pragma once

// The navigator that flies a flight plan (sim/plan.hpp) through the
// autopilot (REQUIREMENTS.md, section 5, the second layer).
//
// **The navigator flies each leg** - from the waypoint before, or from where
// the aircraft was when the plan began - along its great circle: it steers
// for the leg's track where the aircraft is abeam of it, turned back towards
// the leg by 30 degrees for each kilometre off it, up to 30; turned into the
// wind by the drift the aircraft has - the difference between where it points
// and where it goes, averaged over five seconds; and it asks the autopilot for
// the waypoint's altitude and airspeed. A waypoint is passed when it is abeam:
// when the leg ahead of the aircraft is gone. The Earth is a sphere of radius
// 6,371 km for this, which is within 0.5% of the ellipsoid's distances.

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/plan.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::sim {




class Navigator {
public:
    // Flying `plan` from where `aircraft` is now.
    Navigator(const Aircraft& aircraft, FlightPlan plan);

    // The autopilot's modes for the next step, from where the aircraft is: the
    // leg's steering, and the next waypoint's altitude and airspeed. Moves on
    // to the next waypoint as each is passed; after the last it holds the
    // last leg's track. Call it once a step.
    AutopilotModes steer();

    // The leg being flown begins here, where the aircraft is now: for a plan
    // taken over from a take-off, whose first leg is from where the take-off
    // left the aeroplane, not from the threshold it started on.
    void begin_here();

    // The turns flown round the orbit being flown, or 0.
    double turns_flown() const {
        return turned_deg_ / 360.0;
    }
    // Whether it is on an orbit's circle rather than flying to it.
    bool circling() const {
        return circling_;
    }

    // The waypoint being flown to; the plan's size when all are passed.
    std::size_t next() const {
        return next_;
    }
    bool finished() const {
        return next_ >= plan_.waypoints.size();
    }
    const FlightPlan& plan() const {
        return plan_;
    }

private:
    const Aircraft& a_;
    FlightPlan plan_;
    std::size_t next_ = 0;
    // Where the leg being flown begins.
    double from_latitude_deg_ = 0.0;
    double from_longitude_deg_ = 0.0;
    double drift_deg_ = 0.0;
    double last_track_deg_ = 0.0;
    // Round an orbit: whether on its circle, the bearing from its centre when
    // last steered, and how far round it has come.
    bool circling_ = false;
    double around_deg_ = 0.0;
    double turned_deg_ = 0.0;
};

} // namespace glideslope::sim
