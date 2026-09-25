#pragma once

// Flight plans, and the navigator that flies them through the autopilot
// (REQUIREMENTS.md, section 5, the second layer).
//
// **A plan is data**, a text file, one command a line (assets/plans):
//
//   aircraft NAME                                       what it is written for
//   start LATITUDE LONGITUDE ALTITUDE_FT HEADING_DEG AIRSPEED_KT   in the air
//   runway NAME LATITUDE LONGITUDE ELEVATION_FT HEADING_DEG LENGTH_M
//   takeoff HEIGHT_FT                   or on that runway's threshold, to go
//   waypoint NAME LATITUDE LONGITUDE ALTITUDE_FT AIRSPEED_KT
//   orbit NAME LATITUDE LONGITUDE RADIUS_M ALTITUDE_FT AIRSPEED_KT TURNS left|right
//
// with `#` beginning a comment. Degrees are WGS84's, altitudes above sea
// level, airspeeds calibrated.
//
// **A plan starts in the air or on a runway**: `start`, or `runway` and
// `takeoff` together - the runway's threshold, where it faces along it, and
// the take-off autopilot (sim/departure.hpp) flying it to HEIGHT_FT above the
// runway before the navigator takes over. Not both.
//
// **An orbit is a waypoint flown round**: flown to like any other, and from
// its circle's edge flown round it, turning left or right, TURNS times before
// the plan goes on - or, for 0, for as long as the plan is flown. It is
// steered along the circle's tangent where the aircraft will be five seconds
// on, turned in towards it by 90 degrees for each kilometre outside it and out
// for each inside, up to 45.
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
#include "sim/lander.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::sim {

struct FlightPlanError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Waypoint {
    std::string name;
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double altitude_ft = 0.0;
    double airspeed_kts = 0.0;
    // Flown round, if it is an orbit.
    struct Orbit {
        double radius_m = 0.0;
        int turns = 0; // 0: for ever
        bool right = false;
    };
    std::optional<Orbit> orbit;
};

struct FlightPlan {
    std::string aircraft;
    struct Start {
        double latitude_deg = 0.0;
        double longitude_deg = 0.0;
        double altitude_ft = 0.0;
        double heading_deg = 0.0;
        double airspeed_kts = 0.0;
    };
    std::optional<Start> start;
    // Or a take-off from `runway`'s threshold, to `to_ft` above it.
    struct TakeOff {
        Runway runway;
        double to_ft = 0.0;
    };
    std::optional<TakeOff> takeoff;
    std::vector<Waypoint> waypoints;
};

// Throws FlightPlanError naming the line of anything it cannot read, and for a
// plan with no aircraft or no waypoints, with both a start and a take-off, or
// with a runway and no take-off from it or the other way round.
FlightPlan parse_flight_plan(std::string_view text);

// Great-circle distance and initial bearing between two places, metres and
// degrees true.
double distance_m(double latitude_1, double longitude_1, double latitude_2,
                  double longitude_2);
double bearing_deg(double latitude_1, double longitude_1, double latitude_2,
                   double longitude_2);

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
