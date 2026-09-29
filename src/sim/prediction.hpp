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

#include <cstdint>
#include <deque>
#include <map>
#include <optional>

namespace glideslope::sim {

// How far apart the client and the server may be before the correction is
// snapped rather than blended. Twenty metres is about a wingspan of a large
// aeroplane: nearer than that and nobody can tell it was moved, further and
// nobody would believe it had not been.
inline constexpr double snap_beyond_m = 20.0;
// How long a correction small enough to hide is taken up over.
inline constexpr double correction_blend_s = 0.25;
// The most inputs held waiting to be acknowledged. At 60 Hz this is four
// seconds, which is far longer than any round trip this project expects; a
// client further behind than that has a problem this cannot solve.
inline constexpr std::size_t most_unacknowledged = 240;
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

class Prediction {
public:
    // Flies `aircraft`, which the caller owns and must outlive this.
    explicit Prediction(Aircraft& aircraft) : aircraft_(aircraft) {}

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
    Correction reconcile(const Motion& server, std::uint32_t last_applied,
                         std::size_t steps_into, std::uint64_t server_steps);

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
    // How many steps this client has flown, which numbers them.
    std::uint64_t steps() const { return steps_; }
    std::uint32_t newest() const { return held_.empty() ? 0 : held_.back().sequence; }

private:
    struct Applied {
        std::uint32_t sequence = 0;
        Controls controls;
        // Which step of this client's this was, counted from its first.
        std::uint64_t step = 0;
    };

    Aircraft& aircraft_;
    std::deque<Applied> held_;
    // The step each input still held, or still to be put right from, began on.
    std::map<std::uint32_t, std::uint64_t> began_;
    std::uint64_t steps_ = 0;
    // The input the last step was flown on.
    std::uint32_t flying_ = 0;
    // The clocks' difference each recent word implied, newest last.
    std::deque<std::int64_t> offsets_;
};

// How far apart two states are, in metres: over the ground and in height.
double how_far_apart_m(const AircraftState& a, const AircraftState& b);

} // namespace glideslope::sim
