#pragma once

// **An input stamped as it leaves**, by both clients (sim::Prediction::sent):
// the step its prediction has flown to, plus the steps owed by this machine's
// clock - paced, as the client flies - that are not flown yet. The client
// with the window sends before its frame's ticks, which are all owed; the
// command-line client sends after flying them, and owes what its clock has
// moved on since. Either way, the step this client's flying stood at as the
// input left, which is what its clocks' difference is sampled from.
//
// **And the step the input names on the wire** (net::InputFrame::step), so
// that a server held up flies the inputs that came in meanwhile as far apart
// as this client sent them.

#include "net/inputs.hpp"
#include "sim/prediction.hpp"

#include <cstdint>

namespace glideslope::frontend {

inline std::uint32_t stamp_sent(sim::Prediction& prediction, std::uint32_t sequence,
                                std::uint64_t steps_owed) {
    const std::uint64_t step = prediction.steps() + steps_owed;
    prediction.sent(sequence, step);
    // On the wire it is 32 bits, which a server takes only the differences
    // of; the one value that says no step is never sent for one.
    return static_cast<std::uint32_t>(step % net::step_not_said);
}

} // namespace glideslope::frontend
