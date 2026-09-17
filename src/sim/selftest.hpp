#pragma once

// The selftest: a fixed input log, flown, reduced to one hash.
//
// The tripwire for the flight model on one machine. The simulation is floating
// point and machines differ, so the hash is not expected to agree across
// platforms - that is what the cross-platform flight checks are for - but on one
// build on one machine it must not move unless the physics was changed on
// purpose.

#include <cstdint>
#include <filesystem>
#include <string>

#include "sim/aircraft.hpp"

namespace glideslope::sim {

struct SelftestResult {
    std::string model;
    std::int64_t steps = 0;
    std::uint64_t hash = 0; // FNV-1a over the state after every step
    AircraftState final_state;
};

// Reads the input log at `log_file` and flies it. Throws std::runtime_error if
// the log cannot be read or says something this does not understand.
SelftestResult run_selftest(const std::filesystem::path& jsbsim_root,
                            const std::filesystem::path& log_file);

} // namespace glideslope::sim
