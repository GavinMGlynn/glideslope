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
//
// **On a runway of `runway_condition`** (sim/runway_condition.hpp): any but
// dry is wet, and the runway it needs to land is 1.15 times the dry one
// (sim::wet_landing_factor, 14 CFR 121.195(d)).
copilot::Brief brief_for(const std::filesystem::path& data, const std::string& catalogue_id,
                         int runway_condition = 6);

// The same, as a planner's request; the command, airport and runways are
// left for the caller.
copilot::PlanRequest plan_request_for(const std::filesystem::path& data,
                                      const std::string& catalogue_id,
                                      int runway_condition = 6);

// **Where a plan from an airport may land** (copilot::PlanRequest::fields):
// the airports of `all` within `landing_fields_m` of the first end of
// `airport`'s that says its elevation, nearest first, each with all its ends
// that say theirs - at most `most_landing_airports` airports and
// `most_landing_fields` ends. An airport whose ends would pass that cap is
// passed over whole and the next tried, so that a big neighbour does not
// crowd the smaller ones out. None where the airport has no end with an
// elevation.
inline constexpr double landing_fields_m = 40000.0;
inline constexpr std::size_t most_landing_airports = 4;
inline constexpr std::size_t most_landing_fields = 24;
std::vector<world::RunwayEnd> landing_fields(const std::vector<world::RunwayEnd>& all,
                                             const std::vector<world::RunwayEnd>& airport);

} // namespace glideslope::frontend
