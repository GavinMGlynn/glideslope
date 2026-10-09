#pragma once

// **What a model is told of an aircraft, from the data** - for the copilot
// (copilot::Brief) and the planner (copilot::PlanRequest), which see no
// figures file themselves (cmake/Copilot.cmake): its catalogue name, its
// speeds from its figures file and its cruise from the catalogue. Shared by
// the client, the server and glideslope_cli, so that each tells a model the
// same.

#include <filesystem>
#include <cstddef>
#include <string>
#include <vector>

#include "copilot/copilot.hpp"
#include "copilot/planner.hpp"

namespace glideslope::frontend {

// The aircraft `catalogue_id`: its approach speed where it publishes a stall
// (sim::approach_speeds; none for the 747-400 and the F-22A), its climb
// (sim::departure_speeds - published, or measured where nothing published
// gives one), the speeds a plan may fly it at (sim::plan_speeds) and its
// cruise. The task is left for the caller. Throws as they do.
copilot::Brief brief_for(const std::filesystem::path& data, const std::string& catalogue_id);

// The same, as a planner's request; the command, airport and runways are
// left for the caller.
copilot::PlanRequest plan_request_for(const std::filesystem::path& data,
                                      const std::string& catalogue_id);

// **Where a plan from an airport may land** (copilot::PlanRequest::fields):
// the runway ends of `all` that say their elevation within `landing_fields_m`
// of the first such end of `airport`'s, nearest first, at most
// `most_landing_fields` - so that the airport's own and its neighbours' are
// among them. None where the airport has no end with an elevation.
inline constexpr double landing_fields_m = 40000.0;
inline constexpr std::size_t most_landing_fields = 12;
std::vector<world::RunwayEnd> landing_fields(const std::vector<world::RunwayEnd>& all,
                                             const std::vector<world::RunwayEnd>& airport);

} // namespace glideslope::frontend
