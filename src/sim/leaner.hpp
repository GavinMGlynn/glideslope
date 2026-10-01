#pragma once

// Leaning the mixture for best power, as a pilot does with a hand on the
// mixture lever and an eye on what the engine gives: by the engine's answer,
// not by a table of settings.
//
// **Why it is needed.** A piston engine's carburettor or injector meters fuel
// for the air at sea level; as the air thins the mixture goes rich, the power
// falls, and past a point the engine will not fire. JSBSim's piston engine
// (FGPiston::doFuelFlow) meters the fuel as the mixture lever times the sea
// level pressure over the ambient pressure, and its power is the fuel burnt
// times an efficiency that peaks at one fuel/air ratio and falls either side
// (FGPiston's MIXTURE correlation). Left full rich, as the AI used to leave
// it, a Cessna 172P's ceiling was about 8,500 ft against its handbook's
// 13,000, and its engine stopped a thousand feet above that.
//
// **What a pilot does.** The handbooks say to lean for the most power: the
// Cessna 172P's maximum-climb chart is "mixture leaned above 3000 feet for
// maximum RPM" (Pilot's Operating Handbook, 1981, figure 5-6); the Cherokee
// 180's "for best power mixture, lean the mixture until the peak EGT is
// reached, then enrich" (Owner's Handbook, section III); the Cessna 182S's
// is a fuel flow placard, which is the same best power written down. All are
// the one rule: find where the power peaks as the mixture moves, and sit
// there.
//
// **What this does.** It finds that peak by moving the lever, as a pilot
// does, and watching the engine's power: an extremum-seeking loop (Ariyur
// and Krstic, Real-Time Optimization by Extremum-Seeking Control, 2003). The
// lever is moved a hundredth of its travel either way of where it rests,
// once every two seconds; the power's answer, its slow drift with the height
// and the throttle taken out, is multiplied by that movement, which gives
// the way the power rises; and the resting place moves that way, slowly. On
// the lean side of the peak the power rises with the lever, so it goes
// richer; on the rich side it falls, so it goes leaner; at the peak the two
// cancel. Below about 4,000 ft even full rich is leaner than the peak - the
// peak lies past the lever's rich stop - so it rests at its stop, as a
// handbook's "full rich below 3,000 ft" has it.
//
// **Never leaner than chemically correct**, as a bound and not a tendency:
// the most power is always rich of 14.7 parts of air to one of fuel,
// whatever the engine, so leaner than that the lever goes richer at a fixed
// pace, a tenth of its travel a second, whatever the power seems to say.
//
// **Not EGT.** Real pilots lean by the exhaust temperature because it is the
// gauge they have; JSBSim's exhaust temperature peaks near the chemically
// correct ratio, far leaner than its power does, so leaning by it would give
// away a third of the power. The power is what the rule is about, and the
// engine reports it.
//
// **Throttled back it holds the ratio instead.** Below four tenths of the
// throttle the power is too little to lean by - at idle it is mostly the
// engine's own friction, and felt for there the lever wandered to its lean
// stop and a Cessna 182 opened up out of a stall on an engine with little to
// give. So there the ratio of air to fuel the peak was last found at, as the
// engine reports it, is held: the mixture richens as a descent thickens the
// air, as a pilot's hand does on the way down. **Handed an engine, it starts
// by holding the ratio the engine has then**, so a leaner made for an
// approach or a departure from a leaned cruise goes on from the mixture it
// was given rather than from full rich, which high up stops the engine.
//
// **An engine it was leaning that stops is richened**, at the same tenth of
// the travel a second, to full rich, as a pilot's first answer to a lean
// stoppage is: windmilling, JSBSim's piston engine fires again on a mixture
// it can burn. An engine handed to it stopped it leaves alone: a stopped
// engine gives no answer to lean by, and a mixture cut off is the pilot's.

#include "sim/aircraft.hpp"

namespace glideslope::sim {

class MixtureLeaner {
public:
    // Leaning `aircraft`'s engines, from the mixture `mixture` it has now.
    MixtureLeaner(const Aircraft& aircraft, double mixture);

    // The mixture for the next step, with the throttle at `throttle`. Call it
    // once a step.
    double lean(double throttle);

    // Where the lever rests, without the movement that feels for the peak.
    double resting() const {
        return resting_;
    }

private:
    const Aircraft& a_;
    int engines_ = 0;
    double resting_;
    long step_ = 0;
    bool feeling_ = false;   // feeling for the peak, rather than holding
    double best_afr_ = 0.0;  // the air to fuel the peak was last found at
    double slow_power_ = 0.0; // the power, its slow drift only
    double slope_ = 0.0;      // the power's answer times the lever's movement
    bool leaning_running_ = false; // the engine was running when last leaned
};

} // namespace glideslope::sim
