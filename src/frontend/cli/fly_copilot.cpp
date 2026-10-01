#include "frontend/cli/fly_copilot.hpp"

// **An aircraft flown with a language model as its copilot**, headless, over
// the DEM: `glideslope_cli fly-copilot`. The AI flies; the model is asked, as
// the flight goes, whether the route should change, and what it answers is
// checked and flown as a new route (copilot/copilot.hpp) - never as a control.
//
// **When it is asked**: when it is engaged ("the pilot has asked"), when the
// engine stops, when the route has been flown, and otherwise a minute after
// its last answer was taken ("a routine look"). A question outstanding is not
// asked again; what happens meanwhile is asked once it is answered. A
// question the model could not answer - every answer refused, or the
// service failing - changes nothing: the AI flies on, and what happened is
// asked again at once, twice at most.
//
// **When its answer is flown**: `--thinking` simulated seconds after it was
// asked (default 45), or when it comes if that is later - never sooner, so
// that a flight played back from a recording, whose answers come at once,
// takes each answer at the step the recorded flight did, whenever the model
// answered in time. Asking a model now, the flight is paced to the clock
// while a question is outstanding, as a flight in the simulator is, so that
// its seconds of thinking are seconds of flight; otherwise it runs as fast as
// it can. The step never waits for the model either way. 45 s because a model
// asked for a whole route along a coast has taken 38 s to answer; one slower
// than that is said, and its flight plays back otherwise than it flew.
//
// **What it says**: each question and answer, each waypoint passed; with
// `--to LAT LON`, when it came within 3 km of there, and how far the coast
// was from it on the way, looked for every ten seconds (coast_m) from when
// the copilot's first route is flown - the farthest, and in how many looks
// it was within 1 km; with `--engine-fails-at S`, how near it came to
// a runway's threshold after the engine stopped, and how high above the
// ground it was then, and the airspeeds it glided at from 45 s after the
// glide began. It ends at `--minutes` (default 20), or once the
// engine has stopped and it is 300 ft above the ground.

#include "copilot/copilot.hpp"
#include "copilot/provider.hpp"
#include "platform/paths.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/controller.hpp"
#include "sim/crash.hpp"
#include "sim/departure.hpp"
#include "sim/fixed_step.hpp"
#include "sim/lander.hpp"
#include "sim/plan.hpp"
#include "sim/terrain.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/runway_ground.hpp"
#include "world/runways.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

namespace copilot = glideslope::copilot;
namespace sim = glideslope::sim;
namespace world = glideslope::world;

constexpr double feet_per_metre = 3.280839895013123;
constexpr double near_m = 3000.0;         // "came to" the destination
constexpr double coast_within_m = 1000.0; // "near" the coast
constexpr double coast_ring_m = 250.0;
constexpr double coast_farthest_m = 3000.0; // looked for no farther
constexpr double least_above_ground_ft = 300.0;
constexpr std::size_t most_fields = 6;
constexpr double fields_within_m = 40000.0;

double number(std::string_view text, const char* what) {
    const std::string copy(text);
    char* end = nullptr;
    const double v = std::strtod(copy.c_str(), &end);
    if (copy.empty() || *end != '\0' || !std::isfinite(v)) {
        throw std::runtime_error(std::string("fly-copilot: ") + what + " must be a number, not \"" +
                                 copy + "\"");
    }
    return v;
}

// Where `from` is, `metres` away on `bearing`: near enough on a sphere, for
// looking about over a few kilometres.
std::pair<double, double> moved(double lat, double lon, double bearing_deg, double metres) {
    constexpr double radians = 3.14159265358979323846 / 180.0;
    const double north = metres * std::cos(bearing_deg * radians) / 111195.0;
    const double east =
        metres * std::sin(bearing_deg * radians) / (111195.0 * std::cos(lat * radians));
    return {lat + north, lon + east};
}

// **How far the coast is from a place**: the least ring about it, 250 m
// apart and each looked at in 16 directions, inside which there is both land
// and water - the DEM's own water mask, sea, lake or river. Past 3 km,
// infinity.
template <typename Water>
double coast_m(double lat, double lon, const Water& wet) {
    bool land = !wet(lat, lon);
    bool sea = !land;
    for (double r = coast_ring_m; r <= coast_farthest_m + 1.0; r += coast_ring_m) {
        for (int i = 0; i < 16; ++i) {
            const auto [la, lo] = moved(lat, lon, 22.5 * i, r);
            (wet(la, lo) ? sea : land) = true;
        }
        if (land && sea) {
            return r;
        }
    }
    return std::numeric_limits<double>::infinity();
}

} // namespace

int fly_copilot(const std::filesystem::path& data, const std::vector<std::string_view>& args) {
    if (args.size() < 8) {
        std::fprintf(stderr, "glideslope_cli: fly-copilot AIRCRAFT LAT LON FEET HEADING KNOTS TASK\n");
        return 2;
    }
    const std::string aircraft_id(args[1]);
    const double start_lat = number(args[2], "the latitude");
    const double start_lon = number(args[3], "the longitude");
    const double start_ft = number(args[4], "the altitude");
    const double start_heading = number(args[5], "the heading");
    const double start_kts = number(args[6], "the airspeed");
    const std::string task(args[7]);
    std::string provider_name = "openai";
    std::string model;
    std::string record;
    std::string played;
    double minutes = 20.0;
    double thinking_s = 45.0;
    std::optional<double> engine_fails_s;
    std::optional<std::pair<double, double>> destination;
    for (std::size_t i = 8; i < args.size(); ++i) {
        const bool value = i + 1 < args.size();
        if (args[i] == "--provider" && value) {
            provider_name = std::string(args[++i]);
        } else if (args[i] == "--model" && value) {
            model = std::string(args[++i]);
        } else if (args[i] == "--record" && value) {
            record = std::string(args[++i]);
        } else if (args[i] == "--playback" && value) {
            played = std::string(args[++i]);
        } else if (args[i] == "--minutes" && value) {
            minutes = number(args[++i], "--minutes");
        } else if (args[i] == "--thinking" && value) {
            thinking_s = number(args[++i], "--thinking");
        } else if (args[i] == "--engine-fails-at" && value) {
            engine_fails_s = number(args[++i], "--engine-fails-at");
        } else if (args[i] == "--to" && i + 2 < args.size()) {
            const double lat = number(args[++i], "--to's latitude");
            destination = std::pair{lat, number(args[++i], "--to's longitude")};
        } else {
            std::fprintf(stderr, "glideslope_cli: fly-copilot: what is \"%s\"?\n",
                         std::string(args[i]).c_str());
            return 2;
        }
    }
    if (!record.empty() && !played.empty()) {
        std::fprintf(stderr, "glideslope_cli: fly-copilot: --record or --playback, not both\n");
        return 2;
    }

    // **Opt-in, with the player's key**: a provider without one is refused.
    copilot::Post post = copilot::http_post();
    if (!played.empty()) {
        post = copilot::playback(played, copilot::Match::but_numbers);
    } else if (!record.empty()) {
        std::filesystem::remove(record);
        post = copilot::recording(post, record);
    }
    const std::string key = !played.empty()             ? std::string()
                            : provider_name == "openai" ? glideslope::platform::openai_key()
                                                        : glideslope::platform::anthropic_key();
    auto provider = copilot::make_provider(provider_name, key, model, std::move(post), !played.empty());
    const std::string provider_said = provider->name() + ", " + provider->model();

    const sim::CatalogueEntry entry = sim::find_aircraft(data, aircraft_id);
    copilot::Brief brief;
    brief.aircraft = entry.id;
    brief.aircraft_name = entry.name;
    brief.approach_kts = sim::approach_speeds(data, entry.model).vref_kts;
    brief.climb_kts = sim::departure_speeds(data, entry.model).climb_kts;
    brief.cruise_kts = entry.start_airspeed_kts;
    brief.task = task;
    copilot::Copilot helper(std::move(provider), brief);

    // The ground, as fly-plan has it: the DEM, and heights above the sea.
    std::ifstream coverage_file(data / "dem" / "coverage.txt", std::ios::binary);
    if (!coverage_file) {
        throw std::runtime_error("cannot read " + (data / "dem" / "coverage.txt").string());
    }
    const world::DemCoverage coverage(std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    const std::filesystem::path cache = glideslope::platform::cache_directory();
    const world::Fetch fetch = world::http_fetch();
    world::DownloadedTiles tiles(cache, fetch);
    const world::Geoid geoid = world::egm2008_geoid(cache, fetch);
    // The ground the aircraft meets: the DEM, with every runway its own
    // surface (world/runway_ground.hpp), as the server's.
    auto dem = std::make_shared<world::Dem>(coverage, tiles, &geoid);
    auto collision = std::make_shared<world::CollisionGround>(dem, world::runway_surfaces(data));
    const std::vector<world::RunwayEnd> runways = world::world_runways(cache, fetch);
    const auto undulation_ft = [&](double lat, double lon) {
        return geoid.undulation(lat, lon) * feet_per_metre;
    };

    sim::Aircraft aircraft(data / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<sim::FunctionTerrain>(
        [collision](double lat, double lon) { return collision->height_above_ellipsoid(lat, lon); },
        [collision](double lat, double lon) {
            return collision->water(lat, lon) != world::Water::none;
        }));
    sim::InitialConditions ic;
    ic.latitude_deg = start_lat;
    ic.longitude_deg = start_lon;
    ic.altitude_ft = start_ft + undulation_ft(start_lat, start_lon);
    ic.terrain_elevation_ft = collision->height_above_ellipsoid(start_lat, start_lon) * feet_per_metre;
    ic.heading_deg = start_heading;
    ic.airspeed_kts = start_kts;
    ic.gear = 0.0;
    ic.engine_running = true;
    aircraft.initialize(ic);
    sim::Controller controller(aircraft, sim::Controls{});
    // Until the copilot's first route, the AI holds what the aircraft is doing.
    controller.to_ai();

    const auto seconds = [](std::int64_t steps) {
        return static_cast<double>(steps) / static_cast<double>(sim::steps_per_second);
    };
    const auto steps_of = [](double s) {
        return static_cast<std::int64_t>(std::llround(s * static_cast<double>(sim::steps_per_second)));
    };
    const auto lat_now = [&] { return aircraft.property("position/lat-geod-deg"); };
    const auto lon_now = [&] { return aircraft.property("position/long-gc-deg"); };
    const auto sea_level_ft = [&] {
        return aircraft.property("position/h-sl-ft") - undulation_ft(lat_now(), lon_now());
    };
    const auto ground_ft = [&](double lat, double lon) {
        return collision->height_above_geoid(lat, lon) * feet_per_metre;
    };

    // The route being flown, as the model wrote it: heights above the sea.
    std::vector<sim::Waypoint> as_written;
    bool engine_running = true;
    std::optional<double> gliding;
    const auto situation = [&](std::int64_t step, const std::string& event) {
        copilot::Situation now;
        now.seconds = seconds(step);
        now.latitude_deg = lat_now();
        now.longitude_deg = lon_now();
        now.altitude_ft = sea_level_ft();
        now.ground_ft = ground_ft(now.latitude_deg, now.longitude_deg);
        now.heading_deg = aircraft.property("attitude/psi-deg");
        now.airspeed_kts = aircraft.property("velocities/vc-kts");
        now.vertical_speed_fpm = aircraft.property("velocities/h-dot-fps") * 60.0;
        now.engine_running = engine_running;
        now.gliding_kts = gliding;
        if (const sim::Navigator* n = controller.navigator(); n && !n->finished()) {
            now.route.assign(as_written.begin() + static_cast<std::ptrdiff_t>(n->next()),
                             as_written.end());
        }
        // Runway ends nearby that say their elevation, nearest first.
        std::vector<std::pair<double, const world::RunwayEnd*>> near;
        for (const world::RunwayEnd& end : runways) {
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
    };

    sim::GroundJudge judge(entry.seaplane);
    const std::int64_t most_steps = steps_of(minutes * 60.0);
    const std::int64_t thinking_steps = steps_of(thinking_s);
    const std::int64_t routine_steps = steps_of(60.0);
    std::int64_t asked_at = 0;
    std::int64_t answered_at = 0;
    std::optional<std::string> to_ask = "the pilot has asked";
    bool route_flown_said = false;
    int questions = 0;
    int failures = 0;
    int asked_again = 0;
    std::string asked_about;
    int routes = 0;
    int keeps = 0;
    // Paced to the clock while a question is outstanding, asking a model now.
    const bool live = played.empty();
    auto paced_from = std::chrono::steady_clock::now();

    // What is measured.
    std::size_t leg = 0;
    double closest_m = std::numeric_limits<double>::infinity();
    double closest_ft = 0.0;
    std::optional<double> reached_s;
    int coast_samples = 0;
    int coast_seen = 0;
    double coast_farthest = 0.0;
    std::optional<std::int64_t> stopped_at;
    double nearest_field_m = std::numeric_limits<double>::infinity();
    double nearest_field_above_ft = 0.0;
    std::string nearest_field;
    double nearest_field_s = 0.0;
    double glide_slowest = std::numeric_limits<double>::infinity();
    double glide_fastest = 0.0;
    std::vector<world::RunwayEnd> fields_then;
    std::optional<std::int64_t> glide_began;

    std::optional<std::string> playback_ended;
    std::int64_t step = 0;
    for (; step < most_steps; ++step) {
        // **The engine stops**, as a failure does.
        if (engine_fails_s && !stopped_at && step == steps_of(*engine_fails_s)) {
            aircraft.fail_engine(0, false);
            engine_running = false;
            stopped_at = step;
            to_ask = "the engine has stopped";
            fields_then = situation(step, "").fields;
            std::printf("the engine stopped, %.0f s in, at %.0f ft\n", seconds(step), sea_level_ft());
        }
        const sim::Navigator* navigator = controller.navigator();
        if (!route_flown_said && navigator && navigator->finished() && !to_ask) {
            route_flown_said = true;
            to_ask = "the route has been flown";
        }
        if (!to_ask && !helper.asking() && step - answered_at >= routine_steps) {
            to_ask = "a routine look";
        }

        // Asked between two steps; nothing here waits for the model.
        if (to_ask && !helper.asking()) {
            if (helper.ask(situation(step, *to_ask))) {
                ++questions;
                asked_about = *to_ask;
                std::printf("asked, %.0f s in: %s\n", seconds(step), to_ask->c_str());
                to_ask.reset();
                asked_at = step;
                paced_from = std::chrono::steady_clock::now();
            }
        }
        if (helper.asking() && step - asked_at >= thinking_steps) {
            std::optional<copilot::Change> change;
            try {
                change = helper.answered();
            } catch (const copilot::ProviderError& e) {
                // **Played back, every question was answered**: one the
                // recording does not hold is a flight that has gone another
                // way. The flight ends there, says what it measured up to
                // then - so that a check of it can fail on its own terms -
                // and then says this, and fails.
                if (!live) {
                    playback_ended = e.what();
                    break;
                }
                // **No answer is no change**: the AI flies on as it was. What
                // happened is asked again at once, twice at most; a routine
                // look waits for the next.
                ++failures;
                answered_at = step;
                std::printf("no answer, %.0f s in: %s\n", seconds(step), e.what());
                if (asked_about != "a routine look" && ++asked_again <= 2) {
                    to_ask = asked_about;
                }
            }
            if (change) {
                asked_again = 0;
                answered_at = step;
                if (step - asked_at > thinking_steps) {
                    std::printf("answered %.1f s after it was asked, later than the %.0f s "
                                "allowed\n",
                                seconds(step - asked_at), thinking_s);
                }
                for (const std::string& why : change->refused) {
                    std::printf("  refused: %s\n", why.c_str());
                }
                if (change->keep) {
                    ++keeps;
                    std::printf("answered, %.0f s in: keep\n", seconds(step));
                } else {
                    ++routes;
                    std::string names;
                    for (const sim::Waypoint& w : change->plan.waypoints) {
                        names += " " + w.name;
                    }
                    std::printf("answered, %.0f s in: a route of %zu,%s%s\n", seconds(step),
                                change->plan.waypoints.size(), names.c_str(),
                                change->glide_kts
                                    ? (", gliding at " + std::to_string(std::lround(*change->glide_kts)) +
                                       " kt")
                                          .c_str()
                                    : "");
                    as_written = change->plan.waypoints;
                    // Its heights above the sea, as the aircraft's are: above
                    // the ellipsoid.
                    sim::FlightPlan plan = change->plan;
                    for (sim::Waypoint& w : plan.waypoints) {
                        w.altitude_ft += undulation_ft(w.latitude_deg, w.longitude_deg);
                    }
                    controller.replan(std::move(plan));
                    controller.set_glide(change->glide_kts);
                    if (change->glide_kts && !glide_began) {
                        glide_began = step;
                    }
                    gliding = change->glide_kts;
                    route_flown_said = false;
                    leg = 0;
                    closest_m = std::numeric_limits<double>::infinity();
                }
            }
        }
        // **Paced to the clock while the model thinks**, asking one now: its
        // seconds are the flight's.
        if (live && helper.asking()) {
            const auto due = paced_from + std::chrono::microseconds(
                                              (step - asked_at) * 1000000 / sim::steps_per_second);
            std::this_thread::sleep_until(due);
        }

        aircraft.set_controls(controller.fly());
        aircraft.step();
        if (const auto wrecked = judge.judge(aircraft)) {
            std::printf("wrecked, %.0f s in: %s\n", seconds(step), wrecked->c_str());
            return 1;
        }

        const double lat = lat_now();
        const double lon = lon_now();
        const double ft = sea_level_ft();
        // Each waypoint passed.
        navigator = controller.navigator();
        if (navigator && !as_written.empty()) {
            if (navigator->next() != leg && leg < as_written.size()) {
                std::printf("passed %s %.0f m off at %.0f ft, %.0f s in\n",
                            as_written[leg].name.c_str(), closest_m, closest_ft, seconds(step));
                leg = navigator->next();
                closest_m = std::numeric_limits<double>::infinity();
            }
            if (leg < as_written.size()) {
                const double d = sim::distance_m(lat, lon, as_written[leg].latitude_deg,
                                                 as_written[leg].longitude_deg);
                if (d < closest_m) {
                    closest_m = d;
                    closest_ft = ft;
                }
            }
        }
        // The way to the destination, and the coast along it.
        if (destination && !reached_s) {
            if (sim::distance_m(lat, lon, destination->first, destination->second) <= near_m) {
                reached_s = seconds(step);
                std::printf("came within %.0f km of the destination, %.0f s in; the coast at most "
                            "%.0f m from it on the way, and within %.0f km in %d of %d looks\n",
                            near_m / 1000.0, *reached_s, coast_farthest, coast_within_m / 1000.0,
                            coast_seen, coast_samples);
            } else if (routes > 0 && step % steps_of(10.0) == 0) {
                const double d = coast_m(lat, lon, [&](double la, double lo) {
                    return collision->water(la, lo) != world::Water::none;
                });
                ++coast_samples;
                coast_seen += d <= coast_within_m ? 1 : 0;
                coast_farthest = std::max(coast_farthest, d);
            }
        }
        // After the engine has stopped: the glide, and the nearest it came to
        // a runway it could land on, with the height it had there.
        if (stopped_at) {
            const double above_ft = ft - ground_ft(lat, lon);
            if (glide_began && step - *glide_began > steps_of(45.0)) {
                const double kts = aircraft.property("velocities/vc-kts");
                glide_slowest = std::min(glide_slowest, kts);
                glide_fastest = std::max(glide_fastest, kts);
            }
            for (const world::RunwayEnd& end : fields_then) {
                const double d = sim::distance_m(lat, lon, end.latitude_deg, end.longitude_deg);
                if (d < nearest_field_m) {
                    nearest_field_m = d;
                    nearest_field_above_ft = above_ft;
                    nearest_field = end.airport + " " + end.ident;
                    nearest_field_s = seconds(step - *stopped_at);
                }
            }
            if (above_ft < least_above_ground_ft) {
                double from_m = std::numeric_limits<double>::infinity();
                std::string from = "no runway";
                for (const world::RunwayEnd& end : fields_then) {
                    const double d = sim::distance_m(lat, lon, end.latitude_deg, end.longitude_deg);
                    if (d < from_m) {
                        from_m = d;
                        from = end.airport + " " + end.ident;
                    }
                }
                std::printf("down to %.0f ft above the ground, %.0f s after the engine stopped, "
                            "%.0f m from %s's threshold\n",
                            least_above_ground_ft, seconds(step - *stopped_at), from_m, from.c_str());
                break;
            }
        }
    }
    if (stopped_at) {
        std::printf("after the engine stopped: nearest %s's threshold, %.0f m from it at %.0f ft "
                    "above the ground, %.0f s after\n",
                    nearest_field.empty() ? "no runway" : nearest_field.c_str(), nearest_field_m,
                    nearest_field_above_ft, nearest_field_s);
        if (glide_slowest <= glide_fastest) {
            std::printf("gliding at %.0f to %.0f kt from 45 s after the glide began, %.0f s after "
                        "the engine stopped\n",
                        glide_slowest, glide_fastest, seconds(*glide_began - *stopped_at));
        } else {
            std::printf("never glided\n");
        }
    }
    if (destination && !reached_s) {
        std::printf("never came within %.0f km of the destination; the coast at most %.0f m "
                    "from it, and within %.0f km in %d of %d looks\n",
                    near_m / 1000.0, coast_farthest, coast_within_m / 1000.0, coast_seen,
                    coast_samples);
    }
    std::printf("the copilot, %s, was asked %d times: %d new routes, %d kept, %d not "
                "answered; each answer taken between two steps\n",
                provider_said.c_str(), questions, routes, keeps, failures);
    if (playback_ended) {
        std::fprintf(stderr, "glideslope_cli: fly-copilot: %s\n", playback_ended->c_str());
        return 3;
    }
    return 0;
}
