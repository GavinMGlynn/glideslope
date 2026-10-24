#pragma once

// A client's own aircraft: flown locally, corrected by the server.
//
// **The server owns every aircraft, so the client is always guessing.** It
// flies its own JSBSim on its own inputs so that the controls answer on the
// frame they are pressed, and the server tells it, a round trip later, where
// that aircraft really was and which input it had got to. The client then
// puts its JSBSim back to that state and flies forward again through every
// input the server had not yet seen.
//
// **Divergence is expected, not a bug.** JSBSim is floating point and the two
// machines may not even be the same platform, so the replay will not land
// exactly where the client had been. What matters is that the difference is
// bounded and that correcting it is not a jolt: a small correction is taken
// up over `blend_s`, and one too large to hide is snapped, because pretending
// smoothly to be somewhere wrong is worse than a visible jump.
//
// **This owns no socket and no snapshot of its own.** It is handed a state
// and a sequence number and told to reconcile; where they came from is the
// client's business.

#include "sim/aircraft.hpp"
#include "sim/controller.hpp"
#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>

namespace glideslope::sim {

// How far apart the client and the server may be before the correction is
// snapped rather than blended. Twenty metres is about a wingspan of a large
// aeroplane: nearer than that and nobody can tell it was moved, further and
// nobody would believe it had not been.
inline constexpr double snap_beyond_m = 20.0;
// How long a correction small enough to hide is taken up over.
inline constexpr double correction_blend_s = 0.25;
// The most inputs held waiting to be acknowledged - steps, each flown on the
// input it was: four seconds, which is far longer than any round trip this
// project expects; a client further behind than that has a problem this
// cannot solve. (It was 240, four seconds at the 60 Hz it was written for and
// two at 120: CI has drawn a frame in 1.7 s, and at frames held 1.6 s the
// client was put right by 72 m with 240 and by 2.9 m with this
// (PROJECT_STATUS.md, 2026-09-30).)
inline constexpr std::size_t most_unacknowledged =
    static_cast<std::size_t>(4 * steps_per_second);
// How many of the server's words the clocks' difference is the least of: two
// seconds of updates at 25 a second, long enough that one input in it came
// through with next to no jitter, short enough to follow a server whose clock
// runs slow.
inline constexpr std::size_t offset_window = 50;
// **How many words before the clocks' difference is taken as known**: a
// second of updates. Before then the least is still coming down as inputs
// that waited less arrive, and each time it does the client is put right by
// the steps it moved - two metres, three times, a second after joining at
// 200 ms (PROJECT_STATUS.md). Those corrections are hidden like any other;
// a prediction error measured before this is the estimate settling, not the
// prediction.
inline constexpr std::size_t offset_settled = 25;
// **How many of the newest words a pace is steered by** (sim::Pacing): ten,
// four tenths of a simulated second - enough that one came through with
// little jitter, few enough that the loop sees the difference move promptly.
inline constexpr std::size_t pace_window = 10;

class Prediction {
public:
    // Flies `aircraft`, which the caller owns and must outlive this.
    //
    // **An aircraft with a lift system is flown through a pilot's
    // controller** (sim::Controller), as the server flies a player's: her
    // STOVL mode selector hands her lift system to the transition law, which
    // the server runs, so a prediction flying her controls raw never
    // converted while the server's aircraft did. The controller's state
    // (Controller::PilotsLaws) is kept with each step held and put back
    // before the steps are flown again, as the motion is. Any other aircraft
    // is flown on its controls as they are.
    explicit Prediction(Aircraft& aircraft);

    // The controller her lift system is flown through; null for an aircraft
    // without one.
    const Controller* laws() const { return laws_ ? &*laws_ : nullptr; }

    // **What the server holds her on until her first input**
    // (glideslope_server's held_until_moved, as her levers tell it -
    // `WATCH`, net::Watched), put on her before this prediction's first
    // step. A law made on a step - STOVL mode selected - is seeded from the
    // controls she has as it begins (sim::Controller), and the server's,
    // made on her first input, from what it held her on: a prediction whose
    // first input selected STOVL mode seeded its law from a model just
    // loaded, its throttle integral nought where the server's was her trim's
    // (PROJECT_STATUS.md, 2026-10-14). Told this, both are seeded from the
    // same controls. And each step held keeps the controls she had as it
    // began, so that a law made in a step flown again is seeded as it was
    // the first time, not from the newest step's.
    void held_by_server(const Controls& held);

    // One step of the client's own flying: the input for this frame, kept
    // until the server says it has applied it.
    void step(std::uint32_t sequence, const Controls& controls);

    struct Correction {
        // How far the aircraft moved when it was put right.
        double moved_m = 0.0;
        // Whether that was too far to hide, and was snapped.
        bool snapped = false;
        // How many of the client's inputs had to be flown again.
        std::size_t replayed = 0;
        // **The step of this client's the server's word was taken to be
        // about** - the moment it was put back to - when it could be placed.
        // The position this client had predicted after the step before it is
        // what the server's word is held against.
        std::optional<std::uint64_t> at_step;
    };

    // **The server's word.** Puts the aircraft back to `server`, flies it
    // forward again through every input after `last_applied`, and says how
    // far it moved in doing so.
    Correction reconcile(const AircraftSnapshot& server, std::uint32_t last_applied);
    // **The same, from the server's word on its motion alone**, which is what
    // a state update can carry: the client's own engines and actuators, flown
    // on the same inputs, are left as they are. This is what runs over the
    // network; the snapshot above is too large to send and too slow to apply.
    //
    // **And from the moment the server's word was about, on this client's
    // clock**, not from the end of `last_applied`. The server flies an input
    // from when it arrives until the next one does, which the network decides,
    // and says how far into it it had flown (`steps_into`) and how many steps
    // it had flown in all (`server_steps`). So `last_applied` arrived at
    // server step `server_steps - steps_into`, and began here at a step this
    // client knows: the difference is how far the server's clock is behind
    // this one, plus how late the network made that input. **The least of
    // those over the last `offset_window` words is taken as the offset**: the
    // input that waited least says the clocks' difference, and the others
    // add jitter to it. The server's word is then this client's step
    // `server_steps - offset`, and the replay starts there, whichever inputs
    // those steps were flown on. Replayed from the input's end instead - or
    // from its arrival, with the jitter kept in - the client was off by the
    // steps between, metres at an aeroplane's speed.
    //
    // **And the height her STOVL law was asked to hold** at that moment, on
    // the server, when a law flew her there (net::OwnMotion). Each end seeds
    // the law's height from her height above the ground as it sees her at
    // the selector, and the stick moves both alike from there, so the
    // prediction held whatever it was off by at the selector for as long as
    // the law flew her: the controller's state put back with the motion is
    // this client's own, and the median error through a server stayed 108 mm
    // (PROJECT_STATUS.md, 2026-10-15). So it is put to the server's before
    // the steps are flown again - **only where a law flies her in the
    // state put back**: where the server's law flies her and this client's
    // does not yet, or no longer, the two disagree about the selector or its
    // moment, there is no law here to tell, and the next word that finds
    // both flying one puts it right.
    //
    // **And what else of her law it cannot work out for itself**
    // (Transition::Carried): the wing-borne autopilot's pitch integral, put
    // on the law put back with the height, where that law has an autopilot
    // to take it. Seeded a step apart - the selector placed a step off - it
    // stayed apart, 21 mm at the median, as the height did.
    Correction reconcile(const Motion& server, std::uint32_t last_applied,
                         std::size_t steps_into, std::uint64_t server_steps,
                         std::optional<double> stovl_height_asked_ft = std::nullopt,
                         const Transition::Carried& stovl_carried = {});
    // **What a word says of the clocks' difference, and nothing else**: the
    // sample `reconcile` above takes from it, for a word that is not the
    // newest of those heard together. Put right from each of them in turn,
    // the aircraft was flown through every input since, again and again -
    // twenty-five words to a slow second, each replaying up to two seconds
    // of steps, which made the next frame slower still: a client too slow
    // for that read the words ever later and never caught up (glideslope_cli
    // on CI's Ubuntu debug, a frame of 450.9 s and corrections of 42 km;
    // docs/status, 2026-10-23). The newest supersedes them, as it does in a
    // client-side prediction's reconciliation anywhere: the client is put
    // back to the newest authoritative state and flies the inputs since.
    //
    // **Returns the step of this client's the word is about**, as
    // `Correction::at_step` would have had it, so that the word is still
    // held against where this client had flown: superseded, a word is still
    // a measurement of the prediction. Nothing before the clocks' difference
    // is known.
    std::optional<std::uint64_t> hear_clock(std::uint32_t last_applied, std::size_t steps_into,
                                            std::uint64_t server_steps);

    // **The step of this client's at which input `sequence` was sent**, for
    // the clocks' difference to be taken from: the moment it left, not the
    // step it was first flown on. A client sends an input once it has flown
    // the steps owed on the one before, which on a machine too slow for her
    // prediction - or stopped part-way through a pass - can be seconds
    // later than the step it is flown from: the server applied it that much
    // later, and its sample said the clocks were that much further apart.
    // 3 s stopped between flying and sending, an F-35B's samples came 477,
    // 237 and 153 steps against 117 before, and settled near 127 while the
    // slow passes lasted; the least of the window rose with them, step by
    // step, and each step it rose placed words a step further off - 5.5 m
    // (docs/status, 2026-10-24). Stamped at sending, a sample is the time
    // it was on the network and in the server's queue, as NTP's is
    // (RFC 5905 section 8: the transmit timestamp is taken as the packet
    // leaves, and section 10's clock filter keeps the least-delay sample).
    //
    // **Once a client stamps its inputs, an input it did not stamp gives no
    // sample**: how late it was sent cannot be known - the input in force
    // as this prediction began, or one sent before it existed - and a sample
    // of unknown lateness is a spike that may be too low as well as too
    // high, so it is not used, as RFC 5905 section 10's popcorn spike
    // suppressor discards a sample it cannot trust. A client that never
    // stamps (the windowed client, so far) is sampled from where each input
    // began, as before.
    void sent(std::uint32_t sequence, std::uint64_t step);
    // **The newest sample a word gave**, for a test to read: the clocks'
    // difference taken, how many steps after the step its input began on it
    // was sent (nought where it was not stamped), and what it would have
    // said from where its input began.
    struct Sample {
        std::int64_t difference = 0;
        std::int64_t sent_late = 0;
        std::int64_t from_began = 0;
    };
    std::optional<Sample> newest_sample() const { return newest_sample_; }

    // **Another aircraft, as the server's word had it at `server_steps`**, flown
    // on to now: put there, and flown through this client's inputs since the
    // step the clocks' difference places that word at - as a correction is,
    // with the clocks' difference kept, since it is this client's and the
    // server's and not the aircraft's. Put there and not flown on, a
    // take-over heard at the end of a long frame was that frame behind, and
    // what was shown blended across the gap: about 70 m, stepping 3.5 m in a
    // sixtieth of a second on CI (PROJECT_STATUS.md, 2026-09-30). With the
    // difference not yet known it is only put there. Returns how many inputs
    // it flew.
    std::size_t adopt(const Motion& motion, std::uint64_t server_steps);

    std::size_t unacknowledged() const { return held_.size(); }
    // Whether the clocks' difference has been heard enough times to be known.
    bool settled() const { return offsets_.size() >= offset_settled; }
    // **The clocks' difference, in steps**: the least of the last
    // `offset_window` words', which places the server's word on this
    // client's clock - and what a client paces itself by (sim::Pacing).
    // Nothing before a word has said any.
    std::optional<std::int64_t> clocks_difference() const {
        if (offsets_.empty()) {
            return std::nullopt;
        }
        return *std::min_element(offsets_.begin(), offsets_.end());
    }
    // **The same over the last `pace_window` words only** - what a pace is
    // steered by. The least of two seconds of the server's words is ten of
    // real time from a server at a fifth of it, and a loop steered through
    // that lag swung (sim::Pacing).
    std::optional<std::int64_t> recent_clocks_difference() const {
        if (offsets_.empty()) {
            return std::nullopt;
        }
        const auto from = offsets_.size() > pace_window
                              ? offsets_.end() - static_cast<std::ptrdiff_t>(pace_window)
                              : offsets_.begin();
        return *std::min_element(from, offsets_.end());
    }
    // How many steps this client has flown, which numbers them.
    std::uint64_t steps() const { return steps_; }
    // **The server's word that an engine has stopped** (a state update's
    // `engine_stopped`, and which), heard before it is put right by that
    // word, so that the inputs flown again are flown with it stopped. The
    // engines are this client's own, flown on the same inputs, so a tank run
    // dry stops here too; one stopped on the server alone - a failure - did
    // not, and the prediction flew it on, put right correction by
    // correction. So the engine the server names is stopped here, if it runs.
    // **And one it stopped for that word is started again** when the server
    // says none is stopped - an aircraft flown again after a wreck. Returns
    // whether it stopped one.
    bool hear_engine_stopped(std::optional<int> engine);
    int engines_stopped_for_the_server() const { return engines_stopped_; }
    // **The server's word that the aircraft is a wreck** (a state update's
    // condition), heard before it is put right by that word. The server
    // flies a wreck no further - it is held where it struck until it flies
    // again - and so, while it says so, neither does this: each input is
    // kept as before, so the clocks' difference is still told, but no step
    // is flown, forward or again, and the aircraft stays at the server's
    // word. Flown on, a prediction put back at every word to where the
    // server's F-35B struck the ground, and flown through it again for every
    // input since, went to a NaN that a debug JSBSim's tables assert on
    // (FGTable.cpp:618; PROJECT_STATUS.md, 2026-10-14). Heard not a wreck -
    // flown again - it flies on.
    void hear_wrecked(bool wrecked) { wrecked_ = wrecked; }
    bool wrecked() const { return wrecked_; }
    // **A prediction gone to a number not finite is let go of, not flown
    // into JSBSim's tables.** Every input put on the aircraft - each
    // step's controls, the server's motion or snapshot, and the STOVL law's
    // height asked and pitch integral the server's word carries - is looked
    // at before it is put, and the motion each step leaves after it is
    // flown; the first not
    // finite stops this prediction where it is, flying nothing further,
    // forward or again, and says here what it was and at which of its steps.
    // Its client takes the server's state back as on a fresh start
    // (glideslope_cli: a new aircraft from the next word; the window client:
    // the next word on the aircraft it has). Flown on, a NaN reached a table as its
    // key and a debug JSBSim asserted (FGTable.cpp:618) on CI's Ubuntu after
    // two water wrecks, run 39883628869, whose cause was never reproduced
    // (docs/status, 2026-10-23). A NaN made inside a step from finite inputs
    // and a finite state, and taken by a table in the same step, is before
    // this can see it: the client's trap (`--trap-nan`) names that one.
    struct Lost {
        std::string what;      // the input or the number, by name
        std::uint64_t step = 0; // this prediction's step it was found at
    };
    const std::optional<Lost>& lost() const { return lost_; }
    // **When the step being flown - forward or flown again - is flown on the
    // server**, on the session's clock: this client's step `s` is the
    // server's step `s + offset`, flown at `(s + offset + 1) / 120` seconds
    // (the server counts a step before it flies it). Nothing until the
    // clocks' difference is known. What the air a prediction flies is on, so
    // that a gust is met at the same moment of it on both machines, a replay
    // included (frontend::SessionClocked).
    std::optional<double> session_time_s() const;
    std::uint32_t newest() const { return held_.empty() ? 0 : held_.back().sequence; }

private:
    struct Applied {
        std::uint32_t sequence = 0;
        Controls controls;
        // Which step of this client's this was, counted from its first.
        std::uint64_t step = 0;
        // What the motion does not carry, as the step began: flown again from
        // the server's word, the step is flown from this (ReplayState).
        ReplayState before;
        // And the lift system's controller as the step began, if she has one.
        std::optional<Controller::PilotsLaws> laws_before;
        // **And the controls on her as the step began**, where they are
        // known: what a law made on the step is seeded from (sim::
        // Controller), which ReplayState does not carry - every step sets
        // them again - so that flown again it is seeded as it was first.
        std::optional<Controls> commanded_before;
    };

    // One step's controls put on the aircraft: through the controller, for
    // an aircraft with a lift system.
    void put(const Controls& controls);

    // What the motion does not carry put back to the first step held, before
    // it is flown again.
    void as_at_first_held();
    // Every step held flown again, from the first, each kept as it began
    // this time; how many.
    std::size_t fly_held_again();

    Aircraft& aircraft_;
    std::optional<Controller> laws_;
    // The controls last put on her: the step's, or the server's held
    // (held_by_server); none for a model just loaded.
    std::optional<Controls> commanded_;
    std::deque<Applied> held_;
    // The step each input still held, or still to be put right from, began on.
    std::map<std::uint32_t, std::uint64_t> began_;
    // The step each input still held was sent at, where its client said
    // (`sent`); and whether it has ever said.
    std::map<std::uint32_t, std::uint64_t> sent_;
    bool stamps_ = false;
    std::optional<Sample> newest_sample_;
    std::uint64_t steps_ = 0;
    int engines_stopped_ = 0;
    bool wrecked_ = false;
    std::optional<Lost> lost_;
    // Flies `controls` on the aircraft as step `step`, or finds them, or
    // what the step left, not finite and is lost instead.
    void fly(const Controls& controls, std::uint64_t step);
    std::optional<int> stopped_for_the_server_;
    // The input the last step was flown on.
    std::uint32_t flying_ = 0;
    // Which of this client's steps is being flown, or was last.
    std::uint64_t flying_step_ = 0;
    // The clocks' difference each recent word implied, newest last.
    std::deque<std::int64_t> offsets_;
};

// **The first control, or number of a motion, not finite**, by name - a
// NaN or an infinity - or nothing (Prediction::lost).
std::optional<std::string> first_not_finite(const Controls& c);
std::optional<std::string> first_not_finite(const Motion& m);
std::optional<std::string> first_not_finite(const AircraftSnapshot& s);

// How far apart two states are, in metres: over the ground and in height.
double how_far_apart_m(const AircraftState& a, const AircraftState& b);

} // namespace glideslope::sim
