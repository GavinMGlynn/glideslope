#include "frontend/briefs.hpp"

#include <algorithm>
#include <cmath>
#include <string>
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
    // Each airport's ends with an elevation within the radius, nearest
    // first, and the airport as near as its nearest end.
    struct Airport {
        double nearest_m = 0.0;
        std::vector<std::pair<double, const world::RunwayEnd*>> ends;
    };
    std::vector<std::pair<std::string, Airport>> airports;
    for (const world::RunwayEnd& end : all) {
        if (!copilot::plannable(end)) {
            continue;
        }
        const double d = sim::distance_m(from->latitude_deg, from->longitude_deg, end.latitude_deg,
                                         end.longitude_deg);
        if (d > landing_fields_m) {
            continue;
        }
        auto at = std::find_if(airports.begin(), airports.end(),
                               [&](const auto& a) { return a.first == end.airport; });
        if (at == airports.end()) {
            airports.push_back({end.airport, Airport{d, {}}});
            at = airports.end() - 1;
        }
        at->second.nearest_m = std::min(at->second.nearest_m, d);
        at->second.ends.emplace_back(d, &end);
    }
    std::stable_sort(airports.begin(), airports.end(), [](const auto& x, const auto& y) {
        return x.second.nearest_m < y.second.nearest_m;
    });
    // **Whole airports, nearest first**: one with more ends than are left
    // under the cap is passed over, not cut short, and the next tried.
    std::vector<world::RunwayEnd> out;
    std::size_t taken = 0;
    for (auto& [name, a] : airports) {
        if (taken == most_landing_airports) {
            break;
        }
        if (out.size() + a.ends.size() > most_landing_fields) {
            continue;
        }
        std::stable_sort(a.ends.begin(), a.ends.end(),
                         [](const auto& x, const auto& y) { return x.first < y.first; });
        for (const auto& e : a.ends) {
            out.push_back(*e.second);
        }
        ++taken;
    }
    return out;
}

} // namespace glideslope::frontend
