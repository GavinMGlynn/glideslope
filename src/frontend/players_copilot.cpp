#include "frontend/players_copilot.hpp"

#include "copilot/provider.hpp"
#include "platform/paths.hpp"
#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/lander.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geodesy.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace glideslope::frontend {

namespace {

constexpr double feet_per_metre = 3.280839895013123;
constexpr double knots_per_mps = 3600.0 / 1852.0;
constexpr double radians = 3.14159265358979323846 / 180.0;
constexpr std::size_t most_fields = 6;
constexpr double fields_within_m = 40000.0;

} // namespace

struct PlayersCopilot::Ground {
    world::DemCoverage coverage;
    world::Fetch fetch;
    world::DownloadedTiles tiles;
    world::Geoid geoid;
    world::Dem dem;

    Ground(std::string_view coverage_text, const std::filesystem::path& cache)
        : coverage(coverage_text),
          fetch(world::http_fetch()),
          tiles(cache, fetch),
          geoid(world::egm2008_geoid(cache, fetch)),
          dem(coverage, tiles, &geoid) {}
};

PlayersCopilot::PlayersCopilot(const std::filesystem::path& data, PlayersCopilotOptions options)
    : o_(std::move(options)) {
    copilot::Post post = copilot::http_post();
    const bool played = !o_.playback.empty();
    if (played) {
        post = copilot::playback(o_.playback, copilot::Match::but_numbers);
    } else if (!o_.record.empty()) {
        std::filesystem::remove(o_.record);
        post = copilot::recording(post, o_.record);
    }
    // **The player's key, read here and sent to the model, and nowhere else.**
    const std::string key = played                        ? std::string()
                            : o_.provider == "openai"     ? platform::openai_key()
                                                          : platform::anthropic_key();
    auto provider = copilot::make_provider(o_.provider, key, o_.model, std::move(post), played);
    provider_ = provider->name() + ", " + provider->model();

    const sim::CatalogueEntry entry = sim::find_aircraft(data, o_.aircraft);
    copilot::Brief brief;
    brief.aircraft = entry.id;
    brief.aircraft_name = entry.name;
    brief.approach_kts = sim::approach_speeds(data, entry.model).vref_kts;
    brief.climb_kts = sim::departure_speeds(data, entry.model).climb_kts;
    brief.cruise_kts = entry.start_airspeed_kts;
    brief.task = o_.task;
    helper_ = std::make_unique<copilot::Copilot>(std::move(provider), brief);

    std::ifstream coverage_file(data / "dem" / "coverage.txt", std::ios::binary);
    if (!coverage_file) {
        throw std::runtime_error("cannot read " + (data / "dem" / "coverage.txt").string());
    }
    const std::string coverage(std::istreambuf_iterator<char>(coverage_file), {});
    const std::filesystem::path cache = platform::cache_directory();
    ground_ = std::make_unique<Ground>(coverage, cache);
    runways_ = world::world_runways(cache, ground_->fetch);
}

PlayersCopilot::~PlayersCopilot() = default;

std::string PlayersCopilot::provider() const {
    return provider_;
}

std::vector<std::string> PlayersCopilot::said() {
    return std::exchange(said_, {});
}

copilot::Situation PlayersCopilot::situation(double simulation_s, const net::AircraftState& own,
                                             const std::string& event) const {
    const world::Geodetic at = world::to_geodetic({own.x_m, own.y_m, own.z_m});
    copilot::Situation now;
    now.seconds = simulation_s;
    now.latitude_deg = at.latitude_deg;
    now.longitude_deg = at.longitude_deg;
    now.altitude_ft =
        (at.height_m - ground_->geoid.undulation(at.latitude_deg, at.longitude_deg)) *
        feet_per_metre;
    now.ground_ft = ground_->dem.height_above_geoid(at.latitude_deg, at.longitude_deg) * feet_per_metre;
    now.heading_deg = static_cast<double>(own.heading_deg);
    const double vx = static_cast<double>(own.vx_mps);
    const double vy = static_cast<double>(own.vy_mps);
    const double vz = static_cast<double>(own.vz_mps);
    now.airspeed_kts = std::sqrt(vx * vx + vy * vy + vz * vz) * knots_per_mps;
    const double lat = at.latitude_deg * radians;
    const double lon = at.longitude_deg * radians;
    const double up = vx * std::cos(lat) * std::cos(lon) + vy * std::cos(lat) * std::sin(lon) +
                      vz * std::sin(lat);
    now.vertical_speed_fpm = up * feet_per_metre * 60.0;
    now.engine_running = true;
    now.route = route_;
    std::vector<std::pair<double, const world::RunwayEnd*>> near;
    for (const world::RunwayEnd& end : runways_) {
        if (std::isnan(end.elevation_ft)) {
            continue;
        }
        const double d =
            sim::distance_m(now.latitude_deg, now.longitude_deg, end.latitude_deg, end.longitude_deg);
        if (d <= fields_within_m) {
            near.emplace_back(d, &end);
        }
    }
    std::sort(near.begin(), near.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (std::size_t i = 0; i < near.size() && i < most_fields; ++i) {
        now.fields.push_back(*near[i].second);
    }
    now.event = event;
    return now;
}

std::optional<net::CopilotRoute> PlayersCopilot::look(double simulation_s,
                                                      const net::AircraftState& own) {
    if (!wanted_ && !helper_->asking() && answered_at_s_ && o_.routine_s > 0.0 &&
        simulation_s - *answered_at_s_ >= o_.routine_s) {
        wanted_ = "a routine look";
    }
    if (wanted_ && !helper_->asking() && helper_->ask(situation(simulation_s, own, *wanted_))) {
        said_.push_back("asked its copilot, " + *wanted_);
        wanted_.reset();
        asked_at_s_ = simulation_s;
        ++questions_;
        return std::nullopt;
    }
    if (!helper_->asking() || !asked_at_s_ || simulation_s - *asked_at_s_ < o_.thinking_s) {
        return std::nullopt;
    }
    std::optional<copilot::Change> change;
    try {
        change = helper_->answered();
    } catch (const copilot::ProviderError& e) {
        // Played back, a question the recording does not hold is a flight
        // gone another way: said, and not flown past.
        if (!o_.playback.empty()) {
            throw;
        }
        answered_at_s_ = simulation_s;
        said_.push_back(std::string("its copilot did not answer: ") + e.what());
        return std::nullopt;
    }
    if (!change) {
        return std::nullopt;
    }
    answered_at_s_ = simulation_s;
    ++answers_;
    for (const std::string& why : change->refused) {
        said_.push_back("its copilot's answer refused: " + why);
    }
    if (change->keep) {
        said_.push_back("its copilot answered: keep");
        return std::nullopt;
    }
    net::CopilotRoute route;
    route.glide_kts = change->glide_kts;
    std::string names;
    for (const sim::Waypoint& w : change->plan.waypoints) {
        net::RouteWaypoint p;
        p.name = w.name;
        p.latitude_deg = w.latitude_deg;
        p.longitude_deg = w.longitude_deg;
        p.altitude_ft = w.altitude_ft;
        p.airspeed_kts = w.airspeed_kts;
        if (w.orbit) {
            p.orbit = net::RouteWaypoint::Orbit{w.orbit->radius_m,
                                                static_cast<std::uint8_t>(std::clamp(w.orbit->turns, 0, 255)),
                                                w.orbit->right};
        }
        route.waypoints.push_back(std::move(p));
        names += " " + w.name;
    }
    if (route.waypoints.size() > net::most_route_waypoints) {
        said_.push_back("its copilot's route of " + std::to_string(route.waypoints.size()) +
                        " waypoints is more than " + std::to_string(net::most_route_waypoints) +
                        " can be sent: not sent");
        return std::nullopt;
    }
    route_ = change->plan.waypoints;
    said_.push_back("its copilot answered with a route of " +
                    std::to_string(route.waypoints.size()) + ":" + names);
    return route;
}

} // namespace glideslope::frontend
