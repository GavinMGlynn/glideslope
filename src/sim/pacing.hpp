#pragma once

// **How fast a predicting client flies its own aircraft**, steered so that it
// flies at the server's pace.
//
// The server flies each input from when it arrives until the next one does.
// A server behind real time - a slow machine, a loaded runner - flies fewer
// steps of each than a client flying at its own machine's pace, so the two
// fly the same inputs for different numbers of steps and part, by metres at
// an aeroplane's speed. The client cannot make the server keep up; it can fly
// at the server's pace instead.
//
// **What it is steered by is the prediction's own clocks' difference**
// (sim::Prediction::clocks_difference): where the server applied an input,
// less where this client began it, in steps. While the two fly at one pace it
// holds still; a server slower than this client makes it fall, step by step.
// So the difference is held where it was when it was first known, by a
// proportional and integral loop on how far it has moved: fallen, this client
// flies slower; risen, faster. Flown instead at the rate fitted to the
// session clock's updates - open loop - a rate fitted a little low left the
// client falling further behind the server, and nothing was compared
// (PROJECT_STATUS.md, 2026-10-07).
//
// Pure arithmetic on a clock it is handed, so a unit test can set both
// machines' clocks.

#include <cstdint>
#include <optional>

namespace glideslope::sim {

class Pacing {
public:
    // **The loop's gains**, on the difference in seconds (steps over
    // `steps_per_second`): critically damped at half a radian a second, a
    // time constant of two seconds - the same as the window the difference
    // is the least over, and a hundred times the update interval.
    static constexpr double proportional_per_s = 1.0;
    static constexpr double integral_per_s2 = 0.25;
    // The slowest and fastest it flies, as fractions of this machine's clock.
    static constexpr double slowest = 0.1;
    static constexpr double fastest = 2.0;

    // **This machine's clock, paced**: where the client's flying should have
    // got to at `local_s`, advanced at the pace since the last call. The
    // first call starts it at `local_s`.
    double at(double local_s);

    // **The clocks' difference the prediction measures**, in steps, at
    // `local_s`. The first heard after `hold` is the one held to; from it the
    // pace starts at `rate` if given - what the session clock's fit says the
    // server's pace is, which the loop then corrects - or where it was.
    void heard(std::int64_t difference, double local_s, std::optional<double> rate = {});

    // **Before the difference is first known**, flown at `rate` - what the
    // session clock's fit says the server's pace is - so that the loop does
    // not start from this machine's pace with a round trip of inputs flown at
    // it: changed at once from 1 to 0.6, the inputs already flown at 1 put
    // the client off by 4 m as the loop began. Ignored once held. **Never
    // above real time**: a server is behind it or keeping it, and the
    // window client's first fit, from words heard in a burst after a slow
    // start, said 2.0 - the loop then spent ten seconds coming back from it.
    void before_held(double rate);

    // **Held anew from the next difference heard** - a prediction started
    // again, whose steps are numbered from nought - at the pace it has now.
    void hold();

    double pace() const { return pace_; }
    // How far the difference has been from the one held, at worst and last,
    // in steps, since it was held.
    std::int64_t worst_off() const { return worst_off_; }
    std::int64_t off() const { return off_; }
    bool holding() const { return held_.has_value(); }

private:
    std::optional<std::int64_t> held_;
    bool ever_held_ = false;
    double integral_ = 0.0;
    std::optional<double> heard_at_s_;
    std::optional<double> last_local_s_;
    double paced_s_ = 0.0;
    double pace_ = 1.0;
    std::int64_t worst_off_ = 0;
    std::int64_t off_ = 0;
};

} // namespace glideslope::sim
