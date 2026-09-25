// glideslope - the simulator.
//
// It opens a window, or renders headless, on one of its screens: the flight -
// the Cessna over the DEM's terrain, seen from its cockpit, with the HUD - the
// terrain alone, from a point toward another, or a test scene.
//
//   glideslope [--headless] [--gpu-driver NAME] [--size WxH]
//              [--screen flight|terrain|sky|origin|depth]
//              [--at LAT,LON,HEIGHT | --at-ecef X,Y,Z] [--toward LAT,LON,HEIGHT]
//              [--imagery on|off] [--terrain open|ion|google]
//              [--weather STATION [--microburst LAT,LON]...]
//              [--metar REPORT [--station LAT,LON]] [--autopilot] [--plan PLAN]
//              [--view NAME]
//              [--shot FILE] [--shot-at TICK] [--trace]
//
// Test flags. --shot writes the frame drawn at simulation tick --shot-at
// (default 2) as a BMP and exits; while shooting, every frame advances exactly
// two ticks - a sixtieth of a second - whatever the clock says, so the same
// command draws the same frame on every machine. --trace prints the flight's
// state after every tick.

#include "flight.hpp"
#include "online.hpp"
#include "platform/end_process.hpp"
#include "platform/no_crash_dialogs.hpp"
#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sky.hpp"
#include "gfx/terrain_tiles.hpp"
#include "platform/input.hpp"
#include "net/session.hpp"
#include "platform/paths.hpp"
#include "scenes.hpp"
#include "sim/catalogue.hpp"
#include "sim/fixed_step.hpp"
#include "sim/navigator.hpp"
#include "terrain.hpp"
#include "sim/version.hpp"
#include "world/dem.hpp"
#include "world/geodesy.hpp"
#include "world/metar.hpp"
#include "world/weather.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Options {
    bool headless = false;
    std::string driver;
    std::string shot;
    std::int64_t shot_at = 2;
    bool trace = false;
    int width = 1280;
    int height = 720;
    std::string screen = "flight";
    std::optional<glideslope::world::Geodetic> at;
    std::optional<glideslope::world::Ecef> at_ecef;
    std::optional<glideslope::world::Geodetic> toward;
    bool imagery = true;
    std::string terrain_provider = "open";
    std::string mismatch;
    // **Where the server is**, if this client is to join one. `--online`
    // reads it from server.txt; `--server`/`--server-key` say it outright.
    std::string server;     // HOST:PORT
    std::string server_key; // 64 hexadecimal digits
    bool online = false;
    std::vector<glideslope::world::Microburst> microbursts;
    std::string weather_station;
    std::string metar;
    std::optional<std::array<double, 2>> station;
    bool autopilot = false;
    std::string plan;
    std::string aircraft = "c172p";
    std::string view = "cockpit";
    // The phase whose checklist is on screen; empty shows none.
    std::string checklist;
    // A test flag, as --shot and --trace are: the same frame shot with the
    // aeroplane and without it differ in exactly its pixels, which is how a
    // test finds the outline it draws.
    bool draw_aircraft = true;
    // On a server, ride along in the first AI aircraft from the start.
    bool ride_along = false;
    bool on_ground = false;
};

void usage(std::FILE* out) {
    std::fputs(
        "usage: glideslope [--headless] [--gpu-driver vulkan|direct3d12|metal]\n"
        "                  [--size WxH] [--screen flight|terrain|sky|origin|depth]\n"
        "                  [--at LAT,LON,HEIGHT | --at-ecef X,Y,Z]\n"
        "                  [--toward LAT,LON,HEIGHT] [--imagery on|off]\n"
        "                  [--terrain open|ion|google] [--mismatch FILE]\n"
        "                  [--checklist PHASE]\n"
        "                  [--weather STATION [--microburst LAT,LON]...]\n"
        "                  [--metar REPORT [--station LAT,LON]]\n"
        "                  [--aircraft ID] [--on-ground] [--autopilot] [--plan PLAN]\n"
        "                  [--view cockpit|ahead|behind|left|right|above|orbit]\n"
        "                  [--draw-aircraft on|off]\n"
        "                  [--shot FILE] [--shot-at TICK] [--trace]\n"
        "                  [--online | --server HOST PORT --server-key HEX]\n"
        "       glideslope --version | --help\n"
        "\n"
        "  --screen      what to show: the flight (the default), the terrain alone,\n"
        "                or a test scene\n"
        "  --at          where: a latitude and longitude in degrees and a height\n"
        "                in metres above the WGS84 ellipsoid - the flight starts\n"
        "                there, a scene is built there; --at-ecef in Earth-centred\n"
        "                metres, for scenes\n"
        "  --toward      where the terrain screen looks, as --at is given\n"
        "  --imagery     drape the open imagery on the terrain (the default), or\n"
        "                tint it by height instead\n"
        "  --terrain     where the terrain drawn comes from: the open data, by\n"
        "                default, or Cesium ion or Google's Photorealistic 3D Tiles\n"
        "                with your own token or key. The ground the aircraft meets\n"
        "                is the open DEM whichever is drawn\n"
        "  --checklist   show this phase of flight's checklist, which ticks itself\n"
        "                as the aeroplane flies: before-start, taxi, take-off,\n"
        "                climb, cruise, descent, approach, landing, after-landing\n"
        "  --mismatch    measure how far the drawn terrain is from the ground the\n"
        "                aircraft meets, at the places FILE names - one a line, a\n"
        "                name then a latitude and longitude - and print it; nothing\n"
        "                is drawn and nothing is flown\n"
        "  --weather     fly in the weather reported now at an airfield, by its\n"
        "                ICAO code - its METAR, and Open-Meteo's winds aloft\n"
        "  --microburst  a microburst in that weather at a latitude and longitude,\n"
        "                for the flight's first fifteen minutes\n"
        "  --metar       show the terrain screen in the weather a METAR reports:\n"
        "                its cloud, visibility, and rain or snow\n"
        "  --station     where that METAR is observed: on the ground at a latitude\n"
        "                and longitude; by default, beneath --at\n"
        "  --aircraft    the aircraft flown, by its id in the data's catalogue\n"
        "                (glideslope_cli aircraft lists them); the Cessna 172P,\n"
        "                c172p, by default\n"
        "  --on-ground   start standing at --at's latitude and longitude, on the\n"
        "                ground, the engines idling and the brakes on until B is\n"
        "                pressed, rather than flying - or, a seaplane, afloat on\n"
        "                water there\n"
        "  --view        where it is seen from: the cockpit, by default, or outside\n"
        "                it from ahead, behind, left, right or above, or an orbit\n"
        "                around it; V steps through them\n"
        "  --ride-along  on a server, ride along in the first AI aircraft: its\n"
        "                cockpit, its instruments and its controls; W steps through\n"
        "                every aircraft in the sky and back to your own\n"
        "  --draw-aircraft  draw the aeroplane in the outside views (the default),\n"
        "                or leave it out: a test shoots both to find the outline it\n"
        "                draws\n"
        "  --autopilot   the AI flies the aircraft from the start, holding what it\n"
        "                is doing; A hands it between the pilot and the AI\n"
        "  --plan        the AI flies a flight plan - a file, or one in data/plans\n"
        "                by its name - from the plan's start\n"
        "  --shot        write the frame at tick --shot-at (default 2) and exit;\n"
        "                each frame is then two ticks, whatever the clock says, or\n"
        "                as many as keep the flight to 300 frames\n"
        "  --trace       print the flight's state after every tick\n",
        out);
}

std::optional<long> parse_integer(std::string_view text) {
    long value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::array<int, 2>> parse_size(std::string_view text) {
    const auto x = text.find('x');
    if (x == std::string_view::npos) {
        return std::nullopt;
    }
    const auto w = parse_integer(text.substr(0, x));
    const auto h = parse_integer(text.substr(x + 1));
    if (!w || !h || *w <= 0 || *h <= 0 || *w > 16384 || *h > 16384) {
        return std::nullopt;
    }
    return std::array<int, 2>{static_cast<int>(*w), static_cast<int>(*h)};
}

// Three comma-separated numbers.
std::optional<std::array<double, 3>> parse_triple(std::string_view text) {
    std::array<double, 3> values{};
    const std::string copy(text);
    const char* at = copy.c_str();
    for (std::size_t i = 0; i < 3; ++i) {
        char* end = nullptr;
        values[i] = std::strtod(at, &end);
        if (end == at || *end != (i < 2 ? ',' : '\0')) {
            return std::nullopt;
        }
        at = end + 1;
    }
    return values;
}

} // namespace

static int run_program(int argc, char** argv) {
    // First: a failed assert prints and ends the program rather than
    // waiting on a dialog nobody will answer (platform/no_crash_dialogs.hpp).
    glideslope::platform::no_crash_dialogs();
    // Before anything that can log: Cesium Native's log belongs on standard
    // error, not in the middle of a --trace line. See gfx/terrain_tiles.hpp.
    glideslope::gfx::log_to_standard_error();
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    Options o;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        const bool has_value = i + 1 < args.size();
        bool ok = true;
        if (a == "--version") {
            const std::string_view v = glideslope::sim::version();
            std::printf("glideslope %.*s\n", static_cast<int>(v.size()), v.data());
            return 0;
        } else if (a == "--help") {
            usage(stdout);
            return 0;
        } else if (a == "--headless") {
            o.headless = true;
        } else if (a == "--online") {
            o.online = true;
        } else if (a == "--server" && i + 2 < args.size()) {
            o.server = std::string(args[i + 1]) + ":" + std::string(args[i + 2]);
            i += 2;
        } else if (a == "--server-key" && has_value) {
            o.server_key = std::string(args[++i]);
        } else if (a == "--trace") {
            o.trace = true;
        } else if (a == "--gpu-driver" && has_value) {
            o.driver = std::string(args[++i]);
        } else if (a == "--shot" && has_value) {
            o.shot = std::string(args[++i]);
        } else if (a == "--shot-at" && has_value) {
            const auto tick = parse_integer(args[++i]);
            ok = tick && *tick >= 0;
            o.shot_at = tick.value_or(0);
        } else if (a == "--size" && has_value) {
            const auto size = parse_size(args[++i]);
            ok = size.has_value();
            if (size) {
                o.width = (*size)[0];
                o.height = (*size)[1];
            }
        } else if (a == "--screen" && has_value) {
            o.screen = std::string(args[++i]);
            ok = o.screen == "flight" || o.screen == "terrain" || o.screen == "sky" ||
                 o.screen == "origin" || o.screen == "depth";
        } else if (a == "--at" && has_value) {
            const auto g = parse_triple(args[++i]);
            ok = g && (*g)[0] >= -90.0 && (*g)[0] <= 90.0 && (*g)[1] >= -180.0 &&
                 (*g)[1] <= 180.0;
            if (ok) {
                o.at = glideslope::world::Geodetic{(*g)[0], (*g)[1], (*g)[2]};
            }
        } else if (a == "--weather" && has_value) {
            o.weather_station = std::string(args[++i]);
            ok = o.weather_station.size() == 4 &&
                 std::all_of(
                     o.weather_station.begin(), o.weather_station.end(), [](char c) {
                         return std::isalnum(static_cast<unsigned char>(c)) != 0;
                     });
        } else if (a == "--metar" && has_value) {
            o.metar = std::string(args[++i]);
            try {
                glideslope::world::parse_metar(o.metar);
            } catch (const glideslope::world::MetarError&) {
                ok = false;
            }
        } else if (a == "--station" && has_value) {
            const std::string text(args[++i]);
            char* end = nullptr;
            const double lat = std::strtod(text.c_str(), &end);
            ok = *end == ',';
            if (ok) {
                const char* rest = end + 1;
                const double lon = std::strtod(rest, &end);
                ok = end != rest && *end == '\0' && lat >= -90.0 && lat <= 90.0 &&
                     lon >= -180.0 && lon <= 180.0;
                o.station = std::array<double, 2>{lat, lon};
            }
        } else if (a == "--aircraft" && has_value) {
            o.aircraft = std::string(args[++i]);
        } else if (a == "--mismatch" && has_value) {
            o.mismatch = std::string(args[++i]);
        } else if (a == "--terrain" && has_value) {
            o.terrain_provider = std::string(args[++i]);
        } else if (a == "--view" && has_value) {
            o.view = std::string(args[++i]);
        } else if (a == "--checklist" && has_value) {
            o.checklist = std::string(args[++i]);
        } else if (a == "--draw-aircraft" && has_value) {
            const std::string_view value = args[++i];
            ok = value == "on" || value == "off";
            o.draw_aircraft = value == "on";
        } else if (a == "--ride-along") {
            o.ride_along = true;
        } else if (a == "--on-ground") {
            o.on_ground = true;
        } else if (a == "--autopilot") {
            o.autopilot = true;
        } else if (a == "--plan" && has_value) {
            o.plan = std::string(args[++i]);
        } else if (a == "--imagery" && has_value) {
            const std::string_view value = args[++i];
            ok = value == "on" || value == "off";
            o.imagery = value == "on";
        } else if (a == "--microburst" && has_value) {
            const std::string text(args[++i]);
            char* end = nullptr;
            glideslope::world::Microburst burst;
            burst.latitude_deg = std::strtod(text.c_str(), &end);
            ok = *end == ',';
            if (ok) {
                const char* rest = end + 1;
                burst.longitude_deg = std::strtod(rest, &end);
                ok = end != rest && *end == '\0' && burst.latitude_deg >= -90.0 &&
                     burst.latitude_deg <= 90.0 && burst.longitude_deg >= -180.0 &&
                     burst.longitude_deg <= 180.0;
            }
            o.microbursts.push_back(burst);
        } else if (a == "--toward" && has_value) {
            const auto g = parse_triple(args[++i]);
            ok = g && (*g)[0] >= -90.0 && (*g)[0] <= 90.0 && (*g)[1] >= -180.0 &&
                 (*g)[1] <= 180.0;
            if (ok) {
                o.toward = glideslope::world::Geodetic{(*g)[0], (*g)[1], (*g)[2]};
            }
        } else if (a == "--at-ecef" && has_value) {
            const auto e = parse_triple(args[++i]);
            ok = e.has_value();
            if (ok) {
                o.at_ecef = glideslope::world::Ecef{(*e)[0], (*e)[1], (*e)[2]};
            }
        } else {
            ok = false;
        }
        if (!ok) {
            usage(stderr);
            return 2;
        }
    }
    if (o.headless && o.shot.empty()) {
        std::fputs("glideslope: --headless needs --shot, or it has nothing to show\n",
                   stderr);
        return 2;
    }
    if (!o.microbursts.empty() && o.weather_station.empty()) {
        std::fputs("glideslope: a --microburst is put into --weather\n", stderr);
        return 2;
    }
    if (o.screen != "flight" && !o.weather_station.empty()) {
        std::fputs("glideslope: only the flight has --weather\n", stderr);
        return 2;
    }
    if (o.screen != "terrain" && !o.metar.empty()) {
        std::fputs("glideslope: only the terrain screen has --metar; the flight has "
                   "--weather\n",
                   stderr);
        return 2;
    }
    if (o.screen != "flight" && (o.autopilot || !o.plan.empty())) {
        std::fputs("glideslope: only the flight has --autopilot and --plan\n", stderr);
        return 2;
    }
    if (o.screen != "flight" && (o.aircraft != "c172p" || o.on_ground ||
                                 o.view != "cockpit")) {
        std::fputs("glideslope: only the flight has --aircraft, --on-ground and "
                   "--view\n",
                   stderr);
        return 2;
    }
    if (!glideslope::gfx::provider_named(o.terrain_provider)) {
        std::fprintf(stderr,
                     "glideslope: there is no terrain %s; there is %s\n",
                     o.terrain_provider.c_str(),
                     glideslope::gfx::provider_names().c_str());
        return 2;
    }
    if (!glideslope::gfx::view_named(o.view)) {
        std::fprintf(stderr, "glideslope: there is no view %s; there is %s\n",
                     o.view.c_str(), glideslope::gfx::view_names().c_str());
        return 2;
    }
    glideslope::sim::Phase checklist_phase{};
    if (!o.checklist.empty() && !glideslope::sim::phase_of(o.checklist, checklist_phase)) {
        std::string phases;
        for (const glideslope::sim::Phase phase : glideslope::sim::all_phases()) {
            phases += (phases.empty() ? "" : ", ") + glideslope::sim::phase_name(phase);
        }
        std::fprintf(stderr,
                     "glideslope: there is no phase of flight %s; there is %s\n",
                     o.checklist.c_str(), phases.c_str());
        return 2;
    }
    if (o.on_ground && (o.autopilot || !o.plan.empty())) {
        std::fputs("glideslope: the AI cannot take off; --on-ground is flown by the pilot\n",
                   stderr);
        return 2;
    }
    if (o.screen == "flight") {
        const auto catalogue =
            glideslope::sim::read_catalogue(glideslope::platform::data_directory());
        const bool known = std::any_of(catalogue.begin(), catalogue.end(),
                                       [&](const auto& e) { return e.id == o.aircraft; });
        if (!known) {
            std::string ids;
            for (const auto& e : catalogue) {
                ids += (ids.empty() ? "" : ", ") + e.id;
            }
            std::fprintf(stderr, "glideslope: the data holds no aircraft %s; it holds %s\n",
                         o.aircraft.c_str(), ids.c_str());
            return 2;
        }
    }
    if (o.station && o.metar.empty()) {
        std::fputs("glideslope: --station says where a --metar is observed\n", stderr);
        return 2;
    }
    if ((o.screen == "terrain") != (o.at && o.toward)) {
        std::fputs("glideslope: the terrain screen, and only it, looks from --at "
                   "--toward\n",
                   stderr);
        return 2;
    }
    if (o.screen == "flight" && o.at_ecef) {
        std::fputs(
            "glideslope: the flight starts --at a latitude, longitude and height\n",
            stderr);
        return 2;
    }

    // Headless is no window. Elsewhere that is SDL's offscreen video driver, which
    // needs no display; SDL's Metal backend will only start on a video driver
    // that can make a Metal view, which on macOS is Cocoa's, so there headless
    // keeps the native driver and simply opens no window.
#ifndef __APPLE__
    if (o.headless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    }
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        std::fprintf(stderr, "glideslope: SDL did not start: %s\n", SDL_GetError());
        return 1;
    }

    int status = 0;
    SDL_Window* window = nullptr;
    try {
        std::unique_ptr<glideslope::client::Flight> flight;
        glideslope::client::FlightStart start;
        glideslope::client::Scene scene;
        const auto started = std::chrono::steady_clock::now();
        const auto seconds_since_start = [&] {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
                .count();
        };
        // **Joining a server, if this client was told to.** The session is
        // the network's, not this program's: `glideslope_cli` uses the same
        // one. What the client does with it is still only to stay in it -
        // once joined, the flight on screen is the aircraft the server gave it,
        // predicted here (client/online.hpp), and every other one is drawn.
        std::optional<glideslope::net::ClientSession> session;
        std::optional<glideslope::client::Online> online;
        std::optional<glideslope::client::Joined> joined;
        if (o.online || !o.server.empty()) {
            std::string where = o.server;
            std::string key = o.server_key;
            if (o.online) {
                const auto named = glideslope::platform::default_server();
                if (!named) {
                    std::fprintf(stderr,
                                 "glideslope: --online needs a server.txt naming a "
                                 "host, a port and a key\n");
                    return 2;
                }
                where = named->host + ":" + std::to_string(named->port);
                key = named->key_hex;
                std::printf("server.txt: %s port %u\n", named->host.c_str(),
                            static_cast<unsigned>(named->port));
            }
            if (key.empty()) {
                std::fprintf(stderr, "glideslope: --server needs --server-key\n");
                return 2;
            }
            session = glideslope::net::ClientSession::connect(where, key);
            if (!session) {
                std::fprintf(stderr, "glideslope: cannot reach %s\n", where.c_str());
                return 1;
            }
            std::printf("session with %s\n", session->theirs().text().c_str());
            std::fflush(stdout);
            // **Given an aircraft, or not.** A server answers the handshake
            // only once its terrain is built and its aircraft are flying, and
            // from then on says where they are every twenty-fifth of a
            // simulated second. So a session with no word of an aircraft for
            // ten seconds is with a server that has nothing to fly, and this
            // flies alone and says so.
            online.emplace(std::move(*session));
            joined = online->join(10.0, [&] { return seconds_since_start(); });
            if (joined) {
                std::printf("glideslope: the server gave this client aircraft %u, the %s\n",
                            static_cast<unsigned>(joined->number),
                            joined->aircraft_id.c_str());
            } else {
                std::printf("glideslope: the server gave this client no aircraft; "
                            "flying alone\n");
            }
            std::fflush(stdout);
        }

        if (o.screen == "flight") {
            if (o.at) {
                start.latitude_deg = o.at->latitude_deg;
                start.longitude_deg = o.at->longitude_deg;
                start.height_m = o.at->height_m;
            }
            start.aircraft = o.aircraft;
            start.on_ground = o.on_ground;
            start.weather_station = o.weather_station;
            start.microbursts = o.microbursts;
            start.autopilot = o.autopilot;
            if (!o.plan.empty()) {
                // A file, or a plan in the data by its name.
                std::filesystem::path path(o.plan);
                if (!std::filesystem::exists(path)) {
                    path = glideslope::platform::data_directory() / "plans" / o.plan;
                    if (!std::filesystem::exists(path)) {
                        path += ".plan";
                    }
                }
                std::ifstream plan_file(path, std::ios::binary);
                if (!plan_file) {
                    throw std::runtime_error("no flight plan " + o.plan);
                }
                start.plan = glideslope::sim::parse_flight_plan(
                    std::string(std::istreambuf_iterator<char>(plan_file), {}));
                std::printf("glideslope: flying the plan %s, %zu waypoints\n",
                            path.string().c_str(), start.plan->waypoints.size());
            }
            // **On a server, the server's word wins**: what aeroplane and
            // where, over `--aircraft`, `--at` and `--on-ground`; and nothing
            // of the AI's, whose flying there is Phase 7's.
            if (joined) {
                const glideslope::world::Geodetic g = glideslope::world::to_geodetic(
                    {joined->motion.location_ecef_m[0], joined->motion.location_ecef_m[1],
                     joined->motion.location_ecef_m[2]});
                start.aircraft = joined->aircraft_id;
                start.latitude_deg = g.latitude_deg;
                start.longitude_deg = g.longitude_deg;
                start.height_m = g.height_m;
                start.on_ground = false;
                start.autopilot = false;
                start.plan.reset();
            }
            flight = std::make_unique<glideslope::client::Flight>(
                glideslope::platform::data_directory(),
                glideslope::platform::cache_directory(), start);
            if (joined) {
                flight->adopt(joined->motion);
            }
            if (!o.checklist.empty()) {
                flight->show_checklist(checklist_phase);
                if (!flight->showing_checklist()) {
                    std::fprintf(stderr,
                                 "glideslope: the %s ships no checklists\n",
                                 flight->aircraft().id.c_str());
                    return 2;
                }
                std::printf("glideslope: showing the %s checklist\n",
                            o.checklist.c_str());
            }
            std::printf("glideslope: flying the %s (%s)%s\n", flight->aircraft().name.c_str(),
                        flight->aircraft().id.c_str(),
                        !start.on_ground           ? ""
                        : flight->afloat_at_start() ? ", afloat"
                                                    : ", standing on the ground");
            if (!start.weather_station.empty()) {
                std::printf("glideslope: flying in the weather at %s: METAR from "
                            "aviationweather.gov; %s (https://open-meteo.com/), CC BY "
                            "4.0\n",
                            start.weather_station.c_str(),
                            glideslope::world::open_meteo_credit);
            }
        } else if (o.screen == "terrain") {
            scene.camera =
                glideslope::gfx::look_at(glideslope::world::to_ecef(*o.at),
                                         glideslope::world::to_ecef(*o.toward));
            scene.camera.near_m = 1.0;
        } else {
            const glideslope::world::Ecef at =
                o.at_ecef ? *o.at_ecef
                          : (o.at ? glideslope::world::to_ecef(*o.at)
                                  : glideslope::world::Ecef{});
            scene = glideslope::client::make_scene(o.screen, at);
        }

        if (!o.headless) {
            window =
                SDL_CreateWindow("glideslope", o.width, o.height, SDL_WINDOW_RESIZABLE);
            if (window == nullptr) {
                throw std::runtime_error(std::string("no window: ") + SDL_GetError());
            }
        }
        glideslope::gfx::Renderer renderer(o.driver, window, o.width, o.height);
        std::printf("glideslope: GPU driver %s\n", renderer.driver().c_str());

        // **How far the terrain drawn is from the terrain flown.** The ground
        // an aircraft meets is always the open DEM; a visual provider may put
        // its surface somewhere else, and this says where, at each place
        // named. Nothing is drawn and nothing is flown.
        if (!o.mismatch.empty()) {
            std::ifstream places_file(o.mismatch, std::ios::binary);
            if (!places_file) {
                std::fprintf(stderr, "glideslope: cannot read %s\n",
                             o.mismatch.c_str());
                return 2;
            }
            std::vector<std::string> names;
            std::vector<glideslope::world::Geodetic> places;
            std::string line;
            while (std::getline(places_file, line)) {
                if (line.empty() || line[0] == '#') {
                    continue;
                }
                std::istringstream words(line);
                std::string kind;
                std::string name;
                double latitude = 0.0;
                double longitude = 0.0;
                if (!(words >> kind >> name >> latitude >> longitude)) {
                    continue;
                }
                if (kind != "airfield") {
                    continue; // the item asks for airfields
                }
                names.push_back(name);
                places.push_back({latitude, longitude, 0.0});
            }
            if (places.empty()) {
                std::fputs("glideslope: that names no airfield\n", stderr);
                return 2;
            }
            // **One tileset for each whole-degree cell, not one for them
            // all.** The open provider builds its terrain over the region it
            // is given, and a region from Barrow to Boston is most of a
            // continent; the airfields sit in a handful of cells, so each
            // cell is opened, asked about its own, and closed.
            std::vector<std::optional<double>> drawn(places.size(), std::nullopt);
            std::map<std::pair<int, int>, std::vector<std::size_t>> by_cell;
            for (std::size_t i = 0; i < places.size(); ++i) {
                by_cell[{static_cast<int>(std::floor(places[i].latitude_deg)),
                         static_cast<int>(std::floor(places[i].longitude_deg))}]
                    .push_back(i);
            }
            for (const auto& [cell, which] : by_cell) {
                const glideslope::world::GeoRectangle region{
                    static_cast<double>(cell.first),
                    static_cast<double>(cell.second),
                    static_cast<double>(cell.first + 1),
                    static_cast<double>(cell.second + 1)};
                auto measured = glideslope::client::open_terrain_to_measure(
                    renderer, glideslope::platform::data_directory(),
                    glideslope::platform::cache_directory(), region,
                    *glideslope::gfx::provider_named(o.terrain_provider));
                std::vector<glideslope::world::Geodetic> here;
                for (const std::size_t i : which) {
                    here.push_back(places[i]);
                }
                const std::vector<std::optional<double>> got =
                    measured->heights_at(here);
                for (std::size_t j = 0; j < which.size() && j < got.size(); ++j) {
                    drawn[which[j]] = got[j];
                }
            }
            double worst = 0.0;
            std::size_t answered = 0;
            for (std::size_t i = 0; i < places.size(); ++i) {
                const glideslope::world::GroundHeight flown =
                    glideslope::client::ground_at(
                        glideslope::platform::data_directory(),
                        glideslope::platform::cache_directory(),
                        places[i].latitude_deg, places[i].longitude_deg);
                const double flown_m =
                    flown.above_sea_level_m + flown.geoid_m; // above the ellipsoid
                if (!drawn[i]) {
                    // No *drawn* surface here - the flown height is printed
                    // beside it to say what was being compared against.
                    std::printf("mismatch %s %.8f %.8f none-drawn flown %.3f\n",
                                names[i].c_str(), places[i].latitude_deg,
                                places[i].longitude_deg, flown_m);
                    continue;
                }
                const double off = *drawn[i] - flown_m;
                // **A height has to be a height.** The land runs from the
                // Dead Sea's shore to Everest, so a surface put tens of
                // kilometres below the ellipsoid is not terrain that
                // disagrees with the DEM - it is an answer that means
                // nothing, and folding it into a bound would make the bound
                // mean nothing too. It is printed and set aside.
                if (*drawn[i] < -500.0 || *drawn[i] > 9000.0) {
                    std::printf("mismatch %s %.8f %.8f drawn %.3f flown %.3f "
                                "off %+.3f not-a-height\n",
                                names[i].c_str(), places[i].latitude_deg,
                                places[i].longitude_deg, *drawn[i], flown_m, off);
                    continue;
                }
                ++answered;
                worst = std::max(worst, std::abs(off));
                std::printf("mismatch %s %.8f %.8f drawn %.3f flown %.3f off %+.3f\n",
                            names[i].c_str(), places[i].latitude_deg,
                            places[i].longitude_deg, *drawn[i], flown_m, off);
            }
            std::printf("glideslope: %s terrain answered with a height for %zu "
                        "of %zu airfields, worst %.3f m from the ground flown\n",
                        o.terrain_provider.c_str(), answered, places.size(), worst);
            return answered == places.size() ? 0 : 1;
        }
        std::vector<glideslope::gfx::Draw> draws = scene.draws;
        for (glideslope::gfx::Draw& draw : draws) {
            draw.mesh = renderer.add_mesh(scene.meshes.at(draw.mesh));
        }

        // The terrain: under the flight, the cells around where it starts; on
        // the terrain screen, the cell the eye is in. After the renderer, so it
        // is destroyed first: it frees its meshes as it goes.
        std::unique_ptr<glideslope::gfx::TerrainTiles> terrain;
        if (flight || o.screen == "terrain") {
            const glideslope::world::GeoRectangle region =
                flight ? glideslope::client::cells_around(start.latitude_deg,
                                                          start.longitude_deg, 1)
                       : glideslope::client::cells_around(o.at->latitude_deg,
                                                          o.at->longitude_deg, 0);
            terrain = glideslope::client::open_terrain(
                renderer, glideslope::platform::data_directory(),
                glideslope::platform::cache_directory(), region, o.imagery,
                *glideslope::gfx::provider_named(o.terrain_provider));
        }

        // The weather to be seen: the flight's report's, made again when a new
        // one comes; or the terrain screen's --metar, observed on the ground at
        // --station or beneath the eye. After the renderer, so it is destroyed
        // first.
        std::unique_ptr<glideslope::gfx::Sky> sky;
        // Which report the sky is of: its seed, which is its station's and
        // observation time's.
        std::optional<std::uint64_t> sky_of;
        if (!o.metar.empty()) {
            const glideslope::world::Metar metar =
                glideslope::world::parse_metar(o.metar);
            const double lat = o.station ? (*o.station)[0] : o.at->latitude_deg;
            const double lon = o.station ? (*o.station)[1] : o.at->longitude_deg;
            const glideslope::world::GroundHeight ground =
                glideslope::client::ground_at(glideslope::platform::data_directory(),
                                              glideslope::platform::cache_directory(),
                                              lat, lon);
            sky = std::make_unique<glideslope::gfx::Sky>(
                renderer, metar,
                glideslope::gfx::Station{lat, lon, ground.above_sea_level_m,
                                         ground.geoid_m},
                glideslope::world::air_seed_of(metar));
            std::printf("glideslope: the METAR observed %.3f m above sea level, the "
                        "geoid %.3f m above the ellipsoid\n",
                        ground.above_sea_level_m, ground.geoid_m);
        }

        const bool shooting = !o.shot.empty();
        // The view, and the aeroplane's own mesh. The mesh is lit on the way
        // in - the shader has no normals - so it is made again when the sun
        // has moved far enough around the body to see, which is what banking
        // does. In the cockpit there is nothing to draw: the models have no
        // interior, nothing is culled by its facing, and the skin would be
        // drawn over the windscreen.
        glideslope::gfx::View view =
            glideslope::gfx::view_named(o.view).value_or(glideslope::gfx::View::cockpit);
        glideslope::gfx::MeshId aircraft_mesh = 0;
        bool has_aircraft_mesh = false;
        glideslope::world::Ecef lit_by{};
        const auto light_moved = [&](const glideslope::world::Ecef& sun) {
            // Five degrees, as the cosine of the angle between them.
            return sun.x * lit_by.x + sun.y * lit_by.y + sun.z * lit_by.z < 0.9962;
        };
        // On a server: the other aircraft's models, by what they are, and a
        // mesh for each, lit for how it is pointing.
        // Each with where its reference point and its pilot's eye are, and
        // only for an aeroplane the catalogue knows.
        struct OtherModel {
            bool known = false;
            std::optional<glideslope::client::Visual> visual;
            glideslope::client::ModelGeometry geometry;
        };
        std::map<std::string, OtherModel> models;
        const auto model_of = [&](const std::string& id) -> const OtherModel& {
            auto found = models.find(id);
            if (found == models.end()) {
                OtherModel m;
                if (const auto entry = glideslope::sim::known_aircraft(
                        glideslope::platform::data_directory(), id)) {
                    m.known = true;
                    m.visual = glideslope::client::visual_of(glideslope::platform::data_directory(), id);
                    m.geometry = glideslope::client::geometry_of(glideslope::platform::data_directory(),
                                                                 *entry);
                }
                found = models.emplace(id, std::move(m)).first;
            }
            return found->second;
        };
        bool rode_along = false;
        struct OtherMesh {
            glideslope::gfx::MeshId id = 0;
            bool made = false;
            glideslope::world::Ecef lit_by{};
            std::string aircraft_id;
        };
        std::map<std::uint8_t, OtherMesh> other_meshes;
        std::vector<glideslope::client::Other> others_now;
        glideslope::sim::Controls controls;
        if (o.on_ground) {
            // Standing: idling, its wheels down and braked.
            controls.throttle = 0.0;
            controls.gear = 1.0;
            controls.left_brake = controls.right_brake = 1.0;
        } else {
            // The throttle the aircraft's catalogue entry holds its start with;
            // the flight begins in the air, its wheels up.
            controls.throttle = flight ? flight->aircraft().start_throttle : 0.65;
            controls.gear = 0.0;
        }
        const std::filesystem::path bindings_path =
            glideslope::platform::data_directory() / "input" / "bindings.txt";
        std::ifstream bindings_file(bindings_path, std::ios::binary);
        if (!bindings_file) {
            throw std::runtime_error("cannot read " + bindings_path.string());
        }
        glideslope::platform::ControlMapper mapper(glideslope::platform::parse_bindings(
            std::string(std::istreambuf_iterator<char>(bindings_file), {})));
        glideslope::platform::Joysticks joysticks;
        glideslope::platform::KeyboardControls keys;
        glideslope::sim::FixedStep clock;
        auto last = std::chrono::steady_clock::now();
        std::int64_t ticks = 0;
        long frames = 0;
        bool running = true;
        while (running) {
            if (online && !(joined && flight)) {
                // In a session without an aircraft: kept, and nothing more.
                online->idle(seconds_since_start());
            }
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == SDL_SCANCODE_A && flight) {
                    flight->swap_pilot();
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == SDL_SCANCODE_W && online && joined) {
                    // **Ride along**: the next aircraft in the sky, by number,
                    // and after the last, back in your own.
                    std::uint8_t next = glideslope::net::no_aircraft;
                    for (const glideslope::client::Other& other : others_now) {
                        if ((online->watching() == glideslope::net::no_aircraft ||
                             other.number > online->watching()) &&
                            (next == glideslope::net::no_aircraft || other.number < next)) {
                            next = other.number;
                        }
                    }
                    online->watch(next);
                    if (next == glideslope::net::no_aircraft) {
                        std::printf("glideslope: back in your own aircraft\n");
                    } else {
                        std::printf("glideslope: riding along in aircraft %u\n",
                                    static_cast<unsigned>(next));
                    }
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == SDL_SCANCODE_V && flight) {
                    // Round the views, and round again. Nothing about the
                    // flight moves: the camera is worked out afresh each
                    // frame from the aircraft's state.
                    const auto& all = glideslope::gfx::every_view();
                    const auto at = std::find(all.begin(), all.end(), view);
                    view = at == all.end() || at + 1 == all.end() ? all.front()
                                                                  : *(at + 1);
                    std::printf("glideslope: the view is %s\n",
                                std::string(glideslope::gfx::name_of(view)).c_str());
                }
            }
            std::int64_t due = 0;
            if (shooting && !joined) {
                // Two ticks a frame, or as many as keep the flight to 300.
                // (On a server a flight keeps real time, shot or not: the
                // server's aircraft does.)
                due = std::min<std::int64_t>(std::max<std::int64_t>(2, o.shot_at / 300),
                                             o.shot_at - ticks);
            } else {
                const auto now = std::chrono::steady_clock::now();
                due = std::min<std::int64_t>(clock.advance(now - last), 24);
                last = now;
                int key_count = 0;
                const bool* key_state = SDL_GetKeyboardState(&key_count);
                keys.apply(controls,
                           static_cast<double>(due) /
                               static_cast<double>(glideslope::sim::steps_per_second),
                           key_state, key_count);
            }
            mapper.apply(joysticks.read(), controls);
            // On a server, what is flown is what was last sent.
            const glideslope::sim::Controls flown =
                joined && flight ? online->fly(seconds_since_start(), controls, *flight)
                                 : controls;
            for (std::int64_t i = 0; i < due; ++i) {
                if (flight) {
                    flight->step(flown);
                    if (o.trace) {
                        std::printf("%s\n", flight->trace().c_str());
                    }
                }
                ++ticks;
            }

            // The frame shot waits for every terrain tile its view needs, so
            // the same command draws the same terrain everywhere.
            bool shot_now = shooting && ticks >= o.shot_at;
            // **On a server, everybody else as they are now**, and the one
            // being ridden along in, if any.
            if (online && joined) {
                others_now = online->others(seconds_since_start());
                if (o.ride_along && !rode_along) {
                    for (const glideslope::client::Other& other : others_now) {
                        if (other.ai_flying) {
                            online->watch(other.number);
                            rode_along = true;
                            std::printf("glideslope: riding along in aircraft %u\n",
                                        static_cast<unsigned>(other.number));
                            break;
                        }
                    }
                }
                // **Riding along, the shot waits for what it is of**: an
                // aircraft ridden in, and its controls, which come some
                // updates after the server is told - later than the shot's
                // tick on a slow machine, which then drew a HUD without them.
                if (shot_now && o.ride_along &&
                    (!rode_along || (online->watching() != glideslope::net::no_aircraft &&
                                     !online->watched_controls(seconds_since_start())))) {
                    shot_now = false;
                }
            }
            // **On a server, a shot draws only its own frame.** The flight
            // keeps real time there, so the frames before it are as many as
            // the machine can draw - thousands - where a shot flown by ticks
            // has three hundred at most, and drawing them all headless ran
            // the software Vulkan driver out of memory (a tail). Nobody sees
            // them; the flight and the session go on all the same.
            if (shooting && joined && !shot_now) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            // The orbit goes round once a minute of the flight's own time, so
            // the same command puts it in the same place every run.
            constexpr double orbit_rad_per_s = 6.283185307179586 / 60.0;
            const glideslope::client::Other* ridden = nullptr;
            if (online && joined && online->watching() != glideslope::net::no_aircraft) {
                for (const glideslope::client::Other& other : others_now) {
                    if (other.number == online->watching() && !other.aircraft_id.empty() &&
                        model_of(other.aircraft_id).known) {
                        ridden = &other;
                    }
                }
            }
            glideslope::gfx::Placement ridden_placement;
            glideslope::gfx::Camera camera =
                flight ? flight->camera(view, flight->time_s() * orbit_rad_per_s)
                       : scene.camera;
            if (ridden) {
                // **Its seat**: the view from its pilot's eye, or around it,
                // exactly as its own client would have it.
                const OtherModel& m = model_of(ridden->aircraft_id);
                const glideslope::gfx::ModelAlignment alignment =
                    m.visual ? m.visual->alignment : glideslope::gfx::ModelAlignment{};
                ridden_placement = glideslope::client::placement_of(
                    ridden->centre, ridden->heading_deg, ridden->pitch_deg, ridden->roll_deg,
                    alignment, m.geometry);
                const std::array<double, 3> eye{m.geometry.eye_from_reference[0] - alignment.offset[0],
                                                m.geometry.eye_from_reference[1] - alignment.offset[1],
                                                m.geometry.eye_from_reference[2] - alignment.offset[2]};
                camera = glideslope::gfx::camera_for(
                    view, ridden_placement,
                    m.visual ? glideslope::gfx::model_radius(m.visual->model) : 10.0, eye,
                    flight->time_s() * orbit_rad_per_s);
            }
            if (terrain) {
                draws = terrain->update(camera, o.width, o.height, shot_now);
            }
            if (flight && flight->weather_report() != nullptr &&
                flight->weather_report()->air_seed != sky_of) {
                const glideslope::world::WeatherReport& report =
                    *flight->weather_report();
                sky.reset();
                sky = std::make_unique<glideslope::gfx::Sky>(
                    renderer, report.surface.metar, flight->weather_station(),
                    report.air_seed);
                sky_of = report.air_seed;
            }
            std::vector<glideslope::gfx::Draw> drawn = draws;
            glideslope::gfx::Haze haze;
            glideslope::gfx::Colour background = glideslope::gfx::sky;
            if (sky) {
                sky->draw(
                    camera,
                    flight ? flight->time_s()
                           : static_cast<double>(ticks) /
                                 static_cast<double>(glideslope::sim::steps_per_second),
                    drawn);
                haze = sky->haze(camera);
                background = sky->background(camera);
            }
            // The aeroplane itself, in every view but the cockpit.
            if (flight && flight->model() != nullptr && o.draw_aircraft &&
                (view != glideslope::gfx::View::cockpit || ridden != nullptr)) {
                const glideslope::world::Ecef sun = flight->sun_in_body();
                if (!has_aircraft_mesh || light_moved(sun)) {
                    if (has_aircraft_mesh) {
                        renderer.remove_mesh(aircraft_mesh);
                    }
                    aircraft_mesh = renderer.add_mesh(
                        glideslope::gfx::mesh_from_model(*flight->model(), sun));
                    has_aircraft_mesh = true;
                    lit_by = sun;
                }
                glideslope::gfx::Draw draw;
                draw.mesh = aircraft_mesh;
                draw.placement = flight->model_placement();
                drawn.push_back(draw);
            }
            // **Everybody else, on a server**: each with its own model, lit
            // for how it is pointing, and drawn where the updates put it
            // 100 ms ago (client/online.hpp).
            if (online && joined && o.draw_aircraft) {
                // An aircraft no longer in the sky takes its mesh with it.
                for (auto it = other_meshes.begin(); it != other_meshes.end();) {
                    const bool still = std::any_of(
                        others_now.begin(), others_now.end(),
                        [&](const glideslope::client::Other& o2) { return o2.number == it->first; });
                    if (!still && it->second.made) {
                        renderer.remove_mesh(it->second.id);
                    }
                    it = still ? std::next(it) : other_meshes.erase(it);
                }
                for (const glideslope::client::Other& other : others_now) {
                    if (other.aircraft_id.empty()) {
                        continue; // not yet introduced
                    }
                    const OtherModel& model = model_of(other.aircraft_id);
                    if (!model.visual) {
                        continue; // not in the catalogue, or it ships no visual model
                    }
                    // Its own seat is not drawn over its rider's view.
                    if (ridden != nullptr && other.number == ridden->number &&
                        view == glideslope::gfx::View::cockpit) {
                        continue;
                    }
                    const glideslope::gfx::Placement placement = glideslope::client::placement_of(
                        other.centre, other.heading_deg, other.pitch_deg, other.roll_deg,
                        model.visual->alignment, model.geometry);
                    const glideslope::world::Ecef sun =
                        glideslope::client::sun_in_body_of(placement);
                    OtherMesh& mesh = other_meshes[other.number];
                    const bool moved = mesh.made && sun.x * mesh.lit_by.x + sun.y * mesh.lit_by.y +
                                                            sun.z * mesh.lit_by.z <
                                                        0.9962;
                    if (!mesh.made || moved || mesh.aircraft_id != other.aircraft_id) {
                        if (mesh.made) {
                            renderer.remove_mesh(mesh.id);
                        }
                        mesh.id = renderer.add_mesh(
                            glideslope::gfx::mesh_from_model(model.visual->model, sun));
                        mesh.made = true;
                        mesh.lit_by = sun;
                        mesh.aircraft_id = other.aircraft_id;
                    }
                    glideslope::gfx::Draw draw;
                    draw.mesh = mesh.id;
                    draw.placement = placement;
                    drawn.push_back(draw);
                }
            }
            // Whichever provider is drawing, its attribution is on screen.
            // The open data's notices are its own; a streamed provider's come
            // from Cesium Native as its tiles load, which is how ion's and
            // Google's terms are met.
            std::vector<std::string> credits;
            if (terrain) {
                if (terrain->provider() == glideslope::gfx::Provider::open) {
                    credits.emplace_back(glideslope::world::copernicus_dem_notice);
                    if (o.imagery) {
                        credits.emplace_back(glideslope::gfx::open_imagery().credit);
                    }
                }
                for (std::string& credit : terrain->credits()) {
                    if (std::find(credits.begin(), credits.end(), credit) ==
                        credits.end()) {
                        credits.push_back(std::move(credit));
                    }
                }
            }
            if (flight) {
                glideslope::gfx::HudReadings readings = flight->hud();
                if (ridden) {
                    // **What the aircraft ridden in is doing**, as the updates
                    // say: its ground speed - airspeed is not sent - its
                    // height above the sea, where it points, who is flying it
                    // and where its controls are.
                    const glideslope::world::Geodetic g = glideslope::world::to_geodetic(ridden->centre);
                    glideslope::gfx::HudReadings r;
                    r.ground_speed = true;
                    r.airspeed_kts = std::hypot(ridden->north_mps, ridden->east_mps) / 0.514444;
                    r.altitude_ft = (g.height_m - flight->geoid_m(g.latitude_deg, g.longitude_deg)) *
                                    3.280839895013123;
                    r.heading_deg = ridden->heading_deg;
                    r.vertical_speed_fpm = -ridden->down_mps * 196.85039370078738;
                    r.pitch_deg = ridden->pitch_deg;
                    r.roll_deg = ridden->roll_deg;
                    r.ai_flying = ridden->ai_flying;
                    if (const auto c = online->watched_controls(seconds_since_start())) {
                        glideslope::gfx::ControlsShown shown;
                        shown.aileron = c->aileron;
                        shown.elevator = c->elevator;
                        shown.rudder = c->rudder;
                        shown.throttle = c->throttle;
                        shown.flaps = c->flaps;
                        shown.gear = c->gear;
                        r.controls = shown;
                    }
                    readings = r;
                    if (shot_now) {
                        std::printf("glideslope: riding along in aircraft %u, the %s; the camera "
                                    "%.1f m from its centre\n",
                                    static_cast<unsigned>(ridden->number),
                                    ridden->aircraft_id.c_str(),
                                    std::hypot(camera.position.x - ridden->centre.x,
                                               camera.position.y - ridden->centre.y,
                                               camera.position.z - ridden->centre.z));
                        for (const std::string& line : glideslope::gfx::hud_lines(readings)) {
                            std::printf("glideslope: the HUD reads %s\n", line.c_str());
                        }
                    }
                }
                readings.credits.insert(readings.credits.begin(), credits.begin(),
                                        credits.end());
                const glideslope::gfx::Mesh hud =
                    glideslope::gfx::hud_mesh(readings, o.width, o.height);
                renderer.render(camera, drawn, &hud, haze, background);
            } else if (!credits.empty()) {
                const glideslope::gfx::Mesh overlay =
                    glideslope::gfx::credits_mesh(credits, o.width, o.height);
                renderer.render(camera, drawn, &overlay, haze, background);
            } else {
                renderer.render(camera, drawn, nullptr, haze, background);
            }
            ++frames;

            if (shot_now && online && joined && flight) {
                // **What it drew of the server's sky, and how its own was
                // flown**: for a test to read, and for anybody to believe.
                const glideslope::world::Ecef me = flight->model_placement().origin;
                for (const glideslope::client::Other& other : others_now) {
                    const double away = std::hypot(other.centre.x - me.x, other.centre.y - me.y,
                                                   other.centre.z - me.z);
                    std::printf("glideslope: drew aircraft %u, the %s, %.0f m away%s\n",
                                static_cast<unsigned>(other.number),
                                other.aircraft_id.empty() ? "(not yet said)"
                                                          : other.aircraft_id.c_str(),
                                away, other.wrecked ? ", a wreck" : "");
                }
                std::printf("glideslope: predicted: %zu corrections, the worst %.3f m, "
                            "%zu too large to hide\n",
                            online->corrections(), online->worst_correction_m(),
                            online->snapped());
            }
            if (shot_now) {
                if (terrain) {
                    const auto counts = terrain->counts();
                    std::printf("glideslope: terrain of %zu tiles, %zu loaded, the "
                                "deepest at level %zu\n",
                                counts.drawn, counts.loaded, counts.deepest);
                    if (counts.without_imagery > 0) {
                        throw std::runtime_error(
                            std::to_string(counts.without_imagery) +
                            " terrain tiles were drawn without "
                            "their imagery");
                    }
                    if (counts.failed > 0 || counts.skipped > 0) {
                        throw std::runtime_error(std::to_string(counts.failed) +
                                                 " terrain tiles failed and " +
                                                 std::to_string(counts.skipped) +
                                                 " primitives could not be drawn");
                    }
                }
                // Where this frame was seen from, so that a test can project
                // the aeroplane's model from the same camera and see whether
                // it was drawn where the camera puts it.
                const auto& axes = camera.world_from_camera.m;
                std::printf("glideslope: camera %.6f %.6f %.6f", camera.position.x,
                            camera.position.y, camera.position.z);
                for (const double v : axes) {
                    std::printf(" %.9f", v);
                }
                std::printf(" %.9f %d %d\n", camera.vertical_fov_rad, o.width,
                            o.height);
                if (flight && flight->model() != nullptr) {
                    const glideslope::gfx::Placement p = flight->model_placement();
                    std::printf("glideslope: aeroplane %.6f %.6f %.6f", p.origin.x,
                                p.origin.y, p.origin.z);
                    for (const double v : p.world_from_local.m) {
                        std::printf(" %.9f", v);
                    }
                    std::printf("\n");
                }
                // What the checklist block says, so a test can hold what is
                // on the frame to what the flight believes it drew.
                if (flight && flight->showing_checklist()) {
                    glideslope::gfx::HudReadings r = flight->hud();
                    for (const std::string& line :
                         glideslope::gfx::checklist_lines(r.checklist, o.width)) {
                        std::printf("checklist: %s\n", line.c_str());
                    }
                }
                glideslope::gfx::save_bmp(renderer.capture(), o.shot);
                std::printf("glideslope: wrote tick %lld, frame %ld, to %s\n",
                            static_cast<long long>(ticks), frames, o.shot.c_str());
                if (window != nullptr) {
                    std::printf("glideslope: presented %ld of %ld frames to the "
                                "window\n",
                                renderer.presented(), frames);
                }
                running = false;
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glideslope: %s\n", e.what());
        status = 1;
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return status;
}

int main(int argc, char** argv) {
    // The process ends with its C runtime whole until every other thread has
    // stopped - Windows' own threads too (platform/end_process.hpp).
    glideslope::platform::end_process(run_program(argc, argv));
}
