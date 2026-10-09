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
//   land NAME LATITUDE LONGITUDE ELEVATION_FT HEADING_DEG LENGTH_M  after the last
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
// joined at a tangent and steered along it with the bank the circle needs,
// turned in towards it by 90 degrees for each kilometre outside it and out
// for each inside, up to 45 (sim/navigator.cpp).
//
// **A plan may end in a landing**: `land` names the runway - its landing
// threshold, as `runway` does - and once the last waypoint is passed the AI
// flies to the final approach, six and then four miles out on the extended
// centreline and the glidepath, and down it with the approach autopilot,
// handed to the learnt landing at its gate where the aircraft has one
// (sim::Controller). Where a plan's runway lies is the plan's to say; a
// caller that knows the ground better - the server, from the ground it
// collides on - may put its elevation right.

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
    // **Or a bank, flown when there is no heading**: degrees, right wing
    // down positive, within what the aeroplane sustains - a navigator's
    // lateral guidance round an orbit (sim/navigator.cpp), which asks for the
    // turn itself rather than a heading to turn to, and sets no heading with
    // it. **A heading set wins**: whoever sets one on modes that came with a
    // bank flies the heading, so a bank left behind is never flown.
    std::optional<double> bank_deg;
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
    //
    // **It keeps the wing below the angle of attack it stalls at, which the
    // autopilot learns by watching** - the angle at the greatest lift it has
    // seen since the flaps or gear last moved - because nothing tells it
    // where a model's lift peaks. Before the aeroplane has flown past that
    // peak the angle it knows is lower than the real one, and the recovery
    // unloads the wing further than it need: safe, at the cost of height.
    // An autopilot engaged on an aeroplane already stalled has seen only the
    // stalled wing, less lift at more angle; as the nose comes down the lift
    // rises back through its peak and the angle is learned on the way.
    bool speed_on_elevator = false;
    // **A stall's entry: the height held into the stall, whatever the wing's
    // angle.** The pitch envelope's top, 15 degrees, then bounds the flight
    // path rather than the nose: the nose may rise as far above it as the
    // angle of attack the wing has, so the height is held level as the speed
    // comes back to the warning, as the FAA's stall tasks enter it
    // (Airplane Flying Handbook, FAA-H-8083-3C, chapter 5; AC 120-109A's
    // approach to stall). Without it a fighter in her landing configuration,
    // needing 29 degrees of alpha at her warning, sank into it at 4,400
    // ft/min with her nose at 15. Every other mode is as before.
    bool hold_height_to_the_stall = false;
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
    // Or a take-off from `runway`'s threshold, to `to_ft` above it - no
    // lower than `lowest_ft`, where the take-off autopilot hands over.
    struct TakeOff {
        static constexpr double lowest_ft = 100.0;
        Runway runway;
        double to_ft = 0.0;
    };
    std::optional<TakeOff> takeoff;
    std::vector<Waypoint> waypoints;
    // **Landed on, after the last waypoint** (`land`), or none: the plan's
    // last leg then goes on flying the autopilot's last waypoint.
    std::optional<Runway> landing;
};

// Throws FlightPlanError naming the line of anything it cannot read, for a
// line after `land`, and for a plan with no aircraft or no waypoints, with both a start and a take-off, or
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

// **The slowest and fastest a plan may fly an aircraft**, KCAS: its figures
// file's `<plan_speeds>` (sim::plan_speeds), and the weight they were
// measured at - her model's own, which is what a server flies her at.
struct PlanSpeeds {
    double slowest_kts = 0.0;
    double fastest_kts = 0.0;
    // Pounds; 0 where none is known, and then they are flown as given.
    double weight_lbs = 0.0;
};

// **The gust allowance a plan's slowest keeps over the stall warning**, at
// the weight it was measured at: 10 kt. A plan's slowest is flown level and
// round orbits for as long as a plan says, in whatever air there is, so it
// keeps what a pilot keeps on an approach in turbulent air - "the normal
// approach speed plus one-half of the wind gust factor" (FAA, Airplane
// Flying Handbook, FAA-H-8083-3C, chapter 9, "Turbulent Air Approach and
// Landing": 70 kt with 15 kt gusts is flown at 77) - and more: in moderate
// turbulence the AI's held speed was measured to dip up to 8 kt under what
// it asked of a light aeroplane, and 10 kt leaves 2 over that.
// `glideslope_cli plan-speeds` seeks no slowest below the warning plus this.
constexpr double plan_gust_allowance_kts = 10.0;

// **The speeds a plan may fly her at, at what she weighs**: the slowest
// raised by the square root of `weight_lbs` over the weight it was measured
// at, as a stall speed is (sim::for_weight on her approach speeds), where
// she is heavier - so the margin it keeps over her stall warning grows with
// her stall, never shrinks. Never lowered where she is lighter: a slower
// speed than the one measured is one no trial flew. The fastest is kept.
// Returned as given where it names no weight, or within a pound of it.
PlanSpeeds for_weight(const PlanSpeeds& speeds, double weight_lbs);

// Whether `kts`, as a plan writes it in whole knots, is within `speeds`:
// half a knot either side - the one rule the planner, the copilot's routes
// and plan files are all held to.
bool within_plan_speeds(const PlanSpeeds& speeds, double kts);

// **Metres in a degree of latitude and of longitude** at `latitude_deg`, on
// the WGS84 ellipsoid: what a runway's own frame, near its threshold, is
// measured in - by the lander, the take-off, the learnt landing, the
// go-around's circuit and the vacating alike.
double metres_per_degree_latitude(double latitude_deg);
double metres_per_degree_longitude(double latitude_deg);

// Great-circle distance and initial bearing between two places, metres and
// degrees true.
double distance_m(double latitude_1, double longitude_1, double latitude_2,
                  double longitude_2);
double bearing_deg(double latitude_1, double longitude_1, double latitude_2,
                   double longitude_2);

} // namespace glideslope::sim
