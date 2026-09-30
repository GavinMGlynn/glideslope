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
#include <future>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace glideslope::frontend {

static_assert(copilot::most_route_waypoints == net::most_route_waypoints &&
                  copilot::most_waypoint_name_bytes == net::most_waypoint_name_bytes,
              "what a copilot may answer is what a COPILOT_ROUTE may carry");

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
    std::vector<world::RunwayEnd> runways;

    Ground(std::string_view coverage_text, const std::filesystem::path& cache)
        : coverage(coverage_text),
          fetch(world::http_fetch()),
          tiles(cache, fetch),
          geoid(world::egm2008_geoid(cache, fetch)),
          dem(coverage, tiles, &geoid),
          runways(world::world_runways(cache, fetch)) {}
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
    std::string coverage(std::istreambuf_iterator<char>(coverage_file), {});
    const std::filesystem::path cache = platform::cache_directory();
    // Its fetches given up as it goes (`going_`), so that quitting while
    // they are under way does not wait for them.
    ground_ = std::async(std::launch::async,
                         [coverage = std::move(coverage), cache, going = &going_] {
                             const world::FetchesGivenUp given_up(*going);
                             return std::make_shared<Ground>(coverage, cache);
                         })
                  .share();
}

PlayersCopilot::~PlayersCopilot() {
    // **Going**: every fetch of its ground's given up, and the model's
    // request abandoned (copilot::Copilot's destructor); its question first,
    // which may be using this, then its ground - each waited for only as
    // long as giving up takes.
    going_ = true;
    helper_.reset();
    ground_ = {};
}

std::string PlayersCopilot::provider() const {
    return provider_;
}

std::vector<std::string> PlayersCopilot::said() {
    return std::exchange(said_, {});
}

copilot::Situation PlayersCopilot::situation(double simulation_s, const net::AircraftState& own,
                                             const std::string& event,
                                             std::vector<sim::Waypoint> route,
                                             const std::shared_ptr<Ground>& ground) {
    const world::Geodetic at = world::to_geodetic({own.x_m, own.y_m, own.z_m});
    copilot::Situation now;
    now.seconds = simulation_s;
    now.latitude_deg = at.latitude_deg;
    now.longitude_deg = at.longitude_deg;
    now.altitude_ft =
        (at.height_m - ground->geoid.undulation(at.latitude_deg, at.longitude_deg)) *
        feet_per_metre;
    now.ground_ft = ground->dem.height_above_geoid(at.latitude_deg, at.longitude_deg) * feet_per_metre;
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
    now.engine_running = own.condition != net::Condition::engine_stopped;
    now.route = std::move(route);
    std::vector<std::pair<double, const world::RunwayEnd*>> near;
    for (const world::RunwayEnd& end : ground->runways) {
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
    if (gone_) {
        return std::nullopt;
    }
    const bool ai_now = own.controller == net::Controller::ai;
    // **Taken back by the player, it stands by**: nothing more asked, and
    // no answer sent, until the player asks again.
    if (ai_flying_ && !ai_now && engaged_) {
        engaged_ = false;
        wanted_.reset();
        route_.clear();
        said_.push_back("its pilot has taken it back: the copilot stands by");
    }
    ai_flying_ = ai_now;
    // **An engine stopped is asked about while it is engaged**, whoever the
    // server says is flying: a question asked with the engine running may
    // still be out, its route - no glide - to be refused by the server, which
    // then never hands the aircraft over; waiting for that would never ask.
    // Running again - flown again after a wreck - it may be asked again.
    const bool engine_stopped = own.condition == net::Condition::engine_stopped;
    if (!engine_stopped && own.condition == net::Condition::flying) {
        engine_said_ = false;
    }
    if (engine_stopped && !engine_said_ && engaged_) {
        engine_said_ = true;
        wanted_ = "the engine has stopped";
    }
    if (!wanted_ && engaged_ && ai_now && !helper_->asking() && answered_at_s_ &&
        o_.routine_s > 0.0 && simulation_s - *answered_at_s_ >= o_.routine_s) {
        wanted_ = "a routine look";
    }
    if (wanted_ && !helper_->asking()) {
        const std::string event = *wanted_;
        std::vector<sim::Waypoint> route = route_;
        // Its ground waited for on the question's thread - a copy of the
        // future each, for one is not to be read from two threads - and
        // what making it threw taken as the question not answered.
        if (helper_->ask([this, simulation_s, own, event, route = std::move(route),
                          ground = ground_]() mutable {
                const world::FetchesGivenUp given_up(going_);
                return situation(simulation_s, own, event, std::move(route), ground.get());
            })) {
            said_.push_back("asked its copilot, " + event);
            wanted_.reset();
            asked_at_s_ = simulation_s;
            ++questions_;
            return std::nullopt;
        }
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
    } catch (const std::exception& e) {
        // **Its ground could not be had** - a geoid, a DEM or the runways
        // not fetched or not read: every question would throw it again, so
        // the copilot is gone for the session, and says so once.
        gone_ = true;
        said_.push_back(std::string("no copilot: what it is told could not be worked out: ") +
                        e.what());
        return std::nullopt;
    }
    if (!change) {
        return std::nullopt;
    }
    answered_at_s_ = simulation_s;
    ++answers_;
    if (!engaged_) {
        said_.push_back("its copilot answered, and was not heard: its pilot has it");
        return std::nullopt;
    }
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
                                                static_cast<std::uint8_t>(w.orbit->turns),
                                                w.orbit->right};
        }
        route.waypoints.push_back(std::move(p));
        names += " " + w.name;
    }
    route_ = change->plan.waypoints;
    said_.push_back("its copilot answered with a route of " +
                    std::to_string(route.waypoints.size()) + ":" + names);
    return route;
}

} // namespace glideslope::frontend
