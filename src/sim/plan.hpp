#pragma once

// **A flight plan, and nothing that flies it**: the plan, its waypoints, the
// runway it may take off from, and the modes an autopilot is asked for - what
// a pilot in command of an autopilot hands it, and what a language model may
// name (cmake/Copilot.cmake). Nothing here includes the aircraft, its
// controls, or anything that moves them; it is built on its own, as
// glideslope_plan, which links nothing.
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

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::sim {

// The most the autopilot banks to turn, whatever it is asked.
inline constexpr double most_bank_deg = 25.0;

// What the autopilot holds.
struct AutopilotModes {
    // Degrees true; none holds the wings level.
    std::optional<double> heading_deg;
    // Feet above sea level; none holds the vertical speed instead.
    std::optional<double> altitude_ft;
    // Feet a minute: held when there is no altitude, and the rate an altitude
    // is climbed or descended to, whichever way it lies, when there is.
    double vertical_speed_fpm = 700.0;
    // Knots calibrated; none leaves the throttle where it is.
    std::optional<double> airspeed_kts;
    // **The stall recovery: the airspeed on the elevator, at full power**,
    // rather than the height or the vertical speed. The nose goes down until
    // the wing is unloaded and the speed comes, whatever the height is doing,
    // and then holds the speed, climbing on what power is left. It needs
    // `airspeed_kts`, and does nothing without one.
    //
    // **It captures nothing and ends nothing by itself.** The height and the
    // vertical speed above are not flown while it is set, and it climbs for
    // as long as it is left set: the caller clears it once the aeroplane is
    // recovered, and the height or vertical speed it has set is flown from
    // where the pitch is. **Nor does it know where the ground is**: short of
    // the speed it will put the nose as far down as the flight path, to 30
    // degrees, at any height. A recovery started too low for that ends in
    // the ground.
    bool speed_on_elevator = false;
};

// Where to land: the landing threshold, and the runway from it.
struct Runway {
    std::string name;
    double threshold_lat_deg = 0.0;
    double threshold_lon_deg = 0.0;
    double elevation_ft = 0.0; // the threshold's, above sea level
    double heading_deg = 0.0;  // true, the direction of landing
    double length_m = 1500.0;  // from the threshold onwards
};

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

// **The tightest orbit an aircraft flies at `airspeed_kts`**, in metres: two
// and a half times the circle it turns at the autopilot's most bank,
// v^2 / (g tan 25 degrees) - 1,172 m at 90 kt. Tighter than that, the
// autopilot, which banks a degree for each degree off its heading, has no
// bank left to catch up with the circle: at one and a half times, a Cessna
// wandered from 374 to 1,041 m round a 704 m orbit. A plan with an orbit
// tighter than this for its speed is refused.
double least_orbit_radius_m(double airspeed_kts);

// Great-circle distance and initial bearing between two places, metres and
// degrees true.
double distance_m(double latitude_1, double longitude_1, double latitude_2,
                  double longitude_2);
double bearing_deg(double latitude_1, double longitude_1, double latitude_2,
                   double longitude_2);

} // namespace glideslope::sim
