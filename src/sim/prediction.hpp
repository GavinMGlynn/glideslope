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
    };

    // **The server's word.** Puts the aircraft back to `server`, flies it
    // forward again through every input after `last_applied`, and says how
    // far it moved in doing so.
    Correction reconcile(const AircraftSnapshot& server, std::uint32_t last_applied);
    // **The same, from the server's word on its motion alone**, which is what
    // a state update can carry: the client's own engines and actuators, flown
    // on the same inputs, are left as they are. This is what runs over the
    // network; the snapshot above is too large to send and too slow to apply.
    Correction reconcile(const Motion& server, std::uint32_t last_applied);

    std::size_t unacknowledged() const { return held_.size(); }
    std::uint32_t newest() const { return held_.empty() ? 0 : held_.back().sequence; }

private:
    struct Applied {
        std::uint32_t sequence = 0;
        Controls controls;
    };

    Aircraft& aircraft_;
    std::deque<Applied> held_;
};

// How far apart two states are, in metres: over the ground and in height.
double how_far_apart_m(const AircraftState& a, const AircraftState& b);

} // namespace glideslope::sim
