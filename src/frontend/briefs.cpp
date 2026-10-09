#include "frontend/briefs.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

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
    b.glide_slowest_kts = sim::glide_slowest_kts(data, entry.model);
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

std::vector<world::RunwayEnd> landing_fields(const std::vector<world::RunwayEnd>& all,
                                             const std::vector<world::RunwayEnd>& airport) {
    const auto from = std::find_if(airport.begin(), airport.end(),
                                   [](const world::RunwayEnd& e) { return copilot::plannable(e); });
    if (from == airport.end()) {
        return {};
    }
    std::vector<std::pair<double, const world::RunwayEnd*>> near;
    for (const world::RunwayEnd& end : all) {
        if (!copilot::plannable(end)) {
            continue;
        }
        const double d = sim::distance_m(from->latitude_deg, from->longitude_deg, end.latitude_deg,
                                         end.longitude_deg);
        if (d <= landing_fields_m) {
            near.emplace_back(d, &end);
        }
    }
    std::stable_sort(near.begin(), near.end(),
                     [](const auto& x, const auto& y) { return x.first < y.first; });
    std::vector<world::RunwayEnd> out;
    for (std::size_t i = 0; i < near.size() && i < most_landing_fields; ++i) {
        out.push_back(*near[i].second);
    }
    return out;
}

} // namespace glideslope::frontend
