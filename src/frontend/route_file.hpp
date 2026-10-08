#pragma once

// **A copilot's route written as a file**: a plan's `waypoint` and `orbit`
// lines, `glide KT` first if it glides and `land` last if it lands, read as it
// is into the `COPILOT_ROUTE` it would be sent as - checked by nothing here.
// What `glideslope_cli connect --send-route` sends, and what the server's
// `--ai-route` gives an AI aircraft, for tests.

#include "net/messages.hpp"

#include <string>

namespace glideslope::frontend {

// Throws std::runtime_error for a file that cannot be read, and
// sim::FlightPlanError for lines that are not a route's.
net::CopilotRoute route_from_file(const std::string& file);

} // namespace glideslope::frontend
