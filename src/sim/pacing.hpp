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

class Prediction;

class Pacing {
public:
    // **The loop's gains**, on the difference in seconds (steps over
    // `steps_per_second`): critically damped - proportional twice the
    // natural frequency, integral its square - at half a radian a second, a
    // time constant of two seconds - the same as the window the difference
    // is the least over, and fifty times the time between looks at 25 words
    // a second.
    static constexpr double damped_per_s = 0.5;
    // **Slower when looks are sparse**: at most a radian of the loop's
    // natural frequency in `looks_per_radian` looks, so a client whose frames
    // are a second long - looking once a frame, at a difference a frame old,
    // flown on for the next - steers at 0.2 a second, not 0.5. **Why**: at 0.5
    // a look a second is under a look and a half a cycle's radian, a frame of
    // delay on top, and the loop rang - it flew at 1.2 to 1.4 against a server
    // keeping real time, and at 0.68 against one at 0.5 (docs/status,
    // 2026-11-18). At five looks a radian, frames of 0.25, 0.5 and 1 s each
    // settle within 0.002 of the server's pace (the unit test); frames of 2 s
    // settle slower, 0.014 off after 60 s against a server at 0.5, and are
    // not tested. At 25 words a second it is 0.5 as before.
    static constexpr double looks_per_radian = 5.0;
    // The time between looks the gains are set by: an average, a fifth of
    // the way to each new one, each counted at most `longest_look_s` - a
    // machine that stopped once is not taken to look that seldom.
    static constexpr double looks_settle = 0.2;
    static constexpr double longest_look_s = 2.0;
    // **And steered between them** for `steered_looks` times the time
    // between looks, if that is longer than `steered_without_a_word_s`: a
    // client looking once a second flies all of its frame at what the loop
    // asks, not a quarter of a second of it - with a quarter, its pace barely
    // moved from the integral's, and against a server at 0.5 the difference
    // ran 1,300 steps away.
    static constexpr double steered_looks = 1.5;
    // The slowest and fastest it flies, as fractions of this machine's clock.
    static constexpr double slowest = 0.1;
    static constexpr double fastest = 2.0;
    // **The most two paces are taken to part by**, as a fraction of real
    // time: the difference moving faster than this between two words is a
    // jump - a starved server catching up four steps a look, all at once -
    // and what it is held to moves with it, rather than the pace chasing it.
    // **Why a half**: it is not the 0.1 to 2 the pace may be flown at - those
    // bound what the loop may ask for; this bounds what it believes. Once
    // held, the client flies within a few percent of the server's pace, so a
    // real parting is small; a server slowing all at once parts by more (0.7
    // from real time to 0.3), and a half still steers by most of it while the
    // rest is forgiven, so it is followed in under 20 s (the unit test: 0.05
    // took 34). A catch-up of four steps a look, 30 ms apart, is a parting of
    // over 1, and is forgiven as a jump; between them, a half. Plus a step,
    // for the difference being counted in whole steps.
    static constexpr double most_parting = 0.5;

    // **This machine's clock, paced**: where the client's flying should have
    // got to at `local_s`, advanced at the pace since the last call. The
    // first call starts it at `local_s`.
    double at(double local_s);
    // **The same, without advancing it**: where `at(local_s)` would put the
    // client's flying - what an input sent part-way through a pass is
    // stamped with (sim::Prediction::sent). Before the first call, `local_s`.
    double would_be_at(double local_s) const {
        if (!last_local_s_) {
            return local_s;
        }
        return local_s > *last_local_s_ ? paced_s_ + flown_in(local_s - *last_local_s_)
                                        : paced_s_;
    }
    // **How long what the loop asks is flown for, without a word**: a
    // quarter of a second, six words' time. Its proportional part is asked
    // for the time until the next word, 40 ms; through a longer gap - a
    // machine that stopped the client - it is flown at the loop's integral
    // alone, the server's pace as the loop has learnt it. Flown at all of
    // it, an F-35B's client steering 2% fast as it stopped for 3 s gained
    // eight steps on the server, which no word asked for, and the next words
    // were placed six steps off, 8.3 m (docs/status, 2026-10-24). Or
    // `steered_looks` times the time between looks, if that is longer.
    static constexpr double steered_without_a_word_s = 0.25;

    // **The clocks' difference the prediction measures**, in steps, at
    // `local_s`. **Words read together are one look**: a client whose
    // frame is long reads a frame's words at once, all at the frame's
    // `local_s`, and each after the first is taken in place of the one
    // before it, as if the newest had been heard alone - the loop steps once,
    // over the time since the look before. Stepped by each, with no time
    // between them, every move of the difference was a jump: frames held a
    // second sample it once a frame, inputs being sent once a frame, and its
    // move - 30 steps a frame, the client flying at 0.64 of a server keeping
    // real time - came between two words read together and was forgiven,
    // never steered by, and the words were placed 35 to 72 steps off: put
    // right 48 to 159 m (docs/status, 2026-11-18). The first heard after
    // `hold` is the one held to; from it the pace starts at `rate` if given -
    // what the session clock's fit says the server's pace is, which the loop
    // then corrects - or where it was.
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

    // **What both clients do with a word of the server's**, once the
    // prediction has been put right by it: at `rate` while the prediction's
    // clocks' difference is not yet known, if there is a rate; once it is,
    // steered by the newest words' (Prediction::recent_clocks_difference).
    void follow(const Prediction& prediction, double local_s, std::optional<double> rate);

    // **Held anew from the next difference heard** - a prediction started
    // again, whose steps are numbered from nought - at the pace it has now.
    void hold();

    double pace() const { return pace_; }
    // How far the difference has been from the one held, at worst and last,
    // in steps, since it was held.
    std::int64_t worst_off() const { return worst_off_; }
    std::int64_t off() const { return off_; }
    bool holding() const { return held_.has_value(); }

    // **The paced time flown in `dt` of this machine's**: at the pace for
    // up to `steered_without_a_word_s` of it, and at the loop's integral
    // after - what both clients fly a pass by.
    double flown_in(double dt) const;

private:
    std::optional<std::int64_t> held_;
    std::int64_t last_difference_ = 0;
    bool ever_held_ = false;
    // The pace the integral adds to real time.
    double integral_ = 0.0;
    std::optional<double> heard_at_s_;
    double between_looks_s_ = 1.0 / 25.0;
    std::optional<double> last_local_s_;
    double paced_s_ = 0.0;
    double pace_ = 1.0;
    std::int64_t worst_off_ = 0;
    std::int64_t off_ = 0;
    // **The loop as it stood before the words read together** - heard at
    // one `local_s` - so that each word after the first of them is taken in
    // place of the one before, not after it with no time between.
    struct Loop {
        std::optional<std::int64_t> held;
        std::int64_t last_difference = 0;
        bool ever_held = false;
        double integral = 0.0;
        std::optional<double> heard_at_s;
        double between_looks_s = 0.0;
        double pace = 1.0;
        std::int64_t worst_off = 0;
        std::int64_t off = 0;
    };
    std::optional<Loop> before_read_together_;
};

} // namespace glideslope::sim
