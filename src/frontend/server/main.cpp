// glideslope_server - the server, which owns every aircraft.
//
// **What is not here yet.** The server accepts a handshake, gives the client
// a slot and opens what it seals - but nothing is done with what is opened:
// no input reaches an aircraft and no state goes back. `--store` keeps
// nothing. So a client can connect and be counted, and cannot yet fly.
//
// **`--key` is the server's static X25519 secret.** Given, it is checked and
// used; not given, one is minted. Either way the public half is printed at
// startup, because a client cannot begin an `IK` handshake without it.

#include "copilot/copilot.hpp"
#include "frontend/briefs.hpp"
#include "frontend/players_copilot.hpp"
#include "frontend/same_air.hpp"
#include "frontend/server/dashboard.hpp"
#include "frontend/server/window.hpp"
#include "net/budget.hpp"
#include "net/handshake.hpp"
#include "platform/end_process.hpp"
#include "platform/closed_pipes.hpp"
#include "platform/no_crash_dialogs.hpp"
#include "net/inputs.hpp"
#include "net/inside.hpp"
#include "net/keys.hpp"
#include "net/messages.hpp"
#include "net/protocol.hpp"
#include "net/reliable.hpp"
#include "net/sealing.hpp"
#include "net/slots.hpp"
#include "net/state.hpp"
#include "copilot/planner.hpp"
#include "copilot/provider.hpp"
#include "platform/http.hpp"
#include "platform/paths.hpp"
#include "platform/socket.hpp"
#include "platform/store.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/crash.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/lander.hpp"
#include "sim/controller.hpp"
#include "sim/navigator.hpp"
#include "sim/separation.hpp"
#include "sim/fixed_step.hpp"
#include "sim/terrain.hpp"
#include "sim/version.hpp"
#include "world/dem.hpp"
#include "world/geodesy.hpp"
#include "world/download.hpp"
#include "world/runway_ground.hpp"
#include "world/runways.hpp"
#include "world/metar.hpp"
#include "world/weather.hpp"
#include "world/winds_aloft.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

namespace {

// The port the server listens on unless told otherwise. It is this project's
// own, high and unassigned; `docs/REQUIREMENTS.md` names no number.
constexpr std::uint16_t default_port = 47801;
// How many people may fly unless told otherwise.
constexpr int default_players = 4;
// How long a client may be silent before it is let go, seconds.
constexpr double default_timeout_s = 10.0;
// **How often a station's weather is fetched again, and how long a new one
// blends in over**: the client's own, when it flies alone
// (client/flight.hpp) - fifteen minutes, as METARs come at most twice an
// hour, and five, so that no change in the wind is a step.
constexpr double default_weather_refresh_s = 15 * 60.0;
constexpr double default_weather_blend_s = 5 * 60.0;
// A server secret is 32 bytes, written as hexadecimal.
constexpr std::size_t key_hex_length = 64;

// What the server's secret is called in the store. One name, forever: change
// it and every kept key is orphaned and a fresh one minted in its place.
constexpr const char* kept_key_name = "server-secret";
// How often the dashboard is drawn, and how often the settings line is
// repeated when there is no dashboard.
constexpr double dashboard_every_s = 1.0;
// And how often the window is drawn, which answers a click.
constexpr double window_every_s = 0.1;
// The most aircraft a server flies, which is a session's four players.
constexpr std::size_t most_flown = 4;
// How many AI aircraft a server runs unless told otherwise
// (CLAUDE.md: "How many AI aircraft a server runs is a server setting,
// default 4").
constexpr int default_ai = 4;
// The most it will run, so that a number typed wrong cannot ask for
// thousands of flight models.
constexpr int most_ai = 16;
// How far apart the AI aircraft are stacked when they fly one plan, feet:
// twice the vertical minimum they are kept apart by (sim/separation.hpp), so
// that two holding their heights are nowhere near it.
constexpr double ai_stack_ft = glideslope::sim::Separation::layer_ft;

// **How far apart in time AI aircraft planned to take off go**, simulated
// seconds, unless the server is told otherwise (`--ai-spacing`). Two planned
// by different models may well choose the same runway - the CBD orbit's
// recordings both do - and two standing on one threshold collide before
// either has moved. The second is not in the sky until then.
constexpr double default_departure_spacing_s = 90.0;

// An aircraft the server is to fly, and where it starts.
struct Flown {
    std::string id;
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double heading_deg = 0.0; // true; north unless given
};

// **Who plans an AI aircraft's flight**: a language model, asked with the
// server owner's own key, or nobody - and then it flies the plan file.
struct Planner {
    std::string provider; // "openai" or "anthropic"; empty: none
    std::string model;    // empty: the provider's own default
    // A recording to play the answers back from instead of asking, for tests.
    std::filesystem::path playback;
    // Or a file to keep what was asked and answered in (`--hand-over-record`).
    std::filesystem::path record;
};

struct Options {
    int players = default_players;
    std::filesystem::path data;
    std::vector<Flown> fly;
    std::string on_final; // AIRPORT/RUNWAY, or empty
    std::string ai_on_final; // AIRPORT/RUNWAY, or empty
    int ai = default_ai;
    std::filesystem::path plan;
    // Which AI aircraft, numbered from 1, are planned by a model, and by which;
    // and the task each is given to plan.
    std::map<int, Planner> planners;
    std::filesystem::path task;
    // **Who plans an aircraft the AI is given in the air** - one left by a
    // player who goes (`--on-leave ai`) or by a take-over - with the
    // server's key (`--hand-over-planner`): none unless said.
    Planner hand_over_planner;
    // Simulated seconds between one planned AI aircraft's departure and the
    // next's.
    double ai_spacing_s = default_departure_spacing_s;
    // **A failure to give** (`--fail-engine-at S`): at S seconds on the
    // simulation's clock every player's aircraft flying loses its first
    // engine - what a player's copilot is tested gliding from. Below nought:
    // never.
    double fail_engine_at_s = -1.0;
    // For a test: take this many steps as fast as they go, with nobody
    // joining, and stop - simulated time, not the machine's.
    long long steps = 0;
    // How long to run before stopping, or nothing to run until killed.
    double seconds = 0.0;
    std::uint16_t port = default_port;
    std::filesystem::path store;
    std::string key_hex;
    double timeout_s = default_timeout_s;
    bool headless = false;
    bool dry_run = false;
    bool plain = false;
    // The dashboard in a window instead of the terminal, and the window's
    // test flags.
    bool window = false;
    // Stop once everybody who joined has gone, for tests.
    bool until_empty = false;
    // Written once the aircraft are flying, for a test that joins late.
    std::string ready_file;
    bool window_dump = false;
    std::string window_shot;
    std::string window_press;
    // A test's operator (`--drop-once-flown`): drops the first player whose
    // input it has flown, as the window's drop button would.
    bool drop_once_flown = false;
    // For a test: the operator's drop sends no goodbye, as though every copy
    // of it were lost on the way.
    bool lose_goodbyes = false;
    // For a test: stop, telling nobody, once a player's input has been
    // flown and the clock is this far on - a server that falls over or is
    // restarted under its players; below nought, never.
    double stop_once_flown_s = -1.0;
    // What becomes of an aircraft when the person flying it goes.
    bool hand_to_ai_on_leave = false;
    // For a test: the least wall time each step takes, so that a server can
    // be put behind real time on any machine, as a loaded runner puts one.
    double test_step_ms = 0.0;
    // For a test: how fast the simulation's clock runs against real time,
    // set - a server behind real time by a fixed factor, the same on every
    // machine fast enough to keep up with it, where one put behind by
    // sleeping (`test_step_ms`) is as far behind as the machine is slow.
    double test_pace = 1.0;
    // Whether a player may take over an aircraft the AI is flying.
    bool take_over = true;
    // **The weather the server flies, and sends every client** (REQUIREMENTS
    // 6.3): a station's, fetched and fetched again (`--weather`), or a METAR
    // given whole, observed at `--station` (`--metar`), or none - still air.
    std::string weather_station;
    std::string metar;
    std::optional<std::array<double, 3>> station; // latitude, longitude, metres
    // For a test: at `metar_then_s` on the session's clock, this METAR
    // instead, as a fetch again would bring (`--metar-then S REPORT`).
    double metar_then_s = -1.0;
    std::string metar_then;
    // For a test: written once that METAR has taken over, so that a client
    // can join while it blends in (`--changed-file FILE`).
    std::string changed_file;
    double weather_blend_s = default_weather_blend_s;
    double weather_refresh_s = default_weather_refresh_s;
};

void print_usage(std::FILE* out) {
    std::fputs(
        "usage: glideslope_server [OPTION]...\n"
        "\n"
        "  --players N        how many people may fly, 1 to 4 (default 4)\n"
        "  --port N           the UDP port to listen on (default 47801). 0 asks\n"
        "                     the system for any free port, and the server prints\n"
        "                     the one it got, which is what a test wants\n"
        "  --store FILE       an SQLite file where the server's key is kept, so\n"
        "                     that it survives a restart. Made if absent.\n"
        "  --key HEX          the server's secret, 64 hexadecimal characters. Without\n"
        "                     one a fresh key is minted; either way the public half is\n"
        "                     printed at startup, which is what a client needs\n"
        "  --timeout SECONDS  how long a client may be silent (default 10)\n"
        "  --headless         no dashboard; print the settings and run\n"
        "  --data DIR         read data from DIR instead of data/ beside the program\n"
        "  --fly ID@LAT,LON[,HEADING]  fly this aircraft from here, pointing north\n"
        "                     or HEADING; may be given up to four\n"
        "                     times, and the server flies them all at 120 Hz over\n"
        "                     the collision terrain wherever on Earth they are\n"
        "  --players-on-final AIRPORT/RUNWAY  each player starts on the final\n"
        "                     approach to that runway end - 'YSSY/16R' - trimmed at\n"
        "                     the aeroplane's approach speed with the landing flap:\n"
        "                     the first two miles out, the gate the learnt landing\n"
        "                     is offered at, and each after half a mile further\n"
        "  --ai-on-final AIRPORT/RUNWAY  the first AI aircraft flying the plan file\n"
        "                     starts on final to that runway end instead, three\n"
        "                     miles out, and is landed: handed to the learnt landing\n"
        "                     at its gate if its aeroplane has one, or else by the\n"
        "                     approach autopilot. The rest fly the plan\n"
        "  --ai N             how many AI aircraft the server runs (default 4)\n"
        "  --plan FILE        the flight plan they fly (default plans/ in the data)\n"
        "  --ai-planner N=WHO[:MODEL]  AI aircraft N, from 1, is planned by WHO:\n"
        "                     'anthropic' or 'openai', asked with the server's own\n"
        "                     key, or 'none', the plan file. A model with no key is\n"
        "                     refused, said, and the aircraft flies the plan file.\n"
        "                     May be given once for each AI aircraft\n"
        "  --hand-over-planner WHO[:MODEL]  an aircraft the AI is given in the\n"
        "                     air - left by a player who goes (--on-leave ai) or\n"
        "                     by a take-over - is planned from where it is by\n"
        "                     WHO, 'anthropic' or 'openai', asked with the\n"
        "                     server's own key, or 'none' (the default): it flies\n"
        "                     the plan file, left by a player, or holds its\n"
        "                     course, left by a take-over. A model with no key is\n"
        "                     refused, said, and it flies as with none\n"
        "  --hand-over-playback FILE  that model is not asked: its answers are\n"
        "                     played back from FILE, a recording, for tests\n"
        "  --hand-over-record FILE  what that model is asked and answers is kept\n"
        "                     in FILE, a recording\n"
        "  --ai-task FILE     what the models are asked to plan (default\n"
        "                     tasks/sydney-cbd-orbit.task in the data)\n"
        "  --ai-playback N=FILE  AI aircraft N's model is not asked: its answers\n"
        "                     are played back from FILE, a recording, for tests\n"
        "  --fail-engine-at S at S simulated seconds, every player's aircraft\n"
        "                     flying loses its first engine (for tests)\n"
        "  --ai-spacing S     simulated seconds between one planned AI aircraft's\n"
        "                     take-off and the next's (default 90)\n"
        "  --weather STATION  fly the weather now at STATION, an airfield's four\n"
        "                     letters: its METAR and the forecast above it, fetched\n"
        "                     again every --weather-refresh seconds (default 900)\n"
        "                     and blended in over --weather-blend (default 300).\n"
        "                     Every client is sent it, and flies it\n"
        "  --metar REPORT     fly the weather this METAR reports, observed on the\n"
        "                     ground at --station LAT,LON[,METRES] (by default where\n"
        "                     the AI's plan starts, at sea level)\n"
        "  --metar-then S REPORT  with --metar: at S seconds on the session's clock,\n"
        "                     this METAR instead, as a fetch again would bring\n"
        "                     (for tests). Without --weather or --metar, still air\n"
        "  --changed-file FILE  write FILE once --metar-then's METAR has taken\n"
        "                     over - for a test that joins while it blends in\n"
        "  --seconds N        stop after N seconds instead of running until killed\n"
        "  --until-empty      stop once every client that joined has gone and been\n"
        "                     let go - for a test, which then waits on its clients\n"
        "                     rather than on the machine's speed\n"
        "  --ready-file FILE  write FILE once the aircraft are built and flying -\n"
        "                     for a test that must join a session under way\n"
        "  --no-take-over     players may not take over an AI's aircraft (they\n"
        "                     may, unless told this)\n"
        "  --on-leave WHAT    what becomes of an aircraft when the person flying\n"
        "                     it goes: 'remove' takes it out of the sky (the\n"
        "                     default), 'ai' hands it to an AI pilot flying the\n"
        "                     server's plan\n"
        "  --plain            draw the dashboard as plain text, without the escape\n"
        "                     codes that clear the screen, so that a test can read\n"
        "                     it. Not with --headless, which has no dashboard\n"
        "  --window           draw the dashboard in a window instead of the\n"
        "                     terminal, with a button to drop each player; closing\n"
        "                     it stops the server. Needs a display: without this\n"
        "                     the server needs none, as a host in the cloud has\n"
        "  --window-dump      with --window: at the end, print what the window\n"
        "                     drew and what the terminal would have, for tests\n"
        "  --window-shot FILE with --window: at the end, write its last frame as a\n"
        "                     BMP\n"
        "  --window-press LABEL  with --window: press this button - 'drop 0' - the\n"
        "                     first time it is drawn, for tests\n"
        "  --drop-once-flown  drop the first player whose input has been flown, as\n"
        "                     the drop button would, for tests\n"
        "  --lose-goodbyes    send no goodbye at a drop, as though every copy were\n"
        "                     lost on the way, for tests\n"
        "  --stop-once-flown S  stop, telling nobody, once a player's input has\n"
        "                     been flown and S seconds of the simulation have gone,\n"
        "                     as a server restarted under its players, for tests\n"
        "  --steps N          take N steps as fast as they go, with nobody joining,\n"
        "                     then stop - simulated time, for a test\n"
        "  --test-step-ms MS  make every step take at least MS milliseconds, so\n"
        "                     that a test can put the server behind real time\n"
        "  --test-pace F      run the simulation's clock at F (0 to 1) of real\n"
        "                     time, a server behind by a set factor, for tests\n"
        "  --dry-run          print the settings and exit without binding\n"
        "  --version          print the version\n"
        "  --help             print this\n"
        "\n"
        "A fifth player is refused: the session is full at --players.\n",
        out);
}

// What is wrong with an option, in words a person can act on, or empty if
// nothing is.
std::string wrong_with(const Options& o) {
    if (o.players < 1 || o.players > 4) {
        return "--players is " + std::to_string(o.players) +
               ", and a session is 1 to 4 players";
    }
    if (o.fly.size() > most_flown) {
        return "--fly was given " + std::to_string(o.fly.size()) +
               " times, and a session holds at most " + std::to_string(most_flown) +
               " aircraft";
    }
    if (o.ai < 0 || o.ai > most_ai) {
        return "--ai is " + std::to_string(o.ai) + ", and a server runs 0 to " +
               std::to_string(most_ai) + " AI aircraft";
    }
    for (const auto& [n, p] : o.planners) {
        if (n < 1 || n > o.ai) {
            return "--ai-planner names AI aircraft " + std::to_string(n) + ", and there are " +
                   std::to_string(o.ai) + ", numbered from 1";
        }
        if (p.provider.empty() && !p.playback.empty()) {
            return "--ai-playback " + std::to_string(n) +
                   " plays back a model's answers, and --ai-planner gives that aircraft none";
        }
    }
    if (!o.task.empty() && std::none_of(o.planners.begin(), o.planners.end(), [](const auto& p) {
            return !p.second.provider.empty();
        })) {
        return "--ai-task is what a model is asked to plan, and --ai-planner gives no AI "
               "aircraft a model";
    }
    if (o.hand_over_planner.provider.empty() &&
        (!o.hand_over_planner.playback.empty() || !o.hand_over_planner.record.empty())) {
        return "--hand-over-playback and --hand-over-record are a model's answers, and "
               "--hand-over-planner gives none";
    }
    if (!o.hand_over_planner.provider.empty() && !o.hand_to_ai_on_leave && !o.take_over) {
        return "--hand-over-planner plans an aircraft left to the AI by a player who goes "
               "(--on-leave ai) or by a take-over, and this server allows neither";
    }
    if (!o.hand_over_planner.playback.empty() && !o.hand_over_planner.record.empty()) {
        return "--hand-over-playback asks nobody, so there is nothing for --hand-over-record "
               "to keep";
    }
    if (!std::isfinite(o.ai_spacing_s) || o.ai_spacing_s < 0.0) {
        return "--ai-spacing is " + std::to_string(o.ai_spacing_s) +
               ", and a length of time is a number, not negative";
    }
    if (!o.weather_station.empty() && !o.metar.empty()) {
        return "--weather fetches a station's METAR and --metar gives one; the server "
               "flies one weather";
    }
    if (o.station && o.metar.empty()) {
        return "--station says where a --metar is observed";
    }
    if (!o.metar_then.empty() && o.metar.empty()) {
        return "--metar-then changes a --metar, and there is none";
    }
    if (o.weather_refresh_s <= 0.0 && !o.weather_station.empty()) {
        return "--weather-refresh is how often the weather is fetched again, and it is "
               "more than nought seconds";
    }
    if (o.steps < 0) {
        return "--steps is " + std::to_string(o.steps) + ", and a number of steps is not negative";
    }
    if (o.steps > 0 && (o.seconds > 0.0 || o.until_empty || o.window)) {
        return "--steps runs with nobody joining and stops, so --seconds, --until-empty "
               "and --window say nothing with it";
    }
    if (o.window && o.headless) {
        return "--window draws the dashboard in a window and --headless has none, "
               "so the two together say nothing";
    }
    if (o.window && o.plain) {
        return "--plain is how the terminal's dashboard is drawn, and --window draws "
               "it in a window instead";
    }
    if (o.lose_goodbyes && !o.drop_once_flown) {
        return "--lose-goodbyes loses the goodbyes of --drop-once-flown's drop, and there is "
               "none without it";
    }
    if (!o.window && (o.window_dump || !o.window_shot.empty() || !o.window_press.empty())) {
        return "--window-dump, --window-shot and --window-press are about the window, "
               "and there is none without --window";
    }
    if (o.plain && o.headless) {
        return "--plain draws the dashboard and --headless has none, so the two "
               "together say nothing";
    }
    if (o.seconds < 0.0) {
        return "--seconds is " + std::to_string(o.seconds) +
               ", and a length of time is not negative";
    }
    if (o.timeout_s <= 0.0) {
        return "--timeout is " + std::to_string(o.timeout_s) +
               " seconds, and it must be more than nothing";
    }
    if (!o.key_hex.empty()) {
        if (o.key_hex.size() != key_hex_length) {
            return "--key is " + std::to_string(o.key_hex.size()) +
                   " characters, and a server secret is " +
                   std::to_string(key_hex_length);
        }
        for (const char c : o.key_hex) {
            if (std::isxdigit(static_cast<unsigned char>(c)) == 0) {
                return std::string("--key has '") + c +
                       "' in it, which is not a hexadecimal digit";
            }
        }
    }
    return {};
}

// Reads a whole number, or nothing if the text is not one.
std::optional<long long> whole(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::string s(text);
    char* end = nullptr;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return v;
}

std::optional<double> number(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::string s(text);
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return v;
}

// Reads the options. `why` says what was wrong when it answers nothing.
std::optional<Options> parse(const std::vector<std::string_view>& args,
                             std::string& why) {
    Options o;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view a = args[i];
        const auto next = [&](std::string_view& out) {
            if (i + 1 >= args.size()) {
                why = std::string(a) + " wants a value after it";
                return false;
            }
            out = args[++i];
            return true;
        };
        std::string_view value;
        if (a == "--headless") {
            o.headless = true;
        } else if (a == "--drop-once-flown") {
            o.drop_once_flown = true;
        } else if (a == "--lose-goodbyes") {
            o.lose_goodbyes = true;
        } else if (a == "--stop-once-flown") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n || !(*n >= 0.0)) {
                why = "--stop-once-flown wants a number of seconds, not '" + std::string(value) +
                      "'";
                return std::nullopt;
            }
            o.stop_once_flown_s = *n;
        } else if (a == "--window") {
            o.window = true;
        } else if (a == "--until-empty") {
            o.until_empty = true;
        } else if (a == "--ready-file") {
            if (!next(value)) return std::nullopt;
            o.ready_file = std::string(value);
        } else if (a == "--changed-file") {
            if (!next(value)) return std::nullopt;
            o.changed_file = std::string(value);
        } else if (a == "--test-step-ms") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n || *n < 0.0) {
                why = "--test-step-ms wants a number of milliseconds, not '" +
                      std::string(value) + "'";
                return std::nullopt;
            }
            o.test_step_ms = *n;
        } else if (a == "--test-pace") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n || !(*n > 0.0) || *n > 1.0) {
                why = "--test-pace wants a fraction of real time above 0 and at most 1, not '" +
                      std::string(value) + "'";
                return std::nullopt;
            }
            o.test_pace = *n;
        } else if (a == "--window-dump") {
            o.window_dump = true;
        } else if (a == "--window-shot") {
            if (!next(value)) return std::nullopt;
            o.window_shot = std::string(value);
        } else if (a == "--window-press") {
            if (!next(value)) return std::nullopt;
            o.window_press = std::string(value);
        } else if (a == "--dry-run") {
            o.dry_run = true;
        } else if (a == "--no-take-over") {
            o.take_over = false;
        } else if (a == "--on-leave") {
            if (!next(value)) return std::nullopt;
            if (value == "remove") {
                o.hand_to_ai_on_leave = false;
            } else if (value == "ai") {
                o.hand_to_ai_on_leave = true;
            } else {
                why = "--on-leave wants 'remove' or 'ai', not '" +
                      std::string(value) + "'";
                return std::nullopt;
            }
        } else if (a == "--plain") {
            o.plain = true;
        } else if (a == "--players") {
            if (!next(value)) return std::nullopt;
            const auto n = whole(value);
            if (!n) {
                why = "--players wants a whole number, not '" + std::string(value) + "'";
                return std::nullopt;
            }
            o.players = static_cast<int>(std::clamp<long long>(*n, -1000, 1000));
        } else if (a == "--port") {
            if (!next(value)) return std::nullopt;
            const auto n = whole(value);
            if (!n || *n < 0 || *n > 65535) {
                why = "--port wants a number from 0 to 65535, not '" +
                      std::string(value) + "'";
                return std::nullopt;
            }
            o.port = static_cast<std::uint16_t>(*n);
        } else if (a == "--store") {
            if (!next(value)) return std::nullopt;
            o.store = std::filesystem::path(std::string(value));
        } else if (a == "--key") {
            if (!next(value)) return std::nullopt;
            o.key_hex = std::string(value);
        } else if (a == "--data") {
            if (!next(value)) return std::nullopt;
            o.data = std::filesystem::path(std::string(value));
        } else if (a == "--ai") {
            if (!next(value)) return std::nullopt;
            const auto n = whole(value);
            if (!n) {
                why = "--ai wants a whole number, not '" + std::string(value) + "'";
                return std::nullopt;
            }
            o.ai = static_cast<int>(std::clamp<long long>(*n, -1000, 1000));
        } else if (a == "--plan") {
            if (!next(value)) return std::nullopt;
            o.plan = std::filesystem::path(std::string(value));
        } else if (a == "--ai-planner" || a == "--ai-playback") {
            if (!next(value)) return std::nullopt;
            const std::string spec(value);
            const std::size_t equals = spec.find('=');
            const auto n = whole(spec.substr(0, equals == std::string::npos ? 0 : equals));
            if (!n || equals + 1 >= spec.size()) {
                why = std::string(a) + " wants N=" +
                      (a == "--ai-planner" ? "WHO[:MODEL]" : "FILE") + ", not '" + spec + "'";
                return std::nullopt;
            }
            const int number = static_cast<int>(std::clamp<long long>(*n, -1000, 1000));
            const std::string what = spec.substr(equals + 1);
            if (a == "--ai-playback") {
                o.planners[number].playback = std::filesystem::path(what);
            } else {
                const std::size_t colon = what.find(':');
                const std::string who = what.substr(0, colon);
                if (who != "anthropic" && who != "openai" && who != "none") {
                    why = "--ai-planner wants anthropic, openai or none, not '" + who + "'";
                    return std::nullopt;
                }
                Planner& p = o.planners[number];
                p.provider = who == "none" ? std::string() : who;
                p.model = colon == std::string::npos ? std::string() : what.substr(colon + 1);
            }
        } else if (a == "--hand-over-planner") {
            if (!next(value)) return std::nullopt;
            try {
                const glideslope::frontend::HandOverModel m =
                    glideslope::frontend::read_hand_over_model(value);
                o.hand_over_planner.provider = m.provider;
                o.hand_over_planner.model = m.model;
            } catch (const std::invalid_argument& e) {
                why = std::string("--hand-over-planner: ") + e.what();
                return std::nullopt;
            }
        } else if (a == "--hand-over-playback") {
            if (!next(value)) return std::nullopt;
            o.hand_over_planner.playback = std::filesystem::path(std::string(value));
        } else if (a == "--hand-over-record") {
            if (!next(value)) return std::nullopt;
            o.hand_over_planner.record = std::filesystem::path(std::string(value));
        } else if (a == "--ai-task") {
            if (!next(value)) return std::nullopt;
            o.task = std::filesystem::path(std::string(value));
        } else if (a == "--weather") {
            if (!next(value)) return std::nullopt;
            o.weather_station = std::string(value);
            if (o.weather_station.size() != 4 ||
                !std::all_of(o.weather_station.begin(), o.weather_station.end(), [](char c) {
                    return std::isalnum(static_cast<unsigned char>(c)) != 0;
                })) {
                why = "--weather wants a station's four letters, not '" + std::string(value) + "'";
                return std::nullopt;
            }
        } else if (a == "--metar" || a == "--metar-then") {
            if (a == "--metar-then") {
                if (!next(value)) return std::nullopt;
                const auto n = number(value);
                if (!n || !(*n >= 0.0)) {
                    why = "--metar-then wants a number of seconds, not '" + std::string(value) + "'";
                    return std::nullopt;
                }
                o.metar_then_s = *n;
            }
            if (!next(value)) return std::nullopt;
            try {
                (void)glideslope::world::parse_metar(value);
            } catch (const glideslope::world::MetarError& e) {
                why = std::string(a) + " is not a METAR: " + e.what();
                return std::nullopt;
            }
            (a == "--metar" ? o.metar : o.metar_then) = std::string(value);
        } else if (a == "--station") {
            if (!next(value)) return std::nullopt;
            std::array<double, 3> at{0.0, 0.0, 0.0};
            std::string rest(value);
            std::size_t got = 0;
            for (; got < 3 && !rest.empty(); ++got) {
                const std::size_t comma = rest.find(',');
                const auto n = number(rest.substr(0, comma));
                if (!n) {
                    got = 0;
                    break;
                }
                at[got] = *n;
                rest = comma == std::string::npos ? "" : rest.substr(comma + 1);
            }
            if (got < 2 || !rest.empty() || std::abs(at[0]) > 90.0 || std::abs(at[1]) > 180.0) {
                why = "--station wants LAT,LON or LAT,LON,METRES, not '" + std::string(value) + "'";
                return std::nullopt;
            }
            o.station = at;
        } else if (a == "--weather-blend" || a == "--weather-refresh") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n || !(*n >= 0.0) || !std::isfinite(*n)) {
                why = std::string(a) + " wants a number of seconds, not '" + std::string(value) + "'";
                return std::nullopt;
            }
            (a == "--weather-blend" ? o.weather_blend_s : o.weather_refresh_s) = *n;
        } else if (a == "--fail-engine-at") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n || !(*n >= 0.0)) {
                why = "--fail-engine-at wants a number of seconds, not '" + std::string(value) + "'";
                return std::nullopt;
            }
            o.fail_engine_at_s = *n;
        } else if (a == "--ai-spacing") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n) {
                why = "--ai-spacing wants a number of seconds, not '" + std::string(value) + "'";
                return std::nullopt;
            }
            o.ai_spacing_s = *n;
        } else if (a == "--steps") {
            if (!next(value)) return std::nullopt;
            const auto n = whole(value);
            if (!n) {
                why = "--steps wants a whole number, not '" + std::string(value) + "'";
                return std::nullopt;
            }
            o.steps = *n;
        } else if (a == "--seconds") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n) {
                why = "--seconds wants a number of seconds, not '" +
                      std::string(value) + "'";
                return std::nullopt;
            }
            o.seconds = *n;
        } else if (a == "--players-on-final") {
            if (!next(value)) return std::nullopt;
            const std::string spec(value);
            const std::size_t slash = spec.find('/');
            if (slash == std::string::npos || slash == 0 || slash + 1 == spec.size()) {
                why = "--players-on-final wants AIRPORT/RUNWAY, 'YSSY/16R', not '" + spec + "'";
                return std::nullopt;
            }
            o.on_final = spec;
        } else if (a == "--ai-on-final") {
            if (!next(value)) return std::nullopt;
            const std::string spec(value);
            const std::size_t slash = spec.find('/');
            if (slash == std::string::npos || slash == 0 || slash + 1 == spec.size()) {
                why = "--ai-on-final wants AIRPORT/RUNWAY, 'YSSY/16R', not '" + spec + "'";
                return std::nullopt;
            }
            o.ai_on_final = spec;
        } else if (a == "--fly") {
            if (!next(value)) return std::nullopt;
            // ID@LAT,LON[,HEADING] - the aircraft, where it starts, and which
            // way it points: north unless told.
            const std::string spec(value);
            const std::size_t at = spec.find('@');
            const std::size_t comma = spec.find(',', at == std::string::npos ? 0 : at);
            if (at == std::string::npos || comma == std::string::npos || at == 0) {
                why = "--fly wants ID@LAT,LON[,HEADING], not '" + spec + "'";
                return std::nullopt;
            }
            Flown f;
            f.id = spec.substr(0, at);
            const std::size_t third = spec.find(',', comma + 1);
            const auto lat = number(spec.substr(at + 1, comma - at - 1));
            const auto lon = number(spec.substr(
                comma + 1, third == std::string::npos ? std::string::npos : third - comma - 1));
            if (third != std::string::npos) {
                const auto heading = number(spec.substr(third + 1));
                if (!heading || *heading < 0.0 || *heading >= 360.0) {
                    why = "--fly wants a heading from 0 to 360, not '" + spec + "'";
                    return std::nullopt;
                }
                f.heading_deg = *heading;
            }
            if (!lat || !lon || *lat < -90.0 || *lat > 90.0 || *lon < -180.0 ||
                *lon > 180.0) {
                why = "--fly wants a latitude and longitude on Earth, not '" + spec + "'";
                return std::nullopt;
            }
            f.latitude_deg = *lat;
            f.longitude_deg = *lon;
            o.fly.push_back(std::move(f));
        } else if (a == "--timeout") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n) {
                why = "--timeout wants a number of seconds, not '" +
                      std::string(value) + "'";
                return std::nullopt;
            }
            o.timeout_s = *n;
        } else {
            why = "there is no option '" + std::string(a) + "'";
            return std::nullopt;
        }
    }
    why = wrong_with(o);
    if (!why.empty()) {
        return std::nullopt;
    }
    return o;
}

// The settings, as one block, so that a person reading a log can see what the
// server was actually told.
void print_settings(const Options& o, std::FILE* out) {
    std::fprintf(out, "players   %d\n", o.players);
    std::fprintf(out, "port      %u\n", static_cast<unsigned>(o.port));
    std::fprintf(out, "store     %s\n",
                 o.store.empty() ? "(none: a fresh key at every start)"
                                 : o.store.string().c_str());
    std::fprintf(out, "key       %s\n",
                 o.key_hex.empty() ? "(none given: taken from the store, or minted)"
                                   : "given, 64 hexadecimal characters");
    std::fprintf(out, "timeout   %.3f s\n", o.timeout_s);
    std::fprintf(out, "on-leave  %s\n",
                 o.hand_to_ai_on_leave ? "hand the aircraft to an AI pilot"
                                       : "remove the aircraft");
    std::fprintf(out, "dashboard %s\n",
                 o.headless ? "no (--headless)"
                            : (o.window ? "in a window (--window)"
                                        : (o.plain ? "in the terminal, plain text (--plain)"
                                                   : "in the terminal")));
    std::fprintf(out, "seconds   %s\n",
                 o.seconds > 0.0 ? std::to_string(o.seconds).c_str()
                                 : "(until killed)");
    std::fprintf(out, "on final  %s\n",
                 o.on_final.empty() ? "(no: players start where the plan does)"
                                    : o.on_final.c_str());
    std::fprintf(out, "ai final  %s\n",
                 o.ai_on_final.empty() ? "(no: the AI flies its plan)" : o.ai_on_final.c_str());
    std::fprintf(out, "ai        %d aircraft\n", o.ai);
    std::fprintf(out, "plan      %s\n",
                 o.plan.empty() ? "(the data's plans/sydney-harbour.plan)"
                                : o.plan.string().c_str());
    for (int n = 1; n <= o.ai; ++n) {
        const auto p = o.planners.find(n);
        if (p == o.planners.end() || p->second.provider.empty()) {
            continue;
        }
        const std::string how = p->second.playback.empty()
                                    ? ", asked with the server's key"
                                    : ", played back from " + p->second.playback.string();
        std::fprintf(out, "planner   AI %d by %s%s%s%s\n", n, p->second.provider.c_str(),
                     p->second.model.empty() ? "" : ", ", p->second.model.c_str(), how.c_str());
    }
    if (std::any_of(o.planners.begin(), o.planners.end(),
                    [](const auto& p) { return !p.second.provider.empty(); })) {
        std::fprintf(out, "task      %s\n",
                     o.task.empty() ? "(the data's tasks/sydney-cbd-orbit.task)"
                                    : o.task.string().c_str());
        std::fprintf(out, "spacing   %.0f s between planned take-offs\n", o.ai_spacing_s);
    }
    if (!o.hand_over_planner.provider.empty()) {
        std::fprintf(out, "hand-over planned by %s%s%s%s\n", o.hand_over_planner.provider.c_str(),
                     o.hand_over_planner.model.empty() ? "" : ", ",
                     o.hand_over_planner.model.c_str(),
                     o.hand_over_planner.playback.empty()
                         ? ", asked with the server's key"
                         : (", played back from " + o.hand_over_planner.playback.string()).c_str());
    }
    if (o.fail_engine_at_s >= 0.0) {
        std::fprintf(out, "engines   every player's first engine stops %.0f s in\n",
                     o.fail_engine_at_s);
    }
    if (o.steps > 0) {
        std::fprintf(out, "steps     %lld, as fast as they go\n", o.steps);
    }
    if (o.fly.empty()) {
        std::fprintf(out, "flying    (nothing by hand)\n");
    }
    for (const Flown& f : o.fly) {
        std::fprintf(out, "flying    %s from %.6f, %.6f\n", f.id.c_str(),
                     f.latitude_deg, f.longitude_deg);
    }
}

// **Who is connected, and what is sealed to them.** One entry per address
// that has completed a handshake: the keys agreed, the slot given, and when
// it was last heard from, so that a silent client can be let go.
//
// **The address is how a datagram is matched to a session**, which is what
// every UDP protocol does and is why the sealing matters: an address is
// trivially forged, and what stops a forged datagram is that it does not
// open.
struct Connection {
    glideslope::net::PublicKey who;
    std::unique_ptr<glideslope::net::Sealer> sealing;
    std::unique_ptr<glideslope::net::Unsealer> opening;
    double last_heard_s = 0.0;
    // When it was admitted, on the server's clock: what a goodbye is measured
    // from.
    double admitted_s = 0.0;
    std::uint64_t datagrams = 0;
    // **How often it may send** (net/budget.hpp), and how much past it was
    // dropped or ignored - said when the session is let go.
    glideslope::net::Budget datagram_budget{glideslope::net::session_datagrams_per_second};
    glideslope::net::Budget request_budget{glideslope::net::session_requests_per_second};
    std::uint64_t datagrams_past_budget = 0;
    std::uint64_t requests_past_budget = 0;
    std::uint64_t requests = 0;
    // When its first and last requests were read, on the server's clock: a
    // second's worth may be taken at once, and the rate's worth of the time
    // between them more - what a test of the rate holds it to.
    double first_request_s = -1.0;
    double last_request_s = -1.0;
    // What the dashboard shows. Bytes are whole datagrams, envelope and all,
    // because that is what the link carries.
    std::uint64_t bytes_in = 0;
    std::uint64_t bytes_out = 0;
    // The round trip, in seconds, and nothing until one has come back. One
    // ping is outstanding at a time: a second would need a queue of tokens to
    // tell the answers apart, and one a second is enough to draw.
    double ping_s = -1.0;
    std::uint64_t token = 0;
    double token_sent_at_s = -1.0;
    double pinged_at_s = -1.0;
    // **The initiation this session came from, and the answer that was sent
    // back.** A client whose answer went missing sends the same initiation
    // again, and it must get the same answer: a second `Responder` would
    // agree different keys and leave the client sealing under the first set.
    // Kept whole rather than hashed - an initiation is 102 bytes, and a hash
    // would be a second thing to get right for nothing.
    std::vector<std::uint8_t> initiation;
    std::vector<std::uint8_t> answer;
    // **The aircraft this client flies**, by the server's number for it, or
    // `no_aircraft` if the server had none to give - which is what a server
    // with nothing to fly has, since it has loaded no terrain to put one over.
    std::uint8_t aircraft = glideslope::net::no_aircraft;
    // Their inputs, and how far through them the server has got. The number
    // goes back in every state update so the client knows what to reconcile.
    glideslope::net::InputReceiver inputs;
    std::uint32_t last_input_applied = 0;
    // **How many steps the aircraft has been flown on that input**, which
    // goes back with it: an input is flown from when it arrives until the next
    // does, and the client places the server's word on its own clock by it
    // (sim::Prediction).
    std::int64_t steps_into_input = 0;
    // **The newest input heard and not yet applied**, and its number. It is
    // applied after the steps taken in the pass it was read in (apply_input):
    // the time those stand for had passed before it arrived. Applied as it
    // was read, a server held up for 50 ms flew six steps of a new input
    // early, and put its clients off by two metres (2026-09-27). A pass takes
    // at most `most_steps_between_looks`, so a server further behind than
    // that still flies the rest of what it owes on the new input.
    std::optional<std::pair<std::uint32_t, glideslope::sim::Controls>> input_heard;
    // **What must arrive**: the reliable messages to this client, and what
    // each aircraft has been introduced to it as - its model, by number - so
    // that one is introduced once, and again if its number comes to mean
    // another aircraft.
    glideslope::net::Reliable reliable;
    std::map<std::uint8_t, std::pair<std::string, std::string>> introduced;
    // **The aircraft this client is watching** (`WATCH`), whose controls go
    // in its own state updates, or `no_aircraft`.
    std::uint8_t watching = glideslope::net::no_aircraft;
    // **Whether anything sealed under this session has opened yet.** Until
    // it has, the session proves nothing about who made it: anybody can
    // replay a captured initiation from any address, and the server answers
    // it with keys the replayer cannot use. The first datagram that opens
    // shows the sender holds the initiation's ephemeral secret - that it is
    // the client itself, now - and that is when a second session for a key
    // takes over from the first (`take`, the `sealed` arm). Until then it is
    // sent nothing but its handshake answer - no state, no pings, no reliable
    // messages - and nothing but a sealed datagram that opens counts as
    // hearing from it, so it is let go `--timeout` after it was admitted.
    bool proven = false;
    // **What this client has been told of the session** (REQUIREMENTS.md
    // 6.3): the terrain dataset and the session, once; the lobby as last
    // sent, sent again whenever it changes; and the newest weather it has
    // been sent, by the fleet's count of them: -1 before any.
    bool told_session = false;
    std::vector<std::uint8_t> lobby_told;
    int weathers_told = -1;
};

// **Every initiation that has made a session, remembered after the session
// has gone, with the address it came from**: the initiator's ephemeral key -
// its first 32 bytes, which an honest client never uses twice - and the
// address, as text.
//
// **Why.** A client resends its initiation every quarter of a second until it
// is answered, so a server slow to answer has several copies of it on the way.
// While the session they made is live, a copy gets that session's answer
// again. But a copy read after that session was let go used to be taken for a
// new handshake: a new session, a new aircraft, and an answer the client -
// holding the keys of the first - ignored. Nothing it sealed opened under the
// new keys, so the server heard nothing, let the new session go after
// `--timeout`, and counted a player's aircraft that nobody had flown. The
// four-player test on CI counted six (run 36140964489).
//
// **A copy from the address that already took it is now dropped in silence**,
// live session or not. **From any other address it is answered as it always
// was**, with a session its sender cannot read: dropped from every address, a
// copy injected from a spoofed address to arrive first would have kept the
// real client out, its own initiation and every resend dropped (review of
// PR #25). A client that means to start again makes a new initiation, with a
// new ephemeral key, and that is taken as ever.
//
// **What it costs**: a flat ring of `most_remembered` entries, 80 bytes each,
// and a `std::set` of the same keys, a tree node of about 112 bytes each - some
// 3 MiB when full, the oldest forgotten first. Forgetting one only brings back
// what a copy did before: a session nobody can read, from one more address.
class Taken {
public:
    static constexpr std::size_t most_remembered = 16384;
    static constexpr std::size_t address_bytes = 48; // "[v6 address]:port", padded
    using Key = std::array<std::uint8_t, glideslope::net::key_bytes + address_bytes>;

    // The key for `initiation` from `from`, or nothing if it is too short to
    // hold an ephemeral key.
    static std::optional<Key> key_of(std::span<const std::uint8_t> initiation,
                                     const std::string& from) {
        if (initiation.size() < glideslope::net::key_bytes) {
            return std::nullopt;
        }
        Key k{};
        std::copy_n(initiation.begin(), glideslope::net::key_bytes, k.begin());
        const std::size_t n = std::min(from.size(), address_bytes);
        for (std::size_t i = 0; i < n; ++i) {
            k[glideslope::net::key_bytes + i] = static_cast<std::uint8_t>(from[i]);
        }
        return k;
    }
    bool has(const Key& k) const { return set_.count(k) > 0; }
    void remember(const Key& k) {
        if (!set_.insert(k).second) {
            return;
        }
        if (ring_.size() < most_remembered) {
            ring_.push_back(k);
            return;
        }
        set_.erase(ring_[next_]);
        said_.erase(ring_[next_]);
        ring_[next_] = k;
        next_ = (next_ + 1) % most_remembered;
    }
    // **Whether a copy of `k` dropped is yet to be said**: true the first
    // time only. A client dropped by the operator goes on sending its
    // initiation, and saying every copy filled the dashboard's log with them
    // until the drop itself was pushed out of it.
    bool first_said(const Key& k) { return set_.count(k) > 0 && said_.insert(k).second; }

private:
    std::set<Key> set_;
    std::set<Key> said_;
    std::vector<Key> ring_;
    std::size_t next_ = 0;
};

// **How often the server says where everybody is: 25 times for every second
// the simulation flies.** `REQUIREMENTS.md` 6.6 asks for 20 to 30 Hz, and 25
// is the middle of it. An update goes out on the first step at or after each
// twenty-fifth of a simulated second - steps 5, 10, 15, 20, 24, 29, ... since
// 120 / 25 is 4.8 - so it is always a state the simulation reached, never one
// between two steps.
//
// **Counted in simulated time, not on the wall clock.** It used to be once
// per 1/25 s of wall clock, checked once per pass of the loop - and a pass
// took every step that had fallen due in one go. A debug server sharing a
// runner fell behind, its passes grew, and it said where everybody was nine
// times in three seconds (CI run 35857600253). A client cannot use an update
// the simulation has not moved on for anyway.
constexpr std::int64_t states_per_second = 25;

// **At most this many steps between two looks at the network**, and fewer
// than 4.8, so that no pass crosses two twenty-fifths of a second and every
// update is sent. Steps due beyond it are owed, not dropped: a server behind
// real time catches up over the passes after, answering its clients all the
// while, rather than going quiet for as long as the catching up takes.
constexpr std::int64_t most_steps_between_looks = 4;
static_assert(most_steps_between_looks * states_per_second <
                  glideslope::sim::steps_per_second,
              "a pass must not cross two state updates");

// **How many sessions a key may have that have sealed nothing yet**
// (`Connection::proven`). Two: an honest restart needs one beside the old,
// proven session, and a second lets a restart be made while one replayed copy
// is waiting out its timeout.
constexpr int most_unproven_per_key = 2;

// How often the server knocks on a connection. Twice within one `--timeout`
// at the default of ten seconds, and often enough that a number on the
// dashboard is not stale.
constexpr double ping_every_s = 1.0;

// A sealed datagram to one connection, counting what it cost. The sealer's
// sequence number moves whether or not the datagram arrives, which is what
// the replay window on the other end expects.
void send_sealed(glideslope::platform::UdpSocket& socket,
                 const glideslope::platform::Address& to, Connection& c,
                 std::span<const std::uint8_t> plain) {
    glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::sealed);
    w.bytes(c.sealing->seal(plain));
    const std::vector<std::uint8_t> out = w.take();
    if (socket.send(to, std::span<const std::uint8_t>(out.data(), out.size()))) {
        c.bytes_out += out.size();
    }
}

// Feet in a metre, for putting an aircraft above the ground the DEM gives.
constexpr double feet_per_metre = 3.280839895013123;

// **How a model's plan went**, for the end of the run to say: the take-off
// handed over, and the orbit it last flew, how often round and how close.
// **Its height round the orbit is the height it holds there**, from the turn
// it first comes within `level_ft` of it: an aircraft whose layer is higher
// than it could climb to on the way joins the orbit still climbing - a
// layer's 1,000 ft at the autopilot's 700 ft a minute is most of a minute and
// a half - and climbs on in it, as a pilot does; how far round it was by then
// is said, so that one that never gets there is seen.
struct PlanProgress {
    static constexpr double level_ft = 10.0;
    bool departing = false;
    double handed_over_ft = -1.0; // above the runway; below nought: not yet
    std::size_t leg = 0;
    std::string orbit;
    double turns = 0.0;
    double nearest_m = 1e18;
    double farthest_m = 0.0;
    double level_from_turns = -1.0; // below nought: not yet at its height
    double lowest_ft = 1e18;
    double highest_ft = -1e18;
};

// **An AI aircraft that cannot be planned** for want of something the model
// is not to blame for - the airport's runways - rather than refused by it.
struct Unplannable : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// **A model's plan for an AI aircraft**, asked when the server starts, before
// its clock does: the server owner's key, the task file's words, and the
// airport's runways as OurAirports gives them. What comes back is only a
// flight plan, checked (copilot/planner.hpp), and the autopilot flies it.
// Throws copilot::ProviderError for a provider with no key - refused, not
// faked - or for one whose plans were each refused; Unplannable when the
// airport's runways cannot be had.
glideslope::copilot::Planned plan_by_model(const std::filesystem::path& data,
                                           const Planner& planner,
                                           const glideslope::copilot::Task& task,
                                           std::string& said_by) {
    const glideslope::sim::CatalogueEntry entry =
        glideslope::sim::find_aircraft(data, task.aircraft);
    glideslope::copilot::PlanRequest request =
        glideslope::frontend::plan_request_for(data, entry.id);
    request.command = task.command;
    request.airport = task.airport;
    const bool played_back = !planner.playback.empty();
    const std::string key = played_back                    ? std::string()
                            : planner.provider == "openai" ? glideslope::platform::openai_key()
                                                           : glideslope::platform::anthropic_key();
    // The key first: a provider that cannot be asked is refused before
    // anything is fetched for it.
    glideslope::copilot::Post post = played_back
                                         ? glideslope::copilot::playback(planner.playback)
                                         : glideslope::copilot::http_post();
    const auto provider = glideslope::copilot::make_provider(planner.provider, key, planner.model,
                                                             std::move(post), played_back);
    // **The runways, which may not be had** - OurAirports unreachable with no
    // whole copy in the cache (one that is not the pinned file is fetched
    // again): the aircraft cannot be planned, which is said, and it flies
    // the plan file. Not the server stopped.
    try {
        request.runways = glideslope::world::runways_at(
            glideslope::world::world_runways(glideslope::platform::cache_directory(),
                                             glideslope::world::http_fetch()),
            task.airport);
    } catch (const std::exception& e) {
        throw Unplannable(std::string("the runways cannot be read: ") + e.what());
    }
    if (request.runways.empty()) {
        throw Unplannable("OurAirports has no runways at " + task.airport);
    }
    said_by = provider->name() + ", " + provider->model() + (played_back ? ", played back" : "");
    return glideslope::copilot::plan_from_words(*provider, request);
}

// **The aircraft the server flies, and the ground under them.** One `Dem`
// serves them all: it caches tiles as it goes, so two aircraft on opposite
// sides of the world each pull their own and neither waits for the other's.
// `PROJECT_STATUS.md` says a `Dem` is not thread-safe; the server steps every
// aircraft on this one thread, so nothing here shares it.
class Fleet {
public:
    struct Aircraft; // below, with what it holds
    // Every hand-over's question given up first, so that what is still
    // running ends as soon as giving up takes, not when its model answers.
    ~Fleet() {
        for (Aircraft& a : flown_) {
            if (a.hand_over_copilot) {
                a.hand_over_copilot->give_up();
            }
        }
    }
    Fleet(const Fleet&) = delete;
    Fleet& operator=(const Fleet&) = delete;
    Fleet(const std::filesystem::path& data, const std::vector<Flown>& fly, int ai,
          const std::filesystem::path& plan_file, const std::map<int, Planner>& planners,
          const std::filesystem::path& task_file, double departure_spacing_s,
          const Planner& hand_over_planner = {})
        : departure_spacing_s_(departure_spacing_s),
          hand_over_planner_(hand_over_planner),
          hand_over_words_(hand_over_planner.provider.empty()
                               ? std::string()
                               : glideslope::frontend::hand_over_task(data)),
          hand_over_playback_(hand_over_planner.playback.empty()
                                  ? std::optional<glideslope::copilot::Post>{}
                                  : glideslope::copilot::playback(
                                        hand_over_planner.playback,
                                        glideslope::copilot::Match::but_numbers)),
          coverage_(read_coverage(data)),
          fetch_(glideslope::world::http_fetch()),
          tiles_(glideslope::platform::cache_directory(), fetch_),
          geoid_(glideslope::world::egm2008_geoid(
              glideslope::platform::cache_directory(), fetch_)),
          collision_(std::make_shared<glideslope::world::CollisionGround>(
              std::make_shared<glideslope::world::Dem>(coverage_, tiles_, &geoid_),
              glideslope::world::runway_surfaces(data))) {
        // The DEM, with every runway its own surface (world/runway_ground.hpp):
        // what every client's prediction meets too.
        const std::shared_ptr<glideslope::world::CollisionGround> collision = collision_;
        ground_ = std::make_shared<glideslope::sim::FunctionTerrain>(
            [collision](double lat, double lon) {
                return collision->height_above_ellipsoid(lat, lon);
            },
            [collision](double lat, double lon) {
                return collision->water(lat, lon) != glideslope::world::Water::none;
            });
        const auto ground = ground_;
        data_ = data;
        // **The catalogue, read once**: an aeroplane asked for at admission
        // is looked up here, not read from the disk for every initiation.
        for (glideslope::sim::CatalogueEntry& entry : glideslope::sim::read_catalogue(data)) {
            std::string id = entry.id;
            catalogue_.emplace(std::move(id), std::move(entry));
        }
        for (const Flown& f : fly) {
            const glideslope::sim::CatalogueEntry entry =
                glideslope::sim::find_aircraft(data, f.id);
            auto aircraft = std::make_unique<glideslope::sim::Aircraft>(
                data / "jsbsim", entry.model);
            aircraft->set_terrain(ground);
            if (session_air_) {
                aircraft->set_weather(session_air_);
            }
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = f.latitude_deg;
            ic.longitude_deg = f.longitude_deg;
            // Above the ground under it, wherever on Earth that is.
            ic.terrain_elevation_ft =
                collision->height_above_ellipsoid(f.latitude_deg, f.longitude_deg) *
                feet_per_metre;
            ic.altitude_ft = ic.terrain_elevation_ft + 3000.0;
            ic.heading_deg = f.heading_deg;
            ic.airspeed_kts = entry.start_airspeed_kts;
            ic.engine_running = true;
            ic.gear = 0.0;
            aircraft->initialize(ic);
            // **Nobody is flying it and no plan is either, so its autopilot
            // holds the course it started on**: its heading, its height and its
            // speed. Held controls alone - enough power to stay up and nothing
            // else - let a Cessna roll off into a slow turn, 58 degrees of bank
            // in half a minute, so that two set on a collision course circled
            // apart instead.
            glideslope::sim::Controls idling;
            idling.throttle = 0.6;
            flown_.push_back(
                {f.id, std::move(aircraft), nullptr, *free_number(), -1, idling});
            remember_start(flown_.back(), ic, entry.seaplane);
            flown_.back().catalogue_id = f.id;
            flown_.back().model = entry.model;
            flown_.back().operators_course = true;
            learn_speeds(flown_.back());
            hold_course(flown_.back());
        }

        // **The AI aircraft the server runs.** They fly one plan, stacked
        // `ai_stack_ft` apart so that they are not all in the same piece of
        // sky - one plan is what there is to fly, and a server that put four
        // aeroplanes in one place would be hiding that rather than saying it.
        {
            const std::filesystem::path where =
                plan_file.empty() ? data / "plans" / "sydney-harbour.plan" : plan_file;
            std::ifstream in(where, std::ios::binary);
            if (!in) {
                throw std::runtime_error("cannot read the flight plan " +
                                         where.string());
            }
            glideslope::sim::FlightPlan plan = glideslope::sim::parse_flight_plan(
                std::string(std::istreambuf_iterator<char>(in), {}));
            ground_the_landing(plan);
            const glideslope::sim::CatalogueEntry entry =
                glideslope::sim::find_aircraft(data, plan.aircraft);
            // Held to its aircraft's speeds, as a model's plan is.
            glideslope::sim::refuse_what_it_cannot_fly(data, plan, entry.id);
            // A plan may say where to start; one that does not begins at its
            // first waypoint, heading for the next. `parse_flight_plan`
            // refuses a plan with no waypoints, so there is always one.
            glideslope::sim::FlightPlan::Start from;
            if (plan.start) {
                from = *plan.start;
            } else {
                const glideslope::sim::Waypoint& first = plan.waypoints.front();
                from.latitude_deg = first.latitude_deg;
                from.longitude_deg = first.longitude_deg;
                from.altitude_ft = first.altitude_ft;
                from.airspeed_kts = first.airspeed_kts;
                from.heading_deg =
                    plan.waypoints.size() > 1
                        ? glideslope::sim::bearing_deg(
                              first.latitude_deg, first.longitude_deg,
                              plan.waypoints[1].latitude_deg,
                              plan.waypoints[1].longitude_deg)
                        : 0.0;
            }
            // **Each AI aircraft's planner**, if the server gave it one: the
            // task, asked of that model. Asked here, before the clock starts,
            // so nothing waits on a model while aircraft are flying.
            std::optional<glideslope::copilot::Task> task;
            int departures = 0;
            for (int i = 0; i < ai; ++i) {
                const auto planner = planners.find(i + 1);
                if (planner == planners.end() || planner->second.provider.empty()) {
                    continue;
                }
                const std::string name = "AI " + std::to_string(i + 1);
                try {
                    if (!task) {
                        const std::filesystem::path task_at =
                            task_file.empty() ? data / "tasks" / "sydney-cbd-orbit.task"
                                              : task_file;
                        std::ifstream task_in(task_at, std::ios::binary);
                        if (!task_in) {
                            throw std::runtime_error("cannot read the task " + task_at.string());
                        }
                        task = glideslope::copilot::parse_task(
                            std::string(std::istreambuf_iterator<char>(task_in), {}));
                        std::printf("the task: %s, a %s at %s\n", task->command.c_str(),
                                    task->aircraft.c_str(), task->airport.c_str());
                    }
                    std::string by;
                    const glideslope::copilot::Planned planned =
                        plan_by_model(data, planner->second, *task, by);
                    std::printf("%s planned by %s, in %d answer%s\n", name.c_str(), by.c_str(),
                                planned.attempts, planned.attempts == 1 ? "" : "s");
                    for (const std::string& why : planned.refused) {
                        std::printf("%s refused an answer: %s\n", name.c_str(), why.c_str());
                    }
                    std::istringstream lines(planned.text);
                    for (std::string line; std::getline(lines, line);) {
                        std::printf("%s plan: %s\n", name.c_str(), line.c_str());
                    }
                    std::fflush(stdout);
                    add_planned(i, planned.plan, planner->second.provider, departures);
                } catch (const glideslope::copilot::ProviderError& e) {
                    // **Refused, and said**: the aircraft flies the plan
                    // file, as one given no planner does.
                    std::printf("%s: %s is refused: %s; it flies the plan file instead\n",
                                name.c_str(), planner->second.provider.c_str(), e.what());
                    std::fflush(stdout);
                } catch (const Unplannable& e) {
                    std::printf("%s cannot be planned: %s; it flies the plan file instead\n",
                                name.c_str(), e.what());
                    std::fflush(stdout);
                }
            }
            // **Stacked downwards in the order they take off**: the first
            // away flies its plan highest, `ai_stack_ft` above the next, so
            // that each climbs to its own height below every one already
            // gone and, on their first departures in turn, none climbs
            // through another's on the way. Stacked upwards, the second
            // climbed through the first's orbit. One that flies again after
            // a wreck, or is held on the ground past the next one's turn,
            // does climb through: then the monitor holds the others clear.
            for (std::size_t n = 0; n < waiting_.size(); ++n) {
                Aircraft& a = waiting_[n];
                const double stack_ft =
                    static_cast<double>(waiting_.size() - 1 - n) * ai_stack_ft;
                for (glideslope::sim::Waypoint& w : a.own_plan->waypoints) {
                    w.altitude_ft += stack_ft;
                }
                fly_plan(a);
            }
            for (int i = 0; i < ai; ++i) {
                if (std::any_of(waiting_.begin(), waiting_.end(),
                                [&](const Aircraft& w) { return w.ai_number == i + 1; }) ||
                    std::any_of(flown_.begin(), flown_.end(),
                                [&](const Aircraft& w) { return w.ai_number == i + 1; })) {
                    continue;
                }
                auto aircraft = std::make_unique<glideslope::sim::Aircraft>(
                    data / "jsbsim", entry.model);
                aircraft->set_terrain(ground);
                if (session_air_) {
                    aircraft->set_weather(session_air_);
                }
                glideslope::sim::InitialConditions ic;
                ic.latitude_deg = from.latitude_deg;
                ic.longitude_deg = from.longitude_deg;
                ic.altitude_ft = from.altitude_ft +
                                 static_cast<double>(i) * ai_stack_ft;
                ic.heading_deg = from.heading_deg;
                ic.airspeed_kts = from.airspeed_kts;
                ic.engine_running = true;
                ic.gear = 0.0;
                aircraft->initialize(ic);
                auto controller =
                    controller_for(*aircraft, entry.model, glideslope::sim::Controls{});
                // **Its plan flown at its own layer**, not only begun there:
                // stacked at the start alone, every one came down to the
                // plan's heights at its first waypoint and they flew the tour
                // on top of one another, 37 m apart.
                const double stack_ft = static_cast<double>(i) * ai_stack_ft;
                controller->to_ai(stacked(plan, stack_ft));
                flown_.push_back({plan.aircraft + " (AI " + std::to_string(i + 1) + ")",
                                  std::move(aircraft), std::move(controller),
                                  *free_number(), -1, {}});
                remember_start(flown_.back(), ic, entry.seaplane);
                flown_.back().catalogue_id = plan.aircraft;
                flown_.back().model = entry.model;
                learn_speeds(flown_.back());
                flown_.back().on_plan = true;
                flown_.back().stack_ft = stack_ft;
                flown_.back().ai_number = i + 1;
                ++ai_;
            }
            // Kept so that a player joining later starts where the AI did,
            // and so that an aircraft left behind can be handed to an AI
            // pilot to fly the same plan.
            start_ = from;
            plan_ = plan;
            player_model_ = entry.model;
            player_id_ = plan.aircraft;
            player_airspeed_kts_ = entry.start_airspeed_kts;
            player_seaplane_ = entry.seaplane;
        }
    }

    // **A player joining is given an aircraft**, at the place the flight plan
    // starts and stacked clear of everything already in that piece of sky.
    //
    // **Its number is the lowest no player's aircraft is flying under**, not
    // the player's slot. It was the slot, and a slot is the rank of a key
    // among those present (net/slots.hpp): a player whose key sorts first
    // moves everyone after them down one, while their aircraft keep the
    // numbers they were given - so the next to arrive could be handed a number
    // already flying, and two clients each took the other's line of the state
    // update for their own. A number is the aircraft's for as long as it
    // flies; the state update tells each client which is its own.
    //
    // Nothing chooses the aeroplane yet: a player flies whatever the plan
    // flies. `REQUIREMENTS.md` asks for an aircraft the player picks, and
    // that is a session setting nobody has written.
    //
    // **The aeroplane the player asked for** (`asked`, the initiation's
    // payload), if the catalogue has it - looked up among its ids, never
    // joined to a path - and the plan's otherwise, said so. Where it starts
    // is the plan's either way, at the aeroplane's own starting speed.
    // A payload that asked for something unreadable (`unreadable`) gives the
    // plan's, said so.
    std::uint8_t give(std::uint8_t slot, const std::optional<std::string>& asked = std::nullopt,
                      bool unreadable = false) {
        std::string model = player_model_;
        std::string id = player_id_;
        double airspeed_kts = player_airspeed_kts_;
        bool seaplane = player_seaplane_;
        if (unreadable) {
            std::printf("slot %d asked for an aeroplane in a payload that does not read: it "
                        "flies %s\n",
                        static_cast<int>(slot), id.c_str());
            std::fflush(stdout);
        }
        if (asked) {
            if (const auto entry = catalogue_.find(*asked); entry != catalogue_.end()) {
                model = entry->second.model;
                id = entry->second.id;
                airspeed_kts = entry->second.start_airspeed_kts;
                seaplane = entry->second.seaplane;
                std::printf("slot %d asked for %s, and flies it\n", static_cast<int>(slot),
                            id.c_str());
            } else {
                std::printf("slot %d asked for %s, which this server does not have: it "
                            "flies %s\n",
                            static_cast<int>(slot), asked->c_str(), id.c_str());
            }
            std::fflush(stdout);
        }
        auto aircraft = std::make_unique<glideslope::sim::Aircraft>(data_ / "jsbsim", model);
        aircraft->set_terrain(ground_);
        if (session_air_) {
            aircraft->set_weather(session_air_);
        }
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = start_.latitude_deg;
        ic.longitude_deg = start_.longitude_deg;
        // Above everything already flying, so that being given an aircraft
        // never puts one inside another.
        ic.altitude_ft = start_.altitude_ft +
                         static_cast<double>(flown_.size() + 1) * ai_stack_ft;
        ic.heading_deg = start_.heading_deg;
        ic.airspeed_kts = airspeed_kts;
        ic.engine_running = true;
        ic.gear = 0.0;
        // **Or on final** (`--players-on-final`), trimmed down the glidepath
        // at her approach speed with the landing flap and the gear down - an
        // aeroplane whose figures give no approach speed starts where the
        // plan does, said so.
        std::optional<glideslope::sim::ApproachSpeeds> final_speeds;
        if (on_final_) {
            try {
                final_speeds = glideslope::sim::landing_speeds(data_, model);
            } catch (const std::exception&) {
                final_speeds.reset();
            }
            if (!final_speeds) {
                std::printf("slot %d's %s has no approach speed: it starts where the plan does, "
                            "not on final\n", static_cast<int>(slot), id.c_str());
                std::fflush(stdout);
            }
        }
        if (on_final_ && final_speeds) {
            const auto players = static_cast<double>(std::count_if(
                flown_.begin(), flown_.end(), [](const Aircraft& a) { return a.slot >= 0; }));
            const double out_m = (2.0 + 0.5 * players) * 1852.0;
            ic = glideslope::sim::final_approach_start(*on_final_, out_m, final_speeds->vref_kts,
                                                       final_speeds->flap, final_speeds->aim_m,
                                                       3.0);
            ic.terrain_elevation_ft =
                collision_->height_above_ellipsoid(ic.latitude_deg, ic.longitude_deg) *
                feet_per_metre;
        }
        aircraft->initialize(ic);
        std::uint8_t index = 0;
        while (std::any_of(flown_.begin(), flown_.end(), [&](const Aircraft& a) {
            return a.index == index;
        })) {
            ++index;
        }
        // Until their first input arrives they hold enough power to stay up:
        // a player whose aircraft appeared with the throttle shut would be
        // gliding before they had touched anything.
        glideslope::sim::Controls idling;
        idling.throttle = 0.6;
        if (on_final_ && final_speeds) {
            // On final, what the trim found, held until the player moves.
            idling = trimmed_controls(*aircraft);
        }
        flown_.push_back({id + " (slot " + std::to_string(slot) + ")",
                          std::move(aircraft), nullptr, index, static_cast<int>(slot),
                          idling});
        remember_start(flown_.back(), ic, seaplane);
        flown_.back().catalogue_id = id;
        flown_.back().model = model;
        learn_speeds(flown_.back());
        return index;
    }

    // **What becomes of an aircraft when the person flying it goes**, which
    // `REQUIREMENTS.md` 6.5 makes a session setting: it is taken out of the
    // sky, or handed to an AI pilot flying the server's plan. Returns what
    // was done, so that the server can say so.
    //
    // **Handed over, it is renumbered.** A player's aircraft has a player's
    // number, which is about to be free for somebody else; an aircraft keeping
    // it would be mistaken for the new player's.
    bool take(std::uint8_t index, bool hand_to_ai) {
        for (auto it = flown_.begin(); it != flown_.end(); ++it) {
            if (it->slot < 0 || it->index != index) {
                continue;
            }
            const std::optional<std::uint8_t> number = free_number();
            if (!hand_to_ai || plan_.waypoints.empty() || !number) {
                // Said as it goes, as the end of the run says it of the rest:
                // a test that waits for its clients to leave finds their
                // aircraft gone by the end.
                std::printf("  number %d, a player's, banked as far as %.0f degrees\n",
                            static_cast<int>(it->index), it->most_roll_deg);
                retire(std::move(it->hand_over_copilot));
                flown_.erase(it);
                return false;
            }
            it->slot = -1;
            it->index = *number;
            it->id = plan_.aircraft + " (AI, was slot " + std::to_string(index) + ")";
            it->controller =
                controller_for(*it->aircraft, it->model, glideslope::sim::Controls{});
            // On a layer of its own, above every plan-file AI aircraft's.
            it->stack_ft = static_cast<double>(ai_) * ai_stack_ft;
            it->controller->to_ai(stacked(plan_, it->stack_ft));
            ++ai_;
            plan_hand_over(*it, "left by its player", "flies the plan file");
            return true;
        }
        return false;
    }

    // **What the aircraft numbered `index` is being flown by**, held until
    // something says otherwise. False if there is no such aircraft, or if an
    // AI pilot is flying it - a client's inputs must not reach an aeroplane
    // that is not theirs, and that check is here rather than at the caller
    // because this is where the answer is known.
    bool fly(std::uint8_t index, const glideslope::sim::Controls& controls) {
        for (Aircraft& a : flown_) {
            if (a.index == index && !ai_flying(a)) {
                a.held = controls;
                return true;
            }
        }
        return false;
    }

    // **A player's aircraft handed to an AI pilot, or taken back** - Phase 7.
    // It keeps its number and its slot; only who flies it changes. The AI
    // holds what the aircraft is doing (sim::Controller::to_ai), and the
    // controller carries the controls across in either direction, which is
    // what makes a swap no step (the user/AI swap, Phase 4). Returns whether
    // anything changed: a player's aircraft, not already in those hands.
    bool hand(std::uint8_t index, bool to_ai) {
        for (Aircraft& a : flown_) {
            if (a.index != index || a.slot < 0 || a.wrecked_at_s >= 0.0) {
                continue;
            }
            if (to_ai == ai_flying(a)) {
                return false;
            }
            if (!a.controller) {
                a.controller = controller_for(*a.aircraft, a.model, a.held);
            }
            a.copilot_route.clear();
            if (to_ai) {
                a.controller->to_ai();
            } else {
                a.controller->set_pilot(a.held);
                a.controller->to_pilot();
            }
            announced_.push_back(
                {index, to_ai ? glideslope::net::Controller::ai : glideslope::net::Controller::person,
                 static_cast<double>(steps_) /
                     static_cast<double>(glideslope::sim::steps_per_second)});
            std::printf("aircraft %u handed to %s\n", static_cast<unsigned>(index),
                        to_ai ? "the AI" : "its pilot");
            std::fflush(stdout);
            return true;
        }
        return false;
    }

    // **Every player starts on the final approach to `spec`'s runway end**
    // (`--players-on-final AIRPORT/RUNWAY`), as the CLI's landings do: the
    // first two miles out on the centreline and the glidepath - the learnt
    // landing's gate - and each after half a mile further out, so that no
    // two are put in one place. Why not, or nothing.
    std::string players_on_final(const std::string& spec) {
        std::string why;
        on_final_ = runway_end_named(spec, why);
        if (!on_final_) {
            return why;
        }
        std::printf("players start on final to %s, heading %.0f, threshold %.0f ft\n",
                    on_final_->name.c_str(), on_final_->heading_deg, on_final_->elevation_ft);
        std::fflush(stdout);
        return {};
    }

    // **The first plan-file AI aircraft on the final approach to `spec`'s
    // runway end, landed** (`--ai-on-final AIRPORT/RUNWAY`): three miles out
    // on the centreline and the glidepath - outside the learnt landing's gate
    // - trimmed at its approach speed with the landing flap, and flown down
    // by the approach autopilot; if its model has a learnt landing it is
    // handed to it at its gate (sim::Controller::to_ai_approach with the
    // policy), and otherwise landed by the approach autopilot. **One, not
    // all**: nothing clears a runway, so a second landing behind it would
    // land into it; the rest fly their plan. An aeroplane whose figures give
    // no approach speed flies its plan, said. Why not, or nothing.
    std::string ai_on_final(const std::string& spec) {
        std::string why;
        const std::optional<glideslope::sim::Runway> runway = runway_end_named(spec, why);
        if (!runway) {
            return why;
        }
        const double out_nm = 3.0;
        for (Aircraft& a : flown_) {
            if (a.ai_number <= 0 || !a.on_plan) {
                continue;
            }
            std::optional<glideslope::sim::ApproachSpeeds> speeds;
            try {
                speeds = glideslope::sim::landing_speeds(data_, a.model);
            } catch (const std::exception&) {
                speeds.reset();
            }
            if (!speeds) {
                std::printf("aircraft %u's %s has no approach speed: it flies its plan, not "
                            "on final\n", static_cast<unsigned>(a.index), a.model.c_str());
                continue;
            }
            glideslope::sim::InitialConditions ic = glideslope::sim::final_approach_start(
                *runway, out_nm * 1852.0, speeds->vref_kts, speeds->flap, speeds->aim_m, 3.0);
            ic.terrain_elevation_ft =
                collision_->height_above_ellipsoid(ic.latitude_deg, ic.longitude_deg) *
                feet_per_metre;
            a.aircraft->initialize(ic);
            remember_start(a, ic, player_seaplane_);
            a.on_plan = false;
            a.on_final_to = *runway;
            land_on_final(a);
            std::printf("aircraft %u, an AI's %s, starts on final to %s %.1f miles out, %s\n",
                        static_cast<unsigned>(a.index), a.model.c_str(), runway->name.c_str(),
                        out_nm,
                        learnt_for(a.model) ? "to be handed to the learnt landing at its gate"
                                            : "landed by the approach autopilot");
            break;
        }
        std::fflush(stdout);
        return {};
    }

    // An AI aircraft on final (`--ai-on-final`), flown down from where it
    // is: by the approach autopilot, handing her to the learnt landing at
    // its gate where her model has one.
    void land_on_final(Aircraft& a) {
        a.controller = controller_for(*a.aircraft, a.model, trimmed_controls(*a.aircraft));
        const auto speeds = lands_with_.find(a.model);
        if (speeds == lands_with_.end() || !speeds->second) {
            hold_course(a);
            return;
        }
        if (const auto policy = learnt_for(a.model)) {
            a.controller->to_ai_approach(*a.on_final_to, *speeds->second, policy);
        } else {
            a.controller->to_ai_approach(*a.on_final_to, *speeds->second);
        }
        a.learnt_runway = a.on_final_to->name;
        a.learnt_said = false;
        a.learnt_announced = false;
    }

    // **A runway end by `AIRPORT/RUNWAY`** - 'YSSY/16R' - from the strips
    // the collision ground is made from, its threshold's elevation that
    // ground's; or none, with why.
    std::optional<glideslope::sim::Runway> runway_end_named(const std::string& spec,
                                                            std::string& why) {
        const std::size_t slash = spec.find('/');
        const std::string airport = spec.substr(0, slash);
        const std::string ident = spec.substr(slash + 1);
        const auto surfaces = glideslope::world::runway_surfaces(data_);
        for (std::size_t i = 0; i < surfaces->size(); ++i) {
            const glideslope::world::RunwayStrip& strip = surfaces->at(i).strip;
            if (strip.airport != airport) {
                continue;
            }
            for (const bool he : {false, true}) {
                if ((he ? strip.he_ident : strip.le_ident) != ident) {
                    continue;
                }
                const double lat = he ? strip.he_latitude_deg : strip.le_latitude_deg;
                const double lon = he ? strip.he_longitude_deg : strip.le_longitude_deg;
                return glideslope::world::runway_end(
                    *surfaces, i, he, collision_->height_above_ellipsoid(lat, lon) * feet_per_metre);
            }
        }
        why = "no runway " + ident + " at " + airport + " in the runways the ground is made from";
        return std::nullopt;
    }

    // **A player's aircraft handed to the learnt landing** (`CONTROLLER_SWAP`
    // to `LEARNT_LANDING`): only where its model has one
    // (sim::learnt_landing) and it is at that landing's gate on the final
    // approach to a runway of the world's (world::learnt_gate_runway) - the
    // gate the CLI hands it over at. Then the policy flies it down to the
    // touch and the approach autopilot rolls it out, as `glideslope_cli land
    // --learnt` does, through the same controller every hand-over goes
    // through, so the swap steps nothing; taking it back is `hand(index,
    // false)`, as from any AI. Returns why not, or nothing if it was handed
    // over.
    std::string hand_to_learnt(std::uint8_t index) {
        for (Aircraft& a : flown_) {
            if (a.index != index || a.slot < 0) {
                continue;
            }
            if (a.wrecked_at_s >= 0.0) {
                return "it is a wreck";
            }
            if (a.controller && a.controller->learnt()) {
                return "the learnt landing has it already";
            }
            const std::shared_ptr<const glideslope::sim::LearntPolicy> policy =
                learnt_for(a.model);
            if (!policy) {
                return "the " + a.model + " has no learnt landing";
            }
            const glideslope::world::LearntGateFound gate = glideslope::world::learnt_gate_runway(
                *glideslope::world::runway_surfaces(data_), *a.aircraft, *policy,
                [this](double lat, double lon) {
                    return collision_->height_above_ellipsoid(lat, lon);
                });
            if (!gate.runway) {
                return "not at the learnt landing's gate: " + gate.why;
            }
            if (!a.controller) {
                a.controller = controller_for(*a.aircraft, a.model, a.held);
            }
            const auto speeds = lands_with_.find(a.model);
            if (speeds == lands_with_.end() || !speeds->second) {
                return "the " + a.model + " has no approach speeds to roll out at";
            }
            a.copilot_route.clear();
            // From what she flies now: the pilot's held controls, if the
            // pilot has her.
            if (!ai_flying(a)) {
                a.controller->set_pilot(a.held);
            }
            a.controller->to_ai_learnt_approach(*gate.runway, *speeds->second, policy);
            a.learnt_runway = gate.runway->name;
            a.learnt_said = false;
            announced_.push_back({index, glideslope::net::Controller::learnt_landing,
                                  static_cast<double>(steps_) /
                                      static_cast<double>(glideslope::sim::steps_per_second)});
            std::printf("aircraft %u handed to the learnt landing, on final to %s\n",
                        static_cast<unsigned>(index), gate.runway->name.c_str());
            std::fflush(stdout);
            return {};
        }
        return "it is not a player's aircraft";
    }

    // The learnt landing for a model, read once; null for none, and for one
    // that cannot be read - said, not swallowed.
    std::shared_ptr<const glideslope::sim::LearntPolicy> learnt_for(const std::string& model) {
        auto it = learnt_.find(model);
        if (it == learnt_.end()) {
            std::shared_ptr<const glideslope::sim::LearntPolicy> policy;
            try {
                policy = glideslope::sim::learnt_landing(data_, model);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "  %s: its learnt landing cannot be read (%s)\n",
                             model.c_str(), e.what());
            }
            it = learnt_.emplace(model, std::move(policy)).first;
        }
        return it->second;
    }

    // **A plan's landing on the ground it is flown over** (`land`): its
    // threshold's elevation the collision ground's, as a take-off's runway's
    // is - what the wheels meet, so that the flare is flown to it.
    void ground_the_landing(glideslope::sim::FlightPlan& plan) {
        if (plan.landing) {
            plan.landing->elevation_ft =
                collision_->height_above_ellipsoid(plan.landing->threshold_lat_deg,
                                                   plan.landing->threshold_lon_deg) *
                feet_per_metre;
        }
    }

    // **An AI's approach handed to the learnt landing at its gate**
    // (`--ai-on-final`): its controller did it, the step she was inside it;
    // said here and announced to every client, once, as a player's is.
    void learnt_at_gate(Aircraft& a) {
        if (a.slot >= 0 || a.learnt_announced || !a.controller || !a.controller->learnt() ||
            !a.controller->learnt_runway()) {
            return;
        }
        a.learnt_announced = true;
        a.learnt_runway = a.controller->learnt_runway()->name;
        a.learnt_said = false;
        announced_.push_back({a.index, glideslope::net::Controller::learnt_landing,
                              static_cast<double>(steps_) /
                                  static_cast<double>(glideslope::sim::steps_per_second)});
        std::printf("aircraft %u, an AI's, handed to the learnt landing at its gate, on final "
                    "to %s\n",
                    static_cast<unsigned>(a.index), a.learnt_runway.c_str());
        std::fflush(stdout);
    }

    // **What the learnt landing did**, said once when it has stopped her:
    // where she touched and how hard, and where she stopped.
    void learnt_landed(Aircraft& a, std::vector<std::string>& happened) {
        const glideslope::sim::LearntLander* l = a.controller ? a.controller->learnt() : nullptr;
        if (l == nullptr || a.learnt_said || l->stage() != glideslope::sim::LearntLander::Stage::stopped) {
            return;
        }
        a.learnt_said = true;
        char text[256];
        std::snprintf(text, sizeof text,
                      "aircraft %u: the learnt landing touched down on %s at %.0f ft/min, "
                      "%+.2f m across the centreline, and stopped %.0f m along, %+.2f m across",
                      static_cast<unsigned>(a.index), a.learnt_runway.c_str(),
                      l->touchdown_sink_fpm(), l->touchdown_across_m(),
                      -l->rollout().along_m(), l->rollout().across_m());
        happened.emplace_back(text);
    }

    // **A copilot's route for a player's own aircraft** (`COPILOT_ROUTE`,
    // REQUIREMENTS.md section 5 as decided 2026-09-30): planned on the
    // player's client with the player's key, and here an input like any
    // other - **trusted no further than a plan**. It is read as a flight plan
    // from where the aircraft is (sim::parse_flight_plan refuses what it
    // cannot read, and an orbit too tight for its speed), and checked against
    // the aircraft as this server has it, by the checks a copilot's answer is
    // held to (copilot::change_refusal): heights above the ground under it,
    // airspeeds the aircraft flies, nothing more than 200 km away, a glide
    // only with its engine stopped. Refused, nothing changes, and why is
    // returned. Taken, the aircraft is handed to the AI if its player was
    // flying it - announced as any hand-over is - and flies the route from
    // here: its heights above the sea, as the aircraft's are above the
    // ellipsoid. Empty when flown.
    //
    // **Nothing a route holds, and nothing an aircraft lacks, ends the
    // server**: whatever a check throws is a refusal, saying what it was.
    std::string fly_route(std::uint8_t index, const glideslope::net::CopilotRoute& route) {
        try {
            return fly_route_checked(index, route);
        } catch (const std::exception& e) {
            return std::string("it could not be checked: ") + e.what();
        }
    }

    std::string fly_route_checked(std::uint8_t index, const glideslope::net::CopilotRoute& route) {
        Aircraft* found = nullptr;
        for (Aircraft& a : flown_) {
            if (a.index == index && a.slot >= 0) {
                found = &a;
            }
        }
        if (found == nullptr) {
            return "no player's aircraft is number " + std::to_string(index);
        }
        return fly_route_on(*found, route);
    }

    // The checks and the flying, for an aircraft found: a player's, or one
    // given to the AI in the air and planned by the server's model.
    std::string fly_route_on(Aircraft& a, const glideslope::net::CopilotRoute& route) {
        const std::uint8_t index = a.index;
        if (a.wrecked_at_s >= 0.0) {
            return "it is a wreck";
        }
        const glideslope::sim::Aircraft& craft = *a.aircraft;
        const double lat = craft.property("position/lat-geod-deg");
        const double lon = craft.property("position/long-gc-deg");
        const double undulation_ft = geoid_.undulation(lat, lon) * feet_per_metre;
        if (!a.brief) {
            return "its speeds are not known, so no route can be checked for it: " + a.no_brief;
        }
        const glideslope::copilot::Brief& brief = *a.brief;
        glideslope::copilot::Situation now;
        now.latitude_deg = lat;
        now.longitude_deg = lon;
        now.altitude_ft = craft.property("position/h-sl-ft") - undulation_ft;
        // The ground as the aircraft meets it: the collision ground, every
        // runway its own surface - what the player's copilot is told too.
        now.ground_ft = collision_->height_above_geoid(lat, lon) * feet_per_metre;
        now.heading_deg = craft.property("attitude/psi-deg");
        now.airspeed_kts = craft.property("velocities/vc-kts");
        now.engine_running = !engine_stopped(a);
        if (a.controller) {
            now.gliding_kts = a.controller->glide();
        }
        // The route as a plan's lines, read by the plan's own reader.
        std::string text = "aircraft " + a.catalogue_id + "\n";
        char line[256];
        std::snprintf(line, sizeof line, "start %.7f %.7f %.1f %.1f %.1f\n", lat, lon,
                      now.altitude_ft, now.heading_deg, std::max(now.airspeed_kts, 1.0));
        text += line;
        for (const glideslope::net::RouteWaypoint& w : route.waypoints) {
            if (w.orbit) {
                std::snprintf(line, sizeof line, " %.7f %.7f %.1f %.1f %.1f %u %s\n", w.latitude_deg,
                              w.longitude_deg, w.orbit->radius_m, w.altitude_ft, w.airspeed_kts,
                              static_cast<unsigned>(w.orbit->turns), w.orbit->right ? "right" : "left");
                text += "orbit " + w.name + line;
            } else {
                std::snprintf(line, sizeof line, " %.7f %.7f %.1f %.1f\n", w.latitude_deg,
                              w.longitude_deg, w.altitude_ft, w.airspeed_kts);
                text += "waypoint " + w.name + line;
            }
        }
        glideslope::copilot::Change change;
        change.keep = false;
        change.glide_kts = route.glide_kts;
        try {
            change.plan = glideslope::sim::parse_flight_plan(text);
        } catch (const glideslope::sim::FlightPlanError& e) {
            return e.what();
        }
        if (std::string why = glideslope::copilot::change_refusal(brief, now, change); !why.empty()) {
            return why;
        }
        if (!ai_flying(a)) {
            (void)hand(index, true);
        }
        glideslope::sim::FlightPlan plan = change.plan;
        for (glideslope::sim::Waypoint& w : plan.waypoints) {
            w.altitude_ft += geoid_.undulation(w.latitude_deg, w.longitude_deg) * feet_per_metre;
        }
        a.controller->replan(std::move(plan));
        a.controller->set_glide(route.glide_kts);
        // **The player's copilot's route replaces any plan it had**, on
        // purpose: an AI's aircraft a player took over was still marked as
        // flying the server's plan, and a wreck would have started it on
        // that plan again. Now its player's route is what it flies, and
        // wrecked it flies again holding the course it started on, as any
        // player's aircraft the AI flies does (fly_again) - the route gone
        // with the wreck.
        a.on_plan = false;
        a.copilot_route.clear();
        for (const glideslope::net::RouteWaypoint& w : route.waypoints) {
            a.copilot_route.push_back(w.name);
        }
        return {};
    }

    // The simulation's clock.
    double now_s() const {
        return static_cast<double>(steps_) / static_cast<double>(glideslope::sim::steps_per_second);
    }

    // **The weather every aircraft flies** (REQUIREMENTS.md 6.3), and every
    // client is sent: a report, on the session's clock
    // (frontend::SessionClocked) - so that two aircraft side by side meet the
    // same gust whenever each was made. Every aircraft flying now and every
    // one made later flies it.
    //
    // **Over no ground**, on the server and every client alike: the ground's
    // lift is 256 heights of the DEM and a transform at every step
    // (world/lift.hpp), and a predicting client flies every step again at
    // each update - a sanitized build fell 4.7 s behind, and without it a
    // client was 6 m out from a server that had it (2026-10-06). So no end
    // has the ground's lift, and thermals stand on the station's elevation,
    // until the lift is cheap enough for a client to fly it too
    // (COMPLETION_PLAN.md).
    void fly_in(glideslope::world::WeatherReport report, double blend_s) {
        air_ = std::make_shared<glideslope::world::ReportedWeather>(
            std::move(report), &geoid_, blend_s, glideslope::world::GroundAt{});
        session_air_ = std::make_shared<glideslope::frontend::SessionClocked>(
            air_, [this] { return now_s(); });
        for (std::vector<Aircraft>* each : {&flown_, &waiting_}) {
            for (Aircraft& a : *each) {
                a.aircraft->set_weather(session_air_);
            }
        }
        weather_changed_at_s_ = now_s();
        weather_blend_s_ = blend_s;
        ++weathers_;
    }
    // **A new report, blended in from now** over the blend `fly_in` was given.
    void weather_changes(glideslope::world::WeatherReport report) {
        air_->update(std::move(report), now_s());
        previous_changed_at_s_ = weather_changed_at_s_;
        weather_changed_at_s_ = now_s();
        ++weathers_;
    }
    // What is flown, for the clients: none is still air.
    const glideslope::world::WeatherReport* weather() const {
        return air_ ? &air_->report() : nullptr;
    }
    double weather_changed_at_s() const { return weather_changed_at_s_; }
    // **What the weather blends from while it does**, and when that one
    // changed: told first to a client joining mid-blend, which flew the new
    // one whole until the blend ended when told only it. Null when nothing
    // blends.
    const glideslope::world::WeatherReport* weather_blending_from() const {
        return air_ ? air_->blending_from(now_s()) : nullptr;
    }
    double previous_weather_changed_at_s() const { return previous_changed_at_s_; }
    double weather_blend_s() const { return weather_blend_s_; }
    // How many weathers there have been: a client told an older count is told again.
    int weathers() const { return weathers_; }
    // Where the AI's plan starts, for a METAR given with no --station.
    std::optional<std::pair<double, double>> first_place() const {
        for (const std::vector<Aircraft>* each : {&flown_, &waiting_}) {
            if (!each->empty()) {
                const Aircraft& a = each->front();
                return std::make_pair(a.aircraft->property("position/lat-geod-deg"),
                                      a.aircraft->property("position/long-gc-deg"));
            }
        }
        return std::nullopt;
    }

    // **An aircraft given to the AI in the air, planned by the server's
    // model** (`--hand-over-planner`; REQUIREMENTS.md section 5, decided
    // 2026-10-02): asked with the server's key, off the stepping thread
    // (copilot::Copilot), from where the aircraft is - told what it is, how
    // it flies, the runways near it, and the data's hand-over words. What
    // it answers is checked again here as a player's copilot's route is
    // (fly_route_on) and flown. With no planner, or one refused for want of
    // its key, the aircraft goes on as it was given: `as_before`, said.
    void plan_hand_over(Aircraft& a, const std::string& because, const std::string& as_before) {
        retire(std::move(a.hand_over_copilot));
        a.copilot_route.clear();
        a.planned_route_seen = 0;
        a.handed_because = because;
        if (hand_over_planner_.provider.empty()) {
            std::printf("aircraft %u, %s, is planned by no model: it %s\n",
                        static_cast<unsigned>(a.index), because.c_str(), as_before.c_str());
            std::fflush(stdout);
            return;
        }
        try {
            if (!a.brief) {
                throw std::runtime_error("its speeds are not known: " + a.no_brief);
            }
            const bool played_back = hand_over_playback_.has_value();
            const std::string key =
                played_back                                ? std::string()
                : hand_over_planner_.provider == "openai" ? glideslope::platform::openai_key()
                                                           : glideslope::platform::anthropic_key();
            glideslope::copilot::Post post =
                played_back ? *hand_over_playback_
                : hand_over_planner_.record.empty()
                    ? glideslope::copilot::http_post()
                    : glideslope::copilot::recording(glideslope::copilot::http_post(),
                                                     hand_over_planner_.record);
            auto provider = glideslope::copilot::make_provider(
                hand_over_planner_.provider, key, hand_over_planner_.model, std::move(post),
                played_back);
            glideslope::copilot::Brief brief = *a.brief;
            brief.aircraft_name = glideslope::sim::find_aircraft(data_, a.catalogue_id).name;
            brief.task = hand_over_words_;
            const std::string who = provider->name() + ", " + provider->model() +
                                    (played_back ? ", played back" : "");
            a.hand_over_copilot =
                std::make_shared<glideslope::copilot::Copilot>(std::move(provider), brief);
            // Asked now if the world's runways have been read, or as soon as
            // they have (hand_over_answers): the question never waits on
            // them, so giving it up is never held up by their download.
            a.hand_over_asked = false;
            if (runways_ready()) {
                ask_hand_over(a);
            }
            std::printf("aircraft %u, %s, is planned by %s, asked from where it is\n",
                        static_cast<unsigned>(a.index), because.c_str(), who.c_str());
        } catch (const glideslope::copilot::ProviderError& e) {
            std::printf("aircraft %u, %s: %s is refused: %s; it %s instead\n",
                        static_cast<unsigned>(a.index), because.c_str(),
                        hand_over_planner_.provider.c_str(), e.what(), as_before.c_str());
        } catch (const std::exception& e) {
            std::printf("aircraft %u, %s, cannot be planned: %s; it %s instead\n",
                        static_cast<unsigned>(a.index), because.c_str(), e.what(),
                        as_before.c_str());
        }
        std::fflush(stdout);
    }

    bool runways_ready() const {
        return runways_.valid() &&
               runways_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    // **The hand-over's question, asked**: what it is told read here on the
    // stepping thread - the aircraft's properties are for this thread alone -
    // but for the runways near it, looked up on the question's own, from
    // the world's runways, read by now.
    void ask_hand_over(Aircraft& a) {
        glideslope::copilot::Situation now = situation_of(a);
        now.event = "the aircraft has been handed to you, " + a.handed_because;
        const std::shared_future<std::shared_ptr<const std::vector<glideslope::world::RunwayEnd>>>
            runways = runways_;
        a.hand_over_copilot->ask([now, runways]() mutable {
            now.fields = fields_near(*runways.get(), now.latitude_deg, now.longitude_deg);
            return now;
        });
        a.hand_over_asked = true;
    }

    // **A hand-over's question let go**: given up at once, and destroyed now
    // only if nothing of it is running - else kept in `retiring_` until it
    // is (hand_over_answers), so that the stepping thread never waits.
    void retire(std::shared_ptr<glideslope::copilot::Copilot> copilot) {
        if (!copilot) {
            return;
        }
        copilot->give_up();
        if (!copilot->settled()) {
            retiring_.push_back(std::move(copilot));
        }
    }

    // The flight as the server has it, for a model to be told.
    glideslope::copilot::Situation situation_of(const Aircraft& a) {
        const glideslope::sim::Aircraft& craft = *a.aircraft;
        glideslope::copilot::Situation now;
        now.latitude_deg = craft.property("position/lat-geod-deg");
        now.longitude_deg = craft.property("position/long-gc-deg");
        now.altitude_ft = craft.property("position/h-sl-ft") -
                          geoid_.undulation(now.latitude_deg, now.longitude_deg) * feet_per_metre;
        now.ground_ft =
            collision_->height_above_geoid(now.latitude_deg, now.longitude_deg) * feet_per_metre;
        now.heading_deg = craft.property("attitude/psi-deg");
        now.airspeed_kts = craft.property("velocities/vc-kts");
        now.vertical_speed_fpm = craft.property("velocities/h-dot-fps") * 60.0;
        now.engine_running = !engine_stopped(a);
        if (a.controller) {
            now.gliding_kts = a.controller->glide();
            // **The route it flies, told**: an aircraft left by its player
            // flies the server's plan file until the model answers, and the
            // model is asked about that flight, not one holding its course.
            if (const glideslope::sim::Navigator* navigating = a.controller->navigator()) {
                now.route = navigating->still_to_fly();
            }
        }
        return now;
    }

    // Runway ends within 40 km that say their elevation, nearest first: at
    // most six, as a player's copilot is told.
    static std::vector<glideslope::world::RunwayEnd>
    fields_near(const std::vector<glideslope::world::RunwayEnd>& runways, double lat, double lon) {
        std::vector<std::pair<double, const glideslope::world::RunwayEnd*>> nearby;
        for (const glideslope::world::RunwayEnd& end : runways) {
            if (std::isnan(end.elevation_ft)) {
                continue;
            }
            const double d = glideslope::sim::distance_m(lat, lon, end.latitude_deg, end.longitude_deg);
            if (d <= 40000.0) {
                nearby.emplace_back(d, &end);
            }
        }
        std::sort(nearby.begin(), nearby.end(),
                  [](const auto& x, const auto& y) { return x.first < y.first; });
        std::vector<glideslope::world::RunwayEnd> out;
        for (std::size_t i = 0; i < nearby.size() && i < 6; ++i) {
            out.push_back(*nearby[i].second);
        }
        return out;
    }

    // **The answers come in**, between two steps, never waited for: each
    // route checked again against the aircraft as it is now and flown, or
    // refused, and said.
    void hand_over_answers(std::vector<std::string>& happened) {
        // Questions let go, destroyed once nothing of them runs.
        retiring_.erase(std::remove_if(retiring_.begin(), retiring_.end(),
                                       [](const auto& c) { return c->settled(); }),
                        retiring_.end());
        for (Aircraft& a : flown_) {
            if (!a.hand_over_copilot) {
                continue;
            }
            char line[512];
            if (a.wrecked_at_s >= 0.0 || a.slot >= 0) {
                // Wrecked, or a player's again: its question is let go.
                retire(std::move(a.hand_over_copilot));
                continue;
            }
            if (!a.hand_over_asked) {
                if (runways_ready()) {
                    ask_hand_over(a);
                }
                continue;
            }
            std::optional<glideslope::copilot::Change> change;
            try {
                change = a.hand_over_copilot->answered();
            } catch (const std::exception& e) {
                std::snprintf(line, sizeof line, "aircraft %u's model did not plan it: %s",
                              static_cast<unsigned>(a.index), e.what());
                happened.emplace_back(line);
                retire(std::move(a.hand_over_copilot));
                continue;
            }
            if (!change) {
                continue;
            }
            retire(std::move(a.hand_over_copilot));
            for (const std::string& why : change->refused) {
                happened.push_back("aircraft " + std::to_string(a.index) +
                                   "'s model's answer refused: " + why);
            }
            if (change->keep) {
                std::snprintf(line, sizeof line, "aircraft %u's model answered keep",
                              static_cast<unsigned>(a.index));
                std::string kept = line;
                // **What it keeps, said**: the plan it was told it flies,
                // and the waypoint it flies on to - or that it holds.
                const glideslope::sim::Navigator* n =
                    a.controller ? a.controller->navigator() : nullptr;
                if (n != nullptr && !n->finished()) {
                    std::snprintf(line, sizeof line, ": it flies on to %s, %zu of %zu",
                                  n->plan().waypoints[n->next()].name.c_str(), n->next() + 1,
                                  n->plan().waypoints.size());
                    kept += line;
                } else {
                    kept += ": it holds its course";
                }
                happened.push_back(kept);
                continue;
            }
            glideslope::net::CopilotRoute route;
            route.glide_kts = change->glide_kts;
            std::string names;
            for (const glideslope::sim::Waypoint& w : change->plan.waypoints) {
                glideslope::net::RouteWaypoint r;
                r.name = w.name;
                r.latitude_deg = w.latitude_deg;
                r.longitude_deg = w.longitude_deg;
                r.altitude_ft = w.altitude_ft;
                r.airspeed_kts = w.airspeed_kts;
                if (w.orbit) {
                    r.orbit = glideslope::net::RouteWaypoint::Orbit{
                        w.orbit->radius_m, static_cast<std::uint8_t>(w.orbit->turns),
                        w.orbit->right};
                }
                route.waypoints.push_back(std::move(r));
                names += " " + w.name;
            }
            std::string refused;
            try {
                refused = fly_route_on(a, route);
            } catch (const std::exception& e) {
                refused = std::string("it could not be checked: ") + e.what();
            }
            if (refused.empty()) {
                // And how far its first waypoint is, for the half-minute
                // lines after to be measured from.
                const glideslope::net::RouteWaypoint& first = route.waypoints.front();
                std::snprintf(line, sizeof line,
                              "aircraft %u, %s, flies its model's route of %zu:%s, %.0f s in, "
                              "%.0f m from %s",
                              static_cast<unsigned>(a.index), a.handed_because.c_str(),
                              route.waypoints.size(), names.c_str(), now_s(),
                              glideslope::sim::distance_m(
                                  a.aircraft->property("position/lat-geod-deg"),
                                  a.aircraft->property("position/long-gc-deg"),
                                  first.latitude_deg, first.longitude_deg),
                              first.name.c_str());
            } else {
                std::snprintf(line, sizeof line, "aircraft %u: its model's route refused: %s",
                              static_cast<unsigned>(a.index), refused.c_str());
            }
            happened.emplace_back(line);
        }
    }

    // **Whether every aircraft given to the AI in the air with a model to
    // plan it is settled**: its question answered, and a route it flies said
    // to be under way at five half-minute lines. What `--until-empty` waits
    // for, besides its clients, when the server has a hand-over planner.
    bool hand_overs_settled() const {
        return std::none_of(flown_.begin(), flown_.end(), [](const Aircraft& a) {
            return a.hand_over_copilot ||
                   (!a.copilot_route.empty() && a.slot < 0 && a.planned_route_seen < 5);
        });
    }

    // **Where each aircraft on a copilot's route has got to**, a line each:
    // the waypoint it is flying to, and how far off it is.
    std::vector<std::string> copilot_progress() {
        std::vector<std::string> out;
        for (Aircraft& a : flown_) {
            if (a.copilot_route.empty() || !a.controller || a.controller->navigator() == nullptr) {
                continue;
            }
            const glideslope::sim::Navigator& n = *a.controller->navigator();
            const double lat = a.aircraft->property("position/lat-geod-deg");
            const double lon = a.aircraft->property("position/long-gc-deg");
            char line[256];
            if (n.finished()) {
                std::snprintf(line, sizeof line,
                              "aircraft %u has flown its copilot's route of %zu",
                              static_cast<unsigned>(a.index), a.copilot_route.size());
            } else {
                const glideslope::sim::Waypoint& to = n.plan().waypoints[n.next()];
                std::snprintf(line, sizeof line,
                              "aircraft %u on its copilot's route: to %s, %zu of %zu, %.0f m "
                              "from it at %.0f kt, %.0f s in",
                              static_cast<unsigned>(a.index), to.name.c_str(), n.next() + 1,
                              a.copilot_route.size(),
                              glideslope::sim::distance_m(lat, lon, to.latitude_deg,
                                                          to.longitude_deg),
                              a.aircraft->property("velocities/vc-kts"), now_s());
            }
            out.emplace_back(line);
            ++a.planned_route_seen;
        }
        return out;
    }

    // **One step of every aircraft**, which is what "the server owns them"
    // means - and **the collisions resolved**, which is the server's to decide
    // (sim/crash.hpp): each aircraft judged against the ground, and every two
    // still flying against each other. A wreck stays where it hit, not
    // stepped, for `wreck_s` of simulated time, then flies again from where it
    // started. What happened is returned, a line each, for the log.
    std::vector<std::string> step() {
        ++steps_;
        const double now_s =
            static_cast<double>(steps_) / static_cast<double>(glideslope::sim::steps_per_second);
        std::vector<std::string> happened;
        // **The failure it was told to give** (`--fail-engine-at`), once.
        if (fail_engines_at_s_ >= 0.0 && !engines_failed_ && now_s >= fail_engines_at_s_) {
            engines_failed_ = true;
            for (Aircraft& a : flown_) {
                if (a.slot >= 0 && a.wrecked_at_s < 0.0) {
                    a.aircraft->fail_engine(0, false);
                    happened.push_back("aircraft " + std::to_string(a.index) +
                                       "'s engine has stopped");
                }
            }
        }
        // **A planned aircraft whose turn to depart has come** is put on its
        // runway, under the lowest number free - into clear sky. Two due in
        // one step are not kept from each other: told to depart together
        // (`--ai-spacing 0`), the server puts them on their runways together.
        const std::size_t flying_before = flown_.size();
        for (auto it = waiting_.begin(); it != waiting_.end();) {
            const std::optional<std::uint8_t> number = free_number();
            if (it->departs_at_s > now_s || !number || !clear_to_depart(*it, flying_before, happened)) {
                ++it;
                continue;
            }
            it->index = *number;
            happened.push_back("aircraft " + std::to_string(it->index) + ", " + it->id +
                               ", takes off from " + it->own_plan->takeoff->runway.name);
            flown_.push_back(std::move(*it));
            it = waiting_.erase(it);
        }
        // **Every AI aircraft kept clear of the others' heights**, through
        // its autopilot, before anything is flown this step.
        keep_apart(happened);
        for (Aircraft& a : flown_) {
            if (a.wrecked_at_s >= 0.0) {
                if (now_s - a.wrecked_at_s >= wreck_s && may_fly_again(a, happened)) {
                    fly_again(a);
                    happened.push_back("aircraft " + std::to_string(a.index) + ", " + a.id +
                                       ", flies again");
                }
                continue;
            }
            if (a.controller) {
                // The AI pilot: its autopilot and navigator decide the
                // controls, which is what "the LLM plans, the controllers
                // fly" means at this end. A player's aircraft given back to
                // them has one too, flying their inputs, and carrying the
                // controls across from the AI's without a step.
                if (a.slot >= 0) {
                    a.controller->set_pilot(a.held);
                }
                a.aircraft->set_controls(a.controller->fly());
            } else {
                a.aircraft->set_controls(a.held);
            }
            a.aircraft->step();
            a.most_roll_deg =
                std::max(a.most_roll_deg, std::abs(a.aircraft->state().roll_deg));
            if (const auto why = a.judge.judge(*a.aircraft)) {
                wreck(a, now_s, *why, happened);
            }
            if (a.own_plan && a.on_plan) {
                follow(a);
            }
            learnt_at_gate(a);
            learnt_landed(a, happened);
        }
        // What the hand-overs' models have answered, if they have.
        hand_over_answers(happened);
        // Where each copilot's route has got to, every half minute.
        if (steps_ % (30 * glideslope::sim::steps_per_second) == 0) {
            for (std::string& line : copilot_progress()) {
                happened.push_back(std::move(line));
            }
        }
        // How close every two AI aircraft have come, for the end to say.
        measure_apart();
        // Every two still flying, closer than the mean of their wingspans.
        for (std::size_t i = 0; i < flown_.size(); ++i) {
            for (std::size_t j = i + 1; j < flown_.size(); ++j) {
                Aircraft& a = flown_[i];
                Aircraft& b = flown_[j];
                if (a.wrecked_at_s >= 0.0 || b.wrecked_at_s >= 0.0) {
                    continue;
                }
                const glideslope::world::Ecef pa = where(a);
                const glideslope::world::Ecef pb = where(b);
                const double at_a[3] = {pa.x, pa.y, pa.z};
                const double at_b[3] = {pb.x, pb.y, pb.z};
                if (glideslope::sim::collided(at_a, a.span_ft, at_b, b.span_ft)) {
                    wreck(a, now_s, "collided with aircraft " + std::to_string(b.index),
                          happened);
                    wreck(b, now_s, "collided with aircraft " + std::to_string(a.index),
                          happened);
                }
            }
        }
        return happened;
    }

    struct Aircraft {
        std::string id; // as the dashboard names it: "c172p (AI 1)"
        std::unique_ptr<glideslope::sim::Aircraft> aircraft;
        std::unique_ptr<glideslope::sim::Controller> controller; // null: flown by hand
        // **The server's number for it**, and what a state update carries. A
        // player given an aircraft is given the lowest number no aircraft has,
        // which is below `most_slots`; an aircraft nobody is flying gets the
        // lowest free from `most_slots` up (`free_number`). A number changes
        // only when a player's aircraft goes to the AI - left, or left behind
        // by a take-over - because the player's number would be mistaken for
        // the next player's; an aircraft taken over keeps its number.
        std::uint8_t index = 0;
        int slot = -1; // -1: not a person's
        // **What it is being flown by, between one input and the next.** A
        // control is a position, not an event: a stick held over stays over
        // until it is moved, and the aircraft steps 120 times a second while
        // inputs arrive 30 times a second. So the last thing said is held
        // and applied at every step.
        glideslope::sim::Controls held;
        // The furthest it has banked, either way: what a test reads to know
        // that the inputs meant for it were the ones it flew by.
        double most_roll_deg = 0.0;
        // **Where it started, and what it crashes by**: flown again from
        // `start` after a wreck; judged against the ground by `judge`, and
        // against other aircraft by its wingspan.
        glideslope::sim::InitialConditions start{};
        double span_ft = 0.0;
        glideslope::sim::GroundJudge judge{false};
        // What it is, as the catalogue and JSBSim name it: what a client
        // is told (`AIRCRAFT`) so that it can draw it, and fly its own.
        std::string catalogue_id{};
        std::string model{};
        // When it was wrecked, on the simulation's clock, or below nought
        // while it flies.
        double wrecked_at_s = -1.0;
        int wrecks = 0;
        // Flying the server's plan, as an AI aircraft does; otherwise an
        // aircraft with a controller holds the course it started on.
        bool on_plan = false;
        // Which AI aircraft it was made as, from 1; 0 for any other.
        int ai_number = 0;
        // **Its own plan, a model's**, flown instead of the server's, and
        // which model planned it; and, planned to take off, when it does, on
        // the simulation's clock.
        std::optional<glideslope::sim::FlightPlan> own_plan{};
        std::string planned_by{};
        double departs_at_s = 0.0;
        // A wreck whose time is up, kept on the ground until its runway is
        // clear, and said so once.
        bool waits_for_runway = false;
        // **Kept apart** (sim/separation.hpp): which aircraft it is held
        // clear of, by number, while a limit on its height binds - said when
        // it begins and ends; a departure held until the sky over its runway
        // is clear, said once; and a serial, the order it was made in, which
        // its closest approaches are kept by. Who gives way to whom is not
        // this but the order in the sky, `flown_`'s.
        int held_clear_of = -1;
        // How far above the plan file's heights it flies that plan.
        double stack_ft = 0.0;
        // **Put on a course by the operator** (`--fly`), which it holds
        // exactly: given way to, as a person's aircraft is, never moved off
        // it, and not measured - it is how a test builds a collision.
        bool operators_course = false;
        bool waits_to_depart = false;
        std::uint64_t serial = 0;
        glideslope::sim::DepartureSpeeds departure{};
        // How its plan went, for the end of the run to say.
        PlanProgress progress{};
        // **Flying its player's copilot's route** (`COPILOT_ROUTE`): the
        // waypoints' names, as the route gave them, for the log to say
        // where it has got to.
        std::vector<std::string> copilot_route{};
        // **What a copilot's route is checked against**: its speeds, worked
        // out once from its published figures when it is made (learn_speeds)
        // - or why there are none, for an aircraft whose figures give no
        // stall speed or climb rate, whose routes are all refused.
        std::optional<glideslope::copilot::Brief> brief{};
        std::string no_brief{};
        // **Planned by the server's model, given to the AI in the air**
        // (`--hand-over-planner`): the question out, why it was given, and
        // whether its route has been said to be flown five times - what a server
        // waiting on its events waits for (hand_overs_settled).
        std::shared_ptr<glideslope::copilot::Copilot> hand_over_copilot{};
        bool hand_over_asked = false; // or waiting for the world's runways
        std::string handed_because{};
        int planned_route_seen = 0;
        // **Handed to the learnt landing**: the runway it lands on, and
        // whether how it landed has been said.
        std::string learnt_runway{};
        bool learnt_said = false;
        // An AI's approach handed to the learnt landing at its gate, said
        // and announced once (`--ai-on-final`).
        bool learnt_announced = false;
        // **Put on final by the operator** (`--ai-on-final`): the runway it
        // lands on, and is flown down to again if it flies again.
        std::optional<glideslope::sim::Runway> on_final_to{};
    };

    void fail_engines_at(double s) {
        fail_engines_at_s_ = s;
    }
    // **Whether any of its engines has stopped**, as the state update says
    // (`net::Condition::engine_stopped`).
    static bool engine_stopped(const Aircraft& a) {
        return a.aircraft->any_engine_stopped();
    }
    // Whether an AI pilot has it, rather than a person - a controller of its
    // own is not enough to say: a player's aircraft given back keeps one.
    static bool ai_flying(const Aircraft& a) {
        return a.controller && a.controller->flying() == glideslope::sim::Controller::Flying::ai;
    }
    // **How long a wreck stays a wreck**, in simulated seconds, before it
    // flies again (REQUIREMENTS.md 6.4: a crash costs the flight, not the
    // session).
    static constexpr double wreck_s = 5.0;
    // **How far from its threshold anything flying keeps a planned aircraft
    // from taking off again**, metres, in a straight line: an aeroplane just
    // gone is this far along its climb, not on the runway. See `may_fly_again`.
    static constexpr double runway_clear_m = 2000.0;

    const std::vector<Aircraft>& flown() const { return flown_; }

    // **A player takes over an aircraft the AI is flying** - Phase 7's ride
    // along, then take the controls. The aircraft taken becomes theirs: their
    // slot, their inputs, the controls brought from the AI's to theirs at a
    // hand's pace, as a take-back is. The aircraft they had goes to the AI
    // pilot, holding what it is doing, and - as one left behind by a player
    // who goes - takes a number of the AI's, so that a number below the
    // players' is never an AI's. Returns the player's aircraft's number now,
    // or why not: the aircraft is not an AI's, or is a wreck, or there is no
    // such player.
    std::variant<std::uint8_t, std::string> take_over(std::uint8_t player_index,
                                                      std::uint8_t ai_index) {
        Aircraft* player = nullptr;
        Aircraft* taken = nullptr;
        for (Aircraft& a : flown_) {
            if (a.index == player_index && a.slot >= 0) player = &a;
            if (a.index == ai_index) taken = &a;
        }
        if (player == nullptr) return std::string("no such player's aircraft");
        if (taken == nullptr) return std::string("no aircraft " + std::to_string(ai_index));
        if (taken->slot >= 0) return std::string("aircraft " + std::to_string(ai_index) + " is a player's");
        if (!ai_flying(*taken)) return std::string("aircraft " + std::to_string(ai_index) + " is not the AI's");
        if (taken->wrecked_at_s >= 0.0 || player->wrecked_at_s >= 0.0) {
            return std::string("a wreck cannot be taken over, or leave");
        }
        const std::optional<std::uint8_t> number = free_number();
        if (!number) {
            return std::string("no number is free for the aircraft left");
        }
        const double now_s =
            static_cast<double>(steps_) / static_cast<double>(glideslope::sim::steps_per_second);
        // The one taken: the player's now, flown by their inputs.
        taken->slot = player->slot;
        taken->id = taken->catalogue_id + " (slot " + std::to_string(player->slot) + ", taken over)";
        taken->on_plan = false;
        // A model's route it was flying, and a question out for it, are
        // the AI's, and go with it.
        taken->copilot_route.clear();
        retire(std::move(taken->hand_over_copilot));
        taken->held = player->held;
        taken->controller->set_pilot(taken->held);
        taken->controller->to_pilot();
        // The one left: the AI's now, holding what it is doing, and numbered as
        // the AI's are.
        player->slot = -1;
        player->id = player->catalogue_id + " (AI, left by a take-over)";
        if (!player->controller) {
            player->controller =
                controller_for(*player->aircraft, player->model, player->held);
        }
        player->controller->to_ai();
        player->index = *number;
        plan_hand_over(*player, "left by a take-over", "holds its course");
        announced_.push_back({taken->index, glideslope::net::Controller::person, now_s});
        announced_.push_back({player->index, glideslope::net::Controller::ai, now_s});
        std::printf("aircraft %u taken over; aircraft %u, left, now the AI's\n",
                    static_cast<unsigned>(taken->index), static_cast<unsigned>(player->index));
        std::fflush(stdout);
        return taken->index;
    }

    // **The swaps made since last asked**, for every client to be told.
    std::vector<glideslope::net::ControllerSwap> announced() {
        std::vector<glideslope::net::ControllerSwap> out;
        out.swap(announced_);
        return out;
    }

    // **What every aircraft is**, by its number: what each client is told,
    // once, as a reliable `AIRCRAFT` message.
    std::vector<glideslope::net::AircraftDefinition> who() const {
        std::vector<glideslope::net::AircraftDefinition> out;
        for (const Aircraft& a : flown_) {
            out.push_back({a.index, a.catalogue_id, a.model});
        }
        return out;
    }

    // **Where the controls of the aircraft numbered `index` are**, as its
    // flight model has them, for a client riding along in it; nothing if
    // there is no such aircraft.
    std::optional<glideslope::net::Watched> controls_of(std::uint8_t index) const {
        for (const Aircraft& a : flown_) {
            if (a.index != index) {
                continue;
            }
            const glideslope::sim::Aircraft& m = *a.aircraft;
            glideslope::net::Watched w;
            w.aircraft = index;
            w.aileron = m.property("fcs/aileron-cmd-norm");
            w.elevator = -m.property("fcs/elevator-cmd-norm");
            w.rudder = m.property("fcs/rudder-cmd-norm");
            w.throttle = m.property("fcs/throttle-cmd-norm[0]");
            w.flaps = m.property("fcs/flap-cmd-norm");
            if (m.gear_retracts()) {
                w.gear = m.property("gear/gear-cmd-norm");
            }
            if (m.speedbrakes()) {
                w.speedbrake = m.property("fcs/speedbrake-cmd-norm");
            }
            return w;
        }
        return std::nullopt;
    }

    // **The motion of the aircraft numbered `index`**, for its client's
    // prediction to be put right by, or nothing if there is no such aircraft.
    std::optional<glideslope::net::OwnMotion> motion_of(std::uint8_t index) const {
        for (const Aircraft& a : flown_) {
            if (a.index != index) {
                continue;
            }
            const glideslope::sim::Motion m = a.aircraft->motion();
            glideslope::net::OwnMotion out;
            out.x_m = m.location_ecef_m[0];
            out.y_m = m.location_ecef_m[1];
            out.z_m = m.location_ecef_m[2];
            for (std::size_t i = 0; i < 4; ++i) {
                out.attitude[i] = static_cast<float>(m.attitude_local[i]);
            }
            for (std::size_t i = 0; i < 3; ++i) {
                out.uvw_mps[i] = static_cast<float>(m.uvw_mps[i]);
                out.pqr_radps[i] = static_cast<float>(m.pqr_radps[i]);
            }
            return out;
        }
        return std::nullopt;
    }
    int ai() const { return ai_; }
    const std::vector<Aircraft>& waiting() const { return waiting_; }

    // **How a model's plan went**, in a line, or empty for an aircraft not
    // flying one. Heights are above sea level, as the plan's are.
    static std::string progress_of(const Aircraft& a) {
        if (!a.own_plan) {
            return {};
        }
        const PlanProgress& p = a.progress;
        char line[512];
        std::string out = "planned by " + a.planned_by;
        if (p.handed_over_ft >= 0.0) {
            std::snprintf(line, sizeof line, "; took off from %s, handed over %.0f ft above it",
                          a.own_plan->takeoff->runway.name.c_str(), p.handed_over_ft);
            out += line;
        }
        if (!p.orbit.empty() && p.farthest_m > 0.0) {
            std::snprintf(line, sizeof line, "; round %s %.2f turns, %.0f to %.0f m from its centre",
                          p.orbit.c_str(), p.turns, p.nearest_m, p.farthest_m);
            out += line;
            if (p.level_from_turns >= 0.0) {
                std::snprintf(line, sizeof line, ", level from %.2f turns at %.0f to %.0f ft",
                              p.level_from_turns, p.lowest_ft, p.highest_ft);
            } else {
                std::snprintf(line, sizeof line, ", never level at its height");
            }
            out += line;
        }
        return out;
    }
    int tiles_fetched() const { return tiles_.downloads(); }

private:
    // **An AI aircraft flying a model's plan**, standing on the runway the
    // plan takes off from - its height the DEM's at the threshold, which is
    // what it stands on - and departing `departure_spacing_s_` after the one
    // planned before it. The plan's heights are above sea level; the
    // aircraft's are above the ellipsoid, so the geoid is added. **Stacked**
    // once every one is planned, and given its plan then (the constructor):
    // the first away highest, so that two models' orbits of one place are
    // not flown in one piece of sky and, departing in turn, none climbs
    // through another's.
    void add_planned(int i, glideslope::sim::FlightPlan plan, const std::string& provider,
                     int& departures) {
        const glideslope::sim::CatalogueEntry entry =
            glideslope::sim::find_aircraft(data_, plan.aircraft);
        for (glideslope::sim::Waypoint& w : plan.waypoints) {
            w.altitude_ft += geoid_.undulation(w.latitude_deg, w.longitude_deg) * feet_per_metre;
        }
        ground_the_landing(plan);
        auto aircraft = std::make_unique<glideslope::sim::Aircraft>(data_ / "jsbsim", entry.model);
        aircraft->set_terrain(ground_);
        if (session_air_) {
            aircraft->set_weather(session_air_);
        }
        glideslope::sim::InitialConditions ic;
        if (plan.takeoff) {
            glideslope::sim::Runway& runway = plan.takeoff->runway;
            runway.elevation_ft =
                collision_->height_above_ellipsoid(runway.threshold_lat_deg, runway.threshold_lon_deg) *
                feet_per_metre;
            ic.latitude_deg = runway.threshold_lat_deg;
            ic.longitude_deg = runway.threshold_lon_deg;
            ic.altitude_ft = runway.elevation_ft;
            ic.terrain_elevation_ft = runway.elevation_ft;
            ic.heading_deg = runway.heading_deg;
            ic.airspeed_kts = 0.0;
            ic.gear = 1.0;
        } else {
            // A plan checked by the planner always takes off; one that did
            // not would start at its first waypoint, as the plan file's do.
            const glideslope::sim::Waypoint& first = plan.waypoints.front();
            ic.latitude_deg = first.latitude_deg;
            ic.longitude_deg = first.longitude_deg;
            ic.altitude_ft = first.altitude_ft;
            ic.airspeed_kts = first.airspeed_kts;
            ic.gear = 0.0;
        }
        ic.engine_running = true;
        aircraft->initialize(ic);
        Aircraft a{plan.aircraft + " (AI " + std::to_string(i + 1) + ", " + provider + "'s plan)",
                   std::move(aircraft), nullptr, 0, -1, {}};
        remember_start(a, ic, entry.seaplane);
        a.catalogue_id = plan.aircraft;
        a.model = entry.model;
        learn_speeds(a);
        a.on_plan = true;
        a.ai_number = i + 1;
        a.planned_by = provider;
        a.departure = glideslope::sim::departure_speeds(data_, entry.model);
        a.departs_at_s = static_cast<double>(departures) * departure_spacing_s_;
        a.own_plan = std::move(plan);
        a.controller = controller_for(*a.aircraft, a.model, glideslope::sim::Controls{});
        ++departures;
        ++ai_;
        waiting_.push_back(std::move(a));
    }

    // A plan with every height `ft` higher.
    static glideslope::sim::FlightPlan stacked(glideslope::sim::FlightPlan plan, double ft) {
        for (glideslope::sim::Waypoint& w : plan.waypoints) {
            w.altitude_ft += ft;
        }
        return plan;
    }

    // **Its plan given to its AI pilot** from the beginning: its own, taking
    // off if it does, or the server's.
    void fly_plan(Aircraft& a) {
        a.progress = {};
        if (!a.own_plan) {
            a.controller->to_ai(stacked(plan_, a.stack_ft));
        } else if (a.own_plan->takeoff) {
            a.controller->to_ai_flying(*a.own_plan, a.departure);
            a.progress.departing = true;
        } else {
            a.controller->to_ai(*a.own_plan);
        }
    }

    // **How its plan is going**, a step at a time: when the take-off
    // autopilot hands over, and how round an orbit is flown once on its
    // circle: its height from the quarter turn, once level at it, and its
    // circle from the half turn joining it (sim::OrbitFlown says why).
    void follow(Aircraft& a) {
        PlanProgress& p = a.progress;
        const glideslope::sim::AircraftState s = a.aircraft->state();
        if (p.departing && a.controller->departure() == nullptr) {
            p.departing = false;
            p.handed_over_ft = s.altitude_ft - a.own_plan->takeoff->runway.elevation_ft;
        }
        const glideslope::sim::Navigator* navigator = a.controller->navigator();
        if (p.departing || navigator == nullptr || navigator->finished()) {
            return;
        }
        const std::vector<glideslope::sim::Waypoint>& legs = a.own_plan->waypoints;
        if (navigator->next() >= legs.size()) {
            return;
        }
        const glideslope::sim::Waypoint& to = legs[navigator->next()];
        if (!to.orbit || !navigator->circling() || navigator->turns_flown() < 0.25) {
            return;
        }
        if (p.orbit != to.name || p.leg != navigator->next()) {
            const double handed_over_ft = p.handed_over_ft;
            p = {};
            p.handed_over_ft = handed_over_ft;
            p.leg = navigator->next();
            p.orbit = to.name;
        }
        const double d =
            glideslope::sim::distance_m(s.latitude_deg, s.longitude_deg, to.latitude_deg,
                                        to.longitude_deg);
        const double ft =
            s.altitude_ft - geoid_.undulation(s.latitude_deg, s.longitude_deg) * feet_per_metre;
        p.turns = navigator->turns_flown();
        if (p.turns >= 0.5) {
            p.nearest_m = std::min(p.nearest_m, d);
            p.farthest_m = std::max(p.farthest_m, d);
        }
        if (p.level_from_turns < 0.0 &&
            std::abs(s.altitude_ft - to.altitude_ft) <= PlanProgress::level_ft) {
            p.level_from_turns = p.turns;
        }
        if (p.level_from_turns >= 0.0) {
            p.lowest_ft = std::min(p.lowest_ft, ft);
            p.highest_ft = std::max(p.highest_ft, ft);
        }
    }

    // **A controller for an aircraft, told how she lands** (from her
    // figures, once a model): a landing a player made with no approach given
    // to the AI, handed to it on its roll, is landed to the stop rather than
    // held by the plain autopilot. An aeroplane that publishes no stall speed
    // has none to tell, and keeps the plain autopilot there.
    std::unique_ptr<glideslope::sim::Controller>
    controller_for(const glideslope::sim::Aircraft& aircraft, const std::string& model,
                   const glideslope::sim::Controls& controls) {
        auto controller = std::make_unique<glideslope::sim::Controller>(aircraft, controls);
        auto it = lands_with_.find(model);
        if (it == lands_with_.end()) {
            std::optional<glideslope::sim::ApproachSpeeds> speeds;
            try {
                speeds = glideslope::sim::landing_speeds(data_, model);
            } catch (const std::exception& e) {
                // No figures to read: nothing to tell her controller, and
                // said, not swallowed.
                std::fprintf(stderr, "  %s: no approach speeds for a landing taken over on "
                                     "its roll (%s)\n", model.c_str(), e.what());
            }
            it = lands_with_.emplace(model, speeds).first;
        }
        if (it->second) {
            controller->lands_with(*it->second);
        }
        // And the learnt landing, where she has one, for a plan that ends in
        // a landing (`land`).
        controller->lands_learnt(learnt_for(model));
        // **And on which runway**, from the world's (the collision ground's
        // own), braked for what is left of it.
        controller->finds_runways_with(
            [this](const glideslope::sim::Aircraft& rolling) {
                return glideslope::world::runway_rolled_on(
                    collision_->runways(), rolling, [this](double lat, double lon) {
                        return collision_->height_above_ellipsoid(lat, lon);
                    });
            });
        return controller;
    }

    // **An aircraft's speeds, worked out once** as it is made, and kept by
    // catalogue id - which names its model and its cruise both: reading them parses its figures, which is not for the stepping
    // thread to do at every route. An aircraft whose figures give none -
    // the 747-400 publishes no rate of climb - keeps why.
    void learn_speeds(Aircraft& a) {
        auto it = speeds_.find(a.catalogue_id);
        if (it == speeds_.end()) {
            Speeds learnt;
            try {
                learnt.brief = glideslope::frontend::brief_for(data_, a.catalogue_id);
            } catch (const std::exception& e) {
                learnt.why = e.what();
            }
            it = speeds_.emplace(a.catalogue_id, learnt).first;
        }
        a.brief = it->second.brief;
        a.no_brief = it->second.why;
    }

    void remember_start(Aircraft& a, const glideslope::sim::InitialConditions& ic,
                        bool alights_on_water) {
        a.serial = ++made_;
        a.start = ic;
        a.span_ft = a.aircraft->figures().wingspan_ft;
        a.judge = glideslope::sim::GroundJudge(alights_on_water);
    }

    static glideslope::world::Ecef where(const Aircraft& a) {
        const glideslope::sim::AircraftState s = a.aircraft->state();
        return glideslope::world::to_ecef(glideslope::world::Geodetic{
            s.latitude_deg, s.longitude_deg, s.altitude_ft / feet_per_metre});
    }

    static void wreck(Aircraft& a, double now_s, const std::string& why,
                      std::vector<std::string>& happened) {
        if (a.wrecked_at_s >= 0.0) {
            return;
        }
        a.wrecked_at_s = now_s;
        ++a.wrecks;
        happened.push_back("aircraft " + std::to_string(a.index) + ", " + a.id +
                           ", is a wreck: " + why);
    }

    // **The monitor** (sim/separation.hpp), once a step: every aircraft
    // flying, in the order they were put in the sky, as it sees them; the
    // limits it gives, put on the AI's autopilots; and an aircraft held off
    // its own height said when that begins and when it ends. A person's
    // aircraft, and one taking off or landing, is given no limit and is kept
    // clear of by the rest.
    void keep_apart(std::vector<std::string>& happened) {
        std::vector<glideslope::sim::Traffic> traffic;
        std::vector<Aircraft*> who;
        for (Aircraft& a : flown_) {
            if (a.wrecked_at_s >= 0.0) {
                continue;
            }
            const glideslope::sim::AircraftState s = a.aircraft->state();
            glideslope::sim::Traffic t;
            t.latitude_deg = s.latitude_deg;
            t.longitude_deg = s.longitude_deg;
            t.altitude_ft = s.altitude_ft;
            t.north_fps = a.aircraft->property("velocities/v-north-fps");
            t.east_fps = a.aircraft->property("velocities/v-east-fps");
            t.climb_fpm = s.climb_rate_fpm;
            t.ground_ft = s.terrain_elevation_ft;
            t.gives_way =
                ai_flying(a) && a.controller->autopilot_flying() && !a.operators_course;
            if (t.gives_way) {
                t.held_ft = a.controller->autopilot()->modes().altitude_ft;
            }
            traffic.push_back(t);
            who.push_back(&a);
        }
        const std::vector<glideslope::sim::HeightLimit> limits =
            glideslope::sim::separate(traffic);
        for (std::size_t k = 0; k < who.size(); ++k) {
            Aircraft& a = *who[k];
            const glideslope::sim::Traffic& t = traffic[k];
            const glideslope::sim::HeightLimit& limit = limits[k];
            if (a.controller) {
                a.controller->limit_height(limit.floor_ft, limit.ceiling_ft);
            }
            // Whether the limit holds it off where it would go.
            bool binds = false;
            if (t.held_ft) {
                const double h = std::clamp(*t.held_ft, limit.floor_ft.value_or(-1e9),
                                            limit.ceiling_ft.value_or(1e9));
                binds = std::abs(h - *t.held_ft) > 1.0;
            }
            binds = binds || (limit.ceiling_ft && t.altitude_ft > *limit.ceiling_ft) ||
                    (limit.floor_ft && t.altitude_ft < *limit.floor_ft);
            const int clear_of = binds && limit.clear_of
                                     ? static_cast<int>(who[*limit.clear_of]->index)
                                     : -1;
            if (clear_of == a.held_clear_of) {
                continue;
            }
            a.held_clear_of = clear_of;
            char line[256];
            if (clear_of < 0) {
                std::snprintf(line, sizeof line, "aircraft %u, %s, flies its own height again",
                              static_cast<unsigned>(a.index), a.id.c_str());
            } else {
                // Said above sea level, as plans are.
                const double undulation_ft =
                    geoid_.undulation(t.latitude_deg, t.longitude_deg) * feet_per_metre;
                const bool below = limit.ceiling_ft.has_value();
                std::snprintf(line, sizeof line,
                              "aircraft %u, %s, is held %s %.0f ft, clear of aircraft %d",
                              static_cast<unsigned>(a.index), a.id.c_str(),
                              below ? "below" : "above",
                              (below ? *limit.ceiling_ft : *limit.floor_ft) - undulation_ft,
                              clear_of);
            }
            happened.emplace_back(line);
        }
    }

    // **How close every two AI aircraft have come**, a step at a time: in a
    // straight line, in height while within the horizontal minimum, and for
    // how long within both minima at once - separation lost. A person's
    // aircraft is not measured: nothing keeps a person clear.
    void measure_apart() {
        ++measured_steps_;
        const double now = now_s();
        for (std::size_t i = 0; i < flown_.size(); ++i) {
            for (std::size_t j = i + 1; j < flown_.size(); ++j) {
                const Aircraft& a = flown_[i];
                const Aircraft& b = flown_[j];
                if (a.slot >= 0 || b.slot >= 0 || a.operators_course || b.operators_course ||
                    a.wrecked_at_s >= 0.0 || b.wrecked_at_s >= 0.0) {
                    continue;
                }
                const bool a_first = a.serial < b.serial;
                const Aircraft& first = a_first ? a : b;
                const Aircraft& second = a_first ? b : a;
                Approach& ap = approaches_[{first.serial, second.serial}];
                if (ap.first.empty()) {
                    ap.first = first.id;
                    ap.second = second.id;
                }
                ++ap.steps;
                const glideslope::sim::AircraftState sa = a.aircraft->state();
                const glideslope::sim::AircraftState sb = b.aircraft->state();
                const glideslope::world::Ecef pa = where(a);
                const glideslope::world::Ecef pb = where(b);
                ap.closest_m = std::min(
                    ap.closest_m, std::hypot(pa.x - pb.x, pa.y - pb.y, pa.z - pb.z));
                const double over_ground_m = glideslope::sim::distance_m(
                    sa.latitude_deg, sa.longitude_deg, sb.latitude_deg, sb.longitude_deg);
                if (over_ground_m < glideslope::sim::Separation::minimum_m) {
                    const double height_ft = std::abs(sa.altitude_ft - sb.altitude_ft);
                    ap.least_ft_within = std::min(ap.least_ft_within, height_ft);
                    if (height_ft < glideslope::sim::Separation::minimum_ft) {
                        if (ap.lost_steps++ == 0) {
                            ap.first_lost_s = now;
                        }
                    }
                }
            }
        }
    }

    // **A planned aircraft takes off only into clear sky**: nothing flying
    // within the horizontal minimum of its threshold and within the vertical
    // minimum and the margin of its height. Said once while it waits.
    // Only the first `flying` of the aircraft in the sky are looked at: those
    // there before this step's departures.
    bool clear_to_depart(Aircraft& a, std::size_t flying, std::vector<std::string>& happened) {
        for (std::size_t k = 0; k < flying && k < flown_.size(); ++k) {
            const Aircraft& b = flown_[k];
            if (b.wrecked_at_s >= 0.0) {
                continue;
            }
            const glideslope::sim::AircraftState s = b.aircraft->state();
            const double over_ground_m = glideslope::sim::distance_m(
                s.latitude_deg, s.longitude_deg, a.start.latitude_deg, a.start.longitude_deg);
            if (over_ground_m < glideslope::sim::Separation::minimum_m &&
                std::abs(s.altitude_ft - a.start.altitude_ft) <
                    glideslope::sim::Separation::minimum_ft +
                        glideslope::sim::Separation::margin_ft) {
                if (!a.waits_to_depart) {
                    a.waits_to_depart = true;
                    happened.push_back(a.id + " waits to take off until aircraft " +
                                       std::to_string(b.index) + " is clear of " +
                                       a.own_plan->takeoff->runway.name);
                }
                return false;
            }
        }
        return true;
    }

public:
    // **How close every two AI aircraft came**, a line each, and a line for
    // them all: what a run is checked by.
    std::vector<std::string> apart_report() const {
        std::vector<std::string> out;
        std::int64_t lost = 0;
        for (const auto& [serials, ap] : approaches_) {
            char line[512];
            char within[96];
            if (ap.least_ft_within < 1e17) {
                std::snprintf(within, sizeof within, "within 1.5 nm at least %.0f ft apart in height",
                              ap.least_ft_within);
            } else {
                std::snprintf(within, sizeof within, "never within 1.5 nm");
            }
            std::snprintf(line, sizeof line,
                          "apart: %s and %s, over %lld steps, came within %.0f m, %s, "
                          "separation lost for %.2f s",
                          ap.first.c_str(), ap.second.c_str(), static_cast<long long>(ap.steps),
                          ap.closest_m, within,
                          static_cast<double>(ap.lost_steps) /
                              static_cast<double>(glideslope::sim::steps_per_second));
            if (ap.lost_steps > 0) {
                std::snprintf(line + std::strlen(line), sizeof line - std::strlen(line),
                              ", first at %.1f s", ap.first_lost_s);
            }
            out.emplace_back(line);
            lost += ap.lost_steps;
        }
        char line[256];
        std::snprintf(line, sizeof line,
                      "kept apart: %zu pairs of AI aircraft over %lld steps, separation lost for "
                      "%lld steps",
                      approaches_.size(), static_cast<long long>(measured_steps_),
                      static_cast<long long>(lost));
        out.emplace_back(line);
        return out;
    }

private:
    // **Whether a wreck whose time is up may fly again now.** Anything but a
    // planned aircraft that takes off may. That one starts again on its
    // runway's threshold, and two planned aircraft may well have the same
    // one - the CBD orbit's recordings both take off from 16R. Two wrecked
    // together, say by colliding, would be put back on one point in one
    // step and collide again, every `wreck_s`, for ever. So it waits, a
    // wreck, until nothing flying is within `runway_clear_m` of its
    // threshold. The aircraft are gone through in turn, and one that flies
    // again is flying at once, so of two due in one step the second sees the
    // first on the runway and waits for it to go.
    bool may_fly_again(Aircraft& a, std::vector<std::string>& happened) const {
        if (!a.on_plan || !a.own_plan || !a.own_plan->takeoff) {
            return true;
        }
        const glideslope::world::Ecef threshold =
            glideslope::world::to_ecef(glideslope::world::Geodetic{
                a.start.latitude_deg, a.start.longitude_deg, a.start.altitude_ft / feet_per_metre});
        for (const Aircraft& b : flown_) {
            if (&b == &a || b.wrecked_at_s >= 0.0) {
                continue;
            }
            const glideslope::world::Ecef at = where(b);
            const double dx = at.x - threshold.x;
            const double dy = at.y - threshold.y;
            const double dz = at.z - threshold.z;
            if (std::sqrt(dx * dx + dy * dy + dz * dz) < runway_clear_m) {
                if (!a.waits_for_runway) {
                    a.waits_for_runway = true;
                    happened.push_back("aircraft " + std::to_string(a.index) + ", " + a.id +
                                       ", waits for " + a.own_plan->takeoff->runway.name +
                                       " to be clear of aircraft " + std::to_string(b.index) +
                                       " before it flies again");
                }
                return false;
            }
        }
        a.waits_for_runway = false;
        return true;
    }

    // **Flown again from where it started**: the flight model set back to its
    // start, the judge told it has not hit anything, and an AI pilot given
    // its plan again from the beginning. A player's inputs go on arriving and
    // fly it as before.
    void fly_again(Aircraft& a) {
        a.aircraft->initialize(a.start);
        a.copilot_route.clear();
        a.judge.reset();
        a.wrecked_at_s = -1.0;
        a.held_clear_of = -1;
        a.learnt_announced = false;
        if (a.on_plan) {
            a.controller = controller_for(*a.aircraft, a.model, glideslope::sim::Controls{});
            fly_plan(a);
        } else if (a.on_final_to && a.slot < 0) {
            land_on_final(a);
        } else if (ai_flying(a)) {
            hold_course(a);
        } else {
            // **Flown by its player, it flies again as theirs**: their inputs,
            // and no controller left over from having been handed to the AI
            // and taken back - which, kept, was taken for an AI's, and the
            // aircraft given to the AI with nobody told.
            a.controller.reset();
        }
    }

    // **The course it started on, held by its autopilot**: heading, height and
    // speed, from `start`.
    void hold_course(Aircraft& a) {
        a.controller = controller_for(*a.aircraft, a.model, a.held);
        a.controller->to_ai();
        glideslope::sim::AutopilotModes m = a.controller->autopilot()->modes();
        m.heading_deg = a.start.heading_deg;
        m.altitude_ft = a.start.altitude_ft;
        m.airspeed_kts = a.start.airspeed_kts;
        a.controller->autopilot()->set(m);
    }

    static glideslope::world::DemCoverage read_coverage(
        const std::filesystem::path& data) {
        std::ifstream in(data / "dem" / "coverage.txt", std::ios::binary);
        if (!in) {
            throw std::runtime_error("cannot read " +
                                     (data / "dem" / "coverage.txt").string());
        }
        return glideslope::world::DemCoverage(
            std::string(std::istreambuf_iterator<char>(in), {}));
    }

    double departure_spacing_s_;
    // Who plans an aircraft given to the AI in the air, and the world's
    // runways for it to be told of - read on a thread of their own, as the
    // server starts, only when there is such a planner.
    Planner hand_over_planner_;
    // **What a hand-over's model is told, and a recording to play back,
    // read once as the server starts** - a file not had refuses the start,
    // saying so - and never on the stepping thread at a hand-over. One
    // recording is played through in order, hand-over after hand-over.
    std::string hand_over_words_;
    std::optional<glideslope::copilot::Post> hand_over_playback_;
    std::shared_future<std::shared_ptr<const std::vector<glideslope::world::RunwayEnd>>> runways_ =
        hand_over_planner_.provider.empty()
            ? std::shared_future<std::shared_ptr<const std::vector<glideslope::world::RunwayEnd>>>{}
            : std::async(std::launch::async, [] {
                  return std::make_shared<const std::vector<glideslope::world::RunwayEnd>>(
                      glideslope::world::world_runways(glideslope::platform::cache_directory(),
                                                       glideslope::world::http_fetch()));
              }).share();
    glideslope::world::DemCoverage coverage_;
    glideslope::world::Fetch fetch_;
    glideslope::world::DownloadedTiles tiles_;
    glideslope::world::Geoid geoid_;
    std::shared_ptr<glideslope::world::CollisionGround> collision_;
    // The weather (`fly_in`): none until it is given, which is still air.
    std::shared_ptr<glideslope::world::ReportedWeather> air_;
    std::shared_ptr<glideslope::sim::Weather> session_air_;
    double weather_changed_at_s_ = 0.0;
    double previous_changed_at_s_ = 0.0;
    double weather_blend_s_ = 0.0;
    int weathers_ = 0;
    std::shared_ptr<glideslope::sim::FunctionTerrain> ground_;
    std::filesystem::path data_;
    std::vector<Aircraft> flown_;
    // **Hand-over questions let go and not yet finished**: given up, and
    // kept until nothing of them is running (`Copilot::settled`), so that
    // letting one go never waits on the stepping thread - for its model, or
    // for the world's runways it may still be waiting on.
    std::vector<std::shared_ptr<glideslope::copilot::Copilot>> retiring_;
    // AI aircraft planned to take off whose turn has not come: not in the sky.
    std::vector<Aircraft> waiting_;
    // Where a player joining starts, and in what.
    glideslope::sim::FlightPlan::Start start_;
    glideslope::sim::FlightPlan plan_;
    std::string player_model_;
    std::string player_id_;
    // Every aeroplane the catalogue holds, by id, read when the fleet is
    // made (`give`).
    std::map<std::string, glideslope::sim::CatalogueEntry> catalogue_;
    double player_airspeed_kts_ = 0.0;
    bool player_seaplane_ = false;
    std::int64_t steps_ = 0;
    struct Speeds {
        std::optional<glideslope::copilot::Brief> brief;
        std::string why;
    };
    std::map<std::string, Speeds> speeds_;
    // How each model lands, for its controllers (`controller_for`).
    std::map<std::string, std::optional<glideslope::sim::ApproachSpeeds>> lands_with_;
    std::map<std::string, std::shared_ptr<const glideslope::sim::LearntPolicy>> learnt_;
    // Where players start, on final (`--players-on-final`), or none.
    std::optional<glideslope::sim::Runway> on_final_;
    double fail_engines_at_s_ = -1.0;
    bool engines_failed_ = false;
    // **The number to give an aircraft nobody is flying**: the lowest from
    // `most_slots` up that no aircraft has, so that numbers are used again
    // rather than counted up to where they would wrap round into the
    // players' and `no_aircraft`. Nothing if every one is taken, which the
    // server's limits on aircraft keep far off.
    std::optional<std::uint8_t> free_number() const {
        for (unsigned n = glideslope::net::most_slots; n < glideslope::net::no_aircraft; ++n) {
            if (std::none_of(flown_.begin(), flown_.end(), [&](const Aircraft& a) {
                    return a.index == n;
                })) {
                return static_cast<std::uint8_t>(n);
            }
        }
        return std::nullopt;
    }
    int ai_ = 0;
    // How many aircraft have been made, for each to have a serial.
    std::uint64_t made_ = 0;
    // **How close every two AI aircraft have come**, by their serials: what
    // the end of a run says (apart_report).
    struct Approach {
        std::string first;
        std::string second;
        double closest_m = 1e18;      // in a straight line
        double least_ft_within = 1e18; // in height, while within the minimum over the ground
        std::int64_t lost_steps = 0;  // within both minima at once
        std::int64_t steps = 0;       // measured, both flying
        double first_lost_s = -1.0;
    };
    std::map<std::pair<std::uint64_t, std::uint64_t>, Approach> approaches_;
    std::int64_t measured_steps_ = 0;
    // Swaps made and not yet told to every client.
    std::vector<glideslope::net::ControllerSwap> announced_;
};

// The dashboard: who is connected, their ping and their traffic. The rows
// come from the session itself rather than from a count, so what is on
// screen is what the server would send in a `LOBBY`. Empty slots are drawn
// rather than hidden, so the table's shape is the session's player count.
// Bytes, short enough for a column: 938, 12.3k, 4.1M.
std::string in_column(std::uint64_t bytes) {
    char buffer[16];
    if (bytes < 1000) {
        std::snprintf(buffer, sizeof buffer, "%llu",
                      static_cast<unsigned long long>(bytes));
    } else if (bytes < 1000 * 1000) {
        std::snprintf(buffer, sizeof buffer, "%.1fk",
                      static_cast<double>(bytes) / 1000.0);
    } else {
        std::snprintf(buffer, sizeof buffer, "%.1fM",
                      static_cast<double>(bytes) / 1000000.0);
    }
    return buffer;
}

// **Where every aircraft is, as the wire carries it.** Positions come out of
// the flight model as latitude, longitude and height and go on the wire as
// ECEF metres; velocities come out as north, east and down and are turned
// into the same frame, because a client extrapolating between two states
// wants its velocity in the frame its positions are in.
// The two fields meant for one client - `your_aircraft` and
// `last_input_applied` - are left at their defaults here and filled in per
// connection, because the packet is sealed to each one separately anyway.
glideslope::net::StatePacket state_of(const Fleet& fleet, double clock_s) {
    glideslope::net::StatePacket packet;
    packet.simulation_time_s = clock_s;
    for (const Fleet::Aircraft& a : fleet.flown()) {
        if (packet.aircraft.size() >= glideslope::net::most_aircraft_in_a_state) {
            break;
        }
        const glideslope::sim::AircraftState s = a.aircraft->state();
        const glideslope::world::Geodetic where{s.latitude_deg, s.longitude_deg,
                                                s.altitude_ft / feet_per_metre};
        const glideslope::world::Ecef at = glideslope::world::to_ecef(where);
        // JSBSim gives the velocity in the local frame already, in feet a
        // second, so nothing here has to rotate out of body axes.
        const double north_mps =
            a.aircraft->property("velocities/v-north-fps") / feet_per_metre;
        const double east_mps =
            a.aircraft->property("velocities/v-east-fps") / feet_per_metre;
        const double down_mps =
            a.aircraft->property("velocities/v-down-fps") / feet_per_metre;
        const glideslope::world::Ecef v =
            glideslope::world::ned_to_ecef(where, north_mps, east_mps, down_mps);

        glideslope::net::AircraftState out;
        out.index = a.index;
        out.controller = Fleet::ai_flying(a) ? glideslope::net::Controller::ai
                                      : glideslope::net::Controller::person;
        out.condition = a.wrecked_at_s >= 0.0     ? glideslope::net::Condition::wrecked
                        : Fleet::engine_stopped(a) ? glideslope::net::Condition::engine_stopped
                                                   : glideslope::net::Condition::flying;
        if (out.condition == glideslope::net::Condition::engine_stopped) {
            out.stopped_engine =
                static_cast<std::uint8_t>(a.aircraft->first_stopped_engine().value_or(0));
        }
        out.x_m = at.x;
        out.y_m = at.y;
        out.z_m = at.z;
        out.vx_mps = static_cast<float>(v.x);
        out.vy_mps = static_cast<float>(v.y);
        out.vz_mps = static_cast<float>(v.z);
        out.heading_deg = static_cast<float>(s.heading_deg);
        out.pitch_deg = static_cast<float>(s.pitch_deg);
        out.roll_deg = static_cast<float>(s.roll_deg);
        packet.aircraft.push_back(out);
    }
    return packet;
}

// A public key as the session knows an identity: the same thirty-two bytes.
glideslope::net::IdentityKey key_of(const glideslope::net::PublicKey& who) {
    glideslope::net::IdentityKey out{};
    std::copy(who.bytes.begin(), who.bytes.end(), out.begin());
    return out;
}

// **What the dashboard shows, gathered once** for whichever draws it - the
// terminal or the window (dashboard.hpp) - so the two cannot disagree.
glideslope::server::Dashboard gather_dashboard(
    std::uint16_t port, const glideslope::net::Slots& slots,
    const std::map<std::string, Connection>& connections, const Fleet* fleet,
    const glideslope::server::Happenings& happened, double up_s, std::uint64_t datagrams,
    std::uint64_t bytes) {
    glideslope::server::Dashboard d;
    d.heading = "glideslope_server  " + std::string(glideslope::sim::version());
    char traffic[128];
    std::snprintf(traffic, sizeof traffic, "port %u   up %.0f s   %llu datagrams, %llu bytes",
                  static_cast<unsigned>(port), up_s,
                  static_cast<unsigned long long>(datagrams),
                  static_cast<unsigned long long>(bytes));
    d.traffic = traffic;
    const glideslope::net::Lobby lobby = slots.lobby();
    for (const glideslope::net::Lobby::Slot& s : lobby.slots) {
        const bool open = s.controller == glideslope::net::Controller::nobody;
        glideslope::server::DashboardSlot row;
        row.slot = static_cast<int>(s.index);
        row.who = open ? "(open)" : s.name;
        // The connection this slot's traffic is on, if there is one. An AI
        // aircraft holds a slot with nobody at the other end of a socket.
        // Looked up by key each time, not remembered: a slot is a key's rank,
        // and moves when somebody whose key sorts first arrives.
        const Connection* c = nullptr;
        for (const auto& [address, held] : connections) {
            if (slots.slot_of(key_of(held.who)) == s.index) {
                c = &held;
                row.address = address;
                break;
            }
        }
        row.ping = "-";
        if (c != nullptr && c->ping_s >= 0.0) {
            char buffer[16];
            std::snprintf(buffer, sizeof buffer, "%.0f", c->ping_s * 1000.0);
            row.ping = buffer;
        }
        row.in = c != nullptr ? in_column(c->bytes_in) : "-";
        row.out = c != nullptr ? in_column(c->bytes_out) : "-";
        d.slots.push_back(std::move(row));
    }
    if (fleet != nullptr) {
        for (const Fleet::Aircraft& a : fleet->flown()) {
            const glideslope::sim::AircraftState s = a.aircraft->state();
            char line[160];
            std::snprintf(line, sizeof line, "  %-14s %10.5f  %11.5f  %9.0f", a.id.c_str(),
                          s.latitude_deg, s.longitude_deg,
                          a.aircraft->property("position/h-agl-ft"));
            d.flying.emplace_back(line);
        }
    }
    d.happened.assign(happened.lines().begin(), happened.lines().end());
    // Short enough for the window's seventy columns.
    d.footer = "  ping is ms, round trip. Players are dropped from --window.";
    return d;
}

// The dashboard in the terminal.
void print_dashboard(const Options& o, const glideslope::server::Dashboard& d, double up_s) {
    if (o.plain) {
        // No escape codes at all, so that what a test reads is what is drawn.
        // Each pass is marked, because they follow one another down the page
        // instead of replacing each other.
        std::printf("--- dashboard at %.0f s ---\n", up_s);
    } else {
        std::printf("\033[H\033[2J");
    }
    for (const std::string& line : glideslope::server::dashboard_lines(d)) {
        std::printf("%s\n", line.c_str());
    }
    std::fflush(stdout);
}

// **A connection let go**, for going quiet, for saying it was leaving, or by
// the operator's drop button:
// its aircraft taken out of the sky or handed to an AI pilot as `--on-leave`
// says, and its slot given back. Returns the connection after it.
std::map<std::string, Connection>::iterator let_go(
    std::map<std::string, Connection>& connections,
    std::map<std::string, Connection>::iterator it, glideslope::net::Slots& slots,
    Fleet* fleet, const Options& o) {
    // **The slot and the aircraft go with the key's last proven session.**
    // Both belong to a key, not to an address, and one key may be connected
    // from two addresses for a while - a client restarting from a fresh port
    // before the server has let its old session go, or a replayed initiation
    // answered with a session nobody can use. Every session on a key shares
    // its one aircraft.
    //
    // - **A proven session remains**: nothing more goes.
    // - **The last proven one has gone**: every unproven session left on the
    //   key goes with it, and the aircraft and slot. An unproven session has
    //   shown nothing of who holds it (`Connection::proven`), so it holds
    //   nothing once the player has gone: a replay cannot keep a player's
    //   aircraft flying after their goodbye. The price is an honest restart
    //   whose first sealed datagram had not yet arrived when the old session
    //   went; its client is refused, and joins again.
    // - **An unproven session goes and others remain**: nothing more goes.
    // - **It was the key's only session**: the aircraft and slot go.
    const glideslope::net::PublicKey going = it->second.who;
    // **What it sent past its rates**, said when there was any.
    if (it->second.datagrams_past_budget > 0 || it->second.requests_past_budget > 0) {
        std::printf("%s was held to its rates: %llu of %llu sealed datagrams dropped past %.0f "
                    "a second, %llu of %llu requests ignored past %.0f a second, read over "
                    "%.3f s\n",
                    it->first.c_str(),
                    static_cast<unsigned long long>(it->second.datagrams_past_budget),
                    static_cast<unsigned long long>(it->second.datagrams),
                    glideslope::net::session_datagrams_per_second,
                    static_cast<unsigned long long>(it->second.requests_past_budget),
                    static_cast<unsigned long long>(it->second.requests),
                    glideslope::net::session_requests_per_second,
                    it->second.requests > 0
                        ? it->second.last_request_s - it->second.first_request_s
                        : 0.0);
        std::fflush(stdout);
    }
    const std::uint8_t aircraft = it->second.aircraft;
    const bool was_proven = it->second.proven;
    const std::string address = it->first;
    connections.erase(it);
    const auto on_key = [&](const auto& other) { return other.second.who == going; };
    const bool proven_left =
        std::any_of(connections.begin(), connections.end(), [&](const auto& other) {
            return on_key(other) && other.second.proven;
        });
    const bool any_left = std::any_of(connections.begin(), connections.end(), on_key);
    if (proven_left || (!was_proven && any_left)) {
        return connections.upper_bound(address);
    }
    for (auto rest = connections.begin(); rest != connections.end();) {
        if (on_key(*rest)) {
            std::printf("let go %s, unproven, with its key's last proven session\n",
                        rest->first.c_str());
            rest = connections.erase(rest);
        } else {
            ++rest;
        }
    }
    if (fleet != nullptr && aircraft != glideslope::net::no_aircraft) {
        const bool to_ai = fleet->take(aircraft, o.hand_to_ai_on_leave);
        std::printf("their aircraft %s\n",
                    to_ai ? "is now flown by an AI pilot" : "is out of the sky");
    }
    slots.release(key_of(going));
    return connections.upper_bound(address);
}

// **The server's goodbye** to one session: `LEAVING`, sealed under it,
// `leaving_copies` times, each sealed afresh, so that its client stops at once
// rather than knocking on a session that is gone.
void tell_leaving(glideslope::platform::UdpSocket& socket, const std::string& address,
                  Connection& c) {
    // An unproven session is sent nothing but its handshake answer (see
    // `Connection::proven`), a goodbye included.
    if (!c.proven) {
        return;
    }
    if (const auto to = glideslope::platform::address_of(address)) {
        const std::vector<std::uint8_t> leaving{
            static_cast<std::uint8_t>(glideslope::net::Inside::leaving)};
        for (int copy = 0; copy < glideslope::net::leaving_copies; ++copy) {
            send_sealed(socket, *to, c,
                        std::span<const std::uint8_t>(leaving.data(), leaving.size()));
        }
    }
}

// **A player dropped by the operator**, by the window's button or a test's
// `--drop-once-flown`: told so, let go, and kept out.
//
// - **Told**: `LEAVING`, sealed under its session, `leaving_copies` times,
//   each sealed afresh - the server's goodbye, so that the client stops at
//   once rather than knocking on a session that is gone.
// - **Kept out for the rest of the run**: its static key is remembered, and
//   an initiation from it is refused `DROPPED` - whatever its address, and
//   whether or not the goodbye arrived. A client comes back by itself after
//   being let go for silence; after a drop it must not, or the drop button
//   would be a kick that lasts three seconds.
void drop(glideslope::platform::UdpSocket& socket,
          std::map<std::string, Connection>& connections,
          std::map<std::string, Connection>::iterator it, glideslope::net::Slots& slots,
          Fleet* fleet, const Options& o, std::set<std::string>& dropped,
          glideslope::server::Happenings& happened, double now_s) {
    std::printf("dropped %s by the operator\n", it->first.c_str());
    std::fflush(stdout);
    happened.add(now_s, "dropped " + it->first + " by the operator");
    // **Every session on the key goes**, not only the one on the dashboard's
    // row: a key briefly on two addresses is one player, and dropping one of
    // them would leave the player flying from the other.
    const glideslope::net::PublicKey key = it->second.who;
    dropped.insert(key.text());
    for (auto each = connections.begin(); each != connections.end();) {
        if (each->second.who == key) {
            std::printf("let go %s: its key was dropped\n", each->first.c_str());
            if (!o.lose_goodbyes) {
                tell_leaving(socket, each->first, each->second);
            }
            each = let_go(connections, each, slots, fleet, o);
        } else {
            ++each;
        }
    }
}

// **The newest input a client sent, applied**, if one is waiting: what its
// aircraft is flown by from the next step. Not if an AI pilot flies it: the
// input is let go, and the client told nothing has been applied.
void apply_input(Connection& c, Fleet& fleet) {
    if (!c.input_heard) {
        return;
    }
    if (fleet.fly(c.aircraft, c.input_heard->second)) {
        c.last_input_applied = c.input_heard->first;
        c.steps_into_input = 0;
    }
    c.input_heard.reset();
}

void refuse(glideslope::platform::UdpSocket& socket,
            const glideslope::platform::Address& to, glideslope::net::Refusal why) {
    glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::refusal);
    w.u8(static_cast<std::uint8_t>(why));
    const std::vector<std::uint8_t> out = w.take();
    (void)socket.send(to, std::span<const std::uint8_t>(out.data(), out.size()));
}

// **One datagram, taken.** The envelope decides what it is; a handshake makes
// a session and a sealed body is opened under the one its address already
// has. Anything else is refused with a reason, in the clear, because there
// may be no session to seal a refusal with.
void take(glideslope::platform::UdpSocket& socket, const glideslope::net::KeyPair& mine,
          glideslope::net::Slots& slots,
          std::map<std::string, Connection>& connections, Taken& taken,
          glideslope::net::Budget& full_reads, const std::set<std::string>& dropped, Fleet* fleet,
          const glideslope::platform::Address& from,
          std::span<const std::uint8_t> datagram, double now_s, const Options& o,
          glideslope::server::Happenings& happened) {
    glideslope::net::Reader reader(datagram);
    glideslope::net::Envelope envelope;
    glideslope::net::Refusal why{};
    if (!glideslope::net::read_envelope(reader, envelope, why)) {
        refuse(socket, from, why);
        return;
    }
    const std::string who = from.text();
    const std::span<const std::uint8_t> body =
        datagram.subspan(glideslope::net::envelope_size);

    switch (envelope.type) {
    case glideslope::net::Type::handshake_initiation: {
        const auto already = connections.find(who);
        if (already != connections.end()) {
            // **An address that already has a session is not given another
            // one.** Two reasons, and they need different answers.
            //
            // The honest one: the client's answer was lost and it sent the
            // same initiation again. It gets the same answer back, and
            // nothing else changes - not the keys, not the slot, not the
            // sequence numbers.
            //
            // The other: anybody can make a valid `IK` initiation to a public
            // key, from any address they care to write on a datagram. If a
            // different initiation were taken here it would replace a live
            // player's keys, and that player would be off the server for the
            // price of one datagram. So a different one is dropped in
            // silence, and the address is usable again once the timeout
            // sweep has let the old session go.
            if (body.size() == already->second.initiation.size() &&
                std::equal(body.begin(), body.end(),
                           already->second.initiation.begin())) {
                // **Not heard from, for this.** A copy of an initiation is
                // anybody's to send, so it keeps no session alive: an
                // unproven session goes at `--timeout` after it was admitted
                // however often its initiation is resent, and a replayer
                // resending one cannot hold a slot or an aircraft.
                already->second.bytes_in += datagram.size();
                const std::vector<std::uint8_t>& out = already->second.answer;
                if (socket.send(from,
                                std::span<const std::uint8_t>(out.data(), out.size()))) {
                    already->second.bytes_out += out.size();
                }
            }
            return;
        }
        // **An initiation already taken from this address is not taken
        // again** (see `Taken`): a copy that arrives after its session has
        // gone makes nothing. Said once for each initiation, so that a test
        // can tell a copy dropped from one that never arrived.
        const auto remembered = Taken::key_of(body, who);
        if (remembered && taken.has(*remembered)) {
            if (!taken.first_said(*remembered)) {
                return;
            }
            happened.add(now_s, "dropped a copy of an initiation already taken from " + who);
            if (o.headless) {
                std::printf("dropped a copy of an initiation already taken from %s\n",
                            who.c_str());
                std::fflush(stdout);
            }
            return;
        }
        // **A full server reads an initiation only as far as its key**, and
        // only for a key already in: a player started again from a new port,
        // whose old session has not been let go, is let back in at once -
        // the new session sharing the key's slot and aircraft and taking over
        // at the first thing sealed under it that opens (below, in `sealed`)
        // - rather than refused until the old one's `--timeout` has run out.
        // A stranger's key costs one X25519 operation and is refused
        // `SERVER_FULL`, and a full server does at most
        // `full_server_reads_per_second` of those reads a second: past that
        // an initiation is refused unread, as every one was before.
        // **Whatever stops it, a full server says `SERVER_FULL`**, never
        // `BAD_HANDSHAKE`: which it said would tell a forger whether the key
        // it claimed is a player's here.
        const bool full = slots.full();
        if (full && !full_reads.take(now_s)) {
            refuse(socket, from, glideslope::net::Refusal::server_full);
            return;
        }
        glideslope::net::Responder responder(mine);
        if (full) {
            responder.only_for([&slots](const glideslope::net::PublicKey& theirs) {
                return slots.slot_of(key_of(theirs)).has_value();
            });
        }
        // **The payload is the aeroplane asked for** (net::read_asked_aircraft),
        // or nothing.
        std::vector<std::uint8_t> payload;
        const auto answer = responder.answer(body, {}, &payload);
        if (!answer) {
            if (full) {
                refuse(socket, from, glideslope::net::Refusal::server_full);
                if (responder.unwanted() && o.headless) {
                    std::printf("refused %s: the server is full, and its key read no "
                                "further than it took to know it is no player's here "
                                "(%d X25519)\n",
                                who.c_str(), responder.x25519_done());
                    std::fflush(stdout);
                }
                return;
            }
            refuse(socket, from, glideslope::net::Refusal::bad_handshake);
            return;
        }
        // **A key the operator dropped is refused**, for the rest of the run.
        if (dropped.count(answer->session.theirs.text()) > 0) {
            refuse(socket, from, glideslope::net::Refusal::dropped);
            std::printf("refused %s from %s: dropped by the operator\n",
                        answer->session.theirs.text().substr(0, 8).c_str(), who.c_str());
            std::fflush(stdout);
            return;
        }
        glideslope::net::Identity identity;
        identity.key = key_of(answer->session.theirs);
        identity.name = answer->session.theirs.text().substr(0, 8);
        identity.controller = glideslope::net::Controller::person;
        const auto slot = slots.admit(identity);
        if (!slot) {
            refuse(socket, from, glideslope::net::Refusal::server_full);
            return;
        }
        if (remembered) {
            taken.remember(*remembered);
        }
        Connection c;
        c.who = answer->session.theirs;
        c.sealing = std::make_unique<glideslope::net::Sealer>(answer->session.sending);
        c.opening =
            std::make_unique<glideslope::net::Unsealer>(answer->session.receiving);
        c.last_heard_s = now_s;
        c.admitted_s = now_s;
        c.initiation.assign(body.begin(), body.end());
        // **A slot is not an aeroplane.** A server with nothing to fly has no
        // terrain loaded and nowhere to put one, so the client gets a slot
        // and no aircraft, and its state updates say so with `no_aircraft`.
        //
        // **And a key has one aircraft, however many sessions it has.** A
        // second session for a key already connected - a client started again
        // from a new port while its old session is live, or a replayed
        // initiation - shares the aircraft the key has rather than being
        // given another. It takes over from the old session only once
        // something sealed under it opens (`proven`), which a replayer cannot
        // do; until then both are the key's, and the old one flies on.
        const auto same_key =
            std::find_if(connections.begin(), connections.end(), [&](const auto& other) {
                return other.second.who == answer->session.theirs;
            });
        if (same_key != connections.end()) {
            c.aircraft = same_key->second.aircraft;
        } else if (fleet != nullptr) {
            const auto asked = glideslope::net::read_asked_aircraft(
                std::span<const std::uint8_t>(payload.data(), payload.size()));
            c.aircraft = fleet->give(*slot, asked, !payload.empty() && !asked);
        }

        glideslope::net::Writer w =
            glideslope::net::begin(glideslope::net::Type::handshake_response);
        w.bytes(answer->message);
        c.answer = w.take();
        const std::vector<std::uint8_t>& out = c.answer;
        c.bytes_in += datagram.size();
        if (socket.send(from, std::span<const std::uint8_t>(out.data(), out.size()))) {
            c.bytes_out += out.size();
        }
        connections[who] = std::move(c);
        // **At most `most_unproven_per_key` unproven sessions on a key**, the
        // oldest let go to make room: a replayer with one captured initiation
        // and many addresses holds no more than that, and the newest - an
        // honest restart among them - is never the one refused.
        for (;;) {
            auto oldest = connections.end();
            int unproven = 0;
            for (auto each = connections.begin(); each != connections.end(); ++each) {
                if (each->second.who == answer->session.theirs && !each->second.proven) {
                    ++unproven;
                    if (each->first != who &&
                        (oldest == connections.end() ||
                         each->second.admitted_s < oldest->second.admitted_s)) {
                        oldest = each;
                    }
                }
            }
            if (unproven <= most_unproven_per_key || oldest == connections.end()) {
                break;
            }
            std::printf("let go %s, unproven, for a newer session on its key\n",
                        oldest->first.c_str());
            (void)let_go(connections, oldest, slots, fleet, o);
        }
        happened.add(now_s, "admitted " + identity.name + " to slot " +
                                std::to_string(static_cast<int>(*slot)) + " from " + who);
        if (o.headless) {
            std::printf("admitted %s to slot %d\n", identity.name.c_str(),
                        static_cast<int>(*slot));
            std::fflush(stdout);
        }
        return;
    }
    case glideslope::net::Type::sealed: {
        const auto it = connections.find(who);
        if (it == connections.end()) {
            refuse(socket, from, glideslope::net::Refusal::bad_handshake);
            return;
        }
        const auto opened = it->second.opening->open(body);
        if (!opened) {
            // A datagram that does not open is not from this client,
            // whatever its address says. It is dropped without a word: a
            // refusal would only tell a forger that the address was right.
            return;
        }
        Connection& c = it->second;
        if (!c.proven) {
            c.proven = true;
            // **A second session for a key takes over from the first here**,
            // at the first thing sealed under it that opens - proof that its
            // sender holds the initiation's ephemeral secret, which a replay
            // of a captured initiation, from any address, does not. The old
            // sessions on the key are told they are leaving, so that a client
            // still running on one stops rather than joining again and taking
            // the aircraft back; and let go, leaving the key's slot and its
            // aircraft to this one.
            for (auto other = connections.begin(); other != connections.end();) {
                if (other == it || !(other->second.who == c.who)) {
                    ++other;
                    continue;
                }
                std::printf("%s took over from %s: a new session for %s\n", who.c_str(),
                            other->first.c_str(), c.who.text().substr(0, 8).c_str());
                std::fflush(stdout);
                happened.add(now_s, who + " took over from " + other->first);
                c.aircraft = other->second.aircraft;
                tell_leaving(socket, other->first, other->second);
                other = let_go(connections, other, slots, fleet, o);
            }
        }
        c.last_heard_s = now_s;
        ++c.datagrams;
        c.bytes_in += datagram.size();
        // **What is inside the seal.** Its first byte says which kind it is:
        // one of the six in `net/inside.hpp`. A kind this version does not
        // know is ignored rather than refused - a client of a later version
        // may send one and must not be dropped for it.
        if (opened->empty() || !glideslope::net::known_inside((*opened)[0])) {
            return;
        }
        const std::span<const std::uint8_t> inside(opened->data(), opened->size());
        // **Held to its rate** (net/budget.hpp): past
        // `session_datagrams_per_second`, what opened is dropped unread. A
        // goodbye is not: it ends the session, which is what a client
        // sending too much should do.
        if (!glideslope::net::is_leaving(inside) && !c.datagram_budget.take(now_s)) {
            ++c.datagrams_past_budget;
            return;
        }
        switch (static_cast<glideslope::net::Inside>((*opened)[0])) {
        case glideslope::net::Inside::ping: {
            // Sent straight back, so that the other end can measure the trip.
            const auto token = glideslope::net::knock_token(
                glideslope::net::Inside::ping, inside);
            if (token) {
                const std::vector<std::uint8_t> pong =
                    glideslope::net::knock(glideslope::net::Inside::pong, *token);
                send_sealed(socket, from, c,
                            std::span<const std::uint8_t>(pong.data(), pong.size()));
            }
            return;
        }
        case glideslope::net::Inside::pong: {
            const auto token = glideslope::net::knock_token(
                glideslope::net::Inside::pong, inside);
            // Only the ping that is outstanding is answered. An old token, or
            // one nobody sent, says nothing about the trip and is ignored.
            if (token && c.token_sent_at_s >= 0.0 && *token == c.token) {
                c.ping_s = now_s - c.token_sent_at_s;
                c.token_sent_at_s = -1.0;
            }
            return;
        }
        case glideslope::net::Inside::inputs: {
            // **What a client is allowed to say about its own flying, and
            // the only thing.** It sends inputs, never state, so nothing here
            // can put an aircraft anywhere: the worst a client can do is fly
            // its own badly.
            if (fleet == nullptr || c.aircraft == glideslope::net::no_aircraft) {
                return;
            }
            // Only the newest is kept, and applied once this pass's steps -
            // at most `most_steps_between_looks` - are flown (apply_input).
            const auto frames = c.inputs.received(inside.subspan(1));
            for (const glideslope::net::InputFrame& frame : frames) {
                const std::uint32_t newest =
                    c.input_heard ? c.input_heard->first : c.last_input_applied;
                if (frame.sequence > newest) {
                    c.input_heard.emplace(frame.sequence,
                                          glideslope::sim::Controls::from_list(frame.controls));
                }
            }
            return;
        }
        case glideslope::net::Inside::reliable:
            // **What a client may ask**: its own aircraft handed to the AI
            // pilot or taken back (`CONTROLLER_SWAP`), an AI's aircraft taken
            // over, or which aircraft it rides along in (`WATCH`). Anything
            // else is acknowledged and let go: a client flies its own
            // aircraft, or one the AI was flying, and no player's.
            for (const std::vector<std::uint8_t>& message : c.reliable.received(inside.subspan(1))) {
                const std::span<const std::uint8_t> request(message.data(), message.size());
                glideslope::net::ControllerSwap swap;
                const bool is_swap = glideslope::net::read(request, swap);
                // **A take-over asked for**: another aircraft than its own, to
                // a person. **Every one is answered** - made, which the state
                // updates say, or refused (`TAKE_OVER_REFUSED`), to the client
                // that asked - so that a client can tell a take-over refused
                // from one still on its way, whatever refused it.
                const bool take_over_asked = is_swap && swap.aircraft != c.aircraft &&
                                             swap.to == glideslope::net::Controller::person;
                const auto refuse = [&](const std::string& reason) {
                    const std::vector<std::uint8_t> refusal =
                        glideslope::net::write(glideslope::net::TakeOverRefused{swap.aircraft});
                    const bool said = c.reliable.send(
                        std::span<const std::uint8_t>(refusal.data(), refusal.size()));
                    std::printf("aircraft %u not taken over: %s%s\n",
                                static_cast<unsigned>(swap.aircraft), reason.c_str(),
                                said ? "" : " (and the refusal could not be queued)");
                    std::fflush(stdout);
                };
                // **Held to its rate** (net/budget.hpp): past
                // `session_requests_per_second`, a request is acknowledged -
                // the stream needs it - and ignored; a take-over, refused.
                ++c.requests;
                if (c.first_request_s < 0.0) {
                    c.first_request_s = now_s;
                }
                c.last_request_s = now_s;
                if (!c.request_budget.take(now_s)) {
                    ++c.requests_past_budget;
                    if (take_over_asked) {
                        refuse("past this client's rate of requests");
                    }
                    continue;
                }
                // Which aircraft it rides along in: any, or none. It changes
                // only what this client is told.
                glideslope::net::Watch watch;
                if (glideslope::net::read(request, watch)) {
                    c.watching = watch.aircraft;
                    continue;
                }
                // **Another aircraft asked for**: taking over an AI's, if the
                // server allows it, and never a player's.
                if (take_over_asked) {
                    if (fleet == nullptr || c.aircraft == glideslope::net::no_aircraft) {
                        refuse("this client has no aircraft to leave");
                        continue;
                    }
                    if (!o.take_over) {
                        refuse("this server does not allow it");
                        continue;
                    }
                    // What this client had sent is flown by the aircraft it
                    // sent it for.
                    apply_input(c, *fleet);
                    const auto result = fleet->take_over(c.aircraft, swap.aircraft);
                    if (const auto* now = std::get_if<std::uint8_t>(&result)) {
                        // Every session on the key: they share its aircraft.
                        for (auto& [address, other] : connections) {
                            if (other.who == c.who) {
                                other.aircraft = *now;
                            }
                        }
                    } else {
                        refuse(std::get<std::string>(result));
                    }
                    continue;
                }
                // **A copilot's route**, for this client's own aircraft and
                // no other: checked here as any plan is, and flown by the
                // server's AI, or refused.
                glideslope::net::CopilotRoute route;
                if (fleet != nullptr && c.aircraft != glideslope::net::no_aircraft &&
                    glideslope::net::read(
                        std::span<const std::uint8_t>(message.data(), message.size()), route)) {
                    std::string refused = route.aircraft != c.aircraft
                                          ? std::string("it is not this client's aircraft")
                                          : std::string();
                    if (refused.empty()) {
                        apply_input(c, *fleet);
                        refused = fleet->fly_route(c.aircraft, route);
                    }
                    if (refused.empty()) {
                        std::string names;
                        for (const glideslope::net::RouteWaypoint& w : route.waypoints) {
                            names += " " + w.name;
                        }
                        std::printf("aircraft %u flies its copilot's route of %zu:%s%s, %.0f s in\n",
                                    static_cast<unsigned>(c.aircraft), route.waypoints.size(),
                                    names.c_str(),
                                    route.glide_kts
                                        ? (", gliding at " + std::to_string(std::lround(*route.glide_kts)) + " kt").c_str()
                                        : "",
                                    fleet->now_s());
                    } else {
                        std::printf("aircraft %u: a copilot's route refused: %s\n",
                                    static_cast<unsigned>(route.aircraft), refused.c_str());
                    }
                    std::fflush(stdout);
                    continue;
                }
                if (fleet != nullptr && c.aircraft != glideslope::net::no_aircraft &&
                    glideslope::net::read(
                        std::span<const std::uint8_t>(message.data(), message.size()), swap) &&
                    swap.aircraft == c.aircraft &&
                    (swap.to == glideslope::net::Controller::ai ||
                     swap.to == glideslope::net::Controller::person)) {
                    apply_input(c, *fleet);
                    (void)fleet->hand(c.aircraft, swap.to == glideslope::net::Controller::ai);
                } else if (fleet != nullptr && c.aircraft != glideslope::net::no_aircraft &&
                           glideslope::net::read(
                               std::span<const std::uint8_t>(message.data(), message.size()),
                               swap) &&
                           swap.aircraft == c.aircraft &&
                           swap.to == glideslope::net::Controller::learnt_landing) {
                    // **The learnt landing**, for this client's own aircraft
                    // and no other: handed over at its gate, or refused,
                    // saying why here and to the client that asked
                    // (`LEARNT_LANDING_REFUSED`).
                    apply_input(c, *fleet);
                    const std::string refused = fleet->hand_to_learnt(c.aircraft);
                    if (!refused.empty()) {
                        const std::vector<std::uint8_t> refusal = glideslope::net::write(
                            glideslope::net::LearntLandingRefused{c.aircraft, refused});
                        const bool said = c.reliable.send(
                            std::span<const std::uint8_t>(refusal.data(), refusal.size()));
                        std::printf("aircraft %u not handed to the learnt landing: %s%s\n",
                                    static_cast<unsigned>(c.aircraft), refused.c_str(),
                                    said ? "" : " (and the refusal could not be queued)");
                        std::fflush(stdout);
                    }
                }
            }
            return;
        case glideslope::net::Inside::state:
            // A server's to send, and never a client's: ignored.
            return;
        case glideslope::net::Inside::leaving: {
            // **A client saying it is leaving is let go at once**, exactly as
            // the timeout would have: its aircraft as `--on-leave` says, its
            // slot back. It opened under this address's session, so it is
            // this session's own client that said it - a goodbye forged from
            // another address, or sealed under another session, opens under
            // nothing here and was dropped above. A copy of it arriving after
            // this finds no session and is refused like any stranger's.
            if (!glideslope::net::is_leaving(inside)) {
                return;
            }
            const double stayed_s = now_s - c.admitted_s;
            std::printf("let go %s after it said it was leaving, %.3f s after it was "
                        "admitted\n",
                        who.c_str(), stayed_s);
            std::fflush(stdout);
            happened.add(now_s, "let go " + who + " after it said it was leaving");
            (void)let_go(connections, it, slots, fleet, o);
            return;
        }
        }
        return;
    }
    case glideslope::net::Type::handshake_response:
    case glideslope::net::Type::refusal:
        // A server is not answered to and does not refuse a refusal.
        return;
    }
}

int run(const Options& o) {
    // **The window, if one was asked for**, before anything else: a server
    // told to draw a window that cannot is refused at once, with the reason,
    // rather than running without the thing it was asked to show.
    std::unique_ptr<glideslope::server::Window> window;
    if (o.window) {
        std::string why;
        window = glideslope::server::Window::open(why);
        if (!window) {
            std::fprintf(stderr,
                         "glideslope_server: --window needs a display, and could not "
                         "open a window: %s\n",
                         why.c_str());
            return 1;
        }
        if (o.window_dump) {
            window->keep_text();
        }
        if (!o.window_shot.empty()) {
            window->keep_frame();
        }
        if (!o.window_press.empty()) {
            window->press(o.window_press);
        }
    }
    std::optional<glideslope::platform::UdpSocket> socket =
        glideslope::platform::UdpSocket::bound(o.port);
    if (!socket) {
        std::fprintf(stderr, "glideslope_server: cannot listen on port %u\n",
                     static_cast<unsigned>(o.port));
        return 1;
    }

    // **The store**, if one was asked for. It is opened before the key,
    // because the key is the first thing kept in it.
    std::optional<glideslope::platform::Store> store;
    if (!o.store.empty()) {
        try {
            store.emplace(o.store);
        } catch (const glideslope::platform::StoreError& e) {
            std::fprintf(stderr, "glideslope_server: %s\n", e.what());
            return 2;
        }
    }

    // **The server's static key.** Given with `--key`, or read back from the
    // store, or minted. A client cannot begin an `IK` handshake without the
    // public half, and it is given the key out of band - written down, pasted
    // into a command line - so a server that minted a fresh one at every
    // start would lock out every client it had. That is what the store is
    // for: mint once, keep it, and the key survives the restart.
    //
    // `--key` wins over the store and is not written to it: a key given on
    // the command line is the operator's to manage, and quietly copying it
    // into a file they did not ask for is not this program's business.
    glideslope::net::KeyPair mine;
    const char* whence = "minted";
    if (!o.key_hex.empty()) {
        const auto secret = glideslope::net::secret_from_text(o.key_hex);
        if (!secret) {
            std::fprintf(stderr, "glideslope_server: --key is not a key\n");
            return 2;
        }
        mine.secret = *secret;
        mine.publik = glideslope::net::public_from_secret(mine.secret);
        whence = "given";
    } else if (store) {
        if (const auto kept = store->get(kept_key_name)) {
            const auto secret = glideslope::net::secret_from_text(*kept);
            if (!secret) {
                std::fprintf(stderr,
                             "glideslope_server: the key kept in %s is not a key\n",
                             o.store.string().c_str());
                return 2;
            }
            mine.secret = *secret;
            mine.publik = glideslope::net::public_from_secret(mine.secret);
            whence = "kept";
        } else {
            mine = glideslope::net::mint_key_pair();
            try {
                store->set(kept_key_name, glideslope::net::secret_for_keeping(mine.secret));
            } catch (const glideslope::platform::StoreError& e) {
                std::fprintf(stderr, "glideslope_server: %s\n", e.what());
                return 2;
            }
        }
    } else {
        mine = glideslope::net::mint_key_pair();
    }
    std::printf("server key is %s\n", whence);
    std::printf("server key %s\n", mine.publik.text().c_str());
    std::fflush(stdout);

    // The session the server keeps: its slots are what the dashboard shows
    // and what a `LOBBY` would carry.
    glideslope::net::Slots slots(static_cast<std::uint8_t>(o.players));
    std::map<std::string, Connection> connections;
    Taken taken;
    glideslope::net::Budget full_reads(glideslope::net::full_server_reads_per_second);
    std::set<std::string> dropped;
    bool dropped_once = false;
    glideslope::server::Happenings happened;
    bool anyone_joined = false;
    // What the window last drew, for --window-dump.
    glideslope::server::Dashboard last_drawn;

    // The aircraft it flies. Building this reaches the network for terrain,
    // so it is not built at all when there is nothing to fly.
    std::optional<Fleet> fleet;
    if (!o.fly.empty() || o.ai > 0) {
        fleet.emplace(o.data, o.fly, o.ai, o.plan, o.planners, o.task, o.ai_spacing_s,
                      o.hand_over_planner);
        fleet->fail_engines_at(o.fail_engine_at_s);
        if (!o.ai_on_final.empty()) {
            const std::string refused = fleet->ai_on_final(o.ai_on_final);
            if (!refused.empty()) {
                std::fprintf(stderr, "glideslope_server: --ai-on-final %s: %s\n",
                             o.ai_on_final.c_str(), refused.c_str());
                return 2;
            }
        }
        if (!o.on_final.empty()) {
            const std::string refused = fleet->players_on_final(o.on_final);
            if (!refused.empty()) {
                std::fprintf(stderr, "glideslope_server: --players-on-final %s: %s\n",
                             o.on_final.c_str(), refused.c_str());
                return 2;
            }
        }
    }

    // **What every client is told the session is** (REQUIREMENTS.md 6.3):
    // the ground it collides on, and the session itself - a number of its
    // own, drawn afresh at each start, and when it began.
    const glideslope::net::TerrainDataset dataset = glideslope::frontend::collision_dataset(
        o.data.empty() ? glideslope::platform::data_directory() : o.data);
    std::printf("collision ground: %s\n", glideslope::frontend::describe(dataset).c_str());
    glideslope::net::Session session_said;
    {
        std::random_device random;
        session_said.id = (static_cast<std::uint64_t>(random()) << 32) ^ random();
        session_said.name = "glideslope_server on port " + std::to_string(socket->port());
        session_said.began_unix_ms = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    // **The weather it flies**, and sends every client: a METAR given, or a
    // station's fetched, or none - still air. A station that cannot be had
    // stops the server here, saying why, rather than flying it in air its
    // operator did not ask for.
    const auto metar_report = [&o, &fleet](const std::string& metar) {
        glideslope::world::WeatherReport r;
        r.surface.metar = glideslope::world::parse_metar(metar);
        if (o.station) {
            r.surface.latitude_deg = (*o.station)[0];
            r.surface.longitude_deg = (*o.station)[1];
            r.surface.elevation_m = (*o.station)[2];
        } else if (const auto at = fleet->first_place()) {
            r.surface.latitude_deg = at->first;
            r.surface.longitude_deg = at->second;
        }
        r.air_seed = glideslope::world::air_seed_of(r.surface.metar);
        return glideslope::frontend::fit_to_send(std::move(r));
    };
    const auto say_weather = [](const glideslope::world::WeatherReport* r, double at_s) {
        std::printf("weather at %.3f s: %s\n", at_s,
                    r != nullptr ? r->surface.metar.raw.c_str() : "still air");
        std::fflush(stdout);
    };
    std::optional<glideslope::world::WeatherFetch> weather_fetch;
    double weather_fetched_at_s = 0.0;
    bool metar_then_done = o.metar_then.empty();
    if (fleet) {
        if (!o.metar.empty()) {
            fleet->fly_in(metar_report(o.metar), o.weather_blend_s);
        } else if (!o.weather_station.empty()) {
            try {
                fleet->fly_in(glideslope::frontend::fit_to_send(glideslope::world::fetch_weather(
                                  o.weather_station,
                                  glideslope::world::utc_hour(std::chrono::system_clock::now()),
                                  glideslope::world::http_fetch())),
                              o.weather_blend_s);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "glideslope_server: the weather at %s could not be had: %s\n",
                             o.weather_station.c_str(), e.what());
                return 1;
            }
        }
        say_weather(fleet->weather(), 0.0);
    }
    // **The weather changes as it is due**: the test's METAR at its time, on
    // the session's clock, and a station's fetched again every
    // `--weather-refresh` - off this thread, so that the steps do not wait
    // on the network - and blended in from when it came.
    const auto weather_due = [&]() {
        if (!fleet) {
            return;
        }
        if (!metar_then_done && fleet->now_s() >= o.metar_then_s) {
            metar_then_done = true;
            fleet->weather_changes(metar_report(o.metar_then));
            if (!o.changed_file.empty()) {
                std::ofstream(o.changed_file) << "changed\n";
            }
            say_weather(fleet->weather(), fleet->now_s());
        }
        if (o.weather_station.empty()) {
            return;
        }
        if (weather_fetch && weather_fetch->done()) {
            try {
                fleet->weather_changes(glideslope::frontend::fit_to_send(weather_fetch->get()));
                say_weather(fleet->weather(), fleet->now_s());
            } catch (const std::exception& e) {
                std::fprintf(stderr, "glideslope_server: the weather is not updated: %s\n",
                             e.what());
            }
            weather_fetch.reset();
            weather_fetched_at_s = fleet->now_s();
        }
        if (!weather_fetch && fleet->now_s() - weather_fetched_at_s >= o.weather_refresh_s) {
            weather_fetch.emplace(o.weather_station,
                                  glideslope::world::utc_hour(std::chrono::system_clock::now()),
                                  glideslope::world::http_fetch());
        }
    };

    std::printf("listening on port %u\n", static_cast<unsigned>(socket->port()));
    std::fflush(stdout);
    // **Flying from here**: the terrain is built and the clock starts. A test
    // that must join a session already under way waits on this, not on a
    // number of seconds from launch - a debug server on a slow runner took
    // all five of them to get this far.
    if (!o.ready_file.empty()) {
        std::ofstream(o.ready_file) << "flying\n";
    }

    const auto began = std::chrono::steady_clock::now();
    auto last = began;
    glideslope::sim::FixedStep clock;
    // Steps the clock says are due and the fleet has not yet taken, and the
    // steps it has; see most_steps_between_looks.
    std::int64_t owed = 0;
    std::int64_t stepped = 0;
    // The twenty-fifth of a simulated second the last update was for.
    std::int64_t said_where_for = -1;
    std::uint64_t datagrams = 0;
    std::uint64_t bytes = 0;
    double drawn_at_s = -1.0;
    // A datagram is at most this; anything larger is not one of ours.
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    // **Simulated time, as fast as it goes** (`--steps`): nobody joins, and
    // the steps are taken as the owed steps below are.
    for (; o.steps == 0;) {
        // **Everything waiting is read on every pass**, up to a stated most.
        // One datagram a pass was 27 a second from a server behind real time
        // - four steps a pass - against four clients sending 120: the rest
        // waited in the socket or were dropped by the kernel, and a client
        // still joining went unheard long enough to be let go (CI,
        // 2026-09-26). The most keeps a flood from holding the steps up.
        constexpr std::size_t most_datagrams_a_pass = 512;
        std::size_t got = 0;
        auto now = std::chrono::steady_clock::now();
        double up_s = std::chrono::duration<double>(now - began).count();
        for (std::size_t read = 0; read < most_datagrams_a_pass; ++read) {
            glideslope::platform::Address from;
            const std::size_t one = socket->receive(into, from);
            if (one == 0) {
                break;
            }
            got += one;
            ++datagrams;
            bytes += one;
            now = std::chrono::steady_clock::now();
            up_s = std::chrono::duration<double>(now - began).count();
            take(*socket, mine, slots, connections, taken, full_reads, dropped, fleet ? &*fleet : nullptr,
                 from,
                 std::span<const std::uint8_t>(into.data(), one), up_s, o, happened);
        }

        // **The server knocks on every connection once a second**, and the
        // answer is the round trip the dashboard draws. It is a keepalive
        // too: a client that answers is a client the timeout sweep below
        // will not let go, which is why the knock is the server's job and
        // not the client's - the server is the one deciding who has gone.
        for (auto& [address, c] : connections) {
            // **Nothing to a session that has sealed nothing** but its
            // handshake answer: an answer to a replayed initiation is all a
            // replayer can make the server send to an address of its choice.
            if (!c.proven || up_s - c.pinged_at_s < ping_every_s) {
                continue;
            }
            c.pinged_at_s = up_s;
            ++c.token;
            c.token_sent_at_s = up_s;
            const std::vector<std::uint8_t> out =
                glideslope::net::knock(glideslope::net::Inside::ping, c.token);
            const auto to = glideslope::platform::address_of(address);
            if (to) {
                send_sealed(*socket, *to, c,
                            std::span<const std::uint8_t>(out.data(), out.size()));
            }
        }

        // **A test's operator** (`--drop-once-flown`): the first player whose
        // input has been flown is dropped, once.
        if (o.drop_once_flown && !dropped_once) {
            for (auto it = connections.begin(); it != connections.end(); ++it) {
                if (it->second.last_input_applied > 0) {
                    dropped_once = true;
                    drop(*socket, connections, it, slots, fleet ? &*fleet : nullptr, o,
                         dropped, happened, up_s);
                    break;
                }
            }
        }

        // **A test's restart** (`--stop-once-flown`): once a player's input
        // has been flown, the server stops where it is, saying nothing to
        // anybody, as one that falls over or is restarted under its players
        // does - once its clock is as far on as asked, so that one started
        // again is behind where it was.
        if (o.stop_once_flown_s >= 0.0 && fleet && fleet->now_s() >= o.stop_once_flown_s &&
            std::any_of(connections.begin(), connections.end(),
                        [](const auto& each) { return each.second.last_input_applied > 0; })) {
            std::printf("stopped once a player was flown, telling nobody, %.1f s in, at %.1f s "
                        "on its clock\n",
                        up_s, fleet->now_s());
            std::fflush(stdout);
            break;
        }

        // **A client that has gone quiet is let go**, which is what
        // `--timeout` is for. Its slot goes back to the session.
        for (auto it = connections.begin(); it != connections.end();) {
            if (up_s - it->second.last_heard_s > o.timeout_s) {
                std::printf("let go %s after %.1f s of silence\n", it->first.c_str(),
                            up_s - it->second.last_heard_s);
                std::fflush(stdout);
                happened.add(up_s, "let go " + it->first + " after " +
                                       std::to_string(static_cast<int>(
                                           up_s - it->second.last_heard_s)) +
                                       " s of silence");
                it = let_go(connections, it, slots, fleet ? &*fleet : nullptr, o);
            } else {
                ++it;
            }
        }

        // **The simulation steps at a fixed rate**: every step that has
        // become due is taken, and the aircraft are stepped together so that
        // they share one clock.
        if (fleet) {
            // At a set fraction of real time, for a test (`--test-pace`).
            owed += clock.advance(std::chrono::duration_cast<std::chrono::nanoseconds>(
                (now - last) * o.test_pace));
            const std::int64_t n = std::min(owed, most_steps_between_looks);
            for (std::int64_t i = 0; i < n; ++i) {
                weather_due();
                for (const std::string& line : fleet->step()) {
                    std::printf("%s\n", line.c_str());
                    std::fflush(stdout);
                    happened.add(up_s, line);
                }
                if (o.test_step_ms > 0.0) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double, std::milli>(o.test_step_ms));
                }
            }
            owed -= n;
            stepped += n;
            for (auto& connection : connections) {
                connection.second.steps_into_input += n;
                // Heard in this pass: flown from the next step.
                apply_input(connection.second, *fleet);
            }
        }
        last = now;

        // **Where everybody is, 25 times a simulated second, to everybody
        // connected.** Sent once and never repeated: a state update is worth
        // nothing once a newer one exists, so repeating a lost one would
        // deliver a stale position late.
        const std::int64_t twenty_fifth =
            stepped * states_per_second / glideslope::sim::steps_per_second;
        if (fleet && twenty_fifth > said_where_for) {
            said_where_for = twenty_fifth;
            glideslope::net::StatePacket packet =
                state_of(*fleet, static_cast<double>(stepped) /
                                     static_cast<double>(glideslope::sim::steps_per_second));
            const std::vector<glideslope::net::AircraftDefinition> who = fleet->who();
            const std::vector<glideslope::net::ControllerSwap> swaps = fleet->announced();
            const std::vector<std::uint8_t> lobby = glideslope::net::write(slots.lobby());
            for (auto& [address, c] : connections) {
                // Nothing to an unproven session (the pings above).
                if (!c.proven) {
                    continue;
                }
                const auto send = [&c](const std::vector<std::uint8_t>& body) {
                    (void)c.reliable.send(std::span<const std::uint8_t>(body.data(), body.size()));
                };
                // **What the session is, first** (REQUIREMENTS.md 6.3): the
                // ground it collides on - which a client on other ground
                // refuses, before anything is built on it - and the session.
                if (!c.told_session) {
                    c.told_session = true;
                    send(glideslope::net::write(dataset));
                    glideslope::net::Session session = session_said;
                    session.simulation_time_s = fleet->now_s();
                    send(glideslope::net::write(session));
                }
                // **The lobby**, whole, whenever it is not what was last sent.
                if (c.lobby_told != lobby) {
                    c.lobby_told = lobby;
                    send(lobby);
                }
                // **The weather it flies**, on joining and at every change,
                // with the forecast above it after it where there is one.
                if (c.weathers_told != fleet->weathers()) {
                    // **Joining mid-blend, first what it blends from**: told
                    // the newest alone, a client flew it whole until the
                    // blend ended, and its air was not the server's.
                    const glideslope::world::WeatherReport* from =
                        c.weathers_told < 0 ? fleet->weather_blending_from() : nullptr;
                    if (from != nullptr) {
                        const glideslope::frontend::WeatherSaid said_from =
                            glideslope::frontend::weather_said(
                                from, fleet->previous_weather_changed_at_s(),
                                fleet->weather_blend_s());
                        send(glideslope::net::write(said_from.weather));
                        if (said_from.aloft) {
                            send(glideslope::net::write(*said_from.aloft));
                        }
                    }
                    c.weathers_told = fleet->weathers();
                    const glideslope::frontend::WeatherSaid said_air =
                        glideslope::frontend::weather_said(fleet->weather(),
                                                           fleet->weather_changed_at_s(),
                                                           fleet->weather_blend_s());
                    send(glideslope::net::write(said_air.weather));
                    if (said_air.aloft) {
                        send(glideslope::net::write(*said_air.aloft));
                    }
                }
                // **Every aircraft introduced**, and again if its number has
                // come to mean another. Numbers no longer flying are
                // forgotten here, every update: a number taken out of the sky
                // and given to another aircraft later is always introduced
                // again, whatever that aircraft is, because between the two it
                // was not flying. The id and the model are compared as well,
                // for a number that changed hands between two updates.
                std::map<std::uint8_t, std::pair<std::string, std::string>> now_flying;
                for (const glideslope::net::AircraftDefinition& d : who) {
                    now_flying[d.aircraft] = {d.id, d.model};
                    const auto was = c.introduced.find(d.aircraft);
                    if (was == c.introduced.end() ||
                        was->second != std::make_pair(d.id, d.model)) {
                        const std::vector<std::uint8_t> body = glideslope::net::write(d);
                        (void)c.reliable.send(std::span<const std::uint8_t>(body.data(), body.size()));
                    }
                }
                c.introduced = std::move(now_flying);
                // **Every swap, to everybody**: whose aircraft, to whom, and
                // when on the simulation's clock.
                for (const glideslope::net::ControllerSwap& swap : swaps) {
                    const std::vector<std::uint8_t> body = glideslope::net::write(swap);
                    (void)c.reliable.send(std::span<const std::uint8_t>(body.data(), body.size()));
                }
                packet.your_aircraft = c.aircraft;
                packet.last_input_applied = c.last_input_applied;
                // Its own aircraft's motion, which only its own packet carries.
                packet.yours = c.aircraft != glideslope::net::no_aircraft
                                   ? fleet->motion_of(c.aircraft)
                                   : std::nullopt;
                // And how far into its newest input the server had flown it.
                if (packet.yours) {
                    packet.yours->steps_into_input = static_cast<std::uint16_t>(
                        std::min<std::int64_t>(c.steps_into_input, 65535));
                }
                // And the watched aircraft's controls, if it watches one.
                packet.watched = c.watching != glideslope::net::no_aircraft
                                     ? fleet->controls_of(c.watching)
                                     : std::nullopt;
                const auto said = glideslope::net::write_state(packet);
                const auto to = glideslope::platform::address_of(address);
                // What must arrive goes first, so that an aircraft is
                // introduced before the update that first names it - on the
                // wire at least; the network may still reorder them, and a
                // client draws nothing it has not been told the model of.
                if (to) {
                    for (const std::vector<std::uint8_t>& datagram : c.reliable.to_send(up_s)) {
                        std::vector<std::uint8_t> body{
                            static_cast<std::uint8_t>(glideslope::net::Inside::reliable)};
                        body.insert(body.end(), datagram.begin(), datagram.end());
                        send_sealed(*socket, *to, c,
                                    std::span<const std::uint8_t>(body.data(), body.size()));
                    }
                }
                if (said && to) {
                    send_sealed(*socket, *to, c,
                                std::span<const std::uint8_t>(said->data(),
                                                              said->size()));
                }
            }
        }

        // **The window is pumped every pass and drawn ten times a second**,
        // so a click is answered at once; closing it stops the server.
        if (window) {
            if (!window->pump()) {
                std::printf("the window was closed\n");
                break;
            }
            if (up_s - drawn_at_s >= window_every_s) {
                last_drawn = gather_dashboard(socket->port(), slots, connections,
                                              fleet ? &*fleet : nullptr, happened, up_s,
                                              datagrams, bytes);
                const auto to_drop = window->draw(last_drawn);
                drawn_at_s = up_s;
                if (to_drop) {
                    const auto it = connections.find(*to_drop);
                    if (it != connections.end()) {
                        drop(*socket, connections, it, slots, fleet ? &*fleet : nullptr, o,
                             dropped, happened, up_s);
                    }
                }
            }
        } else if (!o.headless && up_s - drawn_at_s >= dashboard_every_s) {
            print_dashboard(o,
                            gather_dashboard(socket->port(), slots, connections,
                                             fleet ? &*fleet : nullptr, happened, up_s,
                                             datagrams, bytes),
                            up_s);
            drawn_at_s = up_s;
        }
        if (o.seconds > 0.0 && up_s >= o.seconds) {
            break;
        }
        // **Everybody who came has gone**, for a test that waits on its
        // clients: a fixed length of time was the machine's speed's to decide,
        // and a slow runner stopped the server before its last client had
        // flown. Once somebody has joined and every connection has been let
        // go, the server stops.
        if (!connections.empty()) {
            anyone_joined = true;
        }
        // **And every aircraft given to the AI in the air is planned and
        // seen flying it** (hand_overs_settled), when a model plans them.
        if (o.until_empty && anyone_joined && connections.empty() &&
            (!fleet || fleet->hand_overs_settled())) {
            std::printf("everybody who joined has gone, %.1f s in\n", up_s);
            break;
        }
        if (got == 0) {
            // Nothing to do: the socket does not block, so without this the
            // server would spin a core for no reason.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    // **What the window showed**, for a test: every line it drew and every
    // button, and the lines the terminal would have printed for the same
    // moment, which must be the same text.
    // **A last frame, of how things ended**, so that what the window last
    // showed is the end and not a frame from before something the operator
    // pressed on it - which on a slow machine was the whole of the drop. A
    // press drawn now does nothing: the server has stopped.
    if (window) {
        const double end_s = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() - began)
                                 .count();
        last_drawn = gather_dashboard(socket->port(), slots, connections,
                                      fleet ? &*fleet : nullptr, happened, end_s,
                                      datagrams, bytes);
        (void)window->draw(last_drawn);
    }
    if (window && o.window_dump) {
        for (const std::string& line : window->drawn()) {
            std::printf("window: %s\n", line.c_str());
        }
        // The terminal's lines for the very facts the window's last frame
        // was drawn from, so that nothing but the drawing can differ.
        for (const std::string& line : glideslope::server::dashboard_lines(last_drawn)) {
            std::printf("terminal: %s\n", line.c_str());
        }
    }
    if (window && !o.window_shot.empty()) {
        if (window->shot(o.window_shot)) {
            // Its size as drawn, which is the window's unless the screen is
            // smaller: a small screen - macOS on CI - shrinks it to fit.
            std::printf("wrote %s, %d by %d\n", o.window_shot.c_str(),
                        window->frame_width(), window->frame_height());
        } else {
            std::fprintf(stderr, "glideslope_server: could not write %s\n",
                         o.window_shot.c_str());
        }
    }

    // **The steps still owed are taken**, with nobody left to answer, so that
    // what it says it flew below is the time it was given, however far behind
    // a busy machine left it.
    if (fleet) {
        if (o.steps > 0) {
            owed = o.steps;
            std::printf("taking %lld steps as fast as they go\n", static_cast<long long>(owed));
        } else {
            std::printf("was %lld step%s behind at the end\n", static_cast<long long>(owed),
                        owed == 1 ? "" : "s");
        }
        for (; owed > 0; --owed) {
            for (const std::string& line : fleet->step()) {
                std::printf("%s\n", line.c_str());
            }
            ++stepped;
        }
    }

    // **What it flew, so that a run can be checked.** One line per aircraft:
    // where it is, how high, and the ground under it - which is the thing
    // that differs between two aircraft on opposite sides of the world.
    std::printf("ran %.3f s, %lld steps\n",
                std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
                    .count(),
                static_cast<long long>(stepped));
    if (fleet) {
        for (const Fleet::Aircraft& a : fleet->flown()) {
            const glideslope::sim::AircraftState s = a.aircraft->state();
            const double agl = a.aircraft->property("position/h-agl-ft");
            // **The ground under it**, which is the thing that differs
            // between two aircraft on opposite sides of the world. Written
            // as a whole number of feet so that a test can compare it
            // without floating-point arithmetic.
            std::printf("flew %s at %.6f, %.6f  %.0f ft agl over ground %lld ft\n",
                        a.id.c_str(), s.latitude_deg, s.longitude_deg, agl,
                        static_cast<long long>(std::llround(s.altitude_ft - agl)));
            std::printf("  number %d, %s, banked as far as %.0f degrees\n",
                        static_cast<int>(a.index),
                        a.slot >= 0 ? "a player's"
                                    : (a.on_plan ? "an AI's" : "held on its course"),
                        a.most_roll_deg);
            if (const std::string went = Fleet::progress_of(a); !went.empty()) {
                std::printf("  %s\n", went.c_str());
            }
        }
        for (const Fleet::Aircraft& a : fleet->waiting()) {
            std::printf("flew %s: not yet, still waiting its turn to take off at %.0f s\n",
                        a.id.c_str(), a.departs_at_s);
        }
        std::printf("ran %d AI aircraft\n", fleet->ai());
        for (const std::string& line : fleet->apart_report()) {
            std::printf("%s\n", line.c_str());
        }
        std::printf("fetched %d terrain tile%s\n", fleet->tiles_fetched(),
                    fleet->tiles_fetched() == 1 ? "" : "s");
    }
    std::fflush(stdout);
    return 0;
}

} // namespace

static int run_program(int argc, char** argv) {
    // First: a failed assert prints and ends the program rather than
    // waiting on a dialog nobody will answer (platform/no_crash_dialogs.hpp).
    glideslope::platform::no_crash_dialogs();
    // And a write to a pipe whose reader has gone fails, rather than ending
    // the program (platform/closed_pipes.hpp).
    glideslope::platform::outlive_closed_pipes();
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    try {
        if (args.size() == 1 && args[0] == "--version") {
            const std::string_view v = glideslope::sim::version();
            std::printf("glideslope_server %.*s\n", static_cast<int>(v.size()),
                        v.data());
            return 0;
        }
        if (args.size() == 1 && args[0] == "--help") {
            print_usage(stdout);
            return 0;
        }
        std::string why;
        const std::optional<Options> o = parse(args, why);
        if (!o) {
            std::fprintf(stderr, "glideslope_server: %s\n", why.c_str());
            print_usage(stderr);
            return 2;
        }
        print_settings(*o, stdout);
        if (o->dry_run) {
            return 0;
        }
        return run(*o);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glideslope_server: %s\n", e.what());
        return 1;
    }
}

int main(int argc, char** argv) {
    // The process ends with its C runtime whole until every other thread has
    // stopped - Windows' own threads too (platform/end_process.hpp).
    glideslope::platform::end_process(run_program(argc, argv));
}
