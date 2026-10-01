#include "sim/leaner.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);
// The feel for the peak: a hundredth of the lever's travel either way, once
// every two seconds.
constexpr double feel = 0.01;
constexpr double feel_period_s = 2.0;
// The power's slow drift is what it was over about the last feel; what is
// left is the lever's doing.
constexpr double drift_s = feel_period_s;
// The way the power rises, averaged over two feels.
constexpr double slope_s = 2.0 * feel_period_s;
// How fast the lever's resting place follows it: for a power rising 1% for
// each hundredth of travel, about 0.03 of the travel a second.
constexpr double follow = 6.0;
// The lever's movement changes the power by about a percent; a change far
// larger than that is something else - a gust, the throttle, the propeller -
// and counts for no more than this.
constexpr double largest_answer = 0.03;
// The lever never rests leaner than this, whatever the power says.
constexpr double leanest = 0.2;
// Petrol's chemically correct ratio of air to fuel.
constexpr double stoichiometric = 14.7;
// **Below this throttle the peak is not felt for** - its power is too little
// to lean by - and the ratio of air to fuel the peak was last found at is
// held instead, the mixture moving at `hold_rate` of its travel a second for
// each part the ratio is off.
constexpr double least_throttle = 0.4;
constexpr double hold_rate = 1.0;

double phase_of(long step) {
    return 2.0 * std::numbers::pi * static_cast<double>(step) * dt / feel_period_s;
}

} // namespace

MixtureLeaner::MixtureLeaner(const Aircraft& aircraft, double mixture)
    : a_(aircraft), engines_(aircraft.figures().engines), resting_(mixture) {}

double MixtureLeaner::lean(double throttle) {
    // Each engine's power for the air it takes in - its manifold pressure
    // times its rpm - so that what the throttle, the propeller and the height
    // do to the power is divided out, and what is left is the mixture's.
    double power = 0.0;
    double afr = 0.0;
    bool running = engines_ > 0;
    for (int i = 0; i < engines_; ++i) {
        const std::string engine = "propulsion/engine[" + std::to_string(i) + "]/";
        running = running && a_.property(engine + "set-running") > 0.0;
        const double air =
            a_.property(engine + "map-inhg") * a_.property(engine + "engine-rpm");
        power += a_.property(engine + "power-hp") / std::max(air, 1.0);
        afr = std::max(afr, a_.property(engine + "AFR"));
    }
    if (!running) {
        feeling_ = false;
        return resting_;
    }
    if (throttle < least_throttle) {
        // **Throttled back, the power says little about the mixture**, and
        // at idle it is mostly friction. So the peak is not felt for: the
        // ratio of air to fuel it was found at is held, by the engine's own
        // report of it, so that a descent enriches the mixture as the air
        // thickens - as a pilot enriches on the way down - and the throttle
        // opened again finds the engine where it was.
        feeling_ = false;
        if (best_afr_ > 0.0 && std::isfinite(afr)) {
            resting_ = std::clamp(resting_ + hold_rate * (afr - best_afr_) / best_afr_ * dt,
                                  leanest, 1.0);
        }
        return resting_;
    }
    if (!feeling_) {
        feeling_ = true;
        slow_power_ = power;
        slope_ = 0.0;
        step_ = 0;
    }
    // The lever was moved by sin(phase) a step ago; the power now is its
    // answer, and the part of it the drift does not explain is the lever's.
    slow_power_ += (power - slow_power_) * dt / drift_s;
    const double answer =
        std::clamp((power - slow_power_) / std::max(std::abs(slow_power_), 1e-9),
                   -largest_answer, largest_answer);
    slope_ += (answer * std::sin(phase_of(step_)) - slope_) * dt / slope_s;
    // **Never leaner than chemically correct.** The most power is always rich
    // of it, whatever the engine; leaner than it, the lever goes back richer
    // whatever the power seems to say.
    const double way = afr > stoichiometric ? std::abs(slope_) : slope_;
    resting_ = std::clamp(resting_ + follow * way * dt, leanest, 1.0);
    if (std::isfinite(afr)) {
        best_afr_ = best_afr_ > 0.0 ? best_afr_ + (afr - best_afr_) * dt / slope_s : afr;
    }
    ++step_;
    return std::clamp(resting_ + feel * std::sin(phase_of(step_)), 0.0, 1.0);
}

} // namespace glideslope::sim
