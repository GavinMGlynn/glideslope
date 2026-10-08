#include "frontend/route_file.hpp"

#include "sim/plan.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace glideslope::frontend {

net::CopilotRoute route_from_file(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot read " + file);
    }
    std::string text{std::istreambuf_iterator<char>(in), {}};
    net::CopilotRoute route;
    if (text.rfind("glide ", 0) == 0) {
        const auto end = text.find('\n');
        route.glide_kts = std::strtod(text.substr(6, end - 6).c_str(), nullptr);
        text = end == std::string::npos ? std::string() : text.substr(end + 1);
    }
    const sim::FlightPlan plan = sim::parse_flight_plan("aircraft any\nstart 0 0 0 0 1\n" + text);
    for (const sim::Waypoint& w : plan.waypoints) {
        net::RouteWaypoint p;
        p.name = w.name;
        p.latitude_deg = w.latitude_deg;
        p.longitude_deg = w.longitude_deg;
        p.altitude_ft = w.altitude_ft;
        p.airspeed_kts = w.airspeed_kts;
        if (w.orbit) {
            p.orbit = net::RouteWaypoint::Orbit{
                w.orbit->radius_m, static_cast<std::uint8_t>(w.orbit->turns), w.orbit->right};
        }
        route.waypoints.push_back(p);
    }
    if (const auto& l = plan.landing) {
        route.landing = net::RouteLanding{l->name, l->threshold_lat_deg, l->threshold_lon_deg,
                                          l->elevation_ft, l->heading_deg, l->length_m};
    }
    return route;
}

} // namespace glideslope::frontend
