#include "frontend/briefs.hpp"

#include <cmath>

#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/lander.hpp"

namespace glideslope::frontend {

copilot::Brief brief_for(const std::filesystem::path& data, const std::string& catalogue_id) {
    const sim::CatalogueEntry entry = sim::find_aircraft(data, catalogue_id);
    copilot::Brief b;
    b.aircraft = entry.id;
    b.aircraft_name = entry.name;
    if (sim::publishes_approach_speed(data, entry.model)) {
        b.approach_kts = std::round(sim::approach_speeds(data, entry.model).vref_kts);
    }
    const sim::PlanSpeeds plannable = sim::plan_speeds(data, entry.model);
    b.slowest_kts = plannable.slowest_kts;
    b.fastest_kts = plannable.fastest_kts;
    b.climb_kts = sim::departure_speeds(data, entry.model).climb_kts;
    b.cruise_kts = entry.start_airspeed_kts;
    return b;
}

copilot::PlanRequest plan_request_for(const std::filesystem::path& data,
                                      const std::string& catalogue_id) {
    const copilot::Brief b = brief_for(data, catalogue_id);
    copilot::PlanRequest r;
    r.aircraft = b.aircraft;
    r.aircraft_name = b.aircraft_name;
    r.approach_kts = b.approach_kts;
    r.slowest_kts = b.slowest_kts;
    r.fastest_kts = b.fastest_kts;
    r.climb_kts = b.climb_kts;
    r.cruise_kts = b.cruise_kts;
    return r;
}

} // namespace glideslope::frontend
