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
    //
    // **And a ground speed and a turn**: `ahead_kt` along the heading it
    // holds and `right_kt` across it, flown by her speed alone, and once
    // they are nothing and she has stopped, held over where she stopped;
    // and that heading turned at `turn_degps` - the F-35B's translational
    // rate command and her pedals' yaw rate (sim/transition.hpp). None is
    // a point held still, as before.
    struct Target {
        double height_ft = 0.0;
        bool land = false;
        double ahead_kt = 0.0;
        double right_kt = 0.0;
        double turn_degps = 0.0;
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

    // **Taken over in the air** from whatever was flying her with `flying`
    // (the transition law, sim/transition.hpp): the point, heading and
    // height it holds are where she is now, and its throttle, fan and posts
    // start where `flying` has them, so that taking over steps nothing.
    // Converted, the nozzle down, or its first steps close the throttle.
    Hover(const Aircraft& aircraft, const Command& flying);

    // **Everything it carries from step to step**: what a client joining
    // her in the hover is told, to fly on as the server's flies
    // (net::LawState), and made again from.
    struct State {
        double standing_ft = 0.0;
        double heading_deg = 0.0;
        double latitude_deg = 0.0;
        double longitude_deg = 0.0;
        bool translating = false;
        double throttle_integral = 0.0;
        double fan_integral = 0.0;
        double post_integral = 0.0;
        Command last;
    };
    State state() const;
    Hover(const Aircraft& aircraft, const State& state);

    // One 120 Hz step's commands.
    Command step(const Target& target);

    // **The point it holds over**, from the next step: degrees of WGS84.
    // Where it was taken over unless this moves it - a plan's vertical
    // landing, the point she was slowed to (sim/transition.hpp).
    void hold_over(double latitude_deg, double longitude_deg) {
        latitude_deg_ = latitude_deg;
        longitude_deg_ = longitude_deg;
        translating_ = false;
    }

    // Its height above where it stood, ft; and how far it has drifted from
    // its point, ft.
    double height_ft() const;
    double drift_ft() const;
    // The point it holds over now, degrees of WGS84.
    double latitude_deg() const {
        return latitude_deg_;
    }
    double longitude_deg() const {
        return longitude_deg_;
    }
    // The heading it holds, degrees.
    double heading_deg() const {
        return heading_deg_;
    }

private:
    const Aircraft& a_;
    Pace pace_;
    double standing_ft_ = 0.0;
    double stovl_throttle_ = 0.0;
    double heading_deg_ = 0.0;
    double latitude_deg_ = 0.0;
    double longitude_deg_ = 0.0;
    // Flying a ground speed asked, or stopping after one.
    bool translating_ = false;
    double throttle_integral_ = 0.0;
    double fan_integral_ = 0.0;
    double post_integral_ = 0.0;
    Command last_;
};

} // namespace glideslope::sim
