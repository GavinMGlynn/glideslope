#pragma once

// **Off the runway after landing, and not on to one that is not clear.**
//
// An aeroplane the AI has landed and stopped is taxied off the runway:
// turned off to the right - away from the left-hand circuit the go-around
// flies (sim/circuit.hpp) - at a walking taxi speed, steered by the rudder
// and the nosewheel, and by the brake on the inside wheel where the rudder
// alone does not turn her; rolled until she is `clear_m` from the
// centreline, and stopped there with the brakes on and the power off. That
// is all a runway has to tell where to go - a threshold, a heading and a
// length; taxiways are not known, so she turns off where she stopped, on to
// the ground beside it, which the collision DEM makes as flat as the strip
// beside a runway is. **Not on to another**: where she is told whether the
// ground beside the runway abeam her is free (`SpotFree`) - another
// aeroplane, landed as she was, turned off where she would and stopped
// there - she rolls on down the centreline at the taxi speed until it is,
// or until she is `turn_by_end_m` from the runway's end, and turns off
// then.
//
// **Who is on a runway** (`on_runway`): anything within its length - from
// 400 m short of the threshold, where the flare begins, to its far end - and
// within `occupied_half_width_m` of its centreline, on the ground or no
// higher than `occupied_ft` over it. An AI aeroplane on an approach goes
// around from below `decide_ft` if it is not clear (Controller::clears_with).

#include "sim/aircraft.hpp"
#include "sim/plan.hpp"

#include <functional>

namespace glideslope::sim {

// Where a place is with respect to a runway, metres: along it from its
// threshold in the landing direction (negative short of it), and right of its
// centreline.
struct OnRunway {
    double along_m = 0.0;
    double across_m = 0.0;
};
OnRunway on_runway_frame(const Runway& runway, double latitude_deg, double longitude_deg);

// Whether something at this place, `height_ft` over the runway, is on it.
bool on_runway(const Runway& runway, double latitude_deg, double longitude_deg,
               double height_ft);

// Whether something at this place, `height_ft` over the runway, is on the
// ground beside it on its right - off it, within `Vacate::spot_along_m`
// along of `along_m` and 500 m of its centreline: where a vacated aeroplane
// stops.
bool beside_runway(const Runway& runway, double latitude_deg, double longitude_deg,
                   double height_ft, double along_m);

struct RunwayClear {
    // Half the widest runway (Lander::runway_half_width_m) and half an
    // A380's wingspan beyond it: a wing over the edge still blocks it.
    static constexpr double occupied_half_width_m = 30.0 + 40.0;
    static constexpr double occupied_ft = 100.0;
    // A landing goes around from below this over the runway, if it is not
    // clear: a pilot's decision on short final.
    static constexpr double decide_ft = 400.0;
    // Where a vacated aeroplane stops: clear of the occupied width, with
    // half an A380's span again in hand.
    static constexpr double clear_m = occupied_half_width_m + 40.0;
};

class Vacate {
public:
    enum class Stage { rolling_on, turning_off, stopping, clear };

    // Whether the ground beside `runway`, on its right, is free within
    // `spot_along_m` either way of `along_m` from its threshold: nothing
    // stopped there. None: free everywhere.
    using SpotFree = std::function<bool(const Runway& runway, double along_m)>;
    // How far along the runway another aeroplane stopped beside it keeps
    // her rolling on: turning off at a walking pace an A380 rolls 560 m on
    // before she is clear, and stopped, should be no nearer than that to
    // the next.
    static constexpr double spot_along_m = 600.0;
    // Turned off by then, free or not.
    static constexpr double turn_by_end_m = 400.0;

    // `aircraft` stopped on `runway`, its controls now `controls`.
    Vacate(const Aircraft& aircraft, const Runway& runway, const Controls& controls,
           SpotFree spot_free = {});

    // One step's controls.
    Controls fly();
    Stage stage() const { return stage_; }
    bool clear() const { return stage_ == Stage::clear; }
    const Runway& runway() const { return runway_; }

    // The taxi speed, knots over the ground.
    static constexpr double taxi_kts = 8.0;
    // How far off the runway's heading she turns, to the right.
    static constexpr double turn_off_deg = 90.0;
    // How fast the heading she is steered to moves round, degrees a second.
    static constexpr double turn_degps = 10.0;
    // And how far ahead of her heading it may be, degrees.
    static constexpr double lead_deg = 10.0;

private:
    const Aircraft& a_;
    Runway runway_;
    Controls c_;
    SpotFree spot_free_;
    Stage stage_ = Stage::rolling_on;
    double throttle_integral_ = 0.0;
    double led_deg_ = 0.0; // off the runway's heading, steered to now
};

} // namespace glideslope::sim
