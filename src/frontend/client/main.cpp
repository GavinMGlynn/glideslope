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
//              [--shot FILE] [--shot-at TICK | --shot-frame N] [--trace]
//              [--memory-every N]
//
// Test flags. --shot writes the frame drawn at simulation tick --shot-at
// (default 2) as a BMP and exits; while shooting, every frame advances exactly
// two ticks - a sixtieth of a second - whatever the clock says, so the same
// command draws the same frame on every machine. --shot-frame shoots frame N,
// and keeps every frame two ticks however many that is: --shot-at would make
// its frames longer to keep a long flight to 300. --memory-every prints the
// memory the process holds every N frames. --trace prints the flight's state
// after every tick.

#include "flight.hpp"
#include "frontend/players_copilot.hpp"
#include "online.hpp"
#include "pass.hpp"
#include "frontend/shown.hpp"
#include "platform/end_process.hpp"
#include "platform/stop.hpp"
#include "platform/closed_pipes.hpp"
#include "platform/no_crash_dialogs.hpp"
#include "gfx/hud.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sky.hpp"
#include "gfx/terrain_tiles.hpp"
#include "platform/input.hpp"
#include "platform/memory.hpp"
#include "net/session.hpp"
#include "platform/paths.hpp"
#include "scenes.hpp"
#include "sim/catalogue.hpp"
#include "sim/figures.hpp"
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
#include <thread>
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
#include <functional>
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
    // Test flag: quit at this tick of a --shot flight, before its shot, at
    // once - no frame drawn, no tile waited for - as a person quitting does.
    std::int64_t quit_at = -1;
    // Test flags: the shot of a frame counted rather than a tick - every
    // frame two ticks, however many there are - and the memory the process
    // holds, printed every so many frames. Zero is neither.
    std::int64_t shot_frame = 0;
    std::int64_t memory_every = 0;
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
    // Given on the command line, rather than the default: on a server it is
    // the aeroplane asked for (net::write_asked_aircraft).
    bool aircraft_given = false;
    std::string view = "cockpit";
    // The phase whose checklist is on screen; empty shows none.
    std::string checklist;
    // The controls' help on screen from the start, as F1 shows it.
    bool show_help = false;
    // A test flag, as --shot and --trace are: the same frame shot with the
    // aeroplane and without it differ in exactly its pixels, which is how a
    // test finds the outline it draws.
    bool draw_aircraft = true;
    // On a server, ride along in the first AI aircraft from the start.
    bool ride_along = false;
    // For tests: stand still this long after joining a server, as a slow
    // machine building its flight does, before building it.
    double slow_start_s = 0.0;
    // For tests: on a server, connect only once this file exists - the
    // server's --ready-file, written once its aircraft are flying.
    std::string after_ready;
    // For a test: written when the program has gone, however it went.
    std::string done;
    // For tests: every pass of the frame loop held this many milliseconds
    // longer, as a slow machine's frames are - CI's sanitized software
    // Vulkan draws one in 250 ms and more.
    double slow_frames_ms = 0.0;
    // Test flag: how often --weather is fetched again, in seconds of flight.
    double weather_refresh_s = glideslope::client::weather_refresh_seconds;
    bool weather_refresh_given = false;
    // And take it over this many seconds of flight in; below nought, never.
    double take_over_after_s = -1.0;
    // For a test: the last update from before a take-over heard again just
    // after it, as the network reorders them.
    bool late_update_after_take_over = false;
    // For a test: A pressed in the frame the take-over is asked, while the
    // server has yet to answer it.
    bool press_a_with_take_over = false;
    // For tests: --take-over-after asks for its own aircraft instead; and
    // first sends this many WATCH requests, past the server's rate for them.
    bool take_over_own = false;
    int watches_before_take_over = 0;
    // On a server, hand its own aircraft to the AI this many seconds of
    // flight in, and take it back this many in, as A does; below nought, never.
    double hand_over_after_s = -1.0;
    double learnt_landing_after_s = -1.0;
    // **For a test: keys pressed**, each S seconds after joining, as a
    // player's finger would - the key's event, and the key held for one
    // pass of the frame loop (`--press-after S KEY`, KEY a letter).
    std::vector<std::pair<double, SDL_Scancode>> presses;
    // **Its copilot** (`--copilot TASK`), asked with the player's own key on
    // this machine when C is pressed; only its route goes to the server.
    std::string copilot_task;
    std::string copilot_provider = "openai";
    std::string copilot_model;
    std::string copilot_playback; // for tests: a recording, asked of nobody
    double copilot_after_s = -1.0; // for tests: as C does, S seconds after joining
    // **The model that plans its aircraft when A hands it to the AI**
    // (`--hand-over-model`): none by default; with one, the copilot is made
    // with it and asked as the aircraft is handed over.
    glideslope::frontend::HandOverModel hand_over_model;
    bool copilot_provider_given = false;
    double take_back_after_s = -1.0;
    // On a server, stall this many seconds of flight in - send nothing and
    // answer nothing, as a stopped process - until the server lets it go.
    double stall_after_s = -1.0;
    // On a server, hold the shot until it has gone back to its old session
    // after a refusal it believed, and been flown in it by an input sent since.
    bool shot_once_back = false;
    // For a test: hold the shot, up to five minutes of the flight past its
    // tick, until the learnt landing has its own aircraft at rest.
    bool shot_once_landed = false;
    // On a server, hold the shot until it has joined again - let go, or its
    // server restarted under it - and been flown by an input sent since.
    bool shot_once_joined_again = false;
    // On a server, ride along in the next aircraft at each of these many
    // seconds of flight in, as W does.
    std::vector<double> next_aircraft_after_s;
    // On a server, choose the next hand-over model at each of these many
    // seconds of flight in, as M does.
    std::vector<double> next_model_after_s;
    bool on_ground = false;
    // The attitude the flight starts at in the air: pitch, bank and heading,
    // degrees (for tests).
    std::optional<std::array<double, 3>> attitude;
};

void usage(std::FILE* out) {
    std::fputs(
        "usage: glideslope [--headless] [--gpu-driver vulkan|direct3d12|metal]\n"
        "                  [--size WxH] [--screen flight|terrain|sky|origin|depth]\n"
        "                  [--at LAT,LON,HEIGHT | --at-ecef X,Y,Z]\n"
        "                  [--toward LAT,LON,HEIGHT] [--imagery on|off]\n"
        "                  [--terrain open|ion|google] [--mismatch FILE]\n"
        "                  [--checklist PHASE] [--show-help]\n"
        "                  [--weather STATION [--microburst LAT,LON]...]\n"
        "                  [--metar REPORT [--station LAT,LON]]\n"
        "                  [--aircraft ID] [--on-ground] [--autopilot] [--plan PLAN]\n"
        "                  [--attitude PITCH,BANK,HEADING]\n"
        "                  [--view cockpit|ahead|behind|left|right|above|orbit]\n"
        "                  [--draw-aircraft on|off]\n"
        "                  [--shot FILE] [--shot-at TICK | --shot-frame N] [--trace]\n"
        "                  [--memory-every N]\n"
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
        "  --show-help   start with every control's key and button on screen, as\n"
        "                F1 shows and hides them in flight\n"
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
        "  --attitude    start the flight in the air at this pitch, bank and\n"
        "                heading, in degrees, rather than level (for tests)\n"
        "  --view        where it is seen from: the cockpit, by default, or outside\n"
        "                it from ahead, behind, left, right or above, or an orbit\n"
        "                around it; V steps through them\n"
        "  --ride-along  on a server, ride along in the first AI aircraft: its\n"
        "                cockpit, its instruments and its controls; W steps through\n"
        "                every aircraft in the sky and back to your own; T takes\n"
        "                over the one ridden in, if the AI flies it\n"
        "  --take-over-after S  riding along, take it over S seconds after\n"
        "                joining\n"
        "  --late-update-after-take-over  for a test: hear the last update from\n"
        "                before a take-over again just after it, as a network\n"
        "                reordering them would\n"
        "  --press-a-with-take-over  for a test: press A in the frame\n"
        "                --take-over-after asks, before the server has answered\n"
        "  --take-over-own  for a test: --take-over-after asks for its own aircraft\n"
        "  --watches-before-take-over N  for a test: N rides along asked first\n"
        "  --copilot TASK  on a server, C asks a language model - with your own\n"
        "                key, on this machine - to fly TASK, and then again each\n"
        "                minute; only its route goes to the server, which checks\n"
        "                it and flies it with its AI. --copilot-provider openai|\n"
        "                anthropic, --copilot-model M\n"
        "  --hand-over-model P  on a server, what plans your aircraft when A\n"
        "                hands it to the AI: anthropic or openai (each with :MODEL\n"
        "                if wanted), asked with your own key on this machine for a\n"
        "                route from where it is, or none (the default), and the AI\n"
        "                holds its course. It is the copilot C asks, too\n"
        "  --hand-over-after S, --take-back-after S  on a server, hand your own\n"
        "                aircraft to the AI S seconds after joining, and take it\n"
        "                back, as A does (for tests)\n"
        "  --learnt-landing-after S  on a server, ask for your own aircraft to be\n"
        "                handed to the learnt landing S seconds after joining, as L\n"
        "                does (for tests)\n"
        "  --press-after S KEY  on a server, press the letter KEY S seconds after\n"
        "                joining - its key's event, and the key held for one pass\n"
        "                of the frame loop; may be given again (for tests)\n"
        "  --stall-after S  on a server, S seconds after joining, send and answer\n"
        "                nothing, as a stopped process, until the server has let\n"
        "                this client go; it then joins again by itself (for tests)\n"
        "  --shot-once-landed  on a server, hold the shot, up to five minutes of\n"
        "                the flight past its tick, until the learnt landing has this\n"
        "                client's aircraft at rest (for tests)\n"
        "  --shot-once-back  on a server, hold the shot, up to a minute of the\n"
        "                flight past its tick, until this client has gone back to\n"
        "                its old session after a refusal it believed, and the\n"
        "                server has flown it by an input sent since (for tests)\n"
        "  --shot-once-joined-again  on a server, hold the shot, up to a minute of\n"
        "                the flight past its tick, until this client has joined\n"
        "                again and the server has flown its new aircraft by an\n"
        "                input sent since (for tests)\n"
        "  --next-aircraft-after S  on a server, ride along in the next aircraft\n"
        "                S seconds after joining, as W does; may be given again\n"
        "  --next-model-after S  on a server, choose the next model to plan a\n"
        "                hand-over S seconds after joining, as M does; may be\n"
        "                given again (for tests)\n"
        "  --quit-at T   end a --shot flight at tick T, before its shot, drawing\n"
        "                nothing more and waiting for nothing (for tests)\n"
        "  --weather-refresh S  fetch --weather again every S seconds of the flight\n"
        "                (at least 1; 900 unless given) (for tests)\n"
        "  --after-ready FILE  on a server, connect only once FILE exists - the\n"
        "                server's --ready-file - waiting up to five minutes (for\n"
        "                tests)\n"
        "  --done FILE   write FILE as the program ends, however it ends - for a\n"
        "                test's other clients to wait on (for tests)\n"
        "  --slow-start S  on a server, stand still S seconds after joining, as a\n"
        "                slow machine building its flight does (for tests)\n"
        "  --slow-frames MS  hold every pass of the frame loop MS milliseconds\n"
        "                longer, as a slow machine's frames are (for tests)\n"
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
        "  --shot-frame  write frame N instead, each frame two ticks however many\n"
        "                frames that is (for tests; not with --shot-at, nor on a\n"
        "                server)\n"
        "  --memory-every  print the memory the process holds every N frames (for\n"
        "                tests)\n"
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

// What a run told to stop says, and the status it ends with: 128 and the
// signal's number (platform/stop.hpp).
static int stopped() {
    std::fputs("glideslope: stopped, as it was told to\n", stderr);
    return glideslope::platform::stop_status();
}

static int run_program(int argc, char** argv) {
    // First: a failed assert prints and ends the program rather than
    // waiting on a dialog nobody will answer (platform/no_crash_dialogs.hpp).
    glideslope::platform::no_crash_dialogs();
    // And a write to a pipe whose reader has gone fails, rather than ending
    // the program (platform/closed_pipes.hpp).
    glideslope::platform::outlive_closed_pipes();
    // Before anything that can log: Cesium Native's log belongs on standard
    // error, not in the middle of a --trace line. See gfx/terrain_tiles.hpp.
    glideslope::gfx::log_to_standard_error();
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    Options o;
    bool shot_at_given = false;
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
            ok = tick && *tick >= 0 && o.shot_frame == 0;
            o.shot_at = tick.value_or(0);
            shot_at_given = true;
        } else if (a == "--shot-frame" && has_value) {
            const auto frame = parse_integer(args[++i]);
            ok = frame && *frame >= 1 && !shot_at_given;
            o.shot_frame = frame.value_or(0);
            // Two ticks a frame, so the frame counted is shot at this tick.
            o.shot_at = 2 * o.shot_frame;
        } else if (a == "--memory-every" && has_value) {
            const auto every = parse_integer(args[++i]);
            ok = every && *every >= 1;
            o.memory_every = every.value_or(0);
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
            o.aircraft_given = true;
        } else if (a == "--mismatch" && has_value) {
            o.mismatch = std::string(args[++i]);
        } else if (a == "--terrain" && has_value) {
            o.terrain_provider = std::string(args[++i]);
        } else if (a == "--view" && has_value) {
            o.view = std::string(args[++i]);
        } else if (a == "--show-help") {
            o.show_help = true;
        } else if (a == "--checklist" && has_value) {
            o.checklist = std::string(args[++i]);
        } else if (a == "--draw-aircraft" && has_value) {
            const std::string_view value = args[++i];
            ok = value == "on" || value == "off";
            o.draw_aircraft = value == "on";
        } else if (a == "--quit-at" && has_value) {
            const auto tick = parse_integer(args[++i]);
            ok = tick && *tick >= 1;
            o.quit_at = tick.value_or(-1);
        } else if (a == "--weather-refresh" && has_value) {
            const std::string text(args[++i]);
            char* end = nullptr;
            o.weather_refresh_s = std::strtod(text.c_str(), &end);
            o.weather_refresh_given = true;
            ok = end != text.c_str() && *end == '\0' && o.weather_refresh_s >= 1.0;
        } else if (a == "--done" && has_value) {
            o.done = std::string(args[++i]);
        } else if (a == "--after-ready" && has_value) {
            o.after_ready = std::string(args[++i]);
        } else if (a == "--slow-start" && has_value) {
            o.slow_start_s = std::strtod(std::string(args[++i]).c_str(), nullptr);
        } else if (a == "--slow-frames" && has_value) {
            // A number of milliseconds above nought, or refused: a word
            // read as nought held nothing, and the test it was for passed
            // without testing.
            const std::string text(args[++i]);
            char* end = nullptr;
            o.slow_frames_ms = std::strtod(text.c_str(), &end);
            ok = end != text.c_str() && *end == '\0' && o.slow_frames_ms > 0.0 &&
                 o.slow_frames_ms <= 60000.0;
            if (!ok) {
                std::fprintf(stderr,
                             "glideslope: --slow-frames wants milliseconds, above nought "
                             "and at most 60000, not '%s'\n",
                             text.c_str());
            }
        } else if (a == "--ride-along") {
            o.ride_along = true;
        } else if (a == "--late-update-after-take-over") {
            o.late_update_after_take_over = true;
        } else if (a == "--press-a-with-take-over") {
            o.press_a_with_take_over = true;
        } else if (a == "--take-over-own") {
            o.take_over_own = true;
        } else if (a == "--watches-before-take-over" && has_value) {
            o.watches_before_take_over =
                std::clamp(std::atoi(std::string(args[++i]).c_str()), 0, 100);
        } else if (a == "--take-over-after" && has_value) {
            o.take_over_after_s = std::strtod(std::string(args[++i]).c_str(), nullptr);
        } else if (a == "--copilot" && has_value) {
            o.copilot_task = std::string(args[++i]);
        } else if (a == "--copilot-provider" && has_value) {
            o.copilot_provider = std::string(args[++i]);
            o.copilot_provider_given = true;
        } else if (a == "--copilot-model" && has_value) {
            o.copilot_model = std::string(args[++i]);
            o.copilot_provider_given = true;
        } else if (a == "--hand-over-model" && has_value) {
            try {
                o.hand_over_model = glideslope::frontend::read_hand_over_model(args[++i]);
            } catch (const std::invalid_argument& e) {
                std::fprintf(stderr, "glideslope: %s\n", e.what());
                ok = false;
            }
        } else if (a == "--copilot-playback" && has_value) {
            o.copilot_playback = std::string(args[++i]);
        } else if (a == "--copilot-after" && has_value) {
            o.copilot_after_s = std::strtod(std::string(args[++i]).c_str(), nullptr);
        } else if (a == "--learnt-landing-after" && has_value) {
            o.learnt_landing_after_s = std::strtod(std::string(args[++i]).c_str(), nullptr);
        } else if (a == "--press-after" && i + 2 < args.size()) {
            const double at_s = std::strtod(std::string(args[++i]).c_str(), nullptr);
            const std::string key(args[++i]);
            if (key.size() != 1 || key[0] < 'A' || key[0] > 'Z') {
                std::fprintf(stderr, "glideslope: --press-after S KEY: KEY is a letter, A to Z\n");
                return 2;
            }
            o.presses.emplace_back(at_s, static_cast<SDL_Scancode>(SDL_SCANCODE_A + (key[0] - 'A')));
        } else if (a == "--hand-over-after" && has_value) {
            o.hand_over_after_s = std::strtod(std::string(args[++i]).c_str(), nullptr);
        } else if (a == "--stall-after" && has_value) {
            o.stall_after_s = std::strtod(std::string(args[++i]).c_str(), nullptr);
        } else if (a == "--shot-once-joined-again") {
            o.shot_once_joined_again = true;
        } else if (a == "--shot-once-back") {
            o.shot_once_back = true;
        } else if (a == "--shot-once-landed") {
            o.shot_once_landed = true;
        } else if (a == "--take-back-after" && has_value) {
            o.take_back_after_s = std::strtod(std::string(args[++i]).c_str(), nullptr);
        } else if (a == "--next-model-after" && has_value) {
            o.next_model_after_s.push_back(
                std::strtod(std::string(args[++i]).c_str(), nullptr));
        } else if (a == "--next-aircraft-after" && has_value) {
            o.next_aircraft_after_s.push_back(
                std::strtod(std::string(args[++i]).c_str(), nullptr));
        } else if (a == "--on-ground") {
            o.on_ground = true;
        } else if (a == "--attitude" && has_value) {
            const auto g = parse_triple(args[++i]);
            ok = g && std::abs((*g)[0]) < 90.0 && std::abs((*g)[1]) <= 180.0 &&
                 (*g)[2] >= 0.0 && (*g)[2] < 360.0;
            if (ok) {
                o.attitude = *g;
            }
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
    // **One copilot**, planned by one model: the hand-over's, when one is
    // chosen, which C asks too.
    if (!o.hand_over_model.provider.empty() && o.copilot_provider_given) {
        std::fputs("glideslope: --hand-over-model chooses the copilot's model: not with "
                   "--copilot-provider or --copilot-model\n",
                   stderr);
        return 2;
    }
    // The copilot's own model, which C asks when no hand-over model is
    // chosen - with M, in flight, as well as at start.
    const std::string own_copilot_provider = o.copilot_provider;
    const std::string own_copilot_model = o.copilot_model;
    if (!o.hand_over_model.provider.empty()) {
        o.copilot_provider = o.hand_over_model.provider;
        o.copilot_model = o.hand_over_model.model;
    }
    if (o.headless && o.shot.empty()) {
        std::fputs("glideslope: --headless needs --shot, or it has nothing to show\n",
                   stderr);
        return 2;
    }
    // **Written as the program ends** (`--done`), after everything else in
    // it has gone - the goodbye said - whichever way it returns.
    struct DoneWhenGone {
        std::string path;
        ~DoneWhenGone() {
            if (!path.empty()) {
                std::ofstream(path) << "gone\n";
            }
        }
    } const done_when_gone{o.done};
    if (o.shot_frame > 0 && (o.shot.empty() || o.online || !o.server.empty())) {
        std::fputs("glideslope: --shot-frame is a frame of a --shot flown alone\n", stderr);
        return 2;
    }
    if (o.shot_once_landed && (o.shot.empty() || (o.server.empty() && !o.online))) {
        std::fputs("glideslope: --shot-once-landed holds a --shot flown on a server\n", stderr);
        return 2;
    }
    if (o.shot_once_back && (o.shot.empty() || (o.server.empty() && !o.online))) {
        std::fputs("glideslope: --shot-once-back holds a --shot flown on a server\n", stderr);
        return 2;
    }
    if (o.shot_once_joined_again && (o.shot.empty() || (o.server.empty() && !o.online))) {
        std::fputs("glideslope: --shot-once-joined-again holds a --shot flown on a server\n",
                   stderr);
        return 2;
    }
    if (!o.microbursts.empty() && o.weather_station.empty()) {
        std::fputs("glideslope: a --microburst is put into --weather\n", stderr);
        return 2;
    }
    if (o.quit_at > 0 && (o.shot.empty() || o.quit_at >= o.shot_at)) {
        std::fputs("glideslope: --quit-at is a tick before a --shot's\n", stderr);
        return 2;
    }
    if (o.weather_refresh_given && o.weather_station.empty()) {
        std::fputs("glideslope: --weather-refresh is how often --weather is fetched again\n",
                   stderr);
        return 2;
    }
    if (!o.weather_station.empty() && (o.online || !o.server.empty())) {
        std::fputs("glideslope: on a server the server's weather is flown, not --weather\n",
                   stderr);
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
    if (o.attitude && (o.screen != "flight" || o.on_ground || !o.plan.empty())) {
        std::fputs("glideslope: --attitude is a flight's begun in the air, without a plan\n",
                   stderr);
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
    // **Told to stop, it stops** - whatever it is waiting on
    // (platform/stop.hpp). SDL's own handlers would turn the signal into a
    // quit event, read only between frames, so SDL installs none.
    glideslope::platform::catch_stop_signals();
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
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
            // **A test's server, waited on as an event** (`--after-ready`):
            // a server answers the handshake only once its terrain is built
            // and its aircraft fly, and the handshake gives up five seconds
            // after it begins. Started beside a debug server on a loaded CI
            // runner, the client gave up first, and the server admitted it
            // a moment later - "cannot reach" written before "listening"
            // (run 36860567602; PROJECT_STATUS.md, 2026-10-02).
            if (!o.after_ready.empty()) {
                const auto asked = std::chrono::steady_clock::now();
                while (!std::filesystem::exists(o.after_ready)) {
                    if (glideslope::platform::stop_requested()) {
                        return 1;
                    }
                    if (std::chrono::steady_clock::now() - asked > std::chrono::minutes(5)) {
                        std::fprintf(stderr, "glideslope: %s never appeared\n",
                                     o.after_ready.c_str());
                        return 1;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }
            if (auto made = glideslope::net::ClientSession::connect(
                    where, key, 5.0, 0.25, o.aircraft_given ? o.aircraft : std::string())) {
                session.emplace(std::move(*made));
            }
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
            online.emplace(std::move(*session), [&] { return seconds_since_start(); });
            if (o.late_update_after_take_over) {
                online->hear_late_update_after_take_over();
            }
            joined = online->join(10.0, [&] { return seconds_since_start(); });
            if (joined) {
                std::printf("glideslope: the server gave this client aircraft %u, the %s\n",
                            static_cast<unsigned>(joined->number),
                            joined->aircraft_id.c_str());
                // **Kept in the session while it builds its flight**, which
                // on a slow machine takes longer than a server waits for a
                // client that says nothing: a Windows debug build was let go
                // in its three seconds, and heard one update in all. Online
                // keeps it, from a thread of its own, whenever this one is
                // away - here, and in any pass of the frame loop.
                if (o.slow_start_s > 0.0) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double>(o.slow_start_s));
                }
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
            if (o.attitude) {
                start.pitch_deg = (*o.attitude)[0];
                start.roll_deg = (*o.attitude)[1];
                start.heading_deg = (*o.attitude)[2];
            }
            start.weather_station = o.weather_station;
            start.microbursts = o.microbursts;
            start.weather_refresh_s = o.weather_refresh_s;
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
                // Held to the aircraft flown - `--aircraft`, not the plan's
                // own, which the flight never reads - and to its speeds, as
                // a model's plan is.
                glideslope::sim::refuse_what_it_cannot_fly(
                    glideslope::platform::data_directory(), *start.plan, o.aircraft);
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
                // Its flaps where the server's are, not run in from up.
                if (joined->levers) {
                    start.flaps = joined->levers->flaps;
                }
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
                if (glideslope::platform::stop_requested()) {
                    break;
                }
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
            if (glideslope::platform::stop_requested()) {
                return stopped();
            }
            double worst = 0.0;
            std::size_t answered = 0;
            for (std::size_t i = 0; i < places.size(); ++i) {
                const glideslope::world::GroundHeight flown =
                    glideslope::client::flown_ground_at(
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
                // **And against the DEM as it is**, runways and all: what
                // the open provider draws from, so that a fault in drawing it
                // is not hidden by the runways' flattening.
                const glideslope::world::GroundHeight raw = glideslope::client::ground_at(
                    glideslope::platform::data_directory(),
                    glideslope::platform::cache_directory(), places[i].latitude_deg,
                    places[i].longitude_deg);
                const double raw_m = raw.above_sea_level_m + raw.geoid_m;
                std::printf("mismatch-dem %s %.8f %.8f drawn %.3f dem %.3f off %+.3f\n",
                            names[i].c_str(), places[i].latitude_deg,
                            places[i].longitude_deg, *drawn[i], raw_m, *drawn[i] - raw_m);
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
            std::string id; // the catalogue's, which it was read for
            bool known = false;
            std::optional<glideslope::client::Visual> visual;
            glideslope::client::ModelGeometry geometry;
        };
        std::map<std::string, OtherModel> models;
        const auto model_of = [&](const std::string& id) -> const OtherModel& {
            auto found = models.find(id);
            if (found == models.end()) {
                OtherModel m;
                m.id = id;
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
        // **Its own aircraft's model for riding along in, loaded before the
        // first frame**, not at the frame that first needs it: riding along
        // in its own seat at a hand-over read the Cessna's model there, 42 ms
        // of a 50 ms frame in a debug build, and windows-release's 102 ms
        // frame at the switch failed the 20 fps floor. Loaded in the first
        // frames instead, it lengthened one while the clocks' difference was
        // being learned (PROJECT_STATUS.md, 2026-09-30).
        // Only on a server, where it may be ridden in; alone nothing is.
        if (flight && online) {
            (void)model_of(flight->aircraft().id);
        }
        bool rode_along = false;
        bool asked_to_take_over = false;
        bool asked_to_hand_over = false;
        bool asked_for_learnt = false;
        // Whether the learnt landing had its own aircraft at the last look:
        // said when it changes.
        bool learnt_had_it = false;
        // **What the server refused, on the HUD**: the notice, and the tick
        // it is shown until - ten seconds of flight.
        std::string notice;
        std::int64_t notice_until = 0;
        // Said with what the HUD reads, once it is drawn: the learnt landing
        // given or refused.
        bool say_the_hud = false;
        // --press-after: the presses made, and the key held for the next
        // pass's keyboard, if any.
        std::size_t presses_made = 0;
        std::optional<SDL_Scancode> test_key_held;
        // The keyboard's passes, and the one the last press was held in: a
        // press waits for a pass with its key up after the last, so that two
        // of one key are two presses.
        std::int64_t key_passes = 0;
        std::int64_t pressed_in_pass = -2;
        // **Landed by the learnt landing**: its own at rest under it - under
        // 0.1 m/s for two seconds on the session's clock, as the
        // command-line client's --until-landed has it - and since when.
        bool learnt_landed = false;
        double at_rest_from_s = -1.0;
        bool asked_to_take_back = false;
        bool asked_to_stall = false;
        int let_go_said = 0;
        int went_back_said = 0;
        std::size_t rode_next = 0;
        std::size_t models_next = 0;
        bool said_the_view = false;
        // **What is shown of its own on a server**, blended across a switch
        // (frontend/shown.hpp), and when it was last worked out.
        glideslope::frontend::OwnShown own_shown;
        double own_framed_s = -1.0;
        // **Its own aircraft handed over, or taken back**: asked of the
        // server, which decides; what it says comes back in the updates.
        // **Ask the copilot** (C, or `--copilot-after`): the model is asked
        // here, with the player's key, and what it answers goes to the server
        // as a route, which the server's AI flies. Without a task, or a key,
        // it says so.
        //
        // **Made as soon as there is an aircraft to be the copilot of**, not
        // at the first C: its ground - a geoid, a DEM and the world's
        // runways, which may be downloaded - is made on a thread of its own
        // from then (frontend/players_copilot.hpp), and nothing it does is
        // on this thread after.
        std::unique_ptr<glideslope::frontend::PlayersCopilot> copilot;
        std::string copilot_refused;
        std::string hand_over_words;
        // **Where its copilot's route was going when sent**, and how far off:
        // a shot waits until the aircraft is nearer it - flown by the server,
        // not refused there, which the server does not say back.
        std::optional<glideslope::sim::Waypoint> route_to;
        double route_sent_m = 0.0;
        double route_now_m = 0.0;
        bool copilot_made = false;
        bool asked_the_copilot = false;
        bool copilot_route_sent = false;
        const auto make_the_copilot = [&]() {
            const bool for_hand_over = !o.hand_over_model.provider.empty();
            if (copilot_made || (o.copilot_task.empty() && !for_hand_over) || !joined) {
                return;
            }
            copilot_made = true;
            // **The hand-over's words**: the copilot's task without one of
            // the player's, and said with the hand-over's question with one.
            // From the installed data beside the program, as the catalogue
            // is. Not had, that is said for what it is - not as a key refused.
            if (for_hand_over) {
                try {
                    hand_over_words =
                        glideslope::frontend::hand_over_task(glideslope::platform::data_directory());
                } catch (const std::exception& e) {
                    copilot_refused = std::string("its words cannot be read: ") + e.what();
                    std::printf("glideslope: the hand-over's words cannot be read: %s\n", e.what());
                    if (o.copilot_task.empty()) {
                        return;
                    }
                }
            }
            try {
                glideslope::frontend::PlayersCopilotOptions c;
                c.aircraft = joined->aircraft_id;
                c.task = o.copilot_task.empty() ? hand_over_words : o.copilot_task;
                c.provider = o.copilot_provider;
                c.model = o.copilot_model;
                c.playback = o.copilot_playback;
                c.routine_s = 60.0;
                copilot = std::make_unique<glideslope::frontend::PlayersCopilot>(
                    glideslope::platform::data_directory(), c);
                std::printf("glideslope: copilot %s ready: %s\n", copilot->provider().c_str(),
                            o.copilot_task.empty() ? "it plans a hand-over (A)"
                            : for_hand_over        ? "C asks it, and it plans a hand-over (A)"
                                                   : "C asks it");
            } catch (const std::exception& e) {
                copilot_refused = o.copilot_provider + " was refused: " + e.what();
                std::printf("glideslope: no copilot: %s\n", e.what());
            }
        };
        // **The next model to plan a hand-over** (M): none, Claude, ChatGPT,
        // and round again - chosen in flight, not only at start. The copilot
        // is made again with it when next asked; one chosen by
        // --copilot-provider stays that.
        const auto next_model = [&]() {
            if (o.copilot_provider_given) {
                std::printf("glideslope: the copilot's model is --copilot-provider's; M "
                            "does not change it\n");
                return;
            }
            static constexpr std::array<const char*, 3> choices{"", "anthropic", "openai"};
            std::size_t now = 0;
            while (now < choices.size() && o.hand_over_model.provider != choices[now]) {
                ++now;
            }
            o.hand_over_model = {choices[(now + 1) % choices.size()], ""};
            o.copilot_provider = o.hand_over_model.provider.empty()
                                     ? own_copilot_provider
                                     : o.hand_over_model.provider;
            o.copilot_model =
                o.hand_over_model.provider.empty() ? own_copilot_model : std::string();
            copilot.reset();
            copilot_made = false;
            copilot_refused.clear();
            hand_over_words.clear();
            std::printf("glideslope: a hand-over is planned by %s (M chooses)\n",
                        o.hand_over_model.provider.empty()
                            ? "no model: the AI holds its course"
                            : o.hand_over_model.provider.c_str());
        };
        const auto ask_the_copilot = [&]() {
            make_the_copilot();
            if (!copilot || o.copilot_task.empty()) {
                std::printf("glideslope: no copilot%s\n",
                            o.copilot_task.empty() ? ": start with --copilot TASK" : "");
                return;
            }
            copilot->ask();
        };
        // **A pressed while a take-over is unanswered waits for the answer**:
        // until then the server may have made another aircraft this
        // client's own, and a hand-over sent for the one it has would go to
        // the one it is leaving. Taken, A is for the aircraft taken; refused
        // (`TAKE_OVER_REFUSED`), for the one it kept.
        bool a_held = false;
        std::int64_t take_over_asked_at = 0;
        const auto ask_for_learnt = [&]() {
            online->hand_to_learnt();
            std::printf("glideslope: asked for aircraft %u to be handed to the learnt landing\n",
                        static_cast<unsigned>(online->mine()));
            std::fflush(stdout);
        };
        const auto hand_over = [&](bool to_ai) {
            online->hand_over(to_ai);
            std::printf("glideslope: asked for aircraft %u to be handed to %s\n",
                        static_cast<unsigned>(online->mine()), to_ai ? "the AI" : "its pilot");
            // **Planned by the model chosen for it**, asked from where the
            // aircraft is; refused - no key - it is held, as with none.
            // Taken back, its copilot stands by at once.
            if (!to_ai && copilot) {
                copilot->taken_back();
            }
            if (to_ai && !o.hand_over_model.provider.empty()) {
                make_the_copilot();
                if (copilot && (!o.copilot_task.empty() || !hand_over_words.empty())) {
                    copilot->handed_over(o.copilot_task.empty() ? std::string() : hand_over_words);
                    asked_the_copilot = true; // and the shot waits for its route
                    std::printf("glideslope: handed over, planned by %s\n",
                                copilot->provider().c_str());
                } else {
                    std::printf("glideslope: handed over, planned by no model (%s): the AI "
                                "holds its course\n",
                                copilot_refused.c_str());
                }
            }
        };
        const auto press_a = [&] {
            if (const auto asked = online->taking_over()) {
                a_held = true;
                std::printf("glideslope: A held until the server answers the take-over of "
                            "aircraft %u\n",
                            static_cast<unsigned>(*asked));
                return;
            }
            hand_over(!online->own_ai_flying());
        };
        struct OtherMesh {
            glideslope::gfx::MeshId id = 0;
            bool made = false;
            glideslope::world::Ecef lit_by{};
            std::string aircraft_id;
        };
        std::map<std::uint8_t, OtherMesh> other_meshes;
        std::vector<glideslope::client::Other> others_now;
        // **Ride along** (W): the next aircraft in the sky, by number, and
        // after the last, back in your own - which, while the AI flies it, is
        // ridden along in as it is drawn from the updates, since the flight
        // here is not flown then and would sit where it was handed over.
        const auto ride_next = [&] {
            const std::uint8_t own = online->own_ai_flying() ? online->mine()
                                                             : glideslope::net::no_aircraft;
            const std::uint8_t from =
                online->watching() == own ? glideslope::net::no_aircraft : online->watching();
            std::uint8_t next = glideslope::net::no_aircraft;
            for (const glideslope::client::Other& other : others_now) {
                if (other.number != own &&
                    (from == glideslope::net::no_aircraft || other.number > from) &&
                    (next == glideslope::net::no_aircraft || other.number < next)) {
                    next = other.number;
                }
            }
            if (next == glideslope::net::no_aircraft) {
                online->watch(own);
                std::printf("glideslope: back in your own aircraft\n");
                said_the_view = true;
            } else {
                online->watch(next);
                std::printf("glideslope: riding along in aircraft %u\n",
                            static_cast<unsigned>(next));
            }
        };
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
        // **On a server, the levers are where the server's aircraft has
        // them** - its throttle, flaps, gear and speedbrake lever - and move
        // from there: begun at this client's own, the first input sent ran
        // the landing flap of an aircraft started on final back in, and it
        // could never be at the learnt landing's gate.
        if (joined && joined->levers) {
            const glideslope::net::Watched& l = *joined->levers;
            controls.throttle = l.throttle;
            controls.flaps = l.flaps;
            if (l.gear) {
                controls.gear = *l.gear;
            }
            if (l.speedbrake) {
                controls.speedbrake = *l.speedbrake;
            }
            std::printf("glideslope: the server's levers kept: throttle %.2f, flaps %.2f\n",
                        controls.throttle, controls.flaps);
            std::fflush(stdout);
        } else if (joined) {
            std::printf("glideslope: the server did not say its levers; this client's own\n");
            std::fflush(stdout);
        }
        const std::filesystem::path bindings_path =
            glideslope::platform::data_directory() / "input" / "bindings.txt";
        std::ifstream bindings_file(bindings_path, std::ios::binary);
        if (!bindings_file) {
            throw std::runtime_error("cannot read " + bindings_path.string());
        }
        const std::vector<glideslope::platform::Binding> bindings =
            glideslope::platform::parse_bindings(
                std::string(std::istreambuf_iterator<char>(bindings_file), {}));
        glideslope::platform::ControlMapper mapper(bindings);
        // **The controls' help**, from the bindings read, so it says what
        // they do; F1 shows and hides it.
        const std::vector<std::string> help = glideslope::platform::controls_help(bindings);
        bool showing_help = o.show_help;
        glideslope::platform::Joysticks joysticks;
        glideslope::platform::KeyboardControls keys;
        glideslope::sim::FixedStep clock;
        auto last = std::chrono::steady_clock::now();
        std::int64_t ticks = 0;
        long frames = 0;
        // When --slow-frames last held a pass of the loop.
        auto held_at = std::chrono::steady_clock::now();
        bool running = true;
        // **Where the time of a pass went**, milliseconds by part, for the
        // longest pass around a switch: the step bounds' 20 fps floor has
        // failed on windows-release in the frame a switch is drawn in, and
        // this says which part of it was long, from the machine it was long
        // on (PROJECT_STATUS.md, 2026-09-30).
        struct PassTimes {
            double total = 0.0, ticks = 0.0, hear = 0.0, scene = 0.0, terrain = 0.0,
                   draws = 0.0, hud = 0.0, render = 0.0;
        };
        PassTimes pass_times;
        std::array<PassTimes, 4> passes_before{};
        PassTimes longest_pass_at_switch;
        double pass_mark = 0.0;
        const auto pass_part = [&](double& part) {
            const double now_s = seconds_since_start();
            part = (now_s - pass_mark) * 1000.0;
            pass_mark = now_s;
        };
        double pass_began = 0.0;
        const auto pass_done = [&] {
            pass_times.total = (seconds_since_start() - pass_began) * 1000.0;
            const int since = own_shown.frames_since_switch();
            if (since == 0) {
                for (const PassTimes& p : passes_before) {
                    if (p.total > longest_pass_at_switch.total) {
                        longest_pass_at_switch = p;
                    }
                }
            }
            if (since <= 4 && pass_times.total > longest_pass_at_switch.total) {
                longest_pass_at_switch = pass_times;
            }
            std::rotate(passes_before.begin(), passes_before.begin() + 1, passes_before.end());
            passes_before.back() = pass_times;
        };
        // **How long this thread was away from its session while it was
        // kept for it** (client::Online): building the flight, said here;
        // flying, said at the shot; and the longest of all, said at the end.
        double longest_away_s = 0.0;
        bool said_away_at_shot = false;
        if (online && joined) {
            longest_away_s = online->longest_kept_since_asked_s();
            std::printf("glideslope: built its flight; away from its session %.1f s at the "
                        "longest meanwhile, and kept in it\n",
                        longest_away_s);
        }
        while (running) {
            pass_times = PassTimes{};
            pass_began = seconds_since_start();
            pass_mark = pass_began;
            if (glideslope::platform::stop_requested()) {
                break;
            }
            if (online && !(joined && flight)) {
                // In a session without an aircraft: kept, and nothing more.
                online->idle(seconds_since_start());
            }
            // **Let go by the server, it joins again by itself**
            // (net::ClientSession), and says so; **dropped by its operator,
            // or refused, it stops**, and says why.
            if (online) {
                const glideslope::client::Standing s = online->standing();
                if (s.let_go > let_go_said) {
                    let_go_said = s.let_go;
                    std::printf("glideslope: let go by the server: refused BAD_HANDSHAKE after "
                                "%.1f s of nothing; joining again\n",
                                s.quiet_when_let_go_s);
                }
                if (s.went_back > went_back_said) {
                    went_back_said = s.went_back;
                    std::printf("glideslope: the old session answered; staying in it\n");
                }
                using Standing = glideslope::net::ClientSession::Standing;
                const char* why = s.standing == Standing::dropped
                                      ? "the server ended this session (dropped, or taken "
                                        "over by a newer session for this key); not joining "
                                        "again"
                                  : s.standing == Standing::refused
                                      ? "refused by the server when joining again: it is full"
                                  : s.standing == Standing::gave_up
                                      ? "let go, and could not join again in a minute"
                                      : nullptr;
                if (why != nullptr) {
                    std::printf("glideslope: %s\n", why);
                    std::fflush(stdout);
                    status = 1;
                    break;
                }
                // **On other ground than the server's, it leaves**
                // (REQUIREMENTS.md 6.3): refused, not fetched - the ground is
                // the data this build carries.
                if (const std::string other = online->other_ground(); !other.empty()) {
                    std::printf("glideslope: refused the server's collision ground: %s\n",
                                other.c_str());
                    std::fflush(stdout);
                    status = 1;
                    break;
                }
            }
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                using glideslope::platform::Command;
                const auto key = [](Command command) {
                    return static_cast<SDL_Scancode>(
                        glideslope::platform::command_key(command).scancode);
                };
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == key(Command::help)) {
                    showing_help = !showing_help;
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == key(Command::swap_pilot) && flight) {
                    // On a server the server owns the aircraft: A asks it.
                    // Without one - no server, or one that has given this
                    // client no aircraft yet - the flight's own pilot swaps.
                    if (online && joined) {
                        press_a();
                    } else {
                        flight->swap_pilot();
                    }
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == key(Command::learnt_landing) && online && joined) {
                    // **L asks for the learnt landing**: the server hands
                    // her over only where her model has one and she is at
                    // its gate on final, and says why not in its log.
                    ask_for_learnt();
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == key(Command::next_model) && online && joined) {
                    next_model();
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == key(Command::copilot) && online && joined) {
                    ask_the_copilot();
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == key(Command::take_over) && online && joined &&
                           online->watching() != glideslope::net::no_aircraft) {
                    // **Take the controls** of the aircraft ridden in, if the
                    // AI is flying it: a player's is never taken, and the
                    // server would refuse without a word.
                    const auto ridden_now = std::find_if(
                        others_now.begin(), others_now.end(),
                        [&](const glideslope::client::Other& other) {
                            return other.number == online->watching();
                        });
                    if (online->watching() == online->mine() && online->own_ai_flying()) {
                        // Its own, which the AI flies: taking it back.
                        hand_over(false);
                    } else if (ridden_now != others_now.end() && ridden_now->ai_flying &&
                               online->take_over(online->watching())) {
                        take_over_asked_at = ticks;
                        std::printf("glideslope: asked to take over aircraft %u\n",
                                    static_cast<unsigned>(online->watching()));
                    } else {
                        std::printf("glideslope: aircraft %u is not the AI's to take over\n",
                                    static_cast<unsigned>(online->watching()));
                    }
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == key(Command::ride_next) && online && joined) {
                    ride_next();
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                           event.key.scancode == key(Command::view) && flight) {
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
            // Each followed by a tenth of a second at full speed, so that
            // the short frames between show what the long ones did, as a
            // slow machine's mixed frames do.
            if (o.slow_frames_ms > 0.0 &&
                std::chrono::steady_clock::now() - held_at >= std::chrono::milliseconds(100)) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double, std::milli>(o.slow_frames_ms));
                held_at = std::chrono::steady_clock::now();
            }
            std::int64_t due = 0;
            if (shooting && !joined) {
                // Two ticks a frame, or as many as keep the flight to 300.
                // (On a server a flight keeps real time, shot or not: the
                // server's aircraft does.)
                // A frame counted is two ticks, however many frames.
                due = std::min<std::int64_t>(
                    o.shot_frame > 0 ? 2 : std::max<std::int64_t>(2, o.shot_at / 300),
                    o.shot_at - ticks);
            } else {
                const auto now = std::chrono::steady_clock::now();
                // **Every tick due is flown on a server, up to four
                // seconds' a pass** (sim::steps_to_fly), where once whatever
                // was past 24 ticks - a fifth of a second - was dropped: a
                // dropped tick there is the prediction falling behind the
                // server's clock unflagged, and CI's sanitized software
                // Vulkan draws a frame in 250 ms and more (PROJECT_STATUS.md,
                // 2026-09-30).
                // **At the server's pace** on a server (sim::Pacing): one
                // behind real time flies each input for fewer ticks, and so,
                // then, does this.
                const double pace = online && flight ? flight->pace() : 1.0;
                due = glideslope::sim::steps_to_fly(
                    clock.advance(std::chrono::duration_cast<std::chrono::nanoseconds>(
                        (now - last) * pace)),
                    online.has_value());
                last = now;
                // The keyboard - with a test's key held (--press-after), as a
                // player's finger holds it.
                int key_count = 0;
                const bool* key_state = SDL_GetKeyboardState(&key_count);
                std::unique_ptr<bool[]> with_test_key;
                if (test_key_held) {
                    const int at = static_cast<int>(*test_key_held);
                    const int count = std::max(key_count, at + 1);
                    with_test_key = std::make_unique<bool[]>(static_cast<std::size_t>(count));
                    for (int k = 0; k < key_count; ++k) {
                        with_test_key[static_cast<std::size_t>(k)] = key_state[k];
                    }
                    with_test_key[static_cast<std::size_t>(at)] = true;
                    key_state = with_test_key.get();
                    key_count = count;
                    test_key_held.reset();
                    pressed_in_pass = key_passes;
                }
                keys.apply(controls, glideslope::sim::key_seconds(due), key_state, key_count);
                ++key_passes;
            }
            mapper.apply(joysticks.read(), controls);
            // Whether its own was predicted up to this pass - flown here on
            // every tick, so that the prediction's clocks' difference holds -
            // before what this pass hears changes it.
            const bool predicted_before =
                online && joined && flight && flight->predicting() && !online->own_ai_flying();
            // **Handed to the AI on a server, it is not predicted**: the
            // server flies it, and it is drawn from the updates.
            const auto tick = [&](const glideslope::sim::Controls& flown) {
                if (flight && !(online && joined && online->own_ai_flying())) {
                    flight->step(flown);
                    if (o.trace) {
                        std::printf("%s\n", flight->trace().c_str());
                    }
                }
                ++ticks;
            };
            if (joined && flight) {
                // **On a server, a pass in the order glideslope::client::
                // fly_a_pass gives and says why**: the stick sent, what has
                // arrived taken in, the ticks flown on the input sent
                // before, the new one flown from the next, and what was
                // taken in heard.
                struct Link {
                    glideslope::client::Online& online;
                    glideslope::client::Flight& flight;
                    const std::function<void()> ticks_began;
                    const std::function<void()> ticks_ended;
                    glideslope::sim::Controls fly(double local_s,
                                                  const glideslope::sim::Controls& stick) {
                        return online.fly(local_s, stick, flight);
                    }
                    void listen(double local_s) {
                        online.listen(local_s);
                        ticks_began();
                    }
                    void ticks_flown() { ticks_ended(); }
                    void flown() { online.flown(flight); }
                    void hear() { online.hear(flight); }
                };
                Link link{*online, *flight, [&] { pass_mark = seconds_since_start(); },
                          [&] { pass_part(pass_times.ticks); }};
                glideslope::client::fly_a_pass(link, seconds_since_start(), controls, due, tick);
            } else {
                pass_mark = seconds_since_start();
                for (std::int64_t i = 0; i < due; ++i) {
                    tick(controls);
                }
                pass_part(pass_times.ticks);
            }

            pass_part(pass_times.hear);
            // **Quitting**, at a tick, straight after the steps that reach it:
            // no frame drawn and no tile waited for, so what is under way -
            // a weather refresh - is under way still. Said, flushed, for a
            // test reading through a pipe to time the way out from here
            // (glideslope_exit_timer, tests/cmake/client_quits_mid_refresh.cmake).
            if (o.quit_at > 0 && ticks >= o.quit_at) {
                if (flight && !o.weather_station.empty()) {
                    std::printf("glideslope: a weather refresh is %s\n",
                                flight->refreshing_weather() ? "under way"
                                                             : "not under way");
                }
                std::printf("glideslope: quitting at tick %lld\n",
                            static_cast<long long>(ticks));
                std::fflush(stdout);
                break;
            }
            // The frame shot waits for every terrain tile its view needs, so
            // the same command draws the same terrain everywhere.
            bool shot_now = shooting && ticks >= o.shot_at;
            if (shot_now && online && joined && !said_away_at_shot) {
                said_away_at_shot = true;
                const double away_s = online->longest_kept_since_asked_s();
                longest_away_s = std::max(longest_away_s, away_s);
                std::printf("glideslope: at the shot, away from its session %.1f s at the "
                            "longest since flying began, and kept in it\n",
                            away_s);
            }
            // **Taken over**: the flight is the aircraft taken over now, from
            // the motion the server gave it - the same aeroplane made to be
            // where that one is, or another built as it.
            if (online && joined && flight) {
                if (o.take_over_after_s >= 0.0 && !asked_to_take_over &&
                    static_cast<double>(ticks) >=
                        o.take_over_after_s * glideslope::sim::steps_per_second &&
                    online->watching() != glideslope::net::no_aircraft) {
                    asked_to_take_over = true;
                    for (int w = 0; w < o.watches_before_take_over; ++w) {
                        online->watch(online->watching());
                    }
                    const std::uint8_t asked =
                        o.take_over_own ? online->mine() : online->watching();
                    if (online->take_over(asked)) {
                        take_over_asked_at = ticks;
                        std::printf("glideslope: asked to take over aircraft %u\n",
                                    static_cast<unsigned>(asked));
                    } else {
                        std::printf("glideslope: aircraft %u not asked for: it is this "
                                    "client's own, or there is no session\n",
                                    static_cast<unsigned>(asked));
                    }
                    if (o.press_a_with_take_over) {
                        press_a();
                    }
                }
                const double joined_s =
                    static_cast<double>(ticks) /
                    static_cast<double>(glideslope::sim::steps_per_second);
                make_the_copilot();
                if (o.copilot_after_s >= 0.0 && !asked_the_copilot &&
                    joined_s >= o.copilot_after_s) {
                    asked_the_copilot = true;
                    ask_the_copilot();
                }
                // Its copilot looked at between frames: never waits for the
                // model, and sends its route when one is due.
                if (copilot && online->own_heard()) {
                    try {
                        const auto& [heard_s, own] = *online->own_heard();
                        const glideslope::world::Geodetic at =
                            glideslope::world::to_geodetic({own.x_m, own.y_m, own.z_m});
                        if (auto route = copilot->look(heard_s, own)) {
                            const glideslope::net::RouteWaypoint& first = route->waypoints.front();
                            route_to = glideslope::sim::Waypoint{};
                            route_to->name = first.name;
                            route_to->latitude_deg = first.latitude_deg;
                            route_to->longitude_deg = first.longitude_deg;
                            route_sent_m = glideslope::sim::distance_m(
                                at.latitude_deg, at.longitude_deg, first.latitude_deg,
                                first.longitude_deg);
                            online->send_route(std::move(*route));
                            copilot_route_sent = true;
                        }
                        if (route_to) {
                            route_now_m = glideslope::sim::distance_m(
                                at.latitude_deg, at.longitude_deg, route_to->latitude_deg,
                                route_to->longitude_deg);
                        }
                    } catch (const std::exception& e) {
                        std::printf("glideslope: copilot: %s\n", e.what());
                    }
                    for (const std::string& line : copilot->said()) {
                        std::printf("glideslope: %s\n", line.c_str());
                    }
                }
                if (o.learnt_landing_after_s >= 0.0 && !asked_for_learnt &&
                    joined_s >= o.learnt_landing_after_s) {
                    asked_for_learnt = true;
                    ask_for_learnt();
                }
                if (const bool learnt = online->own_learnt_landing(); learnt != learnt_had_it) {
                    learnt_had_it = learnt;
                    std::printf("glideslope: %s\n",
                                learnt ? "the server says the learnt landing has this aircraft"
                                       : "the learnt landing no longer has this aircraft");
                    std::fflush(stdout);
                    say_the_hud = learnt;
                    if (learnt) {
                        notice.clear();
                    }
                }
                if (learnt_had_it && !learnt_landed && online->own_heard()) {
                    const auto& [heard_s, own] = *online->own_heard();
                    if (std::hypot(own.vx_mps, own.vy_mps, own.vz_mps) < 0.1F) {
                        if (at_rest_from_s < 0.0) {
                            at_rest_from_s = heard_s;
                        }
                        if (heard_s - at_rest_from_s >= 2.0) {
                            learnt_landed = true;
                            std::printf("glideslope: aircraft %u is at rest, landed by the "
                                        "learnt landing\n",
                                        static_cast<unsigned>(online->mine()));
                            std::fflush(stdout);
                            say_the_hud = true;
                        }
                    } else {
                        at_rest_from_s = -1.0;
                    }
                }
                // **Refused, the player is told why**, on the HUD for ten
                // seconds of flight and here.
                for (const std::string& why : online->refused_learnt_landings()) {
                    std::printf("glideslope: the server refused the learnt landing: %s\n",
                                why.c_str());
                    std::fflush(stdout);
                    notice = "refused: " + why;
                    notice_until = ticks + 10 * glideslope::sim::steps_per_second;
                    say_the_hud = true;
                }
                // --press-after: each key at its time, its event and the key
                // held for the next pass's keyboard, as a finger would.
                while (presses_made < o.presses.size() && !test_key_held &&
                       key_passes > pressed_in_pass + 1 &&
                       joined_s >= o.presses[presses_made].first) {
                    const SDL_Scancode key = o.presses[presses_made].second;
                    ++presses_made;
                    SDL_Event press{};
                    press.type = SDL_EVENT_KEY_DOWN;
                    press.key.scancode = key;
                    press.key.down = true;
                    press.key.repeat = false;
                    SDL_PushEvent(&press);
                    test_key_held = key;
                    std::printf("glideslope: pressed %s, %.1f s in\n", SDL_GetScancodeName(key),
                                joined_s);
                    std::fflush(stdout);
                    break; // one a pass, so that each is a press of its own
                }
                if (o.hand_over_after_s >= 0.0 && !asked_to_hand_over &&
                    joined_s >= o.hand_over_after_s) {
                    asked_to_hand_over = true;
                    hand_over(true);
                }
                if (o.stall_after_s >= 0.0 && !asked_to_stall &&
                    joined_s >= o.stall_after_s) {
                    asked_to_stall = true;
                    online->stall();
                    std::printf("glideslope: stalling, sending and answering nothing, until "
                                "the server lets this client go\n");
                }
                if (o.take_back_after_s >= 0.0 && !asked_to_take_back &&
                    joined_s >= o.take_back_after_s) {
                    asked_to_take_back = true;
                    hand_over(false);
                }
                if (models_next < o.next_model_after_s.size() &&
                    joined_s >= o.next_model_after_s[models_next]) {
                    ++models_next;
                    next_model();
                }
                if (rode_next < o.next_aircraft_after_s.size() &&
                    joined_s >= o.next_aircraft_after_s[rode_next]) {
                    ++rode_next;
                    ride_next();
                }
                if (const auto taken = online->taken_over()) {
                    if (taken->aircraft_id != flight->aircraft().id) {
                        const glideslope::world::Geodetic g = glideslope::world::to_geodetic(
                            {taken->motion.location_ecef_m[0], taken->motion.location_ecef_m[1],
                             taken->motion.location_ecef_m[2]});
                        start.aircraft = taken->aircraft_id;
                        start.latitude_deg = g.latitude_deg;
                        start.longitude_deg = g.longitude_deg;
                        start.height_m = g.height_m;
                        flight = std::make_unique<glideslope::client::Flight>(
                            glideslope::platform::data_directory(),
                            glideslope::platform::cache_directory(), start);
                        has_aircraft_mesh = false;
                        // What the flight it replaces had: the input it is on,
                        // and the checklist on screen, where this one has it.
                        flight->set_input_sequence(online->sequence());
                        if (!o.checklist.empty()) {
                            flight->show_checklist(checklist_phase);
                            if (!flight->showing_checklist()) {
                                std::printf("glideslope: the %s ships no checklists\n",
                                            flight->aircraft().id.c_str());
                            }
                        }
                    }
                    // **Flown on to now** from the word that gave it, where
                    // the prediction knows how: taken over from a flight
                    // predicted up to now (another aeroplane is a flight
                    // built afresh, with nothing predicted, and only put
                    // there).
                    if (predicted_before && !taken->again) {
                        flight->adopt(taken->motion, taken->server_steps);
                    } else {
                        flight->adopt(taken->motion);
                    }
                    // Joined again, it is a new aircraft, which the old
                    // one's place says nothing of: nothing is blended.
                    if (!taken->again) {
                        own_shown.taken_over(taken->number);
                    }
                    joined = *taken;
                    std::printf(taken->again
                                    ? "glideslope: joined again: the server gave this client "
                                      "aircraft %u, the %s\n"
                                    : "glideslope: took over aircraft %u, the %s\n",
                                static_cast<unsigned>(taken->number),
                                taken->aircraft_id.c_str());
                    // **And on what clock**: the new session's, from the
                    // update that gave it, beside the old one's newest - a
                    // server started again counts from nought.
                    if (taken->again && online->old_session_s()) {
                        std::printf("glideslope: joined again at %.1f s on the server's clock, "
                                    "the old session's newest word at %.1f s\n",
                                    static_cast<double>(taken->server_steps) /
                                        static_cast<double>(glideslope::sim::steps_per_second),
                                    *online->old_session_s());
                    }
                }
                for (const std::uint8_t refused : online->refused_take_overs()) {
                    std::printf("glideslope: the server refused to take over aircraft %u\n",
                                static_cast<unsigned>(refused));
                }
                // **No answer in five seconds of flight** - more than any
                // round trip this project expects - and the take-over is
                // given up: held for ever, A would do nothing again.
                if (const auto asked = online->taking_over();
                    asked && ticks - take_over_asked_at > 5 * glideslope::sim::steps_per_second) {
                    online->give_up_take_over();
                    std::printf("glideslope: no answer to the take-over of aircraft %u in 5 s "
                                "of flight: given up\n",
                                static_cast<unsigned>(*asked));
                }
                // **A held for the answer**, which has come: for whichever
                // aircraft is its own now.
                if (a_held && !online->taking_over()) {
                    a_held = false;
                    std::printf("glideslope: A, held, is for aircraft %u\n",
                                static_cast<unsigned>(online->mine()));
                    hand_over(!online->own_ai_flying());
                }
            }
            // **Who flies its own, as the server said**: at a switch a frame
            // is drawn, shot or not, and says what its HUD read. Handed over,
            // it rides along in its own aircraft - its seat, and its controls
            // as the AI moves them - unless riding along in another already.
            bool switched_now = false;
            if (online && joined && online->switched()) {
                switched_now = true;
                if (online->own_ai_flying() &&
                    online->watching() == glideslope::net::no_aircraft) {
                    online->watch(online->mine());
                } else if (!online->own_ai_flying() && online->watching() == online->mine()) {
                    // Taken back: its own flight is flown here again, and
                    // riding along in nothing is riding in it.
                    online->watch(glideslope::net::no_aircraft);
                }
            }
            // **On a server, everybody else as they are now**, and the one
            // being ridden along in, if any: its own among them while the AI
            // flies it.
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
                // A minute of the flight past the shot's tick is as long as any
                // machine needs; past it the shot is drawn as things are, and
                // says what it heard, so a test fails saying why.
                const bool waited_long = ticks >= o.shot_at + 60 * glideslope::sim::steps_per_second;
                if (shot_now && o.ride_along && !waited_long &&
                    (!rode_along || (online->watching() != glideslope::net::no_aircraft &&
                                     !online->watched_controls(seconds_since_start())))) {
                    shot_now = false;
                }
                // **Taken back, the shot waits for the server to have said
                // so** and to have flown it by an input sent since: events,
                // not the clock, which a slow machine outruns. The same
                // minute past the tick bounds it.
                const bool back_unheard =
                    asked_to_take_back &&
                    (online->own_ai_flying() || !online->flown_since_taken_over());
                if (shot_now && back_unheard && !waited_long) {
                    shot_now = false;
                }
                // **Stalled, the shot waits for it to have joined again** and
                // for the server to have flown its new aircraft by an input
                // sent since: the same events, the same minute.
                const bool await_again = asked_to_stall || o.shot_once_joined_again;
                const bool again_unheard =
                    await_again &&
                    (!online->had_by_joining_again() || !online->flown_since_taken_over());
                if (shot_now && again_unheard && !waited_long) {
                    shot_now = false;
                }
                // **Refused by a forger, the shot waits for it to have gone
                // back** to its old session and been flown there by an input
                // sent since: the same events, the same minute.
                const bool back_in_old_unheard =
                    o.shot_once_back &&
                    (!online->gone_back() || !online->flown_since_going_back());
                if (shot_now && back_in_old_unheard && !waited_long) {
                    shot_now = false;
                }
                // **Asked to, the shot waits for the learnt landing to have
                // her at rest**: events, the landing taking two minutes and
                // more; five minutes of the flight past its tick bound it.
                if (shot_now && o.shot_once_landed && !learnt_landed &&
                    ticks < o.shot_at + 300 * glideslope::sim::steps_per_second) {
                    shot_now = false;
                }
                if (shot_now && o.shot_once_landed) {
                    std::printf("glideslope: the shot drawn %.1f s past its tick; %s\n",
                                static_cast<double>(ticks - o.shot_at) /
                                    static_cast<double>(glideslope::sim::steps_per_second),
                                learnt_landed ? "landed by the learnt landing, at rest"
                                              : "not landed by the learnt landing");
                }
                // **Handed over and not taken back, the shot waits for the
                // server to have said the AI has it**: the same minute.
                const bool handed_unheard = asked_to_hand_over && !asked_to_take_back &&
                                            !online->own_ai_flying();
                if (shot_now && handed_unheard && !waited_long) {
                    shot_now = false;
                }
                if (shot_now && o.shot_once_back) {
                    std::printf("glideslope: the shot drawn %.1f s past its tick; %s (input "
                                "%u sent, %u applied, %u when it went back)\n",
                                static_cast<double>(ticks - o.shot_at) /
                                    static_cast<double>(glideslope::sim::steps_per_second),
                                back_in_old_unheard
                                    ? "not yet gone back to its old session and flown"
                                    : "gone back to its old session, and flown by an input "
                                      "sent since",
                                online->sequence(), online->applied(), online->back_at());
                }
                // **Its copilot asked, the shot waits for its route to be
                // sent and the server to have given the AI the aircraft**:
                // events again - the copilot's ground made and the model's
                // answer taken, which on a slow machine is well past any
                // tick. Five minutes of the flight past it bound it.
                // Flown, not refused: 200 m nearer its first waypoint than
                // when it was sent.
                const bool route_flown = route_to && route_now_m <= route_sent_m - 200.0;
                const bool copilot_unheard =
                    asked_the_copilot &&
                    (!copilot_route_sent || !online->own_ai_flying() || !route_flown);
                const bool waited_for_copilot =
                    ticks >= o.shot_at + 300 * glideslope::sim::steps_per_second;
                if (shot_now && copilot_unheard && !waited_for_copilot) {
                    shot_now = false;
                }
                if (shot_now && asked_the_copilot) {
                    std::printf("glideslope: the shot drawn %.1f s past its tick; %s\n",
                                static_cast<double>(ticks - o.shot_at) /
                                    static_cast<double>(glideslope::sim::steps_per_second),
                                copilot_unheard
                                    ? "its copilot's route not yet sent and flown by the AI"
                                    : "its copilot's route sent, and the server says the AI "
                                      "has it");
                    if (route_to) {
                        std::printf("glideslope: its copilot's route: to %s, %.0f m off when "
                                    "sent, %.0f m at the shot\n",
                                    route_to->name.c_str(), route_sent_m, route_now_m);
                    }
                }
                if (shot_now && await_again) {
                    std::printf("glideslope: the shot drawn %.1f s past its tick; %s\n",
                                static_cast<double>(ticks - o.shot_at) /
                                    static_cast<double>(glideslope::sim::steps_per_second),
                                again_unheard ? "not yet joined again and flown"
                                              : "joined again, and flown by an input sent "
                                                "since");
                }
                if (shot_now && asked_to_take_back) {
                    std::printf("glideslope: the shot drawn %.1f s past its tick; the server "
                                "says %s has it%s\n",
                                static_cast<double>(ticks - o.shot_at) /
                                    static_cast<double>(glideslope::sim::steps_per_second),
                                online->own_ai_flying() ? "the AI" : "the pilot",
                                online->flown_since_taken_over()
                                    ? ", flown by an input sent since"
                                    : ", not yet flown by an input sent since");
                }
                if (shot_now && o.ride_along) {
                    std::printf("glideslope: the shot drawn %.1f s past its tick; %zu updates "
                                "carried the watched aircraft's controls\n",
                                static_cast<double>(ticks - o.shot_at) /
                                    static_cast<double>(glideslope::sim::steps_per_second),
                                online->watched_heard());
                }
            }
            // **On a server, a shot draws only its own frame.** The flight
            // keeps real time there, so the frames before it are as many as
            // the machine can draw - thousands - where a shot flown by ticks
            // has three hundred at most. Drawing them all headless once ran
            // the software Vulkan driver out of memory, before the renderer
            // waited on its frames in flight (gfx/renderer.hpp); now they
            // would only take the machine's time from the session. Nobody
            // sees them; the flight and the session go on all the same.
            // **Its own, shown blended across a switch**: from the flight here
            // while predicted, from the updates while the AI flies it, and
            // moved by what is left of the blend - the model where it is
            // drawn and the camera in it alike. Worked out at every frame
            // drawn, and sixty times a second when none is, as a shot on a
            // server draws few: what is measured is what a screen would show.
            std::optional<glideslope::world::Ecef> own_moved; // shown less its source
            bool own_predicted = false;
            if (online && joined && flight) {
                const bool drawing =
                    !(shooting && !shot_now && !switched_now && !said_the_view && !say_the_hud);
                const double local_s = seconds_since_start();
                if (drawing || local_s - own_framed_s >= 1.0 / 60.0) {
                    own_framed_s = local_s;
                    glideslope::client::Other* own_other = nullptr;
                    for (glideslope::client::Other& other : others_now) {
                        if (other.number == online->mine()) {
                            own_other = &other;
                        } else {
                            own_shown.seen(other.number, local_s, other.centre, other.path_mps);
                        }
                    }
                    std::optional<glideslope::frontend::OwnShown::Source> source;
                    // Asked every frame, so that one heard while the AI flew
                    // it is not taken up after a take-back.
                    const bool corrected = online->corrected();
                    if (online->own_ai_flying()) {
                        if (own_other != nullptr) {
                            source = glideslope::frontend::OwnShown::Source{
                                own_other->centre, own_other->path_mps, false, false};
                        }
                    } else if (flight->predicting()) {
                        source = glideslope::frontend::OwnShown::Source{
                            flight->centre(), flight->velocity_ecef_mps(), true, corrected};
                    }
                    if (source) {
                        const glideslope::world::Ecef shown = own_shown.frame(local_s, *source);
                        own_moved = glideslope::world::Ecef{shown.x - source->at.x,
                                                            shown.y - source->at.y,
                                                            shown.z - source->at.z};
                        own_predicted = source->predicted;
                        if (own_other != nullptr && !own_predicted) {
                            own_other->centre = shown;
                        }
                    }
                }
            }
            if (shooting && joined && !shot_now && !switched_now && !said_the_view && !say_the_hud) {
                pass_done();
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
            if (own_moved && own_predicted && !ridden) {
                camera.position.x += own_moved->x;
                camera.position.y += own_moved->y;
                camera.position.z += own_moved->z;
            }
            pass_part(pass_times.scene);
            if (terrain) {
                // **Kept in the session while the shot waits for its
                // terrain**, which on a cold cache is longer than a server
                // waits for a client that says nothing: let go for silence,
                // it would join again, and a test counting sessions would
                // count one too many. Online keeps it while this is away.
                draws = terrain->update(camera, o.width, o.height, shot_now);
                if (glideslope::platform::stop_requested()) {
                    break; // told to stop while it waited: no frame, and no shot
                }
            }
            pass_part(pass_times.terrain);
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
            // The aeroplane itself, in every view but the cockpit - but not
            // where the AI flies it on a server and it is drawn from the
            // updates, as another.
            const bool own_drawn_as_other =
                online && joined &&
                std::any_of(others_now.begin(), others_now.end(),
                            [&](const glideslope::client::Other& other) {
                                return other.number == online->mine();
                            });
            if (flight && flight->model() != nullptr && o.draw_aircraft && !own_drawn_as_other &&
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
                if (own_moved && own_predicted) {
                    draw.placement.origin.x += own_moved->x;
                    draw.placement.origin.y += own_moved->y;
                    draw.placement.origin.z += own_moved->z;
                }
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
                    if (!mesh.made || moved || mesh.aircraft_id != model.id) {
                        if (mesh.made) {
                            renderer.remove_mesh(mesh.id);
                        }
                        mesh.id = renderer.add_mesh(
                            glideslope::gfx::mesh_from_model(model.visual->model, sun));
                        mesh.made = true;
                        mesh.lit_by = sun;
                        // What it was made from, which the shot says.
                        mesh.aircraft_id = model.id;
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
            // Where the help's columns end: above the credits drawn.
            int help_ends_at = o.height;
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
            pass_part(pass_times.draws);
            if (flight) {
                glideslope::gfx::HudReadings readings = flight->hud();
                if (showing_help) {
                    readings.help = help;
                }
                if (online && joined) {
                    // Who flies it is the server's to say - and what with,
                    // where it is the learnt landing.
                    readings.ai_flying = online->own_ai_flying();
                    if (online->own_learnt_landing()) {
                        readings.autopilot = "LEARNT LANDING";
                    }
                }
                if (ticks < notice_until) {
                    readings.notice = notice;
                }
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
                    // Its own, which the learnt landing flies, says so as
                    // the flight's own HUD would.
                    if (ridden->number == online->mine() && online->own_learnt_landing()) {
                        r.autopilot = "LEARNT LANDING";
                    }
                    r.notice = readings.notice;
                    if (const auto c = online->watched_controls(seconds_since_start())) {
                        glideslope::gfx::ControlsShown shown;
                        shown.aileron = c->aileron;
                        shown.elevator = c->elevator;
                        shown.rudder = c->rudder;
                        shown.throttle = c->throttle;
                        shown.flaps = c->flaps;
                        shown.gear = c->gear;
                        shown.speedbrake = c->speedbrake;
                        r.controls = shown;
                    }
                    readings = r;
                    if (shot_now || said_the_view) {
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
                if (said_the_view && !ridden) {
                    std::printf("glideslope: the view is the flight's own, not drawn from "
                                "the updates\n");
                }
                if (switched_now) {
                    std::printf("glideslope: the server says %s has aircraft %u\n",
                                online->own_ai_flying() ? "the AI" : "the pilot",
                                static_cast<unsigned>(online->mine()));
                    for (const std::string& line : glideslope::gfx::hud_lines(readings)) {
                        std::printf("glideslope: the HUD reads %s\n", line.c_str());
                    }
                }
                // The learnt landing given or refused: what the HUD says of it.
                if (say_the_hud) {
                    say_the_hud = false;
                    std::printf("glideslope: the HUD, of the learnt landing:\n");
                    for (const std::string& line : glideslope::gfx::hud_lines(readings)) {
                        std::printf("glideslope: the HUD reads %s\n", line.c_str());
                    }
                }
                if (!ridden && shot_now && online && joined) {
                    std::printf("glideslope: flying aircraft %u, the %s; the server says %s "
                                "has it%s\n",
                                static_cast<unsigned>(joined->number),
                                flight->aircraft().id.c_str(),
                                online->own_ai_flying() ? "the AI" : "the pilot",
                                !online->flown_since_taken_over() ? ""
                                : online->had_by_joining_again()
                                    ? ", and has flown it by inputs sent since it joined again"
                                : online->taken_back()
                                    ? ", and has flown it by inputs sent since it was taken back"
                                    : ", and has flown it by inputs sent since it was taken over");
                    for (const std::string& line : glideslope::gfx::hud_lines(readings)) {
                        std::printf("glideslope: the HUD reads %s\n", line.c_str());
                    }
                }
                readings.credits.insert(readings.credits.begin(), credits.begin(),
                                        credits.end());
                help_ends_at = glideslope::gfx::help_bottom(readings.credits, o.width, o.height);
                {
                    // **The horizon line on the horizon drawn**: the sky - up
                    // from the ellipsoid under the eye - in the axes of the
                    // camera the frame is drawn from, in every view.
                    const glideslope::world::Geodetic eye =
                        glideslope::world::to_geodetic(camera.position);
                    constexpr double radians = 3.14159265358979323846 / 180.0;
                    const glideslope::world::Ecef up_there{
                        std::cos(eye.latitude_deg * radians) * std::cos(eye.longitude_deg * radians),
                        std::cos(eye.latitude_deg * radians) * std::sin(eye.longitude_deg * radians),
                        std::sin(eye.latitude_deg * radians)};
                    const glideslope::world::Ecef in_camera =
                        glideslope::gfx::transpose(camera.world_from_camera) * up_there;
                    readings.sky_in_camera = std::array<double, 3>{in_camera.x, in_camera.y,
                                                                   in_camera.z};
                    readings.vertical_fov_rad = camera.vertical_fov_rad;
                }
                const glideslope::gfx::HorizonLine drawn_horizon =
                    glideslope::gfx::hud_horizon(readings, o.width, o.height);
                if (shot_now) {
                    std::printf("glideslope: the HUD's horizon runs from %.3f,%.3f to "
                                "%.3f,%.3f\n",
                                drawn_horizon.x0, drawn_horizon.y0, drawn_horizon.x1,
                                drawn_horizon.y1);
                }
                const glideslope::gfx::Mesh hud =
                    glideslope::gfx::hud_mesh(readings, o.width, o.height);
                pass_part(pass_times.hud);
                renderer.render(camera, drawn, &hud, haze, background);
            } else if (!credits.empty()) {
                const glideslope::gfx::Mesh overlay =
                    glideslope::gfx::credits_mesh(credits, o.width, o.height);
                renderer.render(camera, drawn, &overlay, haze, background);
            } else {
                renderer.render(camera, drawn, nullptr, haze, background);
            }
            ++frames;
            pass_part(pass_times.render);
            pass_done();
            said_the_view = false;
            if (o.memory_every > 0 && (frames == 1 || frames % o.memory_every == 0)) {
                const auto held = glideslope::platform::memory_held_bytes();
                std::printf("glideslope: frame %ld, memory held %lld\n", frames,
                            held ? static_cast<long long>(*held) : -1LL);
                // Out now: a client that runs out of memory takes what is
                // buffered with it, and these say how it got there.
                std::fflush(stdout);
            }

            if (shot_now && online && joined && flight) {
                // **What it drew of the server's sky, and how its own was
                // flown**: for a test to read, and for anybody to believe.
                const glideslope::world::Ecef me = flight->model_placement().origin;
                // **As the model its mesh was made from**, not as the id it
                // was told: what is on screen.
                for (const glideslope::client::Other& other : others_now) {
                    const double away = std::hypot(other.centre.x - me.x, other.centre.y - me.y,
                                                   other.centre.z - me.z);
                    const auto mesh = other_meshes.find(other.number);
                    const std::string as =
                        mesh != other_meshes.end() && mesh->second.made
                            ? mesh->second.aircraft_id
                            : (other.aircraft_id.empty() ? std::string("(not yet said)")
                                                         : "(no model of the " +
                                                               other.aircraft_id + ")");
                    std::printf("glideslope: drew aircraft %u, the %s, %.0f m away%s\n",
                                static_cast<unsigned>(other.number), as.c_str(), away,
                                other.wrecked ? ", a wreck" : "");
                }
                std::printf("glideslope: predicted: %zu corrections, the worst %.3f m, "
                            "%zu too large to hide\n",
                            online->corrections(), online->worst_correction_m(),
                            online->snapped());
                std::printf("glideslope: the worst while its clocks' difference was learnt "
                            "%.3f m, and once it was known %.3f m\n",
                            online->worst_learning_m(), online->worst_known_m());
                std::printf("glideslope: heard %zu words on its own aircraft over %.2f s of "
                            "the server's time, in %zu frames\n",
                            online->own_words_heard(), online->own_words_span_s(),
                            online->frames_that_heard_own());
                // **How far what it showed of its own stepped**, measured as
                // glideslope_cli measures it (frontend/shown.hpp).
                std::printf("glideslope: own aircraft: %zu switches; the largest step at a "
                            "switch %.3f m, and otherwise %.3f m\n",
                            own_shown.switches(), own_shown.worst_step_at_switch_m(),
                            own_shown.worst_step_otherwise_m());
                if (!own_shown.worst_step_what().empty()) {
                    std::printf("glideslope: the largest step at a switch: %s\n",
                                own_shown.worst_step_what().c_str());
                }
                if (!own_shown.worst_step_otherwise_what().empty()) {
                    std::printf("glideslope: the largest step otherwise: %s\n",
                                own_shown.worst_step_otherwise_what().c_str());
                }
                // **How long its frames were there**: the bounds on those
                // steps are claimed at 20 fps and above, and a test asserts
                // it before it believes one.
                std::printf("glideslope: own aircraft's frames: the longest within four "
                            "of a switch %.0f ms, and the largest step otherwise in one "
                            "%.0f ms long\n",
                            own_shown.longest_frame_at_switch_ms(),
                            own_shown.worst_step_otherwise_frame_ms());
                std::printf("glideslope: own aircraft's longest frame %.0f ms\n",
                            own_shown.longest_frame_ms());
                const PassTimes& l = longest_pass_at_switch;
                std::printf("glideslope: the longest pass around a switch %.1f ms: ticks %.1f, "
                            "hearing %.1f, scene %.1f, terrain %.1f, sky and draws %.1f, HUD "
                            "%.1f, render %.1f\n",
                            l.total, l.ticks, l.hear, l.scene, l.terrain, l.draws, l.hud,
                            l.render);
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
                // **What the help says, column by column**, so a test can
                // read it back off the frame.
                if (flight && showing_help) {
                    std::size_t drawn_lines = 0;
                    for (const glideslope::gfx::HelpColumn& column :
                         glideslope::gfx::help_columns(help, o.width, help_ends_at)) {
                        std::printf("help column %d %d %zu\n", column.layout.left,
                                    column.layout.top, column.lines.size());
                        for (const std::string& line : column.lines) {
                            std::printf("help: %s\n", line.c_str());
                        }
                        drawn_lines += column.lines.size();
                    }
                    std::printf("glideslope: the help drew %zu lines of %zu\n", drawn_lines,
                                help.size());
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
        // **How long the session was kept while this thread was away** -
        // building a flight, or in a long pass - for a test to read.
        if (online) {
            std::printf("glideslope: the session was kept for the frame loop %zu times; "
                        "away %.1f s at the longest\n",
                        online->times_kept(),
                        std::max(longest_away_s, online->longest_kept_since_asked_s()));
        }
        // **Leaving a session says goodbye**, so that the server lets this
        // client go now, not after its timeout of silence. Every other way
        // out of here says it too, as the session goes (net::ClientSession);
        // this one says so, for a test to read.
        if (online && online->standing().standing ==
                          glideslope::net::ClientSession::Standing::joined) {
            online->leave();
            std::printf("glideslope: said goodbye to the server\n");
            std::fflush(stdout);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glideslope: %s\n", e.what());
        status = 1;
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    if (glideslope::platform::stop_requested()) {
        return stopped();
    }
    return status;
}

int main(int argc, char** argv) {
    // The process ends with its C runtime whole until every other thread has
    // stopped - Windows' own threads too (platform/end_process.hpp).
    glideslope::platform::end_process(run_program(argc, argv));
}
