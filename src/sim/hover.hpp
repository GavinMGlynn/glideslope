#pragma once

// **The hover control law** for an aeroplane with a lift system (LiftSystem
// in sim/aircraft.hpp: the F-35B's lift fan, roll posts and swivelling
// nozzle). It holds a height over a point of the ground, climbs to it from
// the ground, and lets down from it to a vertical landing.
//
//   height    the throttle, on a climb or sink rate asked of the height's
//             error, with an integral for the weight;
//   pitch     the fan's thrust either side of its share (the fan is ahead of
//             the centre of gravity), on the attitude and its rate;
//   roll      the posts' air shared between them, likewise;
//   heading   the nozzle swivelled sideways, likewise;
//   position  the attitude asked of the drift and the distance from the
//             point: tilted, the lift pushes her back over it.
//
// Every command moves at most at a hand's pace (Hover::Pace), as a pilot's
// or a flight-control computer's demand does, and the throttle is held
// below the afterburner, where the lift system's thrust is published.

#include "sim/aircraft.hpp"

namespace glideslope::sim {

class Hover {
public:
    // How fast each command may move, a second: the throttle across its
    // range in two seconds, the others across theirs in one.
    struct Pace {
        double throttle_per_s = 0.5;
        double control_per_s = 2.0;
    };

    // What the law is asked: to convert, a height above where it stood on
    // the ground, and whether to let down to land.
    struct Target {
        double height_ft = 0.0;
        bool land = false;
    };

    struct Command {
        Controls controls;
        LiftSystem lift;
    };

    // Fixes the point over which it hovers, the heading it holds and the
    // height its wheels stand at - where `aircraft` is now, on the ground.
    // Throws std::logic_error for an aircraft without a lift system.
    explicit Hover(const Aircraft& aircraft);
    Hover(const Aircraft& aircraft, Pace pace);

    // One 120 Hz step's commands.
    Command step(const Target& target);

    // Its height above where it stood, ft; and how far it has drifted from
    // its point, ft.
    double height_ft() const;
    double drift_ft() const;

private:
    const Aircraft& a_;
    Pace pace_;
    double standing_ft_ = 0.0;
    double stovl_throttle_ = 0.0;
    double heading_deg_ = 0.0;
    double latitude_deg_ = 0.0;
    double longitude_deg_ = 0.0;
    double throttle_integral_ = 0.0;
    double fan_integral_ = 0.0;
    double post_integral_ = 0.0;
    Command last_;
};

} // namespace glideslope::sim
