#pragma once

// **An input stamped as it leaves**, by both clients (sim::Prediction::sent):
// the step its prediction has flown to, plus the steps owed by this machine's
// clock - paced, as the client flies - that are not flown yet. The client
// with the window sends before its frame's ticks, which are all owed; the
// command-line client sends after flying them, and owes what its clock has
// moved on since. Either way, the step this client's flying stood at as the
// input left, which is what its clocks' difference is sampled from.

#include "sim/prediction.hpp"

#include <cstdint>

namespace glideslope::frontend {

inline void stamp_sent(sim::Prediction& prediction, std::uint32_t sequence,
                       std::uint64_t steps_owed) {
    prediction.sent(sequence, prediction.steps() + steps_owed);
}

} // namespace glideslope::frontend
