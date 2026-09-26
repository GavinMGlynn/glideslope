#pragma once

// **What a landing did after its wheels met the runway**, for every test that
// lands an aeroplane: how far she banked, how far her nose went down and how
// high she went again, from the first touch to the stop.
//
// A landing that bounces, or ends on its nose or its back, is not one - and
// touching gently and stopping on the runway say nothing about which way up
// she stopped. The light aircraft's landing tests alone asked this until
// 2026-09-24, and the approach lesson and the circuit landed fourteen
// aeroplanes without it.

#include "sim/aircraft.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace glideslope::test {

struct AfterTouch {
    bool touched = false;
    double worst_roll_deg = 0.0;
    double least_pitch_deg = 0.0;
    double highest_ft = 0.0;
    bool airframe_touched = false;

    // Called every step once she is on the approach; the first step with
    // weight on a wheel - or a hull in the water - is the touch. The height
    // of her centre of gravity then is what "on the ground" is for this
    // aeroplane, and a bounce is measured from it.
    //
    // **Or anything else of hers on the runway, before the wheels.** A B-2A
    // rocking in a Dutch roll down final put its left wingtip on the runway
    // at 8.6 degrees of bank a tenth of a second before either main wheel,
    // and lifted it again: watched from the wheels, that was seen or not by
    // whether the tip was still down when they arrived, and the circuit test
    // passed or failed on a hundredth of a second of timing.
    void watch(const glideslope::sim::Aircraft& a) {
        const double agl_ft = a.property("position/h-agl-ft");
        if (!touched) {
            if (a.property("gear/wow") <= 0.5 && !a.in_water() && !a.contact().airframe) {
                return;
            }
            touched = true;
            ground_agl_ft_ = agl_ft;
            least_pitch_deg = a.property("attitude/theta-deg");
        }
        // A flying boat's hull is her undercarriage on the water, and it is
        // part of her airframe; on land nothing but a wheel belongs down.
        if (a.contact().airframe && !a.in_water()) {
            airframe_touched = true;
        }
        worst_roll_deg = std::max(worst_roll_deg, std::abs(a.property("attitude/phi-deg")));
        least_pitch_deg = std::min(least_pitch_deg, a.property("attitude/theta-deg"));
        highest_ft = std::max(highest_ft, agl_ft - ground_agl_ft_);
    }

    // **She stays on her wheels, the right way up, from the touch to the
    // stop.** Fifteen degrees of bank puts a light aeroplane's wingtip near
    // the runway and an airliner's engine pod on it; a nose ten degrees down
    // is a nosewheel aeroplane through its nose leg or a tailwheel one over
    // on its nose; three feet up is a bounce, not a rollout; and any part of
    // the airframe on the runway - JSBSim's own contact points for a tail, a
    // wingtip or a nose - is not an attitude its gear allows.
    //
    // What she did wrong, one line each, or nothing: a test walking every
    // aeroplane gathers these and fails once at the end, so that one run
    // names every aeroplane that came down badly, not only the first.
    std::vector<std::string> what_went_wrong(const std::string& id) const {
        std::vector<std::string> out;
        if (!touched) {
            out.push_back(id + " never touched down");
            return out;
        }
        if (worst_roll_deg >= 15.0) {
            out.push_back(id + " banked " + std::to_string(worst_roll_deg) +
                          " degrees after touching down");
        }
        if (least_pitch_deg <= -10.0) {
            out.push_back(id + "'s nose went " + std::to_string(least_pitch_deg) +
                          " degrees down after touching down");
        }
        if (airframe_touched) {
            out.push_back(id + " put something other than its wheels on the runway - a "
                               "wingtip, a tail or its nose");
        }
        if (highest_ft >= 3.0) {
            out.push_back(id + " went " + std::to_string(highest_ft) +
                          " ft back into the air after touching down");
        }
        return out;
    }

private:
    double ground_agl_ft_ = 0.0;
};

} // namespace glideslope::test
