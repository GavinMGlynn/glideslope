// glideslope_server - the server, which owns every aircraft.
//
// **What is not here yet.** The server parses its settings, binds its port
// and shows its dashboard. It does not yet fly anything, accept anyone or
// seal anything: the handshake and the sealing are still to do
// (docs/TRANSPORT.md), and flying every aircraft is its own item in
// docs/COMPLETION_PLAN.md. The dashboard therefore shows an empty session,
// which is what there is.
//
// **`--key` is accepted and checked, not minted.** A server secret is an
// X25519 static key; without libsodium there is nothing here to derive its
// public half with, and a secret whose public half cannot be printed is of
// no use to a client. So a key given is checked and kept, and a key not
// given is reported as one this build cannot mint, rather than a random
// number pretending to be one.

#include "net/slots.hpp"
#include "platform/http.hpp"
#include "platform/paths.hpp"
#include "platform/socket.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/controller.hpp"
#include "sim/navigator.hpp"
#include "sim/fixed_step.hpp"
#include "sim/terrain.hpp"
#include "sim/version.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

// The port the server listens on unless told otherwise. It is this project's
// own, high and unassigned; `docs/REQUIREMENTS.md` names no number.
constexpr std::uint16_t default_port = 47801;
// How many people may fly unless told otherwise.
constexpr int default_players = 4;
// How long a client may be silent before it is let go, seconds.
constexpr double default_timeout_s = 10.0;
// A server secret is 32 bytes, written as hexadecimal.
constexpr std::size_t key_hex_length = 64;
// How often the dashboard is drawn, and how often the settings line is
// repeated when there is no dashboard.
constexpr double dashboard_every_s = 1.0;
// The most aircraft a server flies, which is a session's four players.
constexpr std::size_t most_flown = 4;
// How many AI aircraft a server runs unless told otherwise
// (CLAUDE.md: "How many AI aircraft a server runs is a server setting,
// default 4").
constexpr int default_ai = 4;
// The most it will run, so that a number typed wrong cannot ask for
// thousands of flight models.
constexpr int most_ai = 16;
// How far apart the AI aircraft are stacked when they fly one plan, feet.
constexpr double ai_stack_ft = 500.0;

// An aircraft the server is to fly, and where it starts.
struct Flown {
    std::string id;
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
};

struct Options {
    int players = default_players;
    std::filesystem::path data;
    std::vector<Flown> fly;
    int ai = default_ai;
    std::filesystem::path plan;
    // How long to run before stopping, or nothing to run until killed.
    double seconds = 0.0;
    std::uint16_t port = default_port;
    std::filesystem::path store;
    std::string key_hex;
    double timeout_s = default_timeout_s;
    bool headless = false;
    bool dry_run = false;
};

void print_usage(std::FILE* out) {
    std::fputs(
        "usage: glideslope_server [OPTION]...\n"
        "\n"
        "  --players N        how many people may fly, 1 to 4 (default 4)\n"
        "  --port N           the UDP port to listen on (default 47801). 0 asks\n"
        "                     the system for any free port, and the server prints\n"
        "                     the one it got, which is what a test wants\n"
        "  --store FILE       where the session is kept\n"
        "  --key HEX          the server's secret, 64 hexadecimal characters\n"
        "  --timeout SECONDS  how long a client may be silent (default 10)\n"
        "  --headless         no dashboard; print the settings and run\n"
        "  --data DIR         read data from DIR instead of data/ beside the program\n"
        "  --fly ID@LAT,LON   fly this aircraft from here; may be given up to four\n"
        "                     times, and the server flies them all at 120 Hz over\n"
        "                     the collision terrain wherever on Earth they are\n"
        "  --ai N             how many AI aircraft the server runs (default 4)\n"
        "  --plan FILE        the flight plan they fly (default plans/ in the data)\n"
        "  --seconds N        stop after N seconds instead of running until killed\n"
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
        } else if (a == "--dry-run") {
            o.dry_run = true;
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
        } else if (a == "--seconds") {
            if (!next(value)) return std::nullopt;
            const auto n = number(value);
            if (!n) {
                why = "--seconds wants a number of seconds, not '" +
                      std::string(value) + "'";
                return std::nullopt;
            }
            o.seconds = *n;
        } else if (a == "--fly") {
            if (!next(value)) return std::nullopt;
            // ID@LAT,LON - the aircraft and where it starts.
            const std::string spec(value);
            const std::size_t at = spec.find('@');
            const std::size_t comma = spec.find(',', at == std::string::npos ? 0 : at);
            if (at == std::string::npos || comma == std::string::npos || at == 0) {
                why = "--fly wants ID@LAT,LON, not '" + spec + "'";
                return std::nullopt;
            }
            Flown f;
            f.id = spec.substr(0, at);
            const auto lat = number(spec.substr(at + 1, comma - at - 1));
            const auto lon = number(spec.substr(comma + 1));
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
                 o.store.empty() ? "(none: the session is not kept)"
                                 : o.store.string().c_str());
    std::fprintf(out, "key       %s\n",
                 o.key_hex.empty()
                     ? "(none: this build cannot mint one, see --help)"
                     : "given, 64 hexadecimal characters");
    std::fprintf(out, "timeout   %.3f s\n", o.timeout_s);
    std::fprintf(out, "dashboard %s\n", o.headless ? "no (--headless)" : "yes");
    std::fprintf(out, "seconds   %s\n",
                 o.seconds > 0.0 ? std::to_string(o.seconds).c_str()
                                 : "(until killed)");
    std::fprintf(out, "ai        %d aircraft\n", o.ai);
    std::fprintf(out, "plan      %s\n",
                 o.plan.empty() ? "(the data's plans/sydney-harbour.plan)"
                                : o.plan.string().c_str());
    if (o.fly.empty()) {
        std::fprintf(out, "flying    (nothing by hand)\n");
    }
    for (const Flown& f : o.fly) {
        std::fprintf(out, "flying    %s from %.6f, %.6f\n", f.id.c_str(),
                     f.latitude_deg, f.longitude_deg);
    }
}

// Feet in a metre, for putting an aircraft above the ground the DEM gives.
constexpr double feet_per_metre = 3.280839895013123;

// **The aircraft the server flies, and the ground under them.** One `Dem`
// serves them all: it caches tiles as it goes, so two aircraft on opposite
// sides of the world each pull their own and neither waits for the other's.
// `PROJECT_STATUS.md` says a `Dem` is not thread-safe; the server steps every
// aircraft on this one thread, so nothing here shares it.
class Fleet {
public:
    Fleet(const std::filesystem::path& data, const std::vector<Flown>& fly, int ai,
          const std::filesystem::path& plan_file)
        : coverage_(read_coverage(data)),
          fetch_(glideslope::world::http_fetch()),
          tiles_(glideslope::platform::cache_directory(), fetch_),
          geoid_(glideslope::world::egm2008_geoid(
              glideslope::platform::cache_directory(), fetch_)),
          dem_(std::make_shared<glideslope::world::Dem>(coverage_, tiles_, &geoid_)) {
        const std::shared_ptr<glideslope::world::Dem> dem = dem_;
        const auto ground = std::make_shared<glideslope::sim::FunctionTerrain>(
            [dem](double lat, double lon) {
                return dem->height_above_ellipsoid(lat, lon);
            },
            [dem](double lat, double lon) {
                return dem->water(lat, lon) != glideslope::world::Water::none;
            });
        for (const Flown& f : fly) {
            const glideslope::sim::CatalogueEntry entry =
                glideslope::sim::find_aircraft(data, f.id);
            auto aircraft = std::make_unique<glideslope::sim::Aircraft>(
                data / "jsbsim", entry.model);
            aircraft->set_terrain(ground);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = f.latitude_deg;
            ic.longitude_deg = f.longitude_deg;
            // Above the ground under it, wherever on Earth that is.
            ic.terrain_elevation_ft =
                dem->height_above_ellipsoid(f.latitude_deg, f.longitude_deg) *
                feet_per_metre;
            ic.altitude_ft = ic.terrain_elevation_ft + 3000.0;
            ic.airspeed_kts = entry.start_airspeed_kts;
            ic.engine_running = true;
            ic.gear = 0.0;
            aircraft->initialize(ic);
            flown_.push_back({f.id, std::move(aircraft), nullptr});
        }

        // **The AI aircraft the server runs.** They fly one plan, stacked
        // `ai_stack_ft` apart so that they are not all in the same piece of
        // sky - one plan is what there is to fly, and a server that put four
        // aeroplanes in one place would be hiding that rather than saying it.
        if (ai > 0) {
            const std::filesystem::path where =
                plan_file.empty() ? data / "plans" / "sydney-harbour.plan" : plan_file;
            std::ifstream in(where, std::ios::binary);
            if (!in) {
                throw std::runtime_error("cannot read the flight plan " +
                                         where.string());
            }
            const glideslope::sim::FlightPlan plan =
                glideslope::sim::parse_flight_plan(
                    std::string(std::istreambuf_iterator<char>(in), {}));
            const glideslope::sim::CatalogueEntry entry =
                glideslope::sim::find_aircraft(data, plan.aircraft);
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
            for (int i = 0; i < ai; ++i) {
                auto aircraft = std::make_unique<glideslope::sim::Aircraft>(
                    data / "jsbsim", entry.model);
                aircraft->set_terrain(ground);
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
                auto controller = std::make_unique<glideslope::sim::Controller>(
                    *aircraft, glideslope::sim::Controls{});
                controller->to_ai(plan);
                flown_.push_back({plan.aircraft + " (AI " + std::to_string(i + 1) + ")",
                                  std::move(aircraft), std::move(controller)});
                ++ai_;
            }
        }
    }

    // One step of every aircraft, which is what "the server owns them" means.
    void step() {
        for (Aircraft& a : flown_) {
            if (a.controller) {
                // The AI pilot: its autopilot and navigator decide the
                // controls, which is what "the LLM plans, the controllers
                // fly" means at this end.
                a.aircraft->set_controls(a.controller->fly());
            } else {
                glideslope::sim::Controls held;
                held.throttle = 0.6;
                a.aircraft->set_controls(held);
            }
            a.aircraft->step();
        }
    }

    struct Aircraft {
        std::string id;
        std::unique_ptr<glideslope::sim::Aircraft> aircraft;
        std::unique_ptr<glideslope::sim::Controller> controller; // null: flown by hand
    };
    const std::vector<Aircraft>& flown() const { return flown_; }
    int ai() const { return ai_; }
    int tiles_fetched() const { return tiles_.downloads(); }

private:
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

    glideslope::world::DemCoverage coverage_;
    glideslope::world::Fetch fetch_;
    glideslope::world::DownloadedTiles tiles_;
    glideslope::world::Geoid geoid_;
    std::shared_ptr<glideslope::world::Dem> dem_;
    std::vector<Aircraft> flown_;
    int ai_ = 0;
};

// The dashboard: who is connected, their ping and their traffic. The rows
// come from the session itself rather than from a count, so what is on
// screen is what the server would send in a `LOBBY`. There is nobody to show
// until the handshake exists, and saying so is better than an empty table
// that looks like a bug.
void draw_dashboard(const Options& o, const glideslope::net::Slots& slots,
                    const Fleet* fleet, double up_s, std::uint64_t datagrams,
                    std::uint64_t bytes) {
    std::printf("\033[H\033[2J");
    std::printf("glideslope_server  %.*s\n",
                static_cast<int>(glideslope::sim::version().size()),
                glideslope::sim::version().data());
    std::printf("port %u   up %.0f s   %llu datagrams, %llu bytes\n\n",
                static_cast<unsigned>(o.port), up_s,
                static_cast<unsigned long long>(datagrams),
                static_cast<unsigned long long>(bytes));
    std::printf("  slot  who              ping   in     out\n");
    const glideslope::net::Lobby lobby = slots.lobby();
    for (const glideslope::net::Lobby::Slot& s : lobby.slots) {
        const char* who = s.controller == glideslope::net::Controller::nobody
                              ? "(open)"
                              : s.name.c_str();
        std::printf("  %-4d  %-15s  %5s  %5s  %5s\n", static_cast<int>(s.index), who,
                    "-", "-", "-");
    }
    if (fleet != nullptr && !fleet->flown().empty()) {
        std::printf("\n  flying           latitude    longitude     ft agl\n");
        for (const Fleet::Aircraft& a : fleet->flown()) {
            const glideslope::sim::AircraftState s = a.aircraft->state();
            std::printf("  %-14s %10.5f  %11.5f  %9.0f\n", a.id.c_str(),
                        s.latitude_deg, s.longitude_deg,
                        a.aircraft->property("position/h-agl-ft"));
        }
    }
    std::printf("\n  nobody can connect yet: the handshake is not built.\n");
    std::printf("  See docs/TRANSPORT.md, \"What is not here yet\".\n");
    std::fflush(stdout);
}

int run(const Options& o) {
    std::optional<glideslope::platform::UdpSocket> socket =
        glideslope::platform::UdpSocket::bound(o.port);
    if (!socket) {
        std::fprintf(stderr, "glideslope_server: cannot listen on port %u\n",
                     static_cast<unsigned>(o.port));
        return 1;
    }

    // The session the server keeps: its slots are what the dashboard shows
    // and what a `LOBBY` would carry. Nobody is in it, because nobody can
    // connect until the handshake exists.
    const glideslope::net::Slots slots(static_cast<std::uint8_t>(o.players));

    // The aircraft it flies. Building this reaches the network for terrain,
    // so it is not built at all when there is nothing to fly.
    std::optional<Fleet> fleet;
    if (!o.fly.empty() || o.ai > 0) {
        fleet.emplace(o.data, o.fly, o.ai, o.plan);
    }

    std::printf("listening on port %u\n", static_cast<unsigned>(socket->port()));
    std::fflush(stdout);

    const auto began = std::chrono::steady_clock::now();
    auto last = began;
    glideslope::sim::FixedStep clock;
    std::uint64_t datagrams = 0;
    std::uint64_t bytes = 0;
    double drawn_at_s = -1.0;
    // A datagram is at most this; anything larger is not one of ours.
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    for (;;) {
        glideslope::platform::Address from;
        const std::size_t got = socket->receive(into, from);
        if (got > 0) {
            ++datagrams;
            bytes += got;
        }

        const auto now = std::chrono::steady_clock::now();
        const double up_s = std::chrono::duration<double>(now - began).count();

        // **The simulation steps at a fixed rate**: every step that has
        // become due is taken, and the aircraft are stepped together so that
        // they share one clock.
        if (fleet) {
            const std::int64_t due = clock.advance(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - last));
            for (std::int64_t i = 0; i < due; ++i) {
                fleet->step();
            }
        }
        last = now;

        if (!o.headless && up_s - drawn_at_s >= dashboard_every_s) {
            draw_dashboard(o, slots, fleet ? &*fleet : nullptr, up_s, datagrams, bytes);
            drawn_at_s = up_s;
        }
        if (o.seconds > 0.0 && up_s >= o.seconds) {
            break;
        }
        if (got == 0) {
            // Nothing to do: the socket does not block, so without this the
            // server would spin a core for no reason.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    // **What it flew, so that a run can be checked.** One line per aircraft:
    // where it is, how high, and the ground under it - which is the thing
    // that differs between two aircraft on opposite sides of the world.
    std::printf("ran %.3f s, %lld steps\n",
                std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
                    .count(),
                static_cast<long long>(clock.steps_taken()));
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
        }
        std::printf("ran %d AI aircraft\n", fleet->ai());
        std::printf("fetched %d terrain tile%s\n", fleet->tiles_fetched(),
                    fleet->tiles_fetched() == 1 ? "" : "s");
    }
    std::fflush(stdout);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
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
