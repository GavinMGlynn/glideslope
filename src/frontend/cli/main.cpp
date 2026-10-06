// glideslope_cli - the simulation with no window.
//
// It links the simulation and nothing presentational, which is what makes it
// the proof that the simulation can run where there is no window: in CI, in a
// test, and inside the server.

#include "copilot/planner.hpp"
#include "frontend/cli/fly_copilot.hpp"
#include "frontend/briefs.hpp"
#include "frontend/players_copilot.hpp"
#include "frontend/same_air.hpp"
#include "frontend/shown.hpp"
#include "net/told.hpp"
#include "copilot/provider.hpp"
#include "net/handshake.hpp"
#include "platform/end_process.hpp"
#include "platform/closed_pipes.hpp"
#include "platform/no_crash_dialogs.hpp"
#include "net/inputs.hpp"
#include "net/interpolation.hpp"
#include "sim/terrain.hpp"
#include "sim/prediction.hpp"
#include "net/inside.hpp"
#include "net/keys.hpp"
#include "net/messages.hpp"
#include "net/protocol.hpp"
#include "net/reliable.hpp"
#include "net/sealing.hpp"
#include "net/session.hpp"
#include "net/state.hpp"
#include "platform/http.hpp"
#include "platform/socket.hpp"
#include "platform/paths.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/controller.hpp"
#include "sim/crash.hpp"
#include "sim/departure.hpp"
#include "sim/figures.hpp"
#include "sim/fixed_step.hpp"
#include "sim/lander.hpp"
#include "sim/learnt.hpp"
#include "sim/orbit_trial.hpp"
#include "sim/takeoff_trial.hpp"
#include "sim/weather.hpp"
#include "sim/selftest.hpp"
#include "sim/version.hpp"
#include "world/dem.hpp"
#include "world/geodesy.hpp"
#include "world/download.hpp"
#include "world/metar.hpp"
#include "world/runway_ground.hpp"
#include "world/runways.hpp"
#include "world/sky.hpp"
#include "world/weather.hpp"
#include "world/winds_aloft.hpp"

#include <algorithm>
#include <atomic>
#include <optional>
#include <memory>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <deque>
#include <map>
#include <span>
#include <stdexcept>
#include <thread>
#include <string>
#include <string_view>
#include <vector>

namespace {

// **A player's copilot, asked by this client** (`connect ... --copilot`):
// the model asked here, with the player's key, and only the route it answers
// sent to the server (`COPILOT_ROUTE`). Set from the command line; the
// connection it rides is in `stay`.
struct ConnectCopilot {
    std::optional<glideslope::frontend::PlayersCopilotOptions> options;
    double at_s = 0.0;   // asked when the session's clock reaches this
    int answers = 1;     // leaves once this many are answered...
    double stay_s = 60.0; // ...and this long after the last route sent
    std::string route_file; // or, sent as it is: a route the model never saw
    bool route_for_another = false; // ...for another aircraft's number
    bool route_when_wrecked = false; // ...once its own is a wreck
    // **The model that plans its aircraft when it is handed to the AI**
    // (`--hand-over-model P[:MODEL]`, with `--hand-over-at`): none, the
    // default, or a copilot made - for the aircraft the server says is its
    // own, told the data's hand-over words - and asked as it is handed
    // over. Refused, for want of a key, it says so, and the hand-over goes
    // ahead as with none.
    std::optional<glideslope::frontend::HandOverModel> hand_over;
    bool provider_given = false; // --copilot-provider or --copilot-model
    bool hand_over_tried = false; // the copilot for it made, or refused
    std::string hand_over_refused;
    // Taken back (`--take-back-at`): it leaves `stay_s` of the session's
    // clock after an update first shows its player with it again.
    bool taken_back = false;
    std::optional<double> back_since_s;
    // Held by the AI with no model planning it: from when the update first
    // shows the AI flying it, how it was flying, for what is said at the end.
    std::optional<double> held_since_s;
    double held_heading_deg = 0.0;
    double held_height_ft = 0.0;
    std::unique_ptr<glideslope::frontend::PlayersCopilot> seat;
    bool asked = false;
    bool route_sent = false;
    std::optional<double> sent_at_s;
    bool done = false;
};
ConnectCopilot connect_copilot;

// **What the server says the session is** (REQUIREMENTS.md 6.3), as a
// connecting client takes it: its own collision ground, from the data it
// reads, to hold the server's against, and, for a test, whether its
// prediction ignores the server's weather (`--own-air`) - flying still air
// whatever the server flies, as a client that fetched its own would fly the
// wrong air - to show what the server's weather is worth.
struct ConnectAir {
    std::filesystem::path data;
    bool own_air = false;
};
ConnectAir connect_air;

// A route written as a plan's `waypoint` and `orbit` lines, with `glide KT`
// first if it glides, read as it is - checked by nothing on this side.
glideslope::net::CopilotRoute route_from_file(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot read " + file);
    }
    std::string text(std::istreambuf_iterator<char>(in), {});
    glideslope::net::CopilotRoute route;
    if (text.rfind("glide ", 0) == 0) {
        const auto end = text.find('\n');
        route.glide_kts = std::strtod(text.substr(6, end - 6).c_str(), nullptr);
        text = end == std::string::npos ? std::string() : text.substr(end + 1);
    }
    const glideslope::sim::FlightPlan plan =
        glideslope::sim::parse_flight_plan("aircraft any\nstart 0 0 0 0 1\n" + text);
    for (const glideslope::sim::Waypoint& w : plan.waypoints) {
        glideslope::net::RouteWaypoint p;
        p.name = w.name;
        p.latitude_deg = w.latitude_deg;
        p.longitude_deg = w.longitude_deg;
        p.altitude_ft = w.altitude_ft;
        p.airspeed_kts = w.airspeed_kts;
        if (w.orbit) {
            p.orbit = glideslope::net::RouteWaypoint::Orbit{
                w.orbit->radius_m, static_cast<std::uint8_t>(w.orbit->turns), w.orbit->right};
        }
        route.waypoints.push_back(p);
    }
    return route;
}

void print_usage(std::FILE* out) {
    std::fputs(
        "usage: glideslope_cli [--data DIR] COMMAND\n"
        "\n"
        "  --version                 print the version\n"
        "  --help                    print this\n"
        "  aircraft                  list the aircraft the data holds\n"
        "  aircraft NAME             print what NAME's model files say\n"
        "  figures NAME [FIGURE]     fly NAME's published figures, or one, and exit 1\n"
        "                            if any lands out of range\n"
        "  selftest [NAME]           fly NAME's selftest input log (default c172p) "
        "and\n"
        "                            print the hash of its state\n"
        "  height LAT LON            the ground's height there, from the Copernicus "
        "DEM,\n"
        "                            fetching what it needs into the cache directory\n"
        "  weather STATION           the weather now at an airfield: its METAR, and "
        "the\n"
        "                            winds aloft over it\n"
        "  sky REPORT LAT LON        the cloud, haze and precipitation a METAR shows\n"
        "                            observed on the ground at LAT LON, and the\n"
        "                            nearest places in its lowest cloud and its gaps\n"
        "  air                       the air - gusts, turbulence, thermals and a\n"
        "                            ridge's lift - at a thousand places and times,\n"
        "                            for comparing platforms\n"
        "  plan AIRCRAFT AIRPORT COMMAND [--provider openai|anthropic] [--model M]\n"
        "       [--record FILE | --playback FILE] [--out FILE]\n"
        "                            ask a language model to turn COMMAND - \"take\n"
        "                            off and orbit the CBD\" - into a flight plan for\n"
        "                            AIRCRAFT standing at AIRPORT, with your own key\n"
        "                            (openai-key or anthropic-key in the config\n"
        "                            directory), and print it; --record keeps what\n"
        "                            was asked and said, --playback asks nothing and\n"
        "                            plays a recording back instead\n"
        "  land AIRCRAFT [--learnt] [--crosswind KTS] [--across M] [--high M]\n"
        "       [--fuel LBS]\n"
        "                            hand AIRCRAFT to the AI at a final-approach gate\n"
        "                            two miles out - M metres right of the centreline\n"
        "                            and above the glidepath, in KTS of crosswind from\n"
        "                            the left, LBS in each tank (full by default) - on\n"
        "                            a runway at sea level, and say where it touched\n"
        "                            and stopped: the approach autopilot lands it, or\n"
        "                            with --learnt the landing learnt by reinforcement\n"
        "                            learning (data/rl/AIRCRAFT-landing.txt), rolled\n"
        "                            out by the autopilot; exits 1 unless it touched\n"
        "                            and stopped on the runway\n"
        "  takeoff-speeds AIRCRAFT [FROM_KT]\n"
        "                            measure the rotation and climb-away speeds of\n"
        "                            an aircraft that publishes none (its figures\n"
        "                            file's <takeoff_speeds>): taken off a level\n"
        "                            3,500 m runway at the weight it flies a plan at\n"
        "  glide-speeds AIRCRAFT       measure the slowest AIRCRAFT may glide at (its\n"
        "                            figures file's <glide_speeds>): round the\n"
        "                            tightest orbit from 30,000 ft, engines stopped,\n"
        "                            both ways, not stalling, from its slowest\n"
        "                            routed speed up by 5 kt\n"
        "  plan-speeds AIRCRAFT [FROM_KT | --every]\n"
        "                            measure the speeds AIRCRAFT may be planned at\n"
        "                            (its figures file's <plan_speeds>): round the\n"
        "                            tightest orbit allowed, clean, both ways round,\n"
        "                            in calm air and a 10 kt wind, holding its height\n"
        "                            within 50 ft and its speed within 5 kt. The\n"
        "                            slowest is sought up from FROM_KT (default its\n"
        "                            approach speed) in 5 kt steps; the fastest down\n"
        "                            from a fifth over its start speed, the same way.\n"
        "                            --every flies the speeds the file gives instead,\n"
        "                            every 5 kt from its slowest to its fastest, and\n"
        "                            exits 1 unless every one held\n"
        "  fly-plan FILE [--minutes M] [--orbits N]\n"
        "                            fly a flight plan over the DEM with the AI, and\n"
        "                            say how each part of it was flown; an orbit with\n"
        "                            no end is left after N turns (default 2), and\n"
        "                            the flight after M minutes (default 30)\n"
        "  fly-copilot AIRCRAFT LAT LON FEET HEADING KNOTS TASK\n"
        "       [--provider openai|anthropic] [--model M] [--record FILE | --playback FILE]\n"
        "       [--minutes M] [--thinking S] [--engine-fails-at S] [--to LAT LON]\n"
        "                            fly AIRCRAFT from LAT LON at FEET with the AI\n"
        "                            and a language model as its copilot, told TASK\n"
        "                            and asked as the flight goes whether its route\n"
        "                            should change, with your own key; its answer is\n"
        "                            flown S seconds after it is asked (default 45),\n"
        "                            or when it comes. --engine-fails-at stops the\n"
        "                            engine S seconds in; --to says when it came to\n"
        "                            LAT LON, and how much of the way the coast was\n"
        "                            near\n"
        "\n"
        "  connect --server HOST PORT --server-key HEX [SECONDS] [--fly]\n"
        "                            connect to a server named on the command line\n"
        "  connect --online [SECONDS] [--fly]\n"
        "                            connect to the server named in server.txt, a\n"
        "                            line of host, port and public key in\n"
        "                            glideslope's config directory\n"
        "  connect HOST:PORT KEY [SECONDS]\n"
        "                            complete a session with a server and say so;\n"
        "                            with SECONDS, stay that long answering its\n"
        "                            pings so that it can measure the round trip.\n"
        "                            --again sends the initiation a second time\n"
        "                            once the session is up, as a network that\n"
        "                            duplicates a datagram would.\n"
        "                            --again-when-let-go says nothing after the\n"
        "                            handshake until the server has let it go,\n"
        "                            then sends the initiation once more and\n"
        "                            says whether and how it was answered.\n"
        "                            --first-from-elsewhere sends a copy of the\n"
        "                            initiation from another port first, and\n"
        "                            waits for it to be answered.\n"
        "                            --again-from-elsewhere FILE, once the session\n"
        "                            is up, sends a copy of the initiation from\n"
        "                            another port, as a replayer would, waits for it\n"
        "                            to be answered, flies on, and goes on sending\n"
        "                            the copy; FILE is written when the copy's\n"
        "                            session has been let go.\n"
        "                            --until-exists FILE leaves once FILE exists,\n"
        "                            SECONDS the most it will wait.\n"
        "                            --fly sends\n"
        "                            inputs - full aileron - so the server has\n"
        "                            something to fly this client's aircraft by.\n"
        "                            --after N waits N seconds before connecting,\n"
        "                            so as to join a session already running.\n"
        "                            --after-ready FILE counts that wait from when\n"
        "                            FILE appears (the server's --ready-file).\n"
        "                            --key HEX connects as the player whose secret\n"
        "                            key that is, rather than a new one.\n"
        "                            --heard FILE also writes each aircraft heard\n"
        "                            becoming a wreck, and flying again, to FILE.\n"
        "                            --until-flying-again N leaves once N aircraft\n"
        "                            have been heard flying again, SECONDS the most\n"
        "                            it will wait.\n"
        "                            --predict flies its own aircraft (as the model\n"
        "                            the server says it is) ahead of the server, puts\n"
        "                            it right by each update, shows the others 100 ms\n"
        "                            behind, and says how far its own was put right\n"
        "                            --hand-over-at S asks, S seconds in, for its own\n"
        "                            aircraft to be handed to the AI pilot, and\n"
        "                            --take-back-at S for it back; with\n"
        "                            --hand-over-model P, anthropic or openai (each\n"
        "                            with :MODEL if wanted) or none,\n"
        "                            the hand-over is planned by that model, asked\n"
        "                            here with your key (--copilot-record/-playback),\n"
        "                            or by none - the AI holds its course, and it\n"
        "                            leaves --copilot-stay S after; predicting, it says\n"
        "                            how far what it showed of its own stepped then;\n"
        "                            with --long-frame-after-switch it draws nothing\n"
        "                            for 0.4 s after the third frame after each switch,\n"
        "                            a test's long frame, and says how many it built;\n"
        "                            --late-update-after-take-over hears the last\n"
        "                            update from before each take-over again after it,\n"
        "                            as a network that reorders them would\n"
        "                            --take-over-at S asks, S seconds in, to take over\n"
        "                            the AI aircraft it watches, or else the first -\n"
        "                            or --take-over-aircraft N, whichever that is,\n"
        "                            and with --once-the-ai-flies-it, not before an\n"
        "                            update shows the AI flying it\n"
        "                            --watch-ai rides along in an AI's aircraft: told\n"
        "                            its controls, and with --track, writes them\n"
        "                            --dive-after S flies its own into the ground, S\n"
        "                            seconds in: full forward stick and full power\n"
        "                            --own-air (with --predict) flies its own in\n"
        "                            still air, whatever weather the server says it\n"
        "                            flies, to show what the server's is worth\n"
        "                            --track FILE writes where every aircraft was\n"
        "                            heard to be, and, predicting, where each other\n"
        "                            one was drawn, to FILE\n"
        "                            At the end of SECONDS it says goodbye, so that\n"
        "                            the server lets it go at once; --no-goodbye\n"
        "                            leaves in silence instead, for the timeout.\n"
        "                            --done FILE writes FILE when it has gone.\n"
        "                            A client the server has let go joins again by\n"
        "                            itself, for what is left of SECONDS.\n"
        "                            --stall-once-rolled (with --fly) stops, sending\n"
        "                            and answering nothing, once its aircraft has\n"
        "                            rolled past 90 degrees, until the server has let\n"
        "                            it go; joined again, it leaves once its new\n"
        "                            aircraft has rolled past 90 too.\n"
        "                            --leave-once-back (with --fly), having believed a\n"
        "                            refusal, leaves once the server has applied an\n"
        "                            input it sent in the session it is back in - its\n"
        "                            old one or a new one\n"
        "                            --forge-leaving tries, as a forger would, to\n"
        "                            end sessions with goodbyes from the wrong\n"
        "                            address or keys: its own from a second session's\n"
        "                            address and from one with none, and the second\n"
        "                            session's from its own address\n"
        "  --data DIR                read data from DIR instead of data/ beside the\n"
        "                            program\n",
        out);
}

// Every aircraft in the catalogue, and what of it the data holds.
int list_aircraft(const std::filesystem::path& data) {
    for (const glideslope::sim::CatalogueEntry& e :
         glideslope::sim::read_catalogue(data)) {
        const bool figures =
            std::filesystem::exists(data / "figures" / (e.model + ".xml"));
        const bool selftest =
            std::filesystem::exists(data / "selftest" / (e.model + ".log"));
        std::printf("%-12s %-28s model %s, starting at %.0f KCAS%s%s%s\n", e.id.c_str(),
                    e.name.c_str(), e.model.c_str(), e.start_airspeed_kts,
                    e.seaplane ? ", a seaplane" : "", figures ? ", published figures" : "",
                    selftest ? ", a selftest" : "");
    }
    return 0;
}

int print_aircraft(const std::filesystem::path& data, const std::string& model) {
    const glideslope::sim::Aircraft aircraft(data / "jsbsim", model);
    const glideslope::sim::AircraftFigures f = aircraft.figures();
    std::printf("aircraft      %s\n", f.model.c_str());
    std::printf("description   %s\n", f.description.c_str());
    std::printf("wing area     %.1f sq ft\n", f.wing_area_sqft);
    std::printf("wingspan      %.1f ft\n", f.wingspan_ft);
    std::printf("chord         %.1f ft\n", f.chord_ft);
    std::printf("empty weight  %.1f lb\n", f.empty_weight_lbs);
    std::printf("engines       %d\n", f.engines);
    return 0;
}

int fly_figures(const std::filesystem::path& data, const std::string& model,
                const std::string& only) {
    const glideslope::sim::PublishedFigures figures =
        glideslope::sim::read_published_figures(data / "figures" / (model + ".xml"));
    std::printf("%s - %s\n\n", figures.model.c_str(), figures.source.c_str());
    std::printf("  %-22s %10s %22s\n", "figure", "measured", "range");
    int flown = 0;
    int failed = 0;
    for (const auto& spec : figures.figures) {
        if (!only.empty() && spec.name != only) {
            continue;
        }
        const auto result = glideslope::sim::fly_figure(data / "jsbsim", figures, spec);
        ++flown;
        failed += result.passed() ? 0 : 1;
        std::printf("  %-22s %10.2f %9.2f .. %-9.2f %-20s %s\n", spec.name.c_str(),
                    result.measured, spec.low, spec.high, spec.unit.c_str(),
                    result.passed() ? "ok" : "OUT OF RANGE");
    }
    if (flown == 0) {
        std::fprintf(stderr, "glideslope_cli: %s has no figure named %s\n",
                     model.c_str(), only.c_str());
        return 2;
    }
    std::printf("\n%d of %d in range\n", flown - failed, flown);
    return failed == 0 ? 0 : 1;
}

// **The speeds an aircraft may be planned at, measured**: each speed tried
// is asked what a plan may ask of it (sim::holds_plan_speed) - the tightest
// orbit four ways, and a heading in calm air and a crosswind. The slowest is
// sought at speeds rising from FROM_KT, or its approach speed, by 5 kt; the
// fastest at speeds falling from a fifth over its start speed. **What to
// write in its `<plan_speeds>` is printed, by one rule**: where the approach
// speed (or the fifth over the start speed) held, that - nothing a plan was
// let fly before is taken away; where it did not, the first that held, with
// 5 kt to spare. `--every` instead flies the speeds the file gives, every
// 5 kt from its slowest to its fastest, and exits 1 unless every one held.
int plan_speeds(const std::filesystem::path& data, const std::vector<std::string_view>& args) {
    const glideslope::sim::CatalogueEntry entry =
        glideslope::sim::find_aircraft(data, std::string(args[1]));
    const bool every = args.size() >= 3 && args[2] == "--every";
    double from = 0.0;
    if (args.size() >= 3 && !every) {
        const std::string word(args[2]);
        std::size_t used = 0;
        try {
            from = std::stod(word, &used);
        } catch (const std::exception&) {
            used = 0;
        }
        if (used != word.size() || !(from >= 20.0 && from <= 600.0)) {
            throw std::runtime_error("plan-speeds: FROM_KT is a speed from 20 to 600, not '" +
                                     word + "'");
        }
    } else if (!every) {
        from = std::round(glideslope::sim::approach_speeds(data, entry.model).vref_kts);
    }
    const auto say = [](const std::string& line) {
        std::printf("%s\n", line.c_str());
        std::fflush(stdout);
    };
    const auto holds = [&](double kts) {
        return glideslope::sim::holds_plan_speed(data, entry, kts, say);
    };
    if (every) {
        const glideslope::sim::PlanSpeeds speeds = glideslope::sim::plan_speeds(data, entry.model);
        std::size_t held = 0;
        std::size_t asked = 0;
        for (double kts = speeds.slowest_kts;; kts = std::min(kts + 5.0, speeds.fastest_kts)) {
            ++asked;
            if (holds(kts)) {
                ++held;
            }
            if (kts >= speeds.fastest_kts) {
                break;
            }
        }
        std::printf("%s: %zu of %zu speeds from %.0f to %.0f kt held\n", entry.id.c_str(), held,
                    asked, speeds.slowest_kts, speeds.fastest_kts);
        return held == asked ? 0 : 1;
    }
    const double top = std::round(entry.start_airspeed_kts * 1.2);
    double first_held = 0.0;
    for (double kts = from; kts <= top; kts += 5.0) {
        if (holds(kts)) {
            first_held = kts;
            break;
        }
    }
    if (first_held == 0.0) {
        std::printf("%s: no speed from %.0f to %.0f kt holds\n", entry.id.c_str(), from, top);
        return 1;
    }
    double last_held = 0.0;
    for (double kts = top; kts > first_held; kts -= 5.0) {
        if (holds(kts)) {
            last_held = kts;
            break;
        }
    }
    if (last_held == 0.0) {
        std::printf("%s: no speed from %.0f down to %.0f kt holds\n", entry.id.c_str(), top,
                    first_held);
        return 1;
    }
    const double slowest = first_held == from ? first_held : first_held + 5.0;
    const double fastest = last_held == top ? last_held : last_held - 5.0;
    std::printf("%s: slowest held %.0f kt, fastest held %.0f kt\n", entry.id.c_str(),
                first_held, last_held);
    std::printf("%s: write <plan_speeds slowest_kcas=\"%.0f\" fastest_kcas=\"%.0f\">\n",
                entry.id.c_str(), slowest, fastest);
    return 0;
}

// **The slowest a glide may fly an aircraft, measured**
// (sim::glide_tightest_orbit): from the slowest a route may fly it up in 5 kt
// steps, as far as the fastest a plan may, each glided round its tightest
// orbit from 30,000 ft both ways round with its engines stopped, until one
// goes round (sim::GlideOrbitFlown::round) and stalls neither way. What to write in its figures file's `<glide_speeds>` is
// printed.
int glide_speeds(const std::filesystem::path& data, const std::vector<std::string_view>& args) {
    const glideslope::sim::CatalogueEntry entry =
        glideslope::sim::find_aircraft(data, std::string(args[1]));
    glideslope::copilot::Brief brief = glideslope::frontend::brief_for(data, entry.id);
    brief.glide_slowest_kts = 0.0;
    const double from = std::round(glideslope::copilot::slowest_routed_kts(brief));
    const double top = glideslope::sim::plan_speeds(data, entry.model).fastest_kts;
    for (double kts = from; kts <= top; kts += 5.0) {
        bool stalled = false;
        for (const bool right : {false, true}) {
            const auto g = glideslope::sim::glide_tightest_orbit(data, entry, kts, right);
            std::printf("%s glide %.0f kt %s: alpha %.1f (lift peaked at %.1f), round %.2f%s\n",
                        entry.id.c_str(), kts, right ? "right" : "left", g.most_alpha_deg,
                        g.alpha_at_most_lift_deg, g.turns, g.stalled() ? ", STALLED" : "");
            std::fflush(stdout);
            stalled = stalled || g.stalled() || !g.round();
        }
        if (!stalled) {
            std::printf("%s: write <glide_speeds slowest_kcas=\"%.0f\">\n", entry.id.c_str(),
                        kts);
            return 0;
        }
    }
    std::printf("%s: no glide from %.0f to %.0f kt goes round without stalling\n",
                entry.id.c_str(), from, top);
    return 1;
}

// **The speeds an aircraft that publishes none takes off at, measured**
// (sim/takeoff_trial.hpp): at its take-off field length's flap, or none, and
// the weight its model flies a plan at. The rotation is sought at speeds
// rising from FROM_KT (100 by default) by 5 kt, each climbing away at 20
// more, until one lifts off by its rotation (sim::TakeoffFlown::lifted_off);
// then the climb away, the slowest speed at 500 ft of any asked. **What to write in its `<takeoff_speeds>` is printed by
// plan-speeds' rule**: the first rotation that held, with 5 kt to spare.
int takeoff_speeds(const std::filesystem::path& data,
                   const std::vector<std::string_view>& args) {
    const glideslope::sim::CatalogueEntry entry =
        glideslope::sim::find_aircraft(data, std::string(args[1]));
    double from = 100.0;
    if (args.size() >= 3) {
        const std::string word(args[2]);
        std::size_t used = 0;
        try {
            from = std::stod(word, &used);
        } catch (const std::exception&) {
            used = 0;
        }
        if (used != word.size() || !(from >= 20.0 && from <= 400.0)) {
            throw std::runtime_error("takeoff-speeds: FROM_KT is a speed from 20 to 400, not '" +
                                     word + "'");
        }
    }
    const glideslope::sim::MeasuredTakeoff m = glideslope::sim::measure_takeoff_speeds(
        data, entry, from, [](const std::string& line) {
            std::printf("%s\n", line.c_str());
            std::fflush(stdout);
        });
    if (!m.why_not.empty()) {
        std::printf("%s: %s\n", entry.id.c_str(), m.why_not.c_str());
        return 1;
    }
    std::printf("%s: rotation held from %.0f kt; the slowest it climbed away at, %.1f kt\n",
                entry.id.c_str(), m.rotate_kts - 5.0, m.slowest_away_kts);
    std::printf("%s: write <takeoff_speeds rotate_kcas=\"%.0f\" climb_kcas=\"%.0f\" "
                "flaps_deg=\"%.0f\" weight_lbs=\"%.0f\">\n",
                entry.id.c_str(), m.rotate_kts, m.climb_kts, m.flaps_deg, m.weight_lbs);
    return 0;
}

// **A landing handed to the AI at the gate**, the approach autopilot's or the
// learnt one, through the controller as any hand-over is: on a runway at sea
// level over level ground - the runway the landing tests fly to, pointing
// 070 - so that what is shown is the landing, not the terrain.
int land(const std::filesystem::path& data, const std::vector<std::string_view>& args) {
    constexpr double degrees = 180.0 / 3.14159265358979323846;
    constexpr double feet_per_metre = 3.280839895013123;
    const glideslope::sim::CatalogueEntry entry =
        glideslope::sim::find_aircraft(data, std::string(args[1]));
    bool learnt = false;
    double crosswind_kts = 0.0;
    double across_m = 0.0;
    double high_m = 0.0;
    double fuel_lbs = 0.0;
    bool fuel_set = false;
    std::size_t fuel_arg = 0;
    // A number, the whole word and finite: "10kt", "ten", "nan" and "inf" are
    // refused, not read as far as they go.
    const auto number = [&](std::size_t i) {
        if (i >= args.size()) {
            throw std::runtime_error("land: " + std::string(args[i - 1]) + " needs a number");
        }
        const std::string word(args[i]);
        double value = 0.0;
        std::size_t used = 0;
        try {
            value = std::stod(word, &used);
        } catch (const std::exception&) {
            used = 0;
        }
        if (word.empty() || used != word.size() || !std::isfinite(value)) {
            throw std::runtime_error("land: " + std::string(args[i - 1]) + " needs a number, not '" +
                                     word + "'");
        }
        return value;
    };
    for (std::size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--learnt") {
            learnt = true;
        } else if (args[i] == "--crosswind") {
            crosswind_kts = number(++i);
        } else if (args[i] == "--across") {
            across_m = number(++i);
        } else if (args[i] == "--high") {
            high_m = number(++i);
        } else if (args[i] == "--fuel") {
            fuel_lbs = number(++i);
            fuel_arg = i;
            fuel_set = true;
        } else {
            throw std::runtime_error("land: what is " + std::string(args[i]) + "?");
        }
    }
    std::shared_ptr<const glideslope::sim::LearntPolicy> policy;
    if (learnt) {
        const auto file = data / "rl" / (entry.model + "-landing.txt");
        if (!std::filesystem::exists(file)) {
            throw std::runtime_error("land: " + entry.model +
                                     " has no learnt landing (" + file.string() + ")");
        }
        policy = std::make_shared<const glideslope::sim::LearntPolicy>(
            glideslope::sim::LearntPolicy::read(file));
    }
    const glideslope::sim::ApproachSpeeds speeds =
        glideslope::sim::approach_speeds(data, entry.model);

    glideslope::sim::Runway runway;
    runway.name = "070";
    runway.threshold_lat_deg = -33.9461;
    runway.threshold_lon_deg = 151.1772;
    runway.elevation_ft = 0.0;
    runway.heading_deg = 70.0;
    runway.length_m = 3000.0;

    glideslope::sim::Aircraft aircraft(data / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    if (crosswind_kts != 0.0) {
        glideslope::sim::Conditions conditions;
        const double towards = (runway.heading_deg + 90.0) / degrees;
        conditions.wind_north_mps = crosswind_kts * 0.514444 * std::cos(towards);
        conditions.wind_east_mps = crosswind_kts * 0.514444 * std::sin(towards);
        aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(conditions));
    }
    // The same in every tank the model has, and no more than each holds.
    if (fuel_set) {
        const std::vector<double> capacities = aircraft.tank_capacities_lbs();
        if (fuel_lbs < 0.0) {
            throw std::runtime_error("land: --fuel " + std::string(args[fuel_arg]) +
                                     " lb is less than nothing");
        }
        if (capacities.empty()) {
            throw std::runtime_error("land: " + entry.model + " has no fuel tanks for --fuel");
        }
        glideslope::sim::Loading loading;
        for (std::size_t t = 0; t < capacities.size(); ++t) {
            if (fuel_lbs > capacities[t]) {
                char text[160];
                std::snprintf(text, sizeof text,
                              "land: --fuel %g lb will not go in %s's tank %zu, which holds 0 to %g",
                              fuel_lbs, entry.model.c_str(), t, capacities[t]);
                throw std::runtime_error(text);
            }
            loading.tank_lbs[static_cast<int>(t)] = fuel_lbs;
        }
        aircraft.load(loading);
    }
    const double out_m = 2.0 * 1852.0;
    const double heading = runway.heading_deg / degrees;
    const double north_m = -out_m * std::cos(heading) - across_m * std::sin(heading);
    const double east_m = -out_m * std::sin(heading) + across_m * std::cos(heading);
    const double lat = runway.threshold_lat_deg / degrees;
    const double per_deg_lat = 111132.92 - 559.82 * std::cos(2.0 * lat) +
                               1.175 * std::cos(4.0 * lat) - 0.0023 * std::cos(6.0 * lat);
    const double per_deg_lon = 111412.84 * std::cos(lat) - 93.5 * std::cos(3.0 * lat) +
                               0.118 * std::cos(5.0 * lat);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg + north_m / per_deg_lat;
    ic.longitude_deg = runway.threshold_lon_deg + east_m / per_deg_lon;
    ic.altitude_ft = runway.elevation_ft +
                     ((out_m + speeds.aim_m) * std::tan(3.0 / degrees) + high_m) * feet_per_metre;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = policy ? policy->vref_kts : speeds.vref_kts;
    ic.engine_running = true;
    ic.flaps = policy ? policy->flaps : speeds.flap;
    ic.flight_path_deg = -3.0;
    ic.trim = true;
    aircraft.initialize(ic);

    // Flown by its pilot, hands still, and handed to the AI.
    glideslope::sim::Controls pilot;
    pilot.flaps = ic.flaps;
    glideslope::sim::Controller controller(aircraft, pilot);
    if (policy) {
        controller.to_ai_learnt_approach(runway, speeds, policy);
    } else {
        controller.to_ai_approach(runway, speeds);
    }
    bool touched = false;
    long decisions = 0; // the learnt policy's, which says it flew
    bool stopped = false;
    double sink_fpm = 0.0;
    double touch_across_m = 0.0;
    double touch_along_m = 0.0;
    double stop_across_m = 0.0;
    double stop_along_m = 0.0;
    for (long tick = 0; tick < 600L * glideslope::sim::steps_per_second && !stopped; ++tick) {
        aircraft.set_controls(controller.fly());
        aircraft.step();
        const glideslope::sim::Lander* rolling = nullptr;
        if (const auto* l = controller.learnt()) {
            if (l->touched() && !touched) {
                touched = true;
                sink_fpm = l->touchdown_sink_fpm();
                touch_across_m = l->touchdown_across_m();
                touch_along_m = l->touchdown_along_m();
            }
            rolling = &l->rollout();
            decisions = l->decisions();
            stopped = l->stage() == glideslope::sim::LearntLander::Stage::stopped;
        } else if (const auto* a = controller.lander()) {
            if (a->touched() && !touched) {
                touched = true;
                sink_fpm = a->touchdown_sink_fpm();
                touch_across_m = a->touchdown_across_m();
                touch_along_m = a->touchdown_along_m();
            }
            rolling = a;
            stopped = a->stage() == glideslope::sim::Lander::Stage::stopped;
        }
        if (stopped && rolling != nullptr) {
            stop_along_m = -rolling->along_m();
            stop_across_m = rolling->across_m();
        }
    }
    std::printf("%s landed by the %s from 2 nm, %+.0f m across, %+.0f m high, %+.0f kt of "
                "crosswind\n",
                entry.model.c_str(), policy ? "learnt landing" : "approach autopilot",
                across_m, high_m, crosswind_kts);
    if (!touched) {
        std::printf("  never touched down\n");
        return 1;
    }
    std::printf("  touched down at %.0f ft/min, %+.2f m across the centreline, %.0f m past "
                "the threshold\n",
                sink_fpm, touch_across_m, touch_along_m);
    if (!stopped) {
        std::printf("  did not stop\n");
        return 1;
    }
    std::printf("  stopped %.0f m past the threshold, %+.2f m across\n", stop_along_m,
                stop_across_m);
    if (policy) {
        std::printf("  the learnt policy decided %ld times, ten a second, to the touch\n",
                    decisions);
    }
    const bool on_runway = touch_along_m >= 0.0 && touch_along_m <= runway.length_m &&
                           stop_along_m >= 0.0 && stop_along_m <= runway.length_m &&
                           std::abs(stop_across_m) <= 15.0;
    return on_runway ? 0 : 1;
}

int selftest(const std::filesystem::path& data, const std::string& model) {
    const auto result = glideslope::sim::run_selftest(
        data / "jsbsim", data / "selftest" / (model + ".log"));
    const auto& s = result.final_state;
    std::printf("selftest %s: %" PRId64 " steps, %.3f s\n", result.model.c_str(),
                result.steps, s.sim_time_s);
    std::printf("  ends at    %.7f, %.7f, %.1f ft\n", s.latitude_deg, s.longitude_deg,
                s.altitude_ft);
    std::printf("  attitude   roll %.2f, pitch %.2f, heading %.2f deg\n", s.roll_deg,
                s.pitch_deg, s.heading_deg);
    std::printf("  airspeed   %.2f KCAS, climbing %.1f ft/min, engine %.0f RPM\n",
                s.airspeed_kts, s.climb_rate_fpm, s.engine_rpm);
    std::printf("hash %016" PRIx64 "\n", result.hash);
    return 0;
}

int height(const std::filesystem::path& data, std::string_view latitude_text,
           std::string_view longitude_text) {
    const auto number = [](std::string_view text, double low, double high,
                           const char* what) {
        const std::string copy(text);
        char* end = nullptr;
        const double v = std::strtod(copy.c_str(), &end);
        if (copy.empty() || *end != '\0' || !(v >= low && v <= high)) {
            std::fprintf(stderr, "glideslope_cli: %s must be a number from %g to %g\n",
                         what, low, high);
            glideslope::platform::end_process(2);
        }
        return v;
    };
    const double lat = number(latitude_text, -90.0, 90.0, "the latitude");
    const double lon = number(longitude_text, -180.0, 180.0, "the longitude");

    std::ifstream coverage_file(data / "dem" / "coverage.txt", std::ios::binary);
    if (!coverage_file) {
        throw std::runtime_error("cannot read " +
                                 (data / "dem" / "coverage.txt").string());
    }
    const glideslope::world::DemCoverage coverage(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    const std::filesystem::path cache = glideslope::platform::cache_directory();
    const glideslope::world::Fetch fetch = glideslope::world::http_fetch();
    glideslope::world::DownloadedTiles tiles(cache, fetch);
    const glideslope::world::Geoid geoid =
        glideslope::world::egm2008_geoid(cache, fetch);
    glideslope::world::Dem dem(coverage, tiles, &geoid);

    const double above_geoid = dem.height_above_geoid(lat, lon);
    const double undulation = geoid.undulation(lat, lon);
    const glideslope::world::DemCell cell{
        std::clamp(static_cast<int>(std::ceil(lat)) - 1, -90, 89),
        std::clamp(static_cast<int>(std::floor(lon)), -180, 179)};
    const glideslope::world::DemDataset dataset = coverage.at(cell);
    std::printf("height at %.7f, %.7f\n", lat, lon);
    std::printf("  above sea level (EGM2008)   %10.3f m\n", above_geoid);
    std::printf("  geoid above the ellipsoid   %10.3f m\n", undulation);
    std::printf("  above the WGS84 ellipsoid   %10.3f m\n", above_geoid + undulation);
    std::printf("  from                        %s\n",
                dataset == glideslope::world::DemDataset::none
                    ? "no tile: the sea"
                    : glideslope::world::dem_tile_name(dataset, cell).c_str());
    std::printf("  cache                       %s (%d tile%s fetched)\n",
                cache.string().c_str(), tiles.downloads(),
                tiles.downloads() == 1 ? "" : "s");
    std::printf("%s\n", glideslope::world::copernicus_dem_notice);
    return 0;
}

// **Words to a flight plan**, by a language model (copilot/planner.hpp).
int plan_command(const std::filesystem::path& data, const std::vector<std::string_view>& args) {
    const std::string aircraft(args[1]);
    const std::string airport(args[2]);
    const std::string command(args[3]);
    std::string provider_name = "openai";
    std::string model;
    std::string record;
    std::string played;
    std::string out;
    for (std::size_t i = 4; i < args.size(); ++i) {
        const bool value = i + 1 < args.size();
        if (args[i] == "--provider" && value) {
            provider_name = std::string(args[++i]);
        } else if (args[i] == "--model" && value) {
            model = std::string(args[++i]);
        } else if (args[i] == "--record" && value) {
            record = std::string(args[++i]);
        } else if (args[i] == "--playback" && value) {
            played = std::string(args[++i]);
        } else if (args[i] == "--out" && value) {
            out = std::string(args[++i]);
        } else {
            std::fprintf(stderr, "glideslope_cli: plan: what is \"%s\"?\n",
                         std::string(args[i]).c_str());
            return 2;
        }
    }
    if (!record.empty() && !played.empty()) {
        std::fprintf(stderr, "glideslope_cli: plan: --record or --playback, not both\n");
        return 2;
    }
    const glideslope::sim::CatalogueEntry entry = glideslope::sim::find_aircraft(data, aircraft);
    glideslope::copilot::PlanRequest request =
        glideslope::frontend::plan_request_for(data, entry.id);
    request.command = command;
    request.airport = airport;
    request.runways = glideslope::world::runways_at(
        glideslope::world::world_runways(glideslope::platform::cache_directory(),
                                         glideslope::world::http_fetch()),
        airport);
    if (request.runways.empty()) {
        std::fprintf(stderr, "glideslope_cli: plan: OurAirports has no runways at %s\n",
                     airport.c_str());
        return 2;
    }

    glideslope::copilot::Post post = glideslope::copilot::http_post();
    if (!played.empty()) {
        post = glideslope::copilot::playback(played);
    } else if (!record.empty()) {
        std::filesystem::remove(record);
        post = glideslope::copilot::recording(post, record);
    }
    const std::string key = !played.empty()             ? std::string()
                            : provider_name == "openai" ? glideslope::platform::openai_key()
                                                        : glideslope::platform::anthropic_key();
    const auto provider = glideslope::copilot::make_provider(provider_name, key, model,
                                                             std::move(post), !played.empty());
    const glideslope::copilot::Planned planned =
        glideslope::copilot::plan_from_words(*provider, request);
    std::printf("# planned by %s, %s, in %d answer%s%s\n", provider->name().c_str(),
                provider->model().c_str(), planned.attempts, planned.attempts == 1 ? "" : "s",
                !played.empty() ? ", played back" : "");
    for (const std::string& why : planned.refused) {
        std::printf("# refused: %s\n", why.c_str());
    }
    std::printf("%s", planned.text.c_str());
    if (!out.empty()) {
        std::ofstream file(out, std::ios::binary);
        file << planned.text;
        if (!file) {
            throw std::runtime_error("cannot write " + out);
        }
    }
    return 0;
}

// **A flight plan flown by the AI over the DEM**, headless, saying how each
// part of it went: the take-off, each waypoint passed, each orbit flown.
// Heights said are above sea level, as the plan's are; the aircraft flies
// above the ellipsoid, as everything here does.
int fly_plan(const std::filesystem::path& data, const std::vector<std::string_view>& args) {
    double minutes = 30.0;
    int orbits = 2;
    for (std::size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--minutes" && i + 1 < args.size()) {
            minutes = std::strtod(std::string(args[++i]).c_str(), nullptr);
        } else if (args[i] == "--orbits" && i + 1 < args.size()) {
            orbits = std::atoi(std::string(args[++i]).c_str());
        } else {
            std::fprintf(stderr, "glideslope_cli: fly-plan: what is \"%s\"?\n",
                         std::string(args[i]).c_str());
            return 2;
        }
    }
    std::ifstream in{std::filesystem::path(std::string(args[1])), std::ios::binary};
    if (!in) {
        throw std::runtime_error("cannot read " + std::string(args[1]));
    }
    glideslope::sim::FlightPlan plan =
        glideslope::sim::parse_flight_plan(std::string(std::istreambuf_iterator<char>(in), {}));
    const glideslope::sim::CatalogueEntry entry = glideslope::sim::find_aircraft(data, plan.aircraft);
    // Held to its aircraft's speeds, as a model's plan is.
    glideslope::sim::refuse_what_it_cannot_fly(data, plan, entry.id);

    std::ifstream coverage_file(data / "dem" / "coverage.txt", std::ios::binary);
    if (!coverage_file) {
        throw std::runtime_error("cannot read " + (data / "dem" / "coverage.txt").string());
    }
    const glideslope::world::DemCoverage coverage(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    const std::filesystem::path cache = glideslope::platform::cache_directory();
    const glideslope::world::Fetch fetch = glideslope::world::http_fetch();
    glideslope::world::DownloadedTiles tiles(cache, fetch);
    const glideslope::world::Geoid geoid = glideslope::world::egm2008_geoid(cache, fetch);
    auto dem = std::make_shared<glideslope::world::Dem>(coverage, tiles, &geoid);
    // The ground the aircraft meets: the DEM, with every runway its own
    // surface (world/runway_ground.hpp).
    auto ground = std::make_shared<glideslope::world::CollisionGround>(
        dem, glideslope::world::runway_surfaces(data));
    constexpr double feet_per_metre = 3.280839895013123;
    const auto sea_level_ft = [&](double lat, double lon, double ellipsoid_ft) {
        return ellipsoid_ft - geoid.undulation(lat, lon) * feet_per_metre;
    };
    // The plan's heights, above sea level, as the aircraft's are: above the
    // ellipsoid.
    const std::vector<glideslope::sim::Waypoint> as_written = plan.waypoints;
    for (glideslope::sim::Waypoint& w : plan.waypoints) {
        w.altitude_ft += geoid.undulation(w.latitude_deg, w.longitude_deg) * feet_per_metre;
    }

    glideslope::sim::Aircraft aircraft(data / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [ground](double lat, double lon) { return ground->height_above_ellipsoid(lat, lon); },
        [ground](double lat, double lon) {
            return ground->water(lat, lon) != glideslope::world::Water::none;
        }));
    glideslope::sim::InitialConditions ic;
    glideslope::sim::Controller controller(aircraft, glideslope::sim::Controls{});
    double runway_ft = 0.0;
    if (plan.takeoff) {
        // **The runway is where the ground is**: its height is the collision
        // ground's at the threshold, which is what the aircraft stands on and
        // collides with.
        glideslope::sim::Runway& runway = plan.takeoff->runway;
        runway_ft = ground->height_above_ellipsoid(runway.threshold_lat_deg, runway.threshold_lon_deg) *
                    feet_per_metre;
        runway.elevation_ft = runway_ft;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway_ft;
        ic.terrain_elevation_ft = runway_ft;
        ic.heading_deg = runway.heading_deg;
        ic.airspeed_kts = 0.0;
        ic.gear = 1.0;
    } else if (plan.start) {
        ic.latitude_deg = plan.start->latitude_deg;
        ic.longitude_deg = plan.start->longitude_deg;
        ic.altitude_ft = plan.start->altitude_ft +
                         geoid.undulation(ic.latitude_deg, ic.longitude_deg) * feet_per_metre;
        ic.terrain_elevation_ft =
            ground->height_above_ellipsoid(ic.latitude_deg, ic.longitude_deg) * feet_per_metre;
        ic.heading_deg = plan.start->heading_deg;
        ic.airspeed_kts = plan.start->airspeed_kts;
        ic.gear = 0.0;
    } else {
        std::fprintf(stderr, "glideslope_cli: fly-plan: the plan neither starts nor takes off\n");
        return 2;
    }
    ic.engine_running = true;
    aircraft.initialize(ic);
    if (plan.takeoff) {
        controller.to_ai_flying(plan, glideslope::sim::departure_speeds(data, entry.model));
        std::printf("taking off from %s, the ground there %.0f ft above sea level\n",
                    plan.takeoff->runway.name.c_str(),
                    sea_level_ft(ic.latitude_deg, ic.longitude_deg, runway_ft));
    } else {
        controller.to_ai(plan);
    }

    glideslope::sim::GroundJudge judge(entry.seaplane);
    const auto seconds = [](std::int64_t steps) {
        return static_cast<double>(steps) / glideslope::sim::steps_per_second;
    };
    const std::int64_t most_steps =
        static_cast<std::int64_t>(minutes * 60.0) * glideslope::sim::steps_per_second;
    bool departing = controller.departure() != nullptr;
    std::size_t leg = 0;
    double closest_m = std::numeric_limits<double>::infinity();
    double closest_ft = 0.0;
    double nearest_m = std::numeric_limits<double>::infinity();
    double farthest_m = 0.0;
    double lowest_ft = std::numeric_limits<double>::infinity();
    double highest_ft = -std::numeric_limits<double>::infinity();
    double turns = 0.0;
    const auto said_orbit = [&](const glideslope::sim::Waypoint& w, std::int64_t step,
                                const char* how) {
        std::printf("round %s: %.2f turns, %.0f to %.0f m from its centre, at %.0f to %.0f ft, "
                    "%.0f s in%s\n",
                    w.name.c_str(), turns, nearest_m, farthest_m, lowest_ft, highest_ft,
                    seconds(step), how);
    };
    std::int64_t step = 0;
    for (; step < most_steps; ++step) {
        aircraft.set_controls(controller.fly());
        aircraft.step();
        if (const auto wrecked = judge.judge(aircraft)) {
            std::printf("wrecked, %.0f s in: %s\n", seconds(step), wrecked->c_str());
            return 1;
        }
        const double lat = aircraft.property("position/lat-geod-deg");
        const double lon = aircraft.property("position/long-gc-deg");
        const double ft = sea_level_ft(lat, lon, aircraft.property("position/h-sl-ft"));
        if (departing && controller.departure() == nullptr) {
            departing = false;
            std::printf("took off: the take-off autopilot handed over at %.0f ft, %.0f ft above "
                        "the runway, %.0f s in\n",
                        ft, aircraft.property("position/h-sl-ft") - runway_ft, seconds(step));
        }
        const glideslope::sim::Navigator* navigator = controller.navigator();
        if (departing || navigator == nullptr) {
            continue;
        }
        if (navigator->next() != leg) {
            const glideslope::sim::Waypoint& passed = as_written[leg];
            if (passed.orbit) {
                said_orbit(passed, step, "");
            } else {
                std::printf("passed %s %.0f m off at %.0f ft, %.0f s in\n", passed.name.c_str(),
                            closest_m, closest_ft, seconds(step));
            }
            leg = navigator->next();
            closest_m = nearest_m = std::numeric_limits<double>::infinity();
            farthest_m = turns = 0.0;
            lowest_ft = std::numeric_limits<double>::infinity();
            highest_ft = -std::numeric_limits<double>::infinity();
        }
        if (navigator->finished()) {
            std::printf("the plan is flown, %.0f s in\n", seconds(step));
            return 0;
        }
        const glideslope::sim::Waypoint& to = as_written[leg];
        const double d = glideslope::sim::distance_m(lat, lon, to.latitude_deg, to.longitude_deg);
        if (!to.orbit) {
            if (d < closest_m) {
                closest_m = d;
                closest_ft = ft;
            }
            continue;
        }
        // Round an orbit, once on its circle: after the half turn joining it
        // (sim::OrbitFlown says why).
        if (navigator->circling() && navigator->turns_flown() >= 0.5) {
            turns = navigator->turns_flown();
            nearest_m = std::min(nearest_m, d);
            farthest_m = std::max(farthest_m, d);
            lowest_ft = std::min(lowest_ft, ft);
            highest_ft = std::max(highest_ft, ft);
            if (to.orbit->turns == 0 && turns >= orbits) {
                said_orbit(to, step, ", and round for ever: left there");
                return 0;
            }
        }
    }
    std::printf("not flown in %.0f minutes: at %s\n", minutes,
                leg < as_written.size() ? as_written[leg].name.c_str() : "the end");
    return 1;
}

int weather(const std::string& station) {
    constexpr double radians = 3.14159265358979323846 / 180.0;
    constexpr double knots_per_mps = 3600.0 / 1852.0;
    const std::string hour =
        glideslope::world::utc_hour(std::chrono::system_clock::now());
    const glideslope::world::WeatherReport report = glideslope::world::fetch_weather(
        station, hour, glideslope::world::http_fetch());

    const glideslope::world::Metar& m = report.surface.metar;
    std::printf("weather at %s, %.4f, %.4f, %.0f m, reported day %d at %02d%02dZ\n",
                m.station.c_str(), report.surface.latitude_deg,
                report.surface.longitude_deg, report.surface.elevation_m, m.day, m.hour,
                m.minute);
    if (!m.wind_speed_kt) {
        std::printf("  wind          not reported\n");
    } else if (*m.wind_speed_kt == 0.0) {
        std::printf("  wind          calm\n");
    } else {
        if (m.wind_from_deg) {
            std::printf("  wind          from %03.0f at %.0f kt", *m.wind_from_deg,
                        *m.wind_speed_kt);
        } else {
            std::printf("  wind          variable at %.0f kt", *m.wind_speed_kt);
        }
        if (m.gust_kt) {
            std::printf(", gusting %.0f kt", *m.gust_kt);
        }
        std::printf("\n");
    }
    if (m.temperature_c) {
        std::printf("  temperature   %.1f C\n", *m.temperature_c);
    }
    if (m.dewpoint_c) {
        std::printf("  dew point     %.1f C\n", *m.dewpoint_c);
    }
    if (m.qnh_hpa) {
        std::printf("  QNH           %.1f hPa\n", *m.qnh_hpa);
    }

    std::printf("winds near the ground for %s UTC\n", hour.c_str());
    for (const auto& w : report.aloft->near_ground) {
        const double speed = std::hypot(w.wind_north_mps, w.wind_east_mps);
        const double from = std::fmod(
            std::atan2(-w.wind_east_mps, -w.wind_north_mps) / radians + 360.0, 360.0);
        std::printf("  %5.0f m above the ground     from %03.0f at %3.0f kt%s\n",
                    w.height_m, from, speed * knots_per_mps,
                    w.height_m <= 10.0 ? "  (the METAR's is used)" : "");
    }
    std::printf("winds aloft for %s UTC\n", hour.c_str());
    std::printf("  %8s %9s %16s %13s\n", "pressure", "height", "wind", "temperature");
    for (const auto& level : report.aloft->levels) {
        const double speed = std::hypot(level.wind_north_mps, level.wind_east_mps);
        const double from = std::fmod(
            std::atan2(-level.wind_east_mps, -level.wind_north_mps) / radians + 360.0,
            360.0);
        std::printf("  %4.0f hPa %7.0f m   from %03.0f at %3.0f kt %11.1f C%s\n",
                    level.pressure_hpa, level.height_m, from, speed * knots_per_mps,
                    level.temperature_c,
                    level.height_m <= report.surface.elevation_m ? "  (underground)"
                                                                 : "");
    }
    std::printf("METAR from aviationweather.gov, NOAA's Aviation Weather Center\n");
    std::printf("%s (https://open-meteo.com/), CC BY 4.0\n",
                glideslope::world::open_meteo_credit);
    return 0;
}

// The air a fixed gusty report gives at a thousand fixed places and times,
// each wind component in whole 1e-11 m/s: what CI compares across platforms
// (tests/cmake/cross_platform_flights.cmake), whose arithmetic is in integers.
// What a report shows observed on the ground at a place - the DEM's, fetched
// as `height` fetches it: its cloud decks, the nearest places well inside the
// lowest deck's cloud and well inside its gaps - for the frame tests to look
// from - its haze and what falls.
int sky(const std::filesystem::path& data, std::string_view report,
        std::string_view latitude_text, std::string_view longitude_text) {
    namespace world = glideslope::world;
    const auto number = [](std::string_view text, double low, double high,
                           const char* what) {
        const std::string copy(text);
        char* end = nullptr;
        const double v = std::strtod(copy.c_str(), &end);
        if (copy.empty() || *end != '\0' || !(v >= low && v <= high)) {
            std::fprintf(stderr, "glideslope_cli: %s must be a number from %g to %g\n",
                         what, low, high);
            glideslope::platform::end_process(2);
        }
        return v;
    };
    const double lat = number(latitude_text, -90.0, 90.0, "the latitude");
    const double lon = number(longitude_text, -180.0, 180.0, "the longitude");
    const world::Metar metar = world::parse_metar(report);

    // The station's ground, from the DEM, as `height` finds it.
    std::ifstream coverage_file(data / "dem" / "coverage.txt", std::ios::binary);
    if (!coverage_file) {
        throw std::runtime_error("cannot read " +
                                 (data / "dem" / "coverage.txt").string());
    }
    const world::DemCoverage coverage(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    const std::filesystem::path cache = glideslope::platform::cache_directory();
    const world::Fetch fetch = world::http_fetch();
    world::DownloadedTiles tiles(cache, fetch);
    const world::Geoid geoid = world::egm2008_geoid(cache, fetch);
    world::Dem dem(coverage, tiles, &geoid);
    const double elevation = dem.height_above_geoid(lat, lon);
    const double undulation = geoid.undulation(lat, lon);
    const std::vector<world::CloudDeck> decks = world::cloud_decks(metar, elevation);
    std::printf("sky of %s %02d%02d%02dZ over %.7f, %.7f, %.3f m above sea level\n",
                metar.station.c_str(), metar.day, metar.hour, metar.minute, lat, lon,
                elevation);
    std::printf("visibility %.0f m, in haze to %.3f m above sea level\n",
                world::drawn_visibility_m(metar), world::haze_top_m(decks, elevation));
    for (std::size_t d = 0; d < decks.size(); ++d) {
        std::printf("deck %zu cover %.3f base %.3f top %.3f m above sea level; base "
                    "%.3f m above the ellipsoid\n",
                    d + 1, decks[d].cover, decks[d].base_m, decks[d].top_m,
                    decks[d].base_m + undulation);
    }
    if (!decks.empty()) {
        // The nearest places, on a 100 m grid, where the lowest deck is thick
        // cloud or clear for 250 m all round.
        const world::CloudPattern pattern(world::air_seed_of(metar), 0,
                                          decks.front().cover);
        const auto everywhere_near = [&](double e, double n, bool cloudy) {
            for (int i = -1; i <= 1; ++i) {
                for (int j = -1; j <= 1; ++j) {
                    const double d = pattern.density(e + 250.0 * i, n + 250.0 * j);
                    if (cloudy ? d < 0.999 : d > 0.001) {
                        return false;
                    }
                }
            }
            return true;
        };
        constexpr double metres_per_degree = 111319.49;
        for (const bool cloudy : {true, false}) {
            bool found = false;
            for (int r = 0; r <= 100 && !found; ++r) {
                for (int i = -r; i <= r && !found; ++i) {
                    for (int j = -r; j <= r && !found; ++j) {
                        if (std::max(std::abs(i), std::abs(j)) != r ||
                            !everywhere_near(100.0 * i, 100.0 * j, cloudy)) {
                            continue;
                        }
                        std::printf(
                            "%s at %.7f,%.7f\n", cloudy ? "cloudy" : "clear",
                            lat + 100.0 * j / metres_per_degree,
                            lon + 100.0 * i /
                                      (metres_per_degree *
                                       std::cos(lat * 3.14159265358979323846 / 180.0)));
                        found = true;
                    }
                }
            }
            if (!found) {
                std::printf("%s nowhere within 10 km\n", cloudy ? "cloudy" : "clear");
            }
        }
    }
    const world::Falling falling = world::precipitation_of(metar);
    const char* what = falling.what == world::Precipitation::rain      ? "rain"
                       : falling.what == world::Precipitation::drizzle ? "drizzle"
                       : falling.what == world::Precipitation::snow    ? "snow"
                                                                       : "none";
    std::printf("falling %s\n", what);
    return 0;
}

int air() {
    namespace world = glideslope::world;
    // Ground for the air to rise and sink over: a stand-in for the Sandias, a
    // ridge 900 m high 8 km east of the station, and hills about it.
    constexpr double station_lat = 35.0419;
    constexpr double station_lon = -106.6092;
    constexpr double elevation = 1631.0;
    const world::GroundAt ground = [](double lat, double lon) {
        const double east = (lon - station_lon) * 111319.49 *
                            std::cos(station_lat * 3.14159265358979323846 / 180.0);
        const double north = (lat - station_lat) * 111319.49;
        const double ridge = (east - 8000.0) / 3000.0;
        return elevation + 900.0 * std::exp(-ridge * ridge) +
               150.0 * std::sin(north / 2500.0) * std::cos(east / 4000.0);
    };

    // A gusty morning, and a hot, calm afternoon under a well-mixed layer 3 km
    // deep, with a westerly strengthening above it.
    world::WeatherReport gusty;
    gusty.surface.metar =
        world::parse_metar("KABQ 180759Z 18027G37KT 9999 18/16 A2992");
    world::WeatherReport warm;
    warm.surface.metar =
        world::parse_metar("KABQ 182200Z 24008KT 9999 SKC 33/02 A3005");
    world::WindsAloft aloft;
    aloft.latitude_deg = station_lat;
    aloft.longitude_deg = station_lon;
    const struct {
        double pressure;
        double height;
        double temperature;
        double east;
    } levels[] = {{800.0, 2000.0, 26.0, 6.0},
                  {700.0, 3100.0, 15.2, 9.0},
                  {600.0, 4300.0, 8.0, 12.0},
                  {500.0, 5700.0, -2.0, 16.0},
                  {400.0, 7300.0, -14.0, 20.0}};
    for (const auto& l : levels) {
        world::AloftLevel level;
        level.pressure_hpa = l.pressure;
        level.height_m = l.height;
        level.temperature_c = l.temperature;
        level.wind_east_mps = l.east;
        aloft.levels.push_back(level);
    }
    warm.aloft = aloft;
    for (world::WeatherReport* r : {&gusty, &warm}) {
        r->surface.latitude_deg = station_lat;
        r->surface.longitude_deg = station_lon;
        r->surface.elevation_m = elevation;
        r->air_seed = world::air_seed_of(r->surface.metar);
    }
    world::ReportedWeather gusty_weather(gusty, nullptr, 0.0, ground);
    world::ReportedWeather warm_weather(warm, nullptr, 0.0, ground);

    std::uint64_t state = 12345;
    const auto next = [&] {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(state >> 11) / 9007199254740992.0;
    };
    std::printf(
        "air of KABQ 180759Z 18027G37KT, then KABQ 182200Z 24008KT 33/02 under a "
        "well-mixed layer, over a ridge, in 1e-11 m/s north, east and down\n");
    for (int i = 0; i < 1000; ++i) {
        const bool morning = i < 500;
        const double lat = station_lat + (next() - 0.5) * 0.36;
        const double lon = station_lon + (next() - 0.5) * 0.36;
        const double above = next() * (morning ? 1500.0 : 3000.0);
        const double time = next() * 3600.0;
        const double height = (morning ? elevation : ground(lat, lon)) + above;
        const auto c =
            (morning ? gusty_weather : warm_weather).at(lat, lon, height, time);
        std::printf("air %d %lld %lld %lld\n", i,
                    static_cast<long long>(std::llround(c.wind_north_mps * 1e11)),
                    static_cast<long long>(std::llround(c.wind_east_mps * 1e11)),
                    static_cast<long long>(std::llround(c.wind_down_mps * 1e11)));
    }
    return 0;
}

} // namespace

// **Completes a session with a server, and says so.** This is a client in
// the only sense the transport needs: it knows the server's static key, does
// the handshake, seals something and is answered. It flies nothing.
//
// It is here rather than in the client proper because the transport can be
// shown to work long before there is anything to fly over it, and a test
// needs something to point at a server.
// **Staying connected, which is answering the server's knocks.** The server
// pings each connection once a second and draws the round trip on its
// dashboard; a client that answers is also a client the server does not let
// go. Nothing else is sent, because nothing else is defined to go inside a
// sealed body yet - see `docs/TRANSPORT.md`, "What is not here yet".
// **How often a client sends its inputs.** REQUIREMENTS.md does not fix a
// rate; 30 Hz is enough that the server never waits for one and few enough
// that four clients do not flood it, and with four frames in every packet a
// loss of three in a row still loses nothing.
constexpr double inputs_every_s = 1.0 / 30.0;

// **A client that predicts** (`connect ... --predict`): what a real
// client does with its own aircraft and everybody else's, measured.
//
// Its own aircraft is flown here, at 120 Hz, on the inputs this client sends,
// and put right by the motion in each state update (sim::Prediction) - how far
// each correction moved it is the prediction error. Every other aircraft is
// shown 100 ms in the past between the snapshots that arrived
// (net::Interpolated), sampled at 60 Hz as a screen would be, and each frame
// written down (`--track`). What it drew is not judged here: against the
// updates it heard itself it could only agree with itself, since what it lost
// it never knew. A test judges it against a client that heard everything
// (tests/tools/interpolation_check.cpp).
class Predicting {
public:
    // What the server has said each aircraft is (`AIRCRAFT`): its own is
    // flown as the model the server flies it as, and not before it is known.
    void introduced(std::uint8_t aircraft, const std::string& model) {
        models_[aircraft] = model;
    }

    // **The pilot**: the controls keep changing, so that when an input
    // arrives, late or not at all, matters - a stick held still would make
    // every input the same and prediction trivially right.
    static glideslope::sim::Controls pilot(std::uint32_t sequence) {
        const double t = static_cast<double>(sequence) / 30.0;
        glideslope::sim::Controls c;
        c.throttle = 0.7 + 0.2 * std::sin(t * 0.7);
        c.aileron = 0.3 * std::sin(t * 1.3);
        c.elevator = 0.05 * std::sin(t * 0.9);
        c.rudder = 0.05 * std::sin(t * 0.5);
        return c;
    }

    // **The server's weather** (REQUIREMENTS.md 6.3), each whole one as it is
    // heard: its own aircraft flies it, on the session's clock - the server's
    // air, not a fetch of this client's own - with the geoid built the first
    // time a weather that is not still air is heard, from the cache.
    // False, with why, for a weather refused (frontend::HeardAir::heard).
    bool weather(const glideslope::net::Weather& w,
                 const std::optional<glideslope::net::WeatherAloft>& aloft, std::string& why) {
        if (!geoid_for_air_ && !w.metar.empty()) {
            // Built for the first weather that is not still air; still air
            // before it was no weather at all, and nothing is lost.
            geoid_for_air_ = std::make_unique<glideslope::world::Geoid>(
                glideslope::world::egm2008_geoid(glideslope::platform::cache_directory(),
                                                 glideslope::world::http_fetch()));
            air_.reset();
        }
        if (!air_) {
            // **Over no ground**, as the server's is (glideslope_server's
            // Fleet::fly_in says why). The geoid is kept: the wind's profile
            // is in height above the sea.
            // **On the clock of the step being flown** - forward or again,
            // in a replay - as the server will fly it (sim::Prediction::
            // session_time_s), and this machine's estimate of the session's
            // clock before that is known.
            air_.emplace(geoid_for_air_.get(), glideslope::world::GroundAt{}, [this] {
                if (prediction_) {
                    if (const auto t = prediction_->session_time_s()) {
                        return *t;
                    }
                }
                return session_now_s_;
            });
        }
        if (!air_->heard(w, aloft, &why)) {
            return false;
        }
        if (aircraft_ && air_->air()) {
            aircraft_->set_weather(air_->air());
        }
        return true;
    }

    // Flies its own aircraft forward to `local_s`, on the input most recently
    // sent, which is what the server will fly too.
    void advance(double local_s, std::uint32_t sequence, const glideslope::sim::Controls& c) {
        // The session's clock, which the air is on: as this machine's clock
        // makes it now.
        if (clock_.known()) {
            session_now_s_ = clock_.now(local_s);
        }
        const auto due = static_cast<long long>(local_s *
                                                static_cast<double>(glideslope::sim::steps_per_second));
        if (ai_flying_) {
            // The AI pilot flies it, on the server: nothing is flown here.
            stepped_ = due;
            return;
        }
        if (!prediction_) {
            // Nothing to fly yet, but what would have been flown is kept:
            // when the first update comes it is already a trip old, and
            // these are what carry it forward to now (start).
            for (; stepped_ < due; ++stepped_) {
                before_.push_back({sequence, c});
                if (before_.size() > glideslope::sim::most_unacknowledged) {
                    before_.pop_front();
                }
            }
            if (sequence > 0 && (joining_.empty() || joining_.back() != sequence)) {
                joining_.push_back(sequence);
            }
            return;
        }
        // Where it was flown to on this input, step by step, for the
        // server's word on the same input, as far into it, to be held
        // against - once it has had the server's word on any input of its.
        // Before that it flew on from an update a trip old, which it could
        // not carry forward over the time the server flew before any input
        // of its arrived: it is joining, not predicting.
        // Taken back from the AI, likewise, until the server has applied an
        // input sent since and the controls have met the pilot's
        // (sim::Controller: at a hand's pace, full travel in a second).
        // And once the clocks' difference is known (sim::offset_settled):
        // before that the error is the estimate settling.
        const bool answering = answered_ && !resuming_ && local_s >= settled_at_s_;
        const bool counted = answering && prediction_->settled();
        for (; stepped_ < due; ++stepped_) {
            const std::uint64_t step = prediction_->steps();
            prediction_->step(sequence, c);
            predicted_at_.push_back(
                {step, aircraft_->motion().location_ecef_m, counted && sequence > 0});
            if (predicted_at_.size() > glideslope::sim::most_unacknowledged) {
                predicted_at_.pop_front();
            }
        }
        if (stepped_ == due && sequence > 0 && !answering &&
            (joining_.empty() || joining_.back() != sequence)) {
            joining_.push_back(sequence);
        }
    }

    void heard(const glideslope::net::StatePacket& state, double local_s) {
        // The session clock, at the rate it runs (net::SessionClock).
        clock_.heard(state.simulation_time_s, local_s);
        // When each update arrived, and what the clock made of it.
        if (track_) {
            *track_ << "arrived " << state.simulation_time_s << ' ' << local_s << ' '
                    << clock_.now(local_s) << ' ' << clock_.rate() << '\n';
        }
        // The watched aircraft's controls, kept by the time they were true.
        if (state.watched) {
            watched_[state.simulation_time_s] = *state.watched;
            while (watched_.size() > 64) {
                watched_.erase(watched_.begin());
            }
        }
        // **An update older than one already used is not used again** for
        // this client's own aircraft: the network reorders them, the newer
        // one has put it right already, and the inputs the older would have
        // it fly again are gone. The other aircraft take it - interpolation
        // sorts its snapshots itself.
        const bool newest = !reconciled_s_ || state.simulation_time_s > *reconciled_s_;
        if (state.yours && newest && !ai_flying_) {
            reconciled_s_ = state.simulation_time_s;
            const glideslope::net::OwnMotion& y = *state.yours;
            glideslope::sim::Motion m;
            m.location_ecef_m = {y.x_m, y.y_m, y.z_m};
            for (std::size_t i = 0; i < 4; ++i) {
                m.attitude_local[i] = static_cast<double>(y.attitude[i]);
            }
            for (std::size_t i = 0; i < 3; ++i) {
                m.uvw_mps[i] = static_cast<double>(y.uvw_mps[i]);
                m.pqr_radps[i] = static_cast<double>(y.pqr_radps[i]);
            }
            if (!prediction_) {
                const auto model = models_.find(state.your_aircraft);
                if (model != models_.end()) {
                    start(m, model->second, state.last_input_applied,
                          state.yours->steps_into_input);
                }
            } else {
                // **The prediction error**: where the server says the
                // aircraft was when it had got to an input, against where
                // this client had flown it to on that input. A client that
                // lagged the server - put back to each update and never
                // flown forward again - would be corrected by little at a
                // time, and it is this, not the corrections, that shows it.
                // Put right first, which places the server's word on this
                // client's clock (sim::Prediction), and then held against
                // where this client had flown it to by that step.
                const std::uint32_t applied = state.last_input_applied;
                const auto c = prediction_->reconcile(
                    m, applied, state.yours->steps_into_input,
                    static_cast<std::uint64_t>(std::llround(
                        state.simulation_time_s *
                        static_cast<double>(glideslope::sim::steps_per_second))));
                const Predicted* at = nullptr;
                if (c.at_step && *c.at_step > 0) {
                    const auto found = std::find_if(
                        predicted_at_.begin(), predicted_at_.end(),
                        [&](const Predicted& p) { return p.step + 1 == *c.at_step; });
                    at = found != predicted_at_.end() ? &*found : nullptr;
                }
                if (at != nullptr && at->counted) {
                    const double error = std::hypot(at->where[0] - m.location_ecef_m[0],
                                                    at->where[1] - m.location_ecef_m[1],
                                                    at->where[2] - m.location_ecef_m[2]);
                    if (error > worst_error_m_) {
                        worst_error_m_ = error;
                        worst_error_at_s_ = local_s;
                    }
                    ++compared_;
                    errors_m_.push_back(error);
                    // **After a take-over, apart**: updates the server can
                    // only have sent if it is flying the aircraft taken over
                    // by this client's inputs.
                    if (taken_over_ > 0 && !resuming_) {
                        worst_error_since_m_ = std::max(worst_error_since_m_, error);
                        ++compared_since_;
                    }
                }
                // Kept from that step on: a later word is about a later one.
                while (c.at_step && !predicted_at_.empty() &&
                       predicted_at_.front().step + 1 < *c.at_step) {
                    predicted_at_.pop_front();
                }
                // **A correction small enough to hide is hidden** - taken up
                // over the next frames, as sim/prediction.hpp says - and one
                // too large is shown as the jump it is.
                corrected_ = !c.snapped;
                answered_ = answered_ || state.last_input_applied > 0;
                if (resuming_ && state.last_input_applied >= resumed_from_) {
                    resuming_ = false;
                    settled_at_s_ = local_s + 1.0;
                }
                ++corrections_;
                worst_correction_m_ = std::max(worst_correction_m_, c.moved_m);
                if (c.snapped) {
                    ++snapped_;
                    if (taken_over_ > 0) {
                        ++snapped_since_;
                    }
                }
            }
        }
        for (const glideslope::net::AircraftState& a : state.aircraft) {
            if (!origin_) {
                origin_ = glideslope::world::to_geodetic({a.x_m, a.y_m, a.z_m});
                origin_ecef_ = {a.x_m, a.y_m, a.z_m};
            }
            glideslope::net::RemoteState r;
            r.time_s = state.simulation_time_s;
            local(a.x_m, a.y_m, a.z_m, true, r.north_m, r.east_m, r.down_m);
            local(static_cast<double>(a.vx_mps), static_cast<double>(a.vy_mps),
                  static_cast<double>(a.vz_mps), false, r.north_mps, r.east_mps, r.down_mps);
            r.heading_deg = static_cast<double>(a.heading_deg);
            r.pitch_deg = static_cast<double>(a.pitch_deg);
            r.roll_deg = static_cast<double>(a.roll_deg);
            r.wrecked = a.condition == glideslope::net::Condition::wrecked;
            // Its own is kept too: while the AI flies it, it is drawn from
            // the updates like any other.
            if (a.index == state.your_aircraft) {
                own_shown_.received(r);
            } else {
                others_[a.index].received(r);
            }
        }
    }

    // **Another aircraft taken over** (`CONTROLLER_SWAP` to this client, for
    // an AI's): it is this client's own now, predicted from the next update
    // with its motion, as after a take-back - and what is shown of it moves
    // on from where it was being drawn as another, not from where the one
    // left behind was.
    void taken_over(std::uint8_t number, std::uint32_t sequence) {
        display_.taken_over(number);
        // What it had of that aircraft as another carries on as its own.
        const auto was_other = others_.find(number);
        own_shown_ = was_other != others_.end() ? was_other->second
                                                 : glideslope::net::Interpolated{};
        others_.erase(number);
        ai_flying_ = false;
        ++taken_over_;
        resuming_ = true;
        resumed_from_ = sequence + 1;
        prediction_.reset();
        aircraft_.reset();
        before_.clear();
        predicted_at_.clear();
    }

    // **Its own aircraft handed to the AI pilot, or taken back**, as the
    // server said (`CONTROLLER_SWAP`). Handed over, it is no longer
    // predicted - nothing sent here flies it - and is drawn from the updates;
    // taken back, it is predicted again from the next update with its motion.
    // Either way what is shown of it moves across over `blend_s` rather than
    // jumping from where it was shown to where it now is.
    // `sequence` is the newest input sent: the next one is the first the
    // server can apply after a take-back.
    void handed(bool to_ai, std::uint32_t sequence) {
        if (to_ai == ai_flying_) {
            return;
        }
        ai_flying_ = to_ai;
        if (to_ai) {
            ++handed_over_;
        } else {
            ++taken_back_;
            resuming_ = true;
            resumed_from_ = sequence + 1;
            prediction_.reset();
            aircraft_.reset();
            before_.clear();
            predicted_at_.clear();
        }
        display_.switching();
    }

    // Shows every other aircraft at `local_s`, as a 60 Hz screen would.
    void render(double local_s) {
        if (!clock_.known() || local_s - rendered_s_ < 1.0 / 60.0) {
            return;
        }
        if (rendered_s_ >= 0.0) {
            longest_between_frames_s_ = std::max(longest_between_frames_s_, local_s - rendered_s_);
        }
        rendered_s_ = local_s;
        const double now = clock_.now(local_s);
        own_frame(now, local_s);
        // **The watched aircraft's controls, shown as it is**: 100 ms behind
        // the clock, between the two updates either side, as its position is.
        if (track_ && !watched_.empty()) {
            const double at = now - glideslope::net::shown_behind_s;
            auto after = watched_.lower_bound(at);
            if (after != watched_.end() && after != watched_.begin()) {
                const auto before = std::prev(after);
                const double span = after->first - before->first;
                const double k = span > 0.0 ? (at - before->first) / span : 0.0;
                const auto mix = [k](double a, double b) { return a + k * (b - a); };
                const glideslope::net::Watched& a = before->second;
                const glideslope::net::Watched& b = after->second;
                *track_ << "showncontrols " << at << ' ' << static_cast<unsigned>(a.aircraft)
                        << ' ' << mix(a.aileron, b.aileron) << ' '
                        << mix(a.elevator, b.elevator) << ' ' << mix(a.rudder, b.rudder)
                        << ' ' << mix(a.throttle, b.throttle) << ' '
                        << mix(a.flaps, b.flaps) << '\n';
                ++controls_shown_;
            }
        }
        for (auto& [index, shown] : others_) {
            if (!shown.known()) continue;
            const glideslope::net::RemoteState got = shown.at(now);
            ++shown_;
            if (shown.extrapolating()) {
                ++extrapolated_;
            }
            {
                // Where it is shown, for taking it over without a step.
                // Moving as its path moves, per second of this machine's
                // clock - not as the update reports, which a blend back from
                // a guess differs from by up to 20 m/s - so that a take-over
                // blends from where it was going (review, 2026-09-26).
                const std::array<double, 3> here = ecef(got.north_m, got.east_m, got.down_m);
                const std::array<double, 3> path = shown.path_velocity();
                const double rate = clock_.known() ? clock_.rate() : 1.0;
                display_.seen(index, local_s, {here[0], here[1], here[2]},
                            velocity(path[0] * rate, path[1] * rate, path[2] * rate));
            }
            if (track_) {
                // Where it was drawn, back in the Earth-centred frame, and the
                // session time it was drawn as being at.
                std::array<double, 3> at = ecef(got.north_m, got.east_m, got.down_m);
                // After them, what explains a frame drawn wrong: this
                // machine's clock, whether it was a guess, the newest update
                // it had, and how far a blend back from a guess moved it
                // (tools/interpolation_check.cpp prints them).
                *track_ << "shown " << now - glideslope::net::shown_behind_s << ' '
                        << static_cast<unsigned>(index) << ' ' << at[0] << ' ' << at[1]
                        << ' ' << at[2] << ' ' << local_s << ' '
                        << (shown.extrapolating() ? 1 : 0) << ' ' << shown.newest_s() << ' '
                        << shown.blending_m() << '\n';
            }
        }
    }

    void track_to(std::ostream& out) { track_ = &out; }
    void long_frame_after_switch() { long_frame_after_switch_ = true; }

    // **Where its own aircraft is shown this frame**: predicted while this
    // client flies it, drawn from the updates while the AI does, and blended
    // across a switch. The step measured is how far what is shown is from
    // where the frame before it, carried on part by part, puts it, and the
    // blend's own pace - nought for an aircraft moving smoothly, whatever the
    // frames' timing, and the size of any jump - at a switch and everywhere
    // else.
    void own_frame(double now, double local_s) {
        std::optional<std::array<double, 3>> at;
        // Predicted while this client flies it; drawn from the updates while
        // the AI does, and until its prediction starts again after a
        // take-back or a take-over - nothing drawn then would leave a gap the
        // blend afterwards would start from.
        const bool predicted = !ai_flying_ && prediction_ != nullptr;
        // And how fast it moves, as its own flight model or the updates say.
        std::array<double, 3> v_at{};
        constexpr double m_per_ft = 0.3048;
        if (predicted) {
            at = aircraft_->motion().location_ecef_m;
            // Its flight model's north, east and down are where it is, not
            // at the session's origin: turned there, 100 km off they would
            // be a degree out.
            const glideslope::world::Geodetic here =
                glideslope::world::to_geodetic({(*at)[0], (*at)[1], (*at)[2]});
            v_at = turned_to_ecef(here.latitude_deg, here.longitude_deg,
                                  aircraft_->property("velocities/v-north-fps") * m_per_ft,
                                  aircraft_->property("velocities/v-east-fps") * m_per_ft,
                                  aircraft_->property("velocities/v-down-fps") * m_per_ft);
        } else if (own_shown_.known() && origin_) {
            const glideslope::net::RemoteState r = own_shown_.at(now);
            at = ecef(r.north_m, r.east_m, r.down_m);
            // Drawn from the updates, it moves as the interpolation between
            // them moves it - which in a turn, or with jitter, is not the
            // velocity an update reports - so the path's own, as the
            // interpolation says it, per second of this machine's clock.
            // Guessed from where it was a frame ago instead, a jump between
            // two frames was counted again, larger, at the next long one.
            const std::array<double, 3> path = own_shown_.path_velocity();
            const double rate = clock_.known() ? clock_.rate() : 1.0;
            v_at = velocity(path[0] * rate, path[1] * rate, path[2] * rate);
        }
        if (!at) {
            return;
        }
        // **For a test: a long frame just after a switch**, as a runner that
        // stalls draws one, where anything carried on wrongly shows. With
        // `--long-frame-after-switch`, nothing is drawn for 0.4 s after the
        // third frame after a switch - the client goes on flying, sending and
        // hearing all the while; only the drawing waits. The third, not the
        // first: the frames before it are what show a blend too quick, which
        // a long frame at once would have covered - a blend of a millisecond
        // passed. Each is counted, so a test can say every switch had one.
        if (long_frame_after_switch_ && display_.frames_since_switch() == 3 &&
            display_.last_frame_s()) {
            if (local_s - *display_.last_frame_s() < 0.4) {
                if (!building_long_frame_) {
                    building_long_frame_ = true;
                    ++long_frames_;
                }
                return;
            }
            // One the machine made long enough by itself is one all the same.
            if (!building_long_frame_) {
                ++long_frames_;
            }
        }
        building_long_frame_ = false;
        // **Shown, blended and measured by the one model of a display**
        // (frontend/shown.hpp), which the client with the window draws by.
        // One switch overtaken by another before its long frame came - a
        // take-back is two, a frame or two apart, when prediction starts
        // again - has the next one's instead, and is counted as such.
        const std::size_t switches_before = display_.switches();
        const int frames_before = display_.frames_since_switch();
        (void)display_.frame(local_s, glideslope::frontend::OwnShown::Source{
                                        {(*at)[0], (*at)[1], (*at)[2]}, v_at, predicted,
                                        corrected_});
        corrected_ = false;
        if (display_.switches() > switches_before && switches_before > 0 && frames_before < 3) {
            ++overtaken_;
        }
    }

    // What it found, as the lines it says: a test reads them from the file
    // it was given (`--heard`).
    std::vector<std::string> report() const {
        std::vector<std::string> lines;
        char line[256];
        std::snprintf(line, sizeof line,
                      "predicted: %zu corrections, the worst %.3f m, %zu too large to hide",
                      corrections_, worst_correction_m_, snapped_);
        lines.emplace_back(line);
        std::snprintf(line, sizeof line,
                      "prediction error: %zu updates compared, the worst %.3f m (%zu inputs "
                      "flown while joining or taking back, before the server had applied one, "
                      "not compared), "
                      "%.1f s in",
                      compared_, worst_error_m_, joining_.size(), worst_error_at_s_);
        lines.emplace_back(line);
        // **And the median**: what the error is in the run of updates, where
        // the worst is what one slow frame made it (server_weather.cmake).
        if (!errors_m_.empty()) {
            std::vector<double> sorted = errors_m_;
            const auto middle = sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2);
            std::nth_element(sorted.begin(), middle, sorted.end());
            std::snprintf(line, sizeof line, "prediction error median: %.3f m over %zu updates",
                          *middle, sorted.size());
            lines.emplace_back(line);
        }
        if (handed_over_ + taken_back_ + taken_over_ > 0) {
            std::snprintf(line, sizeof line,
                          "own aircraft: handed to the AI %zu times, taken back %zu and another "
                          "taken over %zu; the largest step at a switch %.3f m, and otherwise "
                          "%.3f m",
                          handed_over_, taken_back_, taken_over_,
                          display_.worst_step_at_switch_m(), display_.worst_step_otherwise_m());
            lines.emplace_back(line);
            if (!display_.worst_step_what().empty()) {
                lines.emplace_back("the largest step at a switch: " + display_.worst_step_what());
            }
        }
        if (long_frame_after_switch_) {
            std::snprintf(line, sizeof line,
                          "long frames: one of 0.4 s built after %zu of %zu switches, and %zu "
                          "overtaken by another before theirs",
                          long_frames_, display_.switches(), overtaken_);
            lines.emplace_back(line);
        }
        if (taken_over_ > 0) {
            std::snprintf(line, sizeof line,
                          "after the take-over: %zu updates compared, the worst %.3f m, %zu "
                          "corrections too large to hide",
                          compared_since_, worst_error_since_m_, snapped_since_);
            lines.emplace_back(line);
        }
        std::snprintf(line, sizeof line, "interpolated: %zu aircraft drawn, %zu of them carried on "
                      "past the newest update; the longest between frames %.0f ms",
                      shown_, extrapolated_, longest_between_frames_s_ * 1000.0);
        lines.emplace_back(line);
        return lines;
    }

private:
    void start(const glideslope::sim::Motion& m, const std::string& model,
               std::uint32_t last_applied, std::size_t steps_into) {
        aircraft_ = std::make_unique<glideslope::sim::Aircraft>(
            glideslope::platform::data_directory() / "jsbsim", model);
        aircraft_->set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        const glideslope::world::Geodetic g = glideslope::world::to_geodetic(
            {m.location_ecef_m[0], m.location_ecef_m[1], m.location_ecef_m[2]});
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = g.latitude_deg;
        ic.longitude_deg = g.longitude_deg;
        ic.altitude_ft = g.height_m * 3.28083989501312;
        ic.airspeed_kts = std::hypot(m.uvw_mps[0], m.uvw_mps[1], m.uvw_mps[2]) / 0.514444;
        ic.engine_running = true;
        ic.gear = 0.0;
        aircraft_->initialize(ic);
        // In the server's air, if it has said any.
        if (air_ && air_->air()) {
            aircraft_->set_weather(air_->air());
        }
        aircraft_->set_motion(m);
        prediction_ = std::make_unique<glideslope::sim::Prediction>(*aircraft_);
        // From where the server said it was to where it is now: every step
        // flown since - from as far into the input it had applied as it had
        // flown it, or, if that input was not kept, from the next.
        const auto began = std::find_if(before_.begin(), before_.end(), [&](const auto& b) {
            return b.first == last_applied;
        });
        const auto from = began != before_.end()
                              ? static_cast<std::size_t>(began - before_.begin()) + steps_into
                              : before_.size();
        for (std::size_t i = 0; i < before_.size(); ++i) {
            const auto& [sequence, controls] = before_[i];
            if (began != before_.end() ? i >= from : sequence > last_applied) {
                prediction_->step(sequence, controls);
            }
        }
        before_.clear();
    }

    // Earth-centred to north-east-down about the first aircraft seen: a
    // position (less the origin), or a velocity (as it is).
    void local(double x, double y, double z, bool position, double& n, double& e,
               double& d) const {
        if (position) {
            x -= origin_ecef_[0];
            y -= origin_ecef_[1];
            z -= origin_ecef_[2];
        }
        const double lat = origin_->latitude_deg * 3.14159265358979323846 / 180.0;
        const double lon = origin_->longitude_deg * 3.14159265358979323846 / 180.0;
        n = -std::sin(lat) * std::cos(lon) * x - std::sin(lat) * std::sin(lon) * y +
            std::cos(lat) * z;
        e = -std::sin(lon) * x + std::cos(lon) * y;
        d = -std::cos(lat) * std::cos(lon) * x - std::cos(lat) * std::sin(lon) * y -
            std::sin(lat) * z;
    }

    // A velocity in the session's local frame, north, east and down about
    // its origin, turned into the Earth-centred one.
    std::array<double, 3> velocity(double n, double e, double d) const {
        return turned_to_ecef(origin_->latitude_deg, origin_->longitude_deg, n, e, d);
    }
    // North, east and down where the latitude and longitude are, turned into
    // the Earth-centred frame.
    static std::array<double, 3> turned_to_ecef(double latitude_deg, double longitude_deg,
                                                double n, double e, double d) {
        const double lat = latitude_deg * 3.14159265358979323846 / 180.0;
        const double lon = longitude_deg * 3.14159265358979323846 / 180.0;
        return {-std::sin(lat) * std::cos(lon) * n - std::sin(lon) * e -
                    std::cos(lat) * std::cos(lon) * d,
                -std::sin(lat) * std::sin(lon) * n + std::cos(lon) * e -
                    std::cos(lat) * std::sin(lon) * d,
                std::cos(lat) * n - std::sin(lat) * d};
    }
    std::array<double, 3> ecef(double n, double e, double d) const {
        const double lat = origin_->latitude_deg * 3.14159265358979323846 / 180.0;
        const double lon = origin_->longitude_deg * 3.14159265358979323846 / 180.0;
        return {origin_ecef_[0] - std::sin(lat) * std::cos(lon) * n - std::sin(lon) * e -
                    std::cos(lat) * std::cos(lon) * d,
                origin_ecef_[1] - std::sin(lat) * std::sin(lon) * n + std::cos(lon) * e -
                    std::cos(lat) * std::sin(lon) * d,
                origin_ecef_[2] + std::cos(lat) * n - std::sin(lat) * d};
    }

    std::map<std::uint8_t, std::string> models_;
    // The server's air (`weather`), and the geoid its heights are over.
    std::unique_ptr<glideslope::world::Geoid> geoid_for_air_;
    std::optional<glideslope::frontend::HeardAir> air_;
    double session_now_s_ = 0.0;
    std::unique_ptr<glideslope::sim::Aircraft> aircraft_;
    std::unique_ptr<glideslope::sim::Prediction> prediction_;
    long long stepped_ = 0;
    std::deque<std::pair<std::uint32_t, glideslope::sim::Controls>> before_;
    glideslope::net::SessionClock clock_;
    double rendered_s_ = -1.0;
    std::optional<glideslope::world::Geodetic> origin_;
    std::array<double, 3> origin_ecef_{};
    std::map<std::uint8_t, glideslope::net::Interpolated> others_;
    std::ostream* track_ = nullptr;
    std::size_t shown_ = 0;
    std::size_t extrapolated_ = 0;
    std::map<double, glideslope::net::Watched> watched_;
    std::size_t controls_shown_ = 0;
    // Handing over and taking back (`handed`), and what is shown of its own.
    bool ai_flying_ = false;
    bool resuming_ = false;
    std::uint32_t resumed_from_ = 0;
    double settled_at_s_ = 0.0;
    bool corrected_ = false;
    glideslope::net::Interpolated own_shown_;
    // What is shown of its own, blended across switches and measured.
    glideslope::frontend::OwnShown display_;
    bool long_frame_after_switch_ = false;
    bool building_long_frame_ = false;
    std::size_t long_frames_ = 0;
    std::size_t overtaken_ = 0;
    std::size_t handed_over_ = 0;
    std::size_t taken_over_ = 0;
    std::size_t taken_back_ = 0;
    double longest_between_frames_s_ = 0.0;
    std::optional<double> reconciled_s_;
    // Where each step of its own flying took it, and whether that counts
    // towards the prediction error (not while joining or resuming).
    struct Predicted {
        std::uint64_t step = 0; // sim::Prediction::steps() before it
        std::array<double, 3> where{};
        bool counted = false;
    };
    std::deque<Predicted> predicted_at_;
    std::size_t compared_ = 0;
    // Every error compared, for its median: a wrong wind is in every update,
    // where a slow frame is in a few (`report`).
    std::vector<double> errors_m_;
    std::size_t compared_since_ = 0;
    std::size_t snapped_since_ = 0;
    double worst_error_since_m_ = 0.0;
    std::vector<std::uint32_t> joining_;
    bool answered_ = false;
    double worst_error_m_ = 0.0;
    double worst_error_at_s_ = 0.0;
    std::size_t corrections_ = 0;
    std::size_t snapped_ = 0;
    double worst_correction_m_ = 0.0;
};

// **How long a session may go without anything opening under it before a
// refusal is believed.** The server knocks once a second, and a client that
// has heard nothing for a second knocks too (`stay()`), so three seconds of
// nothing is three of the server's knocks and two of this client's own gone
// unanswered: a session that is not working, whatever the refusal says.
constexpr double quiet_before_believing_s = 3.0;

// **How a stay ended**: it stayed its time, or the server let it go (and it
// may join again), or the operator dropped it (and it may not).
enum class Ended { stayed, let_go, dropped };

// **Whether a datagram is the server's refusal, and for what**: only from
// the server's own address (net/rejoin.hpp).
using glideslope::net::refusal_from;

// A sealed `PING` of this client's own, `token`, to the server.
void knock_on(glideslope::platform::UdpSocket& socket,
              const glideslope::platform::Address& server, glideslope::net::Sealer& sealer,
              std::uint64_t token) {
    const std::vector<std::uint8_t> ping =
        glideslope::net::knock(glideslope::net::Inside::ping, token);
    glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::sealed);
    w.bytes(sealer.seal(std::span<const std::uint8_t>(ping.data(), ping.size())));
    const std::vector<std::uint8_t> out = w.take();
    (void)socket.send(server, std::span<const std::uint8_t>(out.data(), out.size()));
}

// **A test flag's work (`--stall-once-rolled`)**: the client stops, as a
// process suspended or a laptop shut would - sending nothing and answering
// nothing - until the server has let it go. **That is waited for, not
// timed.** What arrives is read and thrown away, as a stopped process would
// never have read it, until nothing has come for three seconds: the server
// knocks once a second, so its knocks have stopped. Then it knocks itself,
// once a second: a `BAD_HANDSHAKE` from the server says the session is gone,
// and the stall is over; anything that opens under the session says it is
// not - a server slow enough to have missed three knocks - and it stalls
// again. Returns false if none of that came in five minutes.
bool stall_until_let_go(glideslope::platform::UdpSocket& socket,
                        const glideslope::platform::Address& server,
                        glideslope::net::Sealer& sealer, glideslope::net::Unsealer& unsealer) {
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    const auto since = [](std::chrono::steady_clock::time_point t) {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
    };
    constexpr double gone_after_s = 3.0;
    constexpr double most_waited_s = 300.0;
    const auto began = std::chrono::steady_clock::now();
    std::uint64_t token = 1u << 20;
    for (;;) {
        auto last_heard = std::chrono::steady_clock::now();
        while (since(last_heard) < gone_after_s) {
            if (since(began) > most_waited_s) {
                return false;
            }
            glideslope::platform::Address from;
            if (socket.receive(into, from) > 0) {
                last_heard = std::chrono::steady_clock::now();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        bool opened = false;
        double knocked_s = -1.0;
        const auto quiet_from = std::chrono::steady_clock::now();
        while (!opened) {
            if (since(began) > most_waited_s) {
                return false;
            }
            if (knocked_s < 0.0 || since(quiet_from) - knocked_s >= 1.0) {
                knock_on(socket, server, sealer, ++token);
                knocked_s = since(quiet_from);
            }
            glideslope::platform::Address from;
            const std::size_t got = socket.receive(into, from);
            if (got <= glideslope::net::envelope_size) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            const std::span<const std::uint8_t> datagram(into.data(), got);
            if (refusal_from(server, from, datagram) == glideslope::net::Refusal::bad_handshake) {
                return true;
            }
            if (datagram[glideslope::net::envelope_size - 1] ==
                    static_cast<std::uint8_t>(glideslope::net::Type::sealed) &&
                unsealer.open(datagram.subspan(glideslope::net::envelope_size))) {
                opened = true;
                std::printf("the session still answers; stalling again\n");
                std::fflush(stdout);
            }
        }
    }
}

// **What a session carries from one stay in it to the next**: the client's
// input frames, numbered from 1, and the reliable stream both ways. A client
// that goes back to its old session after a refusal it believed is in the same
// session as before, and the server's count of its inputs did not start again:
// inputs numbered afresh from 1 were all older than the newest it had applied,
// and every one was dropped until the count passed where it had been. A new
// session starts them all again.
struct SessionStreams {
    glideslope::net::Reliable reliable;
    glideslope::net::InputSender sending;
    std::uint32_t sequence = 0;
};

// Stays in a session: answers the server's knocks, flies if told to, and
// hears what the server says. **Says in `ended` how it ended**, returning
// early when the server has let this session go - a `BAD_HANDSHAKE` from the
// server's address heard after the session had gone quiet
// (`quiet_before_believing_s`) - so that the caller can join again; or when
// the server said goodbye (`LEAVING`), which is the operator dropping it.
int stay(glideslope::platform::UdpSocket& socket,
         const glideslope::platform::Address& server, glideslope::net::Sealer& sealer,
         glideslope::net::Unsealer& unsealer, double seconds,
         const std::string& until_exists,
         std::span<const std::uint8_t> initiation_again, bool fly, const std::string& me,
         const std::string& heard_file, int until_flying_again,
         bool predict, const std::string& track_file, double hand_over_at_s,
         double take_back_at_s, double dive_after_s, bool watch_ai,
         double take_over_at_s, int take_over_aircraft, bool take_over_once_ai,
         bool long_frame_after_switch, bool late_update_after_take_over, bool goodbye,
         bool stall_once_rolled, bool until_rolled, SessionStreams& streams, Ended& ended,
         bool until_applied = false) {
    ended = Ended::stayed;
    // A client that predicts flies a pilot of its own (Predicting::pilot).
    std::optional<Predicting> predicting;
    if (predict) {
        predicting.emplace();
        if (long_frame_after_switch) {
            predicting->long_frame_after_switch();
        }
        fly = true;
    }
    // **What must arrive**: the server's reliable messages, acknowledged,
    // and what each aircraft is, said as it is heard.
    glideslope::net::Reliable& reliable = streams.reliable;
    std::uint8_t mine = glideslope::net::no_aircraft;
    std::optional<double> newest_state_s;
    std::vector<std::uint8_t> before_take_over;
    std::optional<std::vector<std::uint8_t>> hear_again;
    bool asked_to_hand_over = false;
    bool asked_to_watch = false;
    bool asked_to_take_over = false;
    std::uint8_t ai_to_take = glideslope::net::no_aircraft;
    // Whether an update has shown the AI flying it (`--once-the-ai-flies-it`).
    bool ai_flies_it = false;
    bool asked_to_take_back = false;
    // **Stay until what is waited for is heard** (`--until-flying-again N`):
    // a test waiting for a collision and the flying again after it waits for
    // that, with SECONDS only the most it will wait. Thirty seconds of the
    // clock was less than a debug server on CI took to get there.
    int flown_again = 0;
    // **What was heard, into a file of its own** when asked (`--heard FILE`):
    // a test running several clients in one pipeline reads each one's file.
    // Standard error was meant to do it, and on Windows a pipeline's
    // standard error came back empty.
    std::ofstream heard_out;
    if (!heard_file.empty()) {
        heard_out.open(heard_file, std::ios::app);
    }
    // **Where every aircraft was, as heard, and where each was shown**
    // (`--track FILE`): a line for every aircraft in every update, and, from
    // a client that predicts, a line for every other aircraft on every frame
    // it drew. The network checks judge one client's frames against another's
    // updates - one that heard everything, straight from the server.
    std::ofstream track_out;
    if (!track_file.empty()) {
        track_out.open(track_file, std::ios::trunc);
        track_out.precision(4);
        track_out.setf(std::ios::fixed);
        if (predicting) {
            predicting->track_to(track_out);
        }
    }
    const auto say_heard = [&](const std::string& line) {
        std::fprintf(stderr, "client %s: %s\n", me.c_str(), line.c_str());
        if (heard_out) {
            heard_out << "client " << me << ": " << line << '\n';
            heard_out.flush();
        }
    };
    // **Every aircraft's condition, as last heard**, so that a change -
    // a wreck, or a wreck flying again - is said once, when it is heard.
    std::map<std::uint8_t, glideslope::net::Condition> heard_as;
    // **What each aircraft is**, by its number, as the server's `AIRCRAFT`
    // messages said: the catalogue's id, for a hand-over's copilot.
    std::map<std::uint8_t, std::string> ids_heard;
    // **What the server says the session is** (REQUIREMENTS.md 6.3): the
    // ground it collides on - another than this client's is refused, and it
    // leaves - the session, the lobby and the weather it flies.
    glideslope::net::Told told;
    bool refused_ground = false;
    // **A test flag's work**: send the initiation once more, now that the
    // session is up. A network that duplicates a datagram does this by
    // itself, and a server that answered it with a fresh session would leave
    // this client sealing under keys the server had thrown away - so if the
    // pings below stop being answered, that is what happened.
    if (!initiation_again.empty()) {
        (void)socket.send(server, initiation_again);
    }
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    const auto began = std::chrono::steady_clock::now();
    int answered = 0;
    int heard = 0;
    // The simulation's step in the first and the last update heard: a test
    // counts the twenty-fifths of a second between them and expects an update
    // for each.
    long long first_step = -1;
    long long last_step = -1;
    std::size_t aircraft_last = 0;
    // **What this client flies, if it was told to.** Full left aileron and a
    // little up elevator: a thing no AI pilot on a flight plan would ever do,
    // so an aircraft that rolls over is one being flown from here and could
    // not be anything else.
    glideslope::sim::Controls stick;
    stick.throttle = 1.0;
    stick.aileron = -1.0;
    stick.elevator = 0.2;
    glideslope::net::InputSender& sending = streams.sending;
    std::uint32_t& sequence = streams.sequence;
    // The input count when this stay began: `until_applied` waits for the
    // server to apply one sent since - flown from here, now, and not by what
    // it was sent before.
    const std::uint32_t sequence_at_start = sequence;
    double sent_inputs_at_s = -1.0;
    std::uint32_t applied = 0;
    double roll_seen_deg = 0.0;
    double last_heard_s = 0.0;
    // When something last opened under this session, and when this client
    // last knocked for want of it.
    double last_opened_s = 0.0;
    double knocked_at_s = -1.0;
    std::uint64_t knocks = 0;
    double looked_for_file_at_s = -1.0;
    bool drained = true;
    for (;;) {
        double up_s =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
                .count();
        if (until_flying_again > 0 && flown_again >= until_flying_again) {
            break;
        }
        // **On other ground, it leaves**, saying goodbye below.
        if (refused_ground) {
            break;
        }
        // **Stay until the copilot's route has been flown a while**
        // (`--copilot`, `--send-route`), on the session's clock.
        if (connect_copilot.done) {
            break;
        }
        // **Stay until its aircraft has rolled past 90 degrees**
        // (`--stall-once-rolled`, in the session it joins again): what a test
        // waits for to know it flew again, SECONDS only the most.
        if (until_rolled && std::abs(roll_seen_deg) >= 90.0) {
            break;
        }
        // **Stay until the server has applied an input sent in this stay**
        // (`--leave-once-back`, in the session it is back in): flown from
        // here again, which is what a test waits for, SECONDS only the most.
        // **And has answered one of the server's knocks there**, which is
        // what a stay counts as having been in a session: back in the old
        // one, the updates held from before are its answer's to drop now
        // (net::Rejoin), and the first update after it could already carry
        // an input applied.
        if (until_applied && applied > sequence_at_start && answered > 0) {
            break;
        }
        if (stall_once_rolled && std::abs(roll_seen_deg) >= 90.0) {
            stall_once_rolled = false;
            std::printf("stalled once rolled to %.0f degrees\n", roll_seen_deg);
            std::fflush(stdout);
            if (!stall_until_let_go(socket, server, sealer, unsealer)) {
                std::fprintf(stderr, "glideslope_cli: the server never let this client go\n");
                return 1;
            }
            std::printf("let go by the server while stalled; going on\n");
            std::fflush(stdout);
            ended = Ended::let_go;
            return 0;
        }
        // **A session gone quiet is knocked on from this end** - a `PING` of
        // its own, once a second - so that a server that has it answers,
        // and one that has let it go refuses, even from a client that sends
        // nothing else.
        if (up_s - last_opened_s >= 1.0 && up_s - knocked_at_s >= 1.0) {
            knocked_at_s = up_s;
            knock_on(socket, server, sealer, ++knocks);
        }
        // **Stay until a file appears** (`--until-exists FILE`): a test that
        // needs the server kept running while something else happens waits
        // for that thing to say it is done, with SECONDS only the most.
        if (!until_exists.empty() && up_s - looked_for_file_at_s >= 0.1) {
            looked_for_file_at_s = up_s;
            if (std::filesystem::exists(until_exists)) {
                break;
            }
        }
        // **When the time is up, a client that flew waits for the server to
        // have applied the last input it sent** - resending it, in case it
        // was lost - so that what it says about its inputs is what the server
        // did with all of them, not how far behind a slow machine had fallen
        // at the moment it stopped. A minute is the most it waits, and a
        // server that has said nothing for five seconds - gone, or it has
        // dropped this client - is not waited for.
        constexpr double wait_for_the_last_input_s = 60.0;
        constexpr double server_gone_quiet_s = 5.0;
        const bool finishing = up_s >= seconds;
        if (finishing &&
            (!fly || applied >= sequence || up_s >= seconds + wait_for_the_last_input_s ||
             up_s - last_heard_s >= server_gone_quiet_s)) {
            break;
        }
        if (fly && up_s - sent_inputs_at_s >= inputs_every_s) {
            sent_inputs_at_s = up_s;
            if (!finishing) {
                ++sequence;
                // **Into the sea** (`--dive-after`): full forward stick and
                // full power, for a test that needs its own aircraft wrecked.
                if (dive_after_s >= 0.0 && up_s >= dive_after_s) {
                    stick.elevator = -1.0;
                    stick.aileron = 0.0;
                    stick.throttle = 1.0;
                }
                if (predicting) {
                    // What it sends is what it flies: rounded as the wire
                    // rounds it, as TRANSPORT.md asks, so that the two agree.
                    stick = glideslope::sim::Controls::from_list(
                        glideslope::net::as_sent(Predicting::pilot(sequence).as_list()));
                }
                sending.add(sequence, glideslope::net::as_sent(stick.as_list()));
            }
            std::vector<std::uint8_t> body{
                static_cast<std::uint8_t>(glideslope::net::Inside::inputs)};
            const std::vector<std::uint8_t> packet = sending.packet();
            body.insert(body.end(), packet.begin(), packet.end());
            glideslope::net::Writer iw =
                glideslope::net::begin(glideslope::net::Type::sealed);
            iw.bytes(sealer.seal(
                std::span<const std::uint8_t>(body.data(), body.size())));
            const std::vector<std::uint8_t> out = iw.take();
            (void)socket.send(server,
                              std::span<const std::uint8_t>(out.data(), out.size()));
        }

        // **Asking for its own aircraft to be handed to the AI, and back**
        // (`--hand-over-at`, `--take-back-at`), once each, when the time comes
        // and it knows which aircraft is its own.
        const auto ask = [&](glideslope::net::Controller to) {
            glideslope::net::ControllerSwap swap;
            swap.aircraft = mine;
            swap.to = to;
            const std::vector<std::uint8_t> body = glideslope::net::write(swap);
            (void)reliable.send(std::span<const std::uint8_t>(body.data(), body.size()));
        };
        // **The copilot that plans it handed over** (`--hand-over-model`),
        // made once the server has said which aircraft is its own and what
        // it is - or refused, saying why, and the hand-over made as with
        // none. The hand-over waits for that.
        ConnectCopilot& hc = connect_copilot;
        const bool hand_over_model = hc.hand_over && !hc.hand_over->provider.empty();
        if (hand_over_model && !hc.hand_over_tried && mine != glideslope::net::no_aircraft) {
            if (const auto id = ids_heard.find(mine); id != ids_heard.end()) {
                hc.hand_over_tried = true;
                glideslope::frontend::PlayersCopilotOptions o =
                    hc.options ? *hc.options : glideslope::frontend::PlayersCopilotOptions{};
                o.aircraft = id->second;
                o.provider = hc.hand_over->provider;
                o.model = hc.hand_over->model;
                // **The hand-over's words**, the data's - the installed data
                // beside the program, as the catalogue is read here, not
                // `--data`. Not had, that is said for what it is, not as a
                // key refused.
                try {
                    o.task = glideslope::frontend::hand_over_task(
                        glideslope::platform::data_directory());
                } catch (const std::exception& e) {
                    hc.hand_over_refused = "its words cannot be read";
                    say_heard(std::string("the hand-over's words cannot be read: ") + e.what());
                }
                if (hc.hand_over_refused.empty()) {
                    try {
                        hc.seat = std::make_unique<glideslope::frontend::PlayersCopilot>(
                            glideslope::platform::data_directory(), o);
                        hc.asked = true; // asked as it is handed over, not before
                    } catch (const std::exception& e) {
                        hc.hand_over_refused = o.provider + " was refused";
                        say_heard("the hand-over's model, " + o.provider + ", is refused: " +
                                  e.what());
                    }
                }
            }
        }
        if (mine != glideslope::net::no_aircraft) {
            if (hand_over_at_s >= 0.0 && up_s >= hand_over_at_s && !asked_to_hand_over &&
                (!hand_over_model || hc.hand_over_tried)) {
                asked_to_hand_over = true;
                ask(glideslope::net::Controller::ai);
                if (hc.seat && hand_over_model) {
                    hc.seat->handed_over();
                    say_heard("handed over, planned by " + hc.seat->provider());
                } else if (hc.hand_over) {
                    say_heard("handed over, planned by no model" +
                              (hc.hand_over_refused.empty()
                                   ? std::string()
                                   : " (" + hc.hand_over_refused + ")") +
                              ": the AI holds its course");
                }
            }
            if (take_back_at_s >= 0.0 && up_s >= take_back_at_s && !asked_to_take_back) {
                asked_to_take_back = true;
                ask(glideslope::net::Controller::person);
                // Its copilot stands by now, not when an update first shows
                // the player with it.
                if (hc.seat) {
                    hc.seat->taken_back();
                    for (const std::string& line : hc.seat->said()) {
                        say_heard(line);
                    }
                }
                hc.taken_back = true;
            }
            if (take_over_at_s >= 0.0 && up_s >= take_over_at_s && !asked_to_take_over &&
                ai_to_take != glideslope::net::no_aircraft &&
                (!take_over_once_ai || ai_flies_it)) {
                asked_to_take_over = true;
                glideslope::net::ControllerSwap swap;
                swap.aircraft = ai_to_take;
                swap.to = glideslope::net::Controller::person;
                const std::vector<std::uint8_t> body = glideslope::net::write(swap);
                (void)reliable.send(std::span<const std::uint8_t>(body.data(), body.size()));
                say_heard("asked to take over aircraft " + std::to_string(ai_to_take));
            }
        }
        // Acknowledgements of what must arrive, when any are owed.
        for (const std::vector<std::uint8_t>& datagram : reliable.to_send(up_s)) {
            std::vector<std::uint8_t> body{
                static_cast<std::uint8_t>(glideslope::net::Inside::reliable)};
            body.insert(body.end(), datagram.begin(), datagram.end());
            glideslope::net::Writer rw = glideslope::net::begin(glideslope::net::Type::sealed);
            rw.bytes(sealer.seal(std::span<const std::uint8_t>(body.data(), body.size())));
            const std::vector<std::uint8_t> out = rw.take();
            (void)socket.send(server, std::span<const std::uint8_t>(out.data(), out.size()));
        }

        // **Everything waiting is read before a frame is drawn**, as a real
        // client reads its socket dry each frame: reading one datagram a
        // pass, a slow client drew from updates that had arrived and sat
        // unread.
        // Its time up, a client waiting for its last input to be applied
        // makes no new ones, and a prediction keyed by input could only
        // replay nothing and fall a round trip behind: it stops predicting,
        // and measuring, there.
        if (predicting && drained && !finishing) {
            predicting->advance(up_s, sequence, stick);
            predicting->render(up_s);
        }
        std::optional<std::vector<std::uint8_t>> opened;
        if (hear_again) {
            // A test's late update (`--late-update-after-take-over`), heard
            // now as if the network had held it back.
            opened = std::move(hear_again);
            hear_again.reset();
        } else {
            glideslope::platform::Address from;
            const std::size_t got = socket.receive(into, from);
            drained = got == 0;
            if (got <= glideslope::net::envelope_size) {
                if (drained) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                continue;
            }
            last_heard_s = up_s;
            glideslope::net::Reader r(std::span<const std::uint8_t>(into.data(), got));
            glideslope::net::Envelope envelope;
            glideslope::net::Refusal why{};
            if (!glideslope::net::read_envelope(r, envelope, why)) {
                continue;
            }
            // **A refusal is believed only of a session gone quiet**, only
            // from the server's address, and only for `BAD_HANDSHAKE` - "no
            // session here". It is sent in the clear, so anybody can forge
            // one; one heard while the session works is ignored. One heard
            // after nothing has opened for `quiet_before_believing_s` - the
            // server's knocks and this client's own unanswered - says what
            // the silence already did. Not while finishing: a client leaving
            // has no use for another session.
            if (!finishing && up_s - last_opened_s >= quiet_before_believing_s &&
                refusal_from(server, from, std::span<const std::uint8_t>(into.data(), got)) ==
                    glideslope::net::Refusal::bad_handshake) {
                std::printf("let go by the server: refused BAD_HANDSHAKE after %.1f s of "
                            "nothing\n",
                            up_s - last_opened_s);
                std::fflush(stdout);
                ended = Ended::let_go;
                return 0;
            }
            if (envelope.type != glideslope::net::Type::sealed) {
                continue;
            }
            opened = unsealer.open(std::span<const std::uint8_t>(into.data(), got)
                                       .subspan(glideslope::net::envelope_size));
            if (!opened) {
                continue;
            }
            last_opened_s = up_s;
        }
        const std::span<const std::uint8_t> inside(opened->data(), opened->size());
        // **Where everybody is.** Nothing is done with it here beyond
        // counting and printing it: this is the command-line tool, and it has
        // no sky to draw them in.
        if (const auto state = glideslope::net::read_state(inside)) {
            ++heard;
            // **An update the network held back past a newer one** says
            // what was so then: an aircraft since given up is not taken over
            // again by it, nor is its motion taken for this one's own. One
            // from before its own aircraft's number changed is dropped; any
            // other still tells where the others were (2026-09-26: through
            // 200 ms of jitter, "took over aircraft 4, then 1, then 4").
            const bool newest_state = !newest_state_s || state->simulation_time_s > *newest_state_s;
            if (!newest_state && state->your_aircraft != mine) {
                continue;
            }
            if (newest_state) {
                newest_state_s = state->simulation_time_s;
            }
            // **For a test: an update from before each take-over, heard
            // again just after it**, as the network reorders them - built,
            // not waited for. The last update to name the aircraft given up
            // is kept for it.
            if (late_update_after_take_over && newest_state) {
                if (state->your_aircraft == mine) {
                    before_take_over.assign(inside.begin(), inside.end());
                } else if (mine != glideslope::net::no_aircraft &&
                           state->your_aircraft != glideslope::net::no_aircraft &&
                           !before_take_over.empty()) {
                    hear_again = before_take_over;
                    say_heard("an update from before the take-over is heard again after it");
                }
            }
            // **Its own aircraft's number changed**: another taken over -
            // noticed before the update is used, which would otherwise put the
            // aircraft left behind right by the one taken over's motion.
            if (newest_state && mine != glideslope::net::no_aircraft &&
                state->your_aircraft != mine &&
                state->your_aircraft != glideslope::net::no_aircraft) {
                say_heard("took over aircraft " + std::to_string(state->your_aircraft));
                if (predicting) {
                    predicting->taken_over(state->your_aircraft, sequence);
                }
            }
            if (predicting && !finishing) {
                predicting->heard(*state, up_s);
            }
            if (track_out) {
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    track_out << "heard " << state->simulation_time_s << ' '
                              << static_cast<unsigned>(a.index) << ' ' << a.x_m << ' '
                              << a.y_m << ' ' << a.z_m << '\n';
                }
                // The watched aircraft's controls, as this update gave them.
                if (state->watched) {
                    const glideslope::net::Watched& w = *state->watched;
                    track_out << "watched " << state->simulation_time_s << ' '
                              << static_cast<unsigned>(w.aircraft) << ' ' << w.aileron << ' '
                              << w.elevator << ' ' << w.rudder << ' ' << w.throttle << ' '
                              << w.flaps << '\n';
                }
            }
            // **Riding along** (`--watch-ai`): once it knows the aircraft,
            // it asks to watch the lowest-numbered one an AI flies.
            if (watch_ai && !asked_to_watch && state->your_aircraft != glideslope::net::no_aircraft) {
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    if (a.index != state->your_aircraft &&
                        a.controller == glideslope::net::Controller::ai) {
                        glideslope::net::Watch watch;
                        watch.aircraft = a.index;
                        const std::vector<std::uint8_t> body = glideslope::net::write(watch);
                        (void)reliable.send(std::span<const std::uint8_t>(body.data(), body.size()));
                        asked_to_watch = true;
                        say_heard("watching aircraft " + std::to_string(a.index));
                        break;
                    }
                }
            }
            if (newest_state) {
                applied = state->last_input_applied;
                mine = state->your_aircraft;
            }
            // **Its copilot, asked here and answered as a route sent** - or a
            // route read from a file, sent as it is.
            ConnectCopilot& cc = connect_copilot;
            if (newest_state && mine != glideslope::net::no_aircraft &&
                (cc.seat || !cc.route_file.empty())) {
                const double now_s = state->simulation_time_s;
                const auto send_route = [&](glideslope::net::CopilotRoute route) {
                    route.aircraft = mine;
                    const std::vector<std::uint8_t> body = glideslope::net::write(route);
                    (void)reliable.send(std::span<const std::uint8_t>(body.data(), body.size()));
                    cc.sent_at_s = now_s;
                    say_heard("sent its copilot's route of " + std::to_string(route.waypoints.size()) +
                              " at " + std::to_string(std::llround(now_s)) + " s");
                };
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    if (a.index != mine || now_s < cc.at_s) {
                        continue;
                    }
                    if (!cc.route_file.empty() && !cc.route_sent &&
                        (!cc.route_when_wrecked ||
                         a.condition == glideslope::net::Condition::wrecked)) {
                        glideslope::net::CopilotRoute route = route_from_file(cc.route_file);
                        std::uint8_t to = mine;
                        if (cc.route_for_another) {
                            // Another player's: an AI's is refused anyway,
                            // as no player's.
                            for (const glideslope::net::AircraftState& other : state->aircraft) {
                                if (other.index != mine &&
                                    other.controller == glideslope::net::Controller::person) {
                                    to = other.index;
                                    break;
                                }
                            }
                        }
                        if (to != mine || !cc.route_for_another) {
                            cc.route_sent = true;
                            route.aircraft = to;
                            const std::vector<std::uint8_t> body = glideslope::net::write(route);
                            (void)reliable.send(
                                std::span<const std::uint8_t>(body.data(), body.size()));
                            cc.sent_at_s = now_s;
                            say_heard("sent its copilot's route of " +
                                      std::to_string(route.waypoints.size()) + " for aircraft " +
                                      std::to_string(to) + " at " +
                                      std::to_string(std::llround(now_s)) + " s");
                        }
                    }
                    if (cc.seat) {
                        if (!cc.asked) {
                            cc.asked = true;
                            cc.seat->ask();
                        }
                        const auto route = cc.seat->look(now_s, a);
                        for (const std::string& line : cc.seat->said()) {
                            say_heard(line);
                        }
                        if (route) {
                            send_route(*route);
                        }
                    }
                }
                const bool all_answered = !cc.seat || cc.seat->answers() >= cc.answers;
                if (all_answered && cc.sent_at_s && now_s - *cc.sent_at_s >= cc.stay_s) {
                    cc.done = true;
                }
            }
            // **Taken back**: from when an update first shows its player with
            // it, `--copilot-stay` seconds of the session's clock, then it
            // leaves - what a test of a take-back waits for.
            if (newest_state && cc.hand_over && cc.taken_back && mine != glideslope::net::no_aircraft) {
                const double now_s = state->simulation_time_s;
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    if (a.index != mine || a.controller != glideslope::net::Controller::person) {
                        continue;
                    }
                    if (!cc.back_since_s) {
                        cc.back_since_s = now_s;
                    } else if (now_s - *cc.back_since_s >= cc.stay_s) {
                        say_heard("flown by its pilot again for " +
                                  std::to_string(std::llround(now_s - *cc.back_since_s)) + " s");
                        cc.done = true;
                    }
                }
            }
            // **Handed over with no model to plan it** - none chosen, or one
            // refused: the AI holds what the aircraft was doing. How it was
            // flying when the update first showed the AI with it, and how it
            // is `--copilot-stay` seconds of the session's clock later, said;
            // then it leaves.
            if (newest_state && cc.hand_over && !cc.seat && !cc.taken_back && asked_to_hand_over &&
                mine != glideslope::net::no_aircraft) {
                const double now_s = state->simulation_time_s;
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    if (a.index != mine || a.controller != glideslope::net::Controller::ai) {
                        continue;
                    }
                    const double height_ft =
                        glideslope::world::to_geodetic({a.x_m, a.y_m, a.z_m}).height_m *
                        3.280839895013123;
                    const double heading_deg = static_cast<double>(a.heading_deg);
                    if (!cc.held_since_s) {
                        cc.held_since_s = now_s;
                        cc.held_heading_deg = heading_deg;
                        cc.held_height_ft = height_ft;
                    } else if (now_s - *cc.held_since_s >= cc.stay_s) {
                        char line[200];
                        std::snprintf(line, sizeof line,
                                      "held by the AI for %.0f s: heading %.1f to %.1f, "
                                      "height %.0f to %.0f ft",
                                      now_s - *cc.held_since_s, cc.held_heading_deg, heading_deg,
                                      cc.held_height_ft, height_ft);
                        say_heard(line);
                        cc.done = true;
                    }
                }
            }
            // The AI aircraft it would take over: the one it watches, or else
            // the first the AI flies - or the one it is told, whoever's.
            if (take_over_aircraft >= 0) {
                ai_to_take = static_cast<std::uint8_t>(take_over_aircraft);
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    if (a.index == ai_to_take) {
                        ai_flies_it = a.controller == glideslope::net::Controller::ai;
                    }
                }
            } else if (ai_to_take == glideslope::net::no_aircraft) {
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    if (a.index != mine && a.controller == glideslope::net::Controller::ai) {
                        ai_to_take = a.index;
                        break;
                    }
                }
            }
            last_step = std::llround(state->simulation_time_s *
                                     static_cast<double>(glideslope::sim::steps_per_second));
            if (first_step < 0) {
                first_step = last_step;
            }
            // **Its own aircraft**, which the server names in every update
            // because a client cannot reconcile without knowing which line is
            // its own.
            // **Told of a collision**: said on standard error, with this
            // client's name, so that a test running several clients at once
            // can read what each one heard (standard output goes on down a
            // pipe to the next program).
            for (const glideslope::net::AircraftState& a : state->aircraft) {
                const auto was = heard_as.find(a.index);
                if (was == heard_as.end() || was->second != a.condition) {
                    // A wreck flown again first, whatever it flies again
                    // with - an engine stopped among it.
                    if (a.condition == glideslope::net::Condition::wrecked) {
                        say_heard("aircraft " + std::to_string(a.index) + " is a wreck");
                    } else if (was != heard_as.end() &&
                               was->second == glideslope::net::Condition::wrecked) {
                        say_heard("aircraft " + std::to_string(a.index) + " flies again");
                        ++flown_again;
                    }
                    if (a.condition == glideslope::net::Condition::engine_stopped) {
                        say_heard("aircraft " + std::to_string(a.index) + "'s engine has stopped");
                    }
                    heard_as[a.index] = a.condition;
                }
            }
            for (const glideslope::net::AircraftState& a : state->aircraft) {
                if (a.index == state->your_aircraft) {
                    if (std::abs(static_cast<double>(a.roll_deg)) >
                        std::abs(roll_seen_deg)) {
                        roll_seen_deg = static_cast<double>(a.roll_deg);
                    }
                }
            }
            if (heard == 1 || state->aircraft.size() != aircraft_last) {
                aircraft_last = state->aircraft.size();
                std::printf("state: %zu aircraft at %.3f s, mine is %d\n",
                            state->aircraft.size(), state->simulation_time_s,
                            state->your_aircraft == glideslope::net::no_aircraft
                                ? -1
                                : static_cast<int>(state->your_aircraft));
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    const glideslope::world::Geodetic g =
                        glideslope::world::to_geodetic({a.x_m, a.y_m, a.z_m});
                    std::printf("  %u at %.5f, %.5f  %.0f m  heading %.0f\n",
                                static_cast<unsigned>(a.index), g.latitude_deg,
                                g.longitude_deg, g.height_m,
                                static_cast<double>(a.heading_deg));
                }
                std::fflush(stdout);
            }
            continue;
        }
        if (!inside.empty() &&
            inside[0] == static_cast<std::uint8_t>(glideslope::net::Inside::reliable)) {
            for (const std::vector<std::uint8_t>& message : reliable.received(inside.subspan(1))) {
                switch (told.hear(std::span<const std::uint8_t>(message.data(), message.size()))) {
                case glideslope::net::Told::Heard::dataset: {
                    const glideslope::net::TerrainDataset ours =
                        glideslope::frontend::collision_dataset(connect_air.data);
                    const std::string theirs = glideslope::frontend::describe(*told.dataset());
                    if (glideslope::frontend::same_ground(*told.dataset(), ours)) {
                        say_heard("told the collision ground: " + theirs + ", this client's too");
                    } else {
                        // **Refused, not fetched**: the ground is the data
                        // this build carries and the tiles it names, and a
                        // client on other ground is another build or other
                        // data (REQUIREMENTS.md 6.3).
                        say_heard("refused the server's collision ground: it collides on " +
                                  theirs + ", and this client's is " +
                                  glideslope::frontend::describe(ours));
                        refused_ground = true;
                    }
                    break;
                }
                case glideslope::net::Told::Heard::session:
                    say_heard("told the session: " + told.session()->name + ", " +
                              std::to_string(told.session()->id) + ", begun at " +
                              std::to_string(told.session()->began_unix_ms) + " ms");
                    break;
                case glideslope::net::Told::Heard::lobby: {
                    std::string line = "told the lobby: " +
                                       std::to_string(told.lobby()->players_allowed) +
                                       " players allowed";
                    for (const glideslope::net::Lobby::Slot& sl : told.lobby()->slots) {
                        line += ", slot " + std::to_string(sl.index) +
                                (sl.controller == glideslope::net::Controller::nobody
                                     ? " open"
                                     : sl.controller == glideslope::net::Controller::person
                                           ? " a person's"
                                           : " the AI's");
                    }
                    say_heard(line);
                    break;
                }
                case glideslope::net::Told::Heard::weather: {
                    const glideslope::net::Weather& w = *told.weather();
                    char when[96];
                    std::snprintf(when, sizeof(when), " (weather %d, from %.3f s over %.0f s%s)",
                                  told.weathers(), w.changed_at_s, w.blend_s,
                                  told.aloft() ? ", with the forecast above it" : "");
                    say_heard("told the weather: " + (w.metar.empty() ? "still air" : w.metar) +
                              when);
                    std::string why;
                    if (predicting && !connect_air.own_air &&
                        !predicting->weather(w, told.aloft(), why)) {
                        say_heard("refused the server's weather, and flies the last: " + why);
                    }
                    break;
                }
                case glideslope::net::Told::Heard::nothing:
                    break;
                }
                glideslope::net::AircraftDefinition d;
                if (glideslope::net::read(
                        std::span<const std::uint8_t>(message.data(), message.size()), d)) {
                    say_heard("aircraft " + std::to_string(d.aircraft) + " is " + d.id);
                    ids_heard[d.aircraft] = d.id;
                    // **The model is the catalogue's for that id**, never the
                    // wire's: a directory a server named would be joined to a
                    // path as it stood. An id not in the catalogue flies
                    // nothing here.
                    if (predicting) {
                        if (const auto entry = glideslope::sim::known_aircraft(
                                glideslope::platform::data_directory(), d.id)) {
                            predicting->introduced(d.aircraft, entry->model);
                        }
                    }
                }
                glideslope::net::ControllerSwap swap;
                if (glideslope::net::read(
                        std::span<const std::uint8_t>(message.data(), message.size()), swap)) {
                    const bool to_ai = swap.to == glideslope::net::Controller::ai;
                    say_heard("aircraft " + std::to_string(swap.aircraft) +
                              (to_ai ? " handed to the AI" : " handed to its pilot"));
                    if (predicting && swap.aircraft == mine) {
                        predicting->handed(to_ai, sequence);
                    }
                }
            }
            continue;
        }
        // **The server's goodbye** (`LEAVING`, sealed under this session,
        // so the server's own): the operator dropped this client. It does
        // not come back.
        if (glideslope::net::is_leaving(inside)) {
            say_heard("the server ended this session (dropped, or taken over by a newer "
                      "session for this key); not joining again");
            std::printf("the server ended this session (dropped, or taken over by a newer "
                        "session for this key)\n");
            std::fflush(stdout);
            ended = Ended::dropped;
            return 1;
        }
        const auto token =
            glideslope::net::knock_token(glideslope::net::Inside::ping, inside);
        if (!token) {
            continue;
        }
        const std::vector<std::uint8_t> pong =
            glideslope::net::knock(glideslope::net::Inside::pong, *token);
        glideslope::net::Writer w =
            glideslope::net::begin(glideslope::net::Type::sealed);
        w.bytes(sealer.seal(std::span<const std::uint8_t>(pong.data(), pong.size())));
        const std::vector<std::uint8_t> out = w.take();
        (void)socket.send(server, std::span<const std::uint8_t>(out.data(), out.size()));
        ++answered;
    }
    // **A client that is leaving says so** (`LEAVING`), so that the server
    // lets it go now rather than after its `--timeout` of silence. Not
    // reliable - this client will not wait to hear it acknowledged - so it
    // goes `leaving_copies` times, each sealed afresh: the first to arrive
    // lets the session go, and the server's timeout is there if none does.
    if (goodbye) {
        const std::vector<std::uint8_t> leaving{
            static_cast<std::uint8_t>(glideslope::net::Inside::leaving)};
        for (int copy = 0; copy < glideslope::net::leaving_copies; ++copy) {
            glideslope::net::Writer lw = glideslope::net::begin(glideslope::net::Type::sealed);
            lw.bytes(sealer.seal(std::span<const std::uint8_t>(leaving.data(), leaving.size())));
            const std::vector<std::uint8_t> out = lw.take();
            (void)socket.send(server, std::span<const std::uint8_t>(out.data(), out.size()));
        }
        std::printf("said goodbye\n");
    }
    std::printf("stayed %.1f s, answered %d ping%s and heard %d state update%s\n",
                seconds, answered, answered == 1 ? "" : "s", heard,
                heard == 1 ? "" : "s");
    if (heard > 0) {
        std::printf("state updates from step %lld to step %lld of the simulation\n",
                    first_step, last_step);
        // And to `--heard`: which steps a client was told of is how a test
        // counts, in simulated steps, the gap between two of them.
        say_heard("state updates from step " + std::to_string(first_step) + " to step " +
                  std::to_string(last_step));
    }
    if (fly) {
        std::printf("sent %u input frames, the server applied %u\n", sequence,
                    applied);
        say_heard("sent " + std::to_string(sequence) + " input frames, the server applied " +
                  std::to_string(applied));
        std::printf("my aircraft rolled to %.0f degrees\n", roll_seen_deg);
    }
    if (predicting) {
        for (const std::string& line : predicting->report()) {
            say_heard(line);
        }
    }
    if (refused_ground) {
        std::printf("left: the server collides on other ground than this client\n");
        return 1;
    }
    return answered > 0 ? 0 : 1;
}

// **A test flag's work (`--again-when-let-go`)**: a client whose session the
// server has let go, and a copy of its initiation arriving after that - which
// is what a server slow to read its socket saw on CI, a copy sent before the
// answer came and read after the session it made had gone.
//
// It says nothing after the handshake, answering none of the server's knocks,
// so that the server lets it go after its `--timeout`. **That is waited for,
// not timed**: the server knocks once a second on every session it has, so
// three seconds without a datagram from it mean the session is gone. Then the
// same initiation goes once more, and for `seconds` it listens: an answer
// the same as the first says the session was still there and the situation
// was not built; a different one is a fresh session made from a copy; nothing
// is what a server that remembers what it has taken says. The verdict goes to
// `--heard FILE`.
int again_once_let_go(glideslope::platform::UdpSocket& socket,
                      const glideslope::platform::Address& server,
                      std::span<const std::uint8_t> initiation,
                      std::span<const std::uint8_t> first_answer, double seconds,
                      const std::string& heard_file) {
    const std::vector<std::uint8_t> answered(first_answer.begin(), first_answer.end());
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    const auto since = [](std::chrono::steady_clock::time_point t) {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
    };
    constexpr double gone_after_s = 3.0;
    constexpr double most_waited_s = 300.0;
    const auto began = std::chrono::steady_clock::now();
    auto last_heard = began;
    while (since(last_heard) < gone_after_s) {
        if (since(began) > most_waited_s) {
            std::fprintf(stderr, "glideslope_cli: the server never let this client go\n");
            return 1;
        }
        glideslope::platform::Address from;
        if (socket.receive(into, from) > 0) {
            last_heard = std::chrono::steady_clock::now();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    // **Five copies, a tenth of a second apart**, as a client whose session
    // has gone goes on sending: the server must drop every one and say so
    // once, where saying each filled its dashboard's log.
    for (int copy = 0; copy < 5; ++copy) {
        if (copy > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        (void)socket.send(server, initiation);
    }
    std::string verdict = "the initiation again was not answered";
    const auto sent = std::chrono::steady_clock::now();
    while (since(sent) < seconds) {
        glideslope::platform::Address from;
        const std::size_t got = socket.receive(into, from);
        if (got <= glideslope::net::envelope_size) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        glideslope::net::Reader r(std::span<const std::uint8_t>(into.data(), got));
        glideslope::net::Envelope envelope;
        glideslope::net::Refusal why{};
        if (!glideslope::net::read_envelope(r, envelope, why) ||
            envelope.type != glideslope::net::Type::handshake_response) {
            continue;
        }
        const bool same = got == answered.size() &&
                          std::equal(answered.begin(), answered.end(), into.begin());
        verdict = same ? "the initiation again was answered as before"
                       : "the initiation again was answered afresh";
        break;
    }
    std::printf("%s\n", verdict.c_str());
    if (!heard_file.empty()) {
        std::ofstream(heard_file, std::ios::app) << verdict << '\n';
    }
    return 0;
}

// **Goodbyes a forger could send (`--forge-leaving`), none of which is the
// session's own client saying it, from its own address, under its own keys.**
// Three of them:
//
// - this session's goodbye, sealed under its keys, sent from `other`'s
//   address - a session that is live, whose keys it does not open under;
// - `other`'s goodbye, sealed under its keys, sent from this client's address;
// - this session's goodbye again, from a third address that has no session
//   at all, which the server refuses (`BAD_HANDSHAKE`). That refusal is
//   waited for, and it is the last sent, so all three are known to have
//   reached the server before this client stays.
//
// A replay of a goodbye into a later session from the same address is the
// second of these: a later session has keys of its own. A copy within the
// same session is the sealing's replay window's to refuse, and is tested
// there.
//
// None may let anybody go. The server says, as it lets each go, how long after
// its admission that was, and a test holds that to the whole stay.
bool forge_goodbyes(glideslope::platform::UdpSocket& socket,
                    const glideslope::platform::Address& server,
                    glideslope::net::Sealer& sealer, glideslope::net::ClientSession& other,
                    const std::string& heard_file) {
    // Said to standard output, and to `--heard FILE` for a test in a pipeline.
    const auto say = [&](const std::string& line) {
        std::printf("%s\n", line.c_str());
        if (!heard_file.empty()) {
            std::ofstream(heard_file, std::ios::app) << line << '\n';
        }
    };
    const std::vector<std::uint8_t> leaving{
        static_cast<std::uint8_t>(glideslope::net::Inside::leaving)};
    const std::span<const std::uint8_t> plain(leaving.data(), leaving.size());
    glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::sealed);
    w.bytes(sealer.seal(plain));
    const std::vector<std::uint8_t> mine = w.take();
    const std::span<const std::uint8_t> mine_bytes(mine.data(), mine.size());

    other.send_from_here(mine_bytes);
    say("sent this session's goodbye from another session's address");

    const std::vector<std::uint8_t> theirs = other.sealed(plain);
    (void)socket.send(server, std::span<const std::uint8_t>(theirs.data(), theirs.size()));
    say("sent another session's goodbye from this session's address");

    auto nowhere = glideslope::platform::UdpSocket::bound(0);
    if (!nowhere) {
        std::fprintf(stderr, "glideslope_cli: cannot open a third socket\n");
        return false;
    }
    std::vector<std::uint8_t> back(glideslope::platform::largest_datagram);
    const auto asked = std::chrono::steady_clock::now();
    double sent_at_s = -1.0;
    for (;;) {
        const double waited_s =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - asked).count();
        if (waited_s > 60.0) {
            std::fprintf(stderr, "glideslope_cli: the goodbye from an address with no "
                                 "session was never refused\n");
            return false;
        }
        // Sent again while no refusal comes, as UDP needs; every copy is the
        // same datagram, so it is a replay as well as a forgery.
        if (waited_s - sent_at_s >= 0.25) {
            (void)nowhere->send(server, mine_bytes);
            sent_at_s = waited_s;
        }
        glideslope::platform::Address from;
        const std::size_t got = nowhere->receive(back, from);
        if (got == glideslope::net::envelope_size + 1 &&
            back[glideslope::net::envelope_size - 1] ==
                static_cast<std::uint8_t>(glideslope::net::Type::refusal)) {
            say("sent this session's goodbye from an address with no session, and it "
                "was refused, reason " +
                std::to_string(static_cast<unsigned>(back[glideslope::net::envelope_size])));
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// **What became of a connection, said on standard error and to `--heard
// FILE`** as well, on every way out: a test in a pipeline reads the file,
// and a client that failed without writing it left the test saying only
// that it had written nothing.
void say_outcome(const std::string& heard_file, const std::string& line) {
    std::fprintf(stderr, "glideslope_cli: %s\n", line.c_str());
    if (!heard_file.empty()) {
        std::ofstream(heard_file, std::ios::app) << line << '\n';
    }
}

// **How joining again came out**: a new session; the old one after all - it
// answered a knock, so it was never gone; refused, and why; or no answer in a
// minute. And how many datagrams opened under the old session that were not
// its answer, and were not gone back for.
struct Rejoined {
    std::optional<glideslope::net::SessionKeys> keys;
    bool old_session_answers = false;
    std::optional<glideslope::net::Refusal> refused;
    int stale = 0;
};

// **Joins again, as a client the server has let go** (`stay()`'s
// `Ended::let_go`), by `net::Rejoin` - the same piece the client with the
// window joins again by: a new initiation with the same static key, resent
// every quarter of a second until it is answered, a minute the most, and the
// old session knocked on meanwhile. **Back to the old session only on its
// answer to that knock**: anything else that opens under it may have been
// sealed before the server let it go, and going back on it left a ghost
// session on the server and this client lost for its timeout.
// `SERVER_FULL` and `DROPPED` end it, with the reason.
Rejoined join_again(glideslope::platform::UdpSocket& socket,
                    const glideslope::platform::Address& server,
                    const glideslope::net::KeyPair& mine, const glideslope::net::PublicKey& theirs,
                    glideslope::net::Sealer& old_sealing, glideslope::net::Unsealer& old_opening) {
    Rejoined out;
    glideslope::net::Rejoin rejoin(mine, theirs, old_sealing, old_opening);
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    const auto began = std::chrono::steady_clock::now();
    for (;;) {
        const double waited =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        if (waited > 60.0) {
            return out;
        }
        for (const std::vector<std::uint8_t>& datagram : rejoin.due(waited)) {
            (void)socket.send(server,
                              std::span<const std::uint8_t>(datagram.data(), datagram.size()));
        }
        glideslope::platform::Address from;
        const std::size_t got = socket.receive(into, from);
        if (got <= glideslope::net::envelope_size) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        using Heard = glideslope::net::Rejoin::Heard;
        const Heard heard =
            rejoin.hear(server, from, std::span<const std::uint8_t>(into.data(), got));
        out.stale = rejoin.stale();
        switch (heard) {
        case Heard::server_full:
            out.refused = glideslope::net::Refusal::server_full;
            return out;
        case Heard::dropped:
            out.refused = glideslope::net::Refusal::dropped;
            return out;
        case Heard::old_session_answers:
            out.old_session_answers = true;
            return out;
        case Heard::joined:
            out.keys = rejoin.keys();
            return out;
        case Heard::nothing:
            break;
        }
    }
}

// **A test flag's work (`--again-from-elsewhere FILE`)**: a copy of this
// client's initiation, replayed from a port of its own while the session is
// live - what anybody who saw it on the wire could send - and kept up as a
// replayer who wanted to hold the session open would.
//
// `answered()` sends the copy every quarter of a second until the server
// answers it, a minute the most. From then on a thread goes on sending it
// every quarter of a second, faster than any `--timeout` a test uses, with a
// sealed datagram that opens under nothing beside it: while the copy's
// session lives, that is dropped without a word; once it has gone, it is
// refused, as a stranger's is. On the refusal the thread writes FILE: that the
// copy's session was let go, and how many datagrams it was sent besides its
// answer - a replayer's session should be sent none.
class CopyFromElsewhere {
public:
    CopyFromElsewhere(const glideslope::platform::Address& server,
                      std::span<const std::uint8_t> initiation, std::string file)
        : server_(server), initiation_(initiation.begin(), initiation.end()),
          file_(std::move(file)), socket_(glideslope::platform::UdpSocket::bound(0)) {}
    CopyFromElsewhere(const CopyFromElsewhere&) = delete;
    CopyFromElsewhere& operator=(const CopyFromElsewhere&) = delete;
    ~CopyFromElsewhere() {
        stop_ = true;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    bool answered() {
        if (!socket_) {
            std::fprintf(stderr, "glideslope_cli: no second port for the copy\n");
            return false;
        }
        std::vector<std::uint8_t> back(glideslope::platform::largest_datagram);
        const auto asked = std::chrono::steady_clock::now();
        double resent_at_s = -1.0;
        for (;;) {
            const double waited_s =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - asked).count();
            if (waited_s > 60.0) {
                std::fprintf(stderr,
                             "glideslope_cli: the copy from elsewhere was never answered\n");
                return false;
            }
            if (waited_s - resent_at_s >= 0.25) {
                (void)socket_->send(server_, initiation_);
                resent_at_s = waited_s;
            }
            glideslope::platform::Address from;
            const std::size_t got = socket_->receive(back, from);
            if (type_of(back, got) == glideslope::net::Type::handshake_response) {
                std::printf("the copy from elsewhere, sent while the session was live, was "
                            "answered\n");
                std::fflush(stdout);
                thread_ = std::thread([this] { keep_up(); });
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

private:
    static std::optional<glideslope::net::Type> type_of(const std::vector<std::uint8_t>& d,
                                                        std::size_t got) {
        if (got < glideslope::net::envelope_size) {
            return std::nullopt;
        }
        return static_cast<glideslope::net::Type>(d[glideslope::net::envelope_size - 1]);
    }

    void keep_up() {
        glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::sealed);
        w.bytes(std::vector<std::uint8_t>(40, 0));
        const std::vector<std::uint8_t> unopenable = w.take();
        std::vector<std::uint8_t> back(glideslope::platform::largest_datagram);
        std::uint64_t others = 0;
        auto sent_at = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        while (!stop_) {
            if (std::chrono::steady_clock::now() - sent_at >= std::chrono::milliseconds(250)) {
                sent_at = std::chrono::steady_clock::now();
                (void)socket_->send(server_, initiation_);
                (void)socket_->send(server_, unopenable);
            }
            glideslope::platform::Address from;
            const std::size_t got = socket_->receive(back, from);
            const auto type = type_of(back, got);
            if (type == glideslope::net::Type::sealed) {
                ++others;
            } else if (type == glideslope::net::Type::refusal) {
                std::ofstream(file_, std::ios::app)
                    << "the copy's session was let go; it was sent " << others
                    << " datagrams besides its answer\n";
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    glideslope::platform::Address server_;
    std::vector<std::uint8_t> initiation_;
    std::string file_;
    std::optional<glideslope::platform::UdpSocket> socket_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

int connect_to(const std::string& where, const std::string& key_hex, double stay_s,
               bool again, bool fly, double after_s, const std::string& secret_hex = "",
               const std::string& heard_file = "", int until_flying_again = 0,
               const std::string& ready_file = "", bool predict = false,
               const std::string& track_file = "", double hand_over_at_s = -1.0,
               double take_back_at_s = -1.0, double dive_after_s = -1.0,
               bool watch_ai = false, bool again_when_let_go = false,
               bool first_from_elsewhere = false, const std::string& until_exists = "",
               double take_over_at_s = -1.0, int take_over_aircraft = -1,
               bool take_over_once_ai = false, bool long_frame_after_switch = false,
               bool late_update_after_take_over = false, bool goodbye = true,
               bool forge_leaving = false, bool stall_once_rolled = false,
               const std::string& again_from_elsewhere = "", bool leave_once_back = false) {
    // **A test flag's work**: join a session that is already running. A
    // client that connects the instant the server does learns nothing about
    // whether the server was flying before it arrived. With `--after-ready`
    // the wait begins when the server says it is flying, not when this
    // started: a debug server on a slow runner spent the whole of a five
    // second wait building its terrain. Five minutes is the most it waits for
    // that.
    if (!ready_file.empty()) {
        const auto asked = std::chrono::steady_clock::now();
        while (!std::filesystem::exists(ready_file)) {
            if (std::chrono::steady_clock::now() - asked > std::chrono::minutes(5)) {
                std::fprintf(stderr, "glideslope_cli: %s never appeared\n", ready_file.c_str());
                return 1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    if (after_s > 0.0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<long long>(after_s * 1000.0)));
    }
    const auto address = glideslope::platform::address_of(where);
    if (!address) {
        std::fprintf(stderr, "glideslope_cli: %s is not an address\n", where.c_str());
        return 2;
    }
    const auto theirs = glideslope::net::public_from_text(key_hex);
    if (!theirs) {
        std::fprintf(stderr, "glideslope_cli: that is not a server key\n");
        return 2;
    }
    auto socket = glideslope::platform::UdpSocket::bound(0);
    if (!socket) {
        std::fprintf(stderr, "glideslope_cli: cannot open a socket\n");
        return 1;
    }

    // **A test flag's work, too**: `--key` makes this client a known player,
    // so that a test can choose which way the players' keys sort and so
    // build the order of arrival it means to, rather than hope for it.
    glideslope::net::KeyPair mine = glideslope::net::mint_key_pair();
    if (!secret_hex.empty()) {
        const auto secret = glideslope::net::secret_from_text(secret_hex);
        if (!secret) {
            std::fprintf(stderr, "glideslope_cli: --key wants 64 hexadecimal digits\n");
            return 2;
        }
        mine.secret = *secret;
        mine.publik = glideslope::net::public_from_secret(*secret);
    }
    glideslope::net::Initiator initiator(mine, *theirs);
    glideslope::net::Writer w =
        glideslope::net::begin(glideslope::net::Type::handshake_initiation);
    w.bytes(initiator.begin());
    const std::vector<std::uint8_t> first = w.take();

    // **A test flag's work (`--first-from-elsewhere`)**: a copy of this
    // initiation from another address, answered, before this address sends
    // it at all - what somebody who saw it on the wire could inject from a
    // spoofed address to arrive first. This client must still get a session
    // of its own from its own address. The copy's answer is waited for, not
    // timed, so that it has been taken before the real one goes.
    if (first_from_elsewhere) {
        auto elsewhere = glideslope::platform::UdpSocket::bound(0);
        if (!elsewhere) {
            std::fprintf(stderr, "glideslope_cli: cannot open a second socket\n");
            return 1;
        }
        std::vector<std::uint8_t> back(glideslope::platform::largest_datagram);
        const auto asked = std::chrono::steady_clock::now();
        double resent_at_s = -1.0;
        for (;;) {
            const double waited_s = std::chrono::duration<double>(
                                        std::chrono::steady_clock::now() - asked)
                                        .count();
            if (waited_s > 60.0) {
                std::fprintf(stderr, "glideslope_cli: the copy from elsewhere was "
                                     "never answered\n");
                return 1;
            }
            if (waited_s - resent_at_s >= 0.25) {
                (void)elsewhere->send(*address, std::span<const std::uint8_t>(
                                                    first.data(), first.size()));
                resent_at_s = waited_s;
            }
            glideslope::platform::Address from;
            const std::size_t got = elsewhere->receive(back, from);
            if (got > glideslope::net::envelope_size &&
                back[glideslope::net::envelope_size - 1] ==
                    static_cast<std::uint8_t>(glideslope::net::Type::handshake_response)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::printf("the copy from elsewhere was answered\n");
    }
    if (!socket->send(*address,
                      std::span<const std::uint8_t>(first.data(), first.size()))) {
        std::fprintf(stderr, "glideslope_cli: cannot send to %s\n", where.c_str());
        return 1;
    }

    // Wait for the answer, resending while nothing comes: a handshake over
    // UDP has to expect its first datagram to be lost, and a server that is
    // not listening yet answers nothing at all. The same initiation goes out
    // each time - `Noise_IK` makes one initiation, and a second would be a
    // second handshake - which the server treats as a duplicate.
    //
    // **A minute before giving up**, not five seconds: a server builds its
    // terrain after binding its socket and answers nothing until it has, and
    // a debug server on a busy CI runner took longer than five seconds - so
    // clients started with it gave up, and tests found fewer players than
    // they had started. What arrives meanwhile waits in the socket.
    constexpr double resend_every_s = 0.25;
    constexpr double give_up_after_s = 60.0;
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    const auto began = std::chrono::steady_clock::now();
    double sent_at_s = 0.0;
    for (;;) {
        glideslope::platform::Address from;
        const std::size_t got = socket->receive(into, from);
        const double waited =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
                .count();
        if (got > glideslope::net::envelope_size) {
            glideslope::net::Reader r(std::span<const std::uint8_t>(into.data(), got));
            glideslope::net::Envelope envelope;
            glideslope::net::Refusal why{};
            if (glideslope::net::read_envelope(r, envelope, why)) {
                if (envelope.type == glideslope::net::Type::refusal) {
                    const unsigned reason = into[glideslope::net::envelope_size];
                    std::fprintf(stderr, "glideslope_cli: refused, reason %u\n", reason);
                    // Said to the file too: a test in a pipeline reads that.
                    if (!heard_file.empty()) {
                        std::ofstream(heard_file, std::ios::app)
                            << "refused, reason " << reason << '\n';
                    }
                    return 1;
                }
                if (envelope.type == glideslope::net::Type::handshake_response) {
                    const auto session = initiator.finish(
                        std::span<const std::uint8_t>(into.data(), got)
                            .subspan(glideslope::net::envelope_size));
                    if (!session) {
                        say_outcome(heard_file, "the answer did not open");
                        return 1;
                    }
                    // And seal something, so the session is used and not
                    // merely agreed. A pong nobody pinged for: the server
                    // ignores a token it did not send, so this costs it
                    // nothing and shows the seal works in this direction.
                    glideslope::net::Sealer sealer(session->sending);
                    glideslope::net::Unsealer unsealer(session->receiving);
                    const std::vector<std::uint8_t> plain =
                        glideslope::net::knock(glideslope::net::Inside::pong, 0);
                    glideslope::net::Writer sw =
                        glideslope::net::begin(glideslope::net::Type::sealed);
                    sw.bytes(sealer.seal(
                        std::span<const std::uint8_t>(plain.data(), plain.size())));
                    const std::vector<std::uint8_t> out = sw.take();
                    (void)socket->send(
                        *address,
                        std::span<const std::uint8_t>(out.data(), out.size()));
                    std::printf("session with %s\n", session->theirs.text().c_str());
                    std::printf("sealed %zu bytes to it\n", out.size());
                    // **A test flag's work (`--again-from-elsewhere`)**: a
                    // copy of this client's initiation, replayed from another
                    // port while its session is live - what anybody who saw
                    // it on the wire could send. The server answers it with
                    // a session nobody can use; it must not take this one's
                    // aircraft or end this session for it.
                    std::optional<CopyFromElsewhere> copy;
                    if (!again_from_elsewhere.empty()) {
                        copy.emplace(*address,
                                     std::span<const std::uint8_t>(first.data(), first.size()),
                                     again_from_elsewhere);
                        if (!copy->answered()) {
                            return 1;
                        }
                    }
                    if (again_when_let_go) {
                        return again_once_let_go(
                            *socket, *address,
                            std::span<const std::uint8_t>(first.data(), first.size()),
                            std::span<const std::uint8_t>(into.data(), got), stay_s,
                            heard_file);
                    }
                    if (stay_s <= 0.0) {
                        return 0;
                    }
                    // **A test flag's work (`--forge-leaving`)**: goodbyes
                    // that are not this session's own, each from where a
                    // forger could send one. A second session is made, and
                    // lives until this one has stayed - it says goodbye
                    // itself when it goes, as its own client should.
                    std::optional<glideslope::net::ClientSession> other =
                        forge_leaving
                            ? glideslope::net::ClientSession::connect(where, key_hex, 60.0)
                            : std::nullopt;
                    if (forge_leaving) {
                        if (!other ||
                            !forge_goodbyes(*socket, *address, sealer, *other, heard_file)) {
                            return 1;
                        }
                    }
                    Ended ended = Ended::stayed;
                    SessionStreams streams;
                    int rc = stay(*socket, *address, sealer, unsealer, stay_s,
                                  until_exists,
                                  again ? std::span<const std::uint8_t>(first.data(),
                                                                       first.size())
                                        : std::span<const std::uint8_t>(),
                                  fly, mine.publik.text().substr(0, 8), heard_file,
                                  until_flying_again, predict, track_file, hand_over_at_s,
                                  take_back_at_s, dive_after_s, watch_ai, take_over_at_s,
                                  take_over_aircraft, take_over_once_ai,
                                  long_frame_after_switch, late_update_after_take_over,
                                  goodbye, stall_once_rolled, false, streams, ended);
                    // **A client the server has let go joins again by
                    // itself**, for what is left of its stay, and flies as
                    // it did. What it was told to do once - hand over, take
                    // over, stall - it has done, and does not do again. One
                    // the operator dropped does not.
                    glideslope::net::Sealer* sealing = &sealer;
                    glideslope::net::Unsealer* opening = &unsealer;
                    std::unique_ptr<glideslope::net::Sealer> owned_sealer;
                    std::unique_ptr<glideslope::net::Unsealer> owned_unsealer;
                    while (ended == Ended::let_go) {
                        const double left_s =
                            stay_s - std::chrono::duration<double>(
                                         std::chrono::steady_clock::now() - began)
                                         .count();
                        if (left_s <= 0.0) {
                            break;
                        }
                        const Rejoined rejoined =
                            join_again(*socket, *address, mine, *theirs, *sealing, *opening);
                        // **What opened under the old session and was not
                        // gone back for**, said so that a test can tell a
                        // join made with stale updates arriving from one
                        // made without.
                        std::printf("while joining again, %d opened under the old session "
                                    "that were not its answer\n",
                                    rejoined.stale);
                        if (rejoined.refused) {
                            say_outcome(heard_file,
                                        "let go, and refused when joining again, reason " +
                                            std::to_string(
                                                static_cast<unsigned>(*rejoined.refused)));
                            return 1;
                        }
                        if (rejoined.old_session_answers) {
                            std::printf("the old session answered; staying in it, its inputs "
                                        "numbered on from %u\n",
                                        streams.sequence);
                        } else if (rejoined.keys) {
                            std::printf("joined again: session with %s\n",
                                        rejoined.keys->theirs.text().c_str());
                            owned_sealer =
                                std::make_unique<glideslope::net::Sealer>(rejoined.keys->sending);
                            owned_unsealer = std::make_unique<glideslope::net::Unsealer>(
                                rejoined.keys->receiving);
                            sealing = owned_sealer.get();
                            opening = owned_unsealer.get();
                            streams = SessionStreams{};
                        } else {
                            say_outcome(heard_file, "let go, and could not join again");
                            return 1;
                        }
                        std::fflush(stdout);
                        rc = stay(*socket, *address, *sealing, *opening, left_s, until_exists,
                                  std::span<const std::uint8_t>(), fly,
                                  mine.publik.text().substr(0, 8), heard_file,
                                  until_flying_again, predict, track_file, -1.0, -1.0,
                                  dive_after_s, watch_ai, -1.0, -1, false,
                                  long_frame_after_switch, late_update_after_take_over,
                                  goodbye, false, stall_once_rolled, streams, ended,
                                  leave_once_back);
                    }
                    return rc;
                }
            }
        }
        if (waited > give_up_after_s) {
            say_outcome(heard_file, "no answer from " + where);
            return 1;
        }
        if (waited - sent_at_s >= resend_every_s) {
            (void)socket->send(
                *address, std::span<const std::uint8_t>(first.data(), first.size()));
            sent_at_s = waited;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

static int run_program(int argc, char** argv) {
    // First: a failed assert prints and ends the program rather than
    // waiting on a dialog nobody will answer (platform/no_crash_dialogs.hpp).
    glideslope::platform::no_crash_dialogs();
    // And a write to a pipe whose reader has gone fails, rather than ending
    // the program (platform/closed_pipes.hpp).
    glideslope::platform::outlive_closed_pipes();
    std::vector<std::string_view> args(argv + 1, argv + argc);
    try {
        std::filesystem::path data;
        if (args.size() >= 2 && args[0] == "--data") {
            data = std::filesystem::path(std::string(args[1]));
            args.erase(args.begin(), args.begin() + 2);
        } else {
            data = glideslope::platform::data_directory();
        }
        connect_air.data = data;

        if (args.size() == 1 && args[0] == "--version") {
            const std::string_view v = glideslope::sim::version();
            std::printf("glideslope_cli %.*s\n", static_cast<int>(v.size()), v.data());
            return 0;
        }
        if (args.size() == 1 && args[0] == "--help") {
            print_usage(stdout);
            return 0;
        }
        if (args.size() == 1 && args[0] == "aircraft") {
            return list_aircraft(data);
        }
        if (args.size() == 2 && args[0] == "aircraft") {
            return print_aircraft(data, std::string(args[1]));
        }
        if ((args.size() == 2 || args.size() == 3) && args[0] == "figures") {
            return fly_figures(data, std::string(args[1]),
                               args.size() == 3 ? std::string(args[2]) : "");
        }
        // **`connect --online`**, which reads the default server out of
        // `server.txt` rather than being told it (`REQUIREMENTS.md` 6.6). The
        // file names a host, a port and the server's public key - everything
        // a client needs and nothing that is a secret.
        // **`connect --server HOST PORT --server-key HEX`**, the other form
        // `REQUIREMENTS.md` 6.6 names: the same three things as `server.txt`,
        // given on the command line instead of in a file.
        if (args.size() >= 5 && args[0] == "connect" && args[1] == "--server") {
            std::string host;
            std::string port;
            std::string key;
            double stay_s = 0.0;
            bool fly = false;
            for (std::size_t i = 1; i < args.size(); ++i) {
                if (args[i] == "--server" && i + 2 < args.size()) {
                    host = std::string(args[i + 1]);
                    port = std::string(args[i + 2]);
                    i += 2;
                } else if (args[i] == "--server-key" && i + 1 < args.size()) {
                    key = std::string(args[i + 1]);
                    ++i;
                } else if (args[i] == "--fly") {
                    fly = true;
                } else {
                    stay_s = std::strtod(std::string(args[i]).c_str(), nullptr);
                }
            }
            if (host.empty() || port.empty() || key.empty()) {
                std::fprintf(stderr, "glideslope_cli: --server wants a host and a "
                                     "port, and --server-key the key\n");
                return 2;
            }
            return connect_to(host + ":" + port, key, stay_s, false, fly, 0.0);
        }
        if (args.size() >= 2 && args[0] == "connect" && args[1] == "--online") {
            const auto server = glideslope::platform::default_server();
            if (!server) {
                std::fprintf(stderr,
                             "glideslope_cli: --online needs a server.txt naming a "
                             "host, a port and a key. None was found in "
                             "glideslope's config directory, and "
                             "GLIDESLOPE_SERVER_TXT names none either\n");
                return 2;
            }
            std::printf("server.txt: %s port %u\n", server->host.c_str(),
                        static_cast<unsigned>(server->port));
            double stay_s = 0.0;
            bool fly = false;
            for (std::size_t i = 2; i < args.size(); ++i) {
                if (args[i] == "--fly") {
                    fly = true;
                    continue;
                }
                stay_s = std::strtod(std::string(args[i]).c_str(), nullptr);
            }
            const std::string where =
                server->host + ":" + std::to_string(server->port);
            return connect_to(where, server->key_hex, stay_s, false, fly, 0.0);
        }
        if (args.size() >= 3 && args.size() <= 37 && args[0] == "connect") {
            double stay_s = 0.0;
            bool again = false;
            bool again_when_let_go = false;
            bool first_from_elsewhere = false;
            std::string again_from_elsewhere;
            std::string until_exists;
            bool fly = false;
            double after_s = 0.0;
            std::string secret_hex;
            std::string heard_file;
            int until_flying_again = 0;
            std::string ready_file;
            bool predict = false;
            double hand_over_at_s = -1.0;
            double take_back_at_s = -1.0;
            double dive_after_s = -1.0;
            bool watch_ai = false;
            double take_over_at_s = -1.0;
            int take_over_aircraft = -1;
            bool take_over_once_ai = false;
            std::string track_file;
            bool long_frame_after_switch = false;
            bool late_update_after_take_over = false;
            bool goodbye = true;
            bool forge_leaving = false;
            bool stall_once_rolled = false;
            bool leave_once_back = false;
            std::string done_file;
            for (std::size_t i = 3; i < args.size(); ++i) {
                if (args[i] == "--done" && i + 1 < args.size()) {
                    done_file = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--stall-once-rolled") {
                    stall_once_rolled = true;
                    continue;
                }
                if (args[i] == "--leave-once-back") {
                    leave_once_back = true;
                    continue;
                }
                if (args[i] == "--no-goodbye") {
                    goodbye = false;
                    continue;
                }
                if (args[i] == "--forge-leaving") {
                    forge_leaving = true;
                    continue;
                }
                if (args[i] == "--late-update-after-take-over") {
                    late_update_after_take_over = true;
                    continue;
                }
                if (args[i] == "--long-frame-after-switch") {
                    long_frame_after_switch = true;
                    continue;
                }
                if (args[i] == "--own-air") {
                    connect_air.own_air = true;
                    continue;
                }
                if (args[i] == "--track" && i + 1 < args.size()) {
                    track_file = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--predict") {
                    predict = true;
                    continue;
                }
                // **Its copilot** (`--copilot AIRCRAFT TASK`), asked with the
                // player's own key; `--copilot-provider`, `--copilot-model`,
                // `--copilot-record FILE` or `--copilot-playback FILE`,
                // `--copilot-at S` on the session's clock, `--copilot-answers N`
                // and `--copilot-stay S` after the last route sent.
                if (args[i] == "--copilot" && i + 2 < args.size()) {
                    glideslope::frontend::PlayersCopilotOptions c;
                    if (connect_copilot.options) {
                        c = *connect_copilot.options;
                    }
                    c.aircraft = std::string(args[i + 1]);
                    c.task = std::string(args[i + 2]);
                    connect_copilot.options = c;
                    i += 2;
                    continue;
                }
                if (args[i].starts_with("--copilot-") && i + 1 < args.size()) {
                    const std::string v(args[i + 1]);
                    const bool of_the_model = args[i] == "--copilot-provider" ||
                                              args[i] == "--copilot-routine" ||
                                              args[i] == "--copilot-model" ||
                                              args[i] == "--copilot-record" ||
                                              args[i] == "--copilot-playback";
                    if (of_the_model && !connect_copilot.options) {
                        connect_copilot.options.emplace();
                    }
                    glideslope::frontend::PlayersCopilotOptions scratch;
                    glideslope::frontend::PlayersCopilotOptions& c =
                        of_the_model ? *connect_copilot.options : scratch;
                    if (args[i] == "--copilot-provider") {
                        c.provider = v;
                        connect_copilot.provider_given = true;
                    } else if (args[i] == "--copilot-model") {
                        c.model = v;
                        connect_copilot.provider_given = true;
                    } else if (args[i] == "--copilot-record") {
                        c.record = v;
                    } else if (args[i] == "--copilot-playback") {
                        c.playback = v;
                    } else if (args[i] == "--copilot-routine") {
                        char* end = nullptr;
                        c.routine_s = std::strtod(v.c_str(), &end);
                        if (end == v.c_str() || *end != '\0' || !(c.routine_s > 0.0) ||
                            !std::isfinite(c.routine_s)) {
                            std::fprintf(stderr,
                                         "glideslope_cli: --copilot-routine wants a number of "
                                         "seconds more than nought, not '%s'\n",
                                         v.c_str());
                            return 2;
                        }
                    } else if (args[i] == "--copilot-at") {
                        connect_copilot.at_s = std::strtod(v.c_str(), nullptr);
                    } else if (args[i] == "--copilot-answers") {
                        connect_copilot.answers = std::atoi(v.c_str());
                    } else if (args[i] == "--copilot-stay") {
                        connect_copilot.stay_s = std::strtod(v.c_str(), nullptr);
                    } else {
                        std::fprintf(stderr, "glideslope_cli: what is %s?\n",
                                     std::string(args[i]).c_str());
                        return 2;
                    }
                    ++i;
                    continue;
                }
                // **A route sent as it is** (`--send-route FILE`), at
                // `--copilot-at`: what a client could send that its copilot
                // never checked, for the server to refuse.
                // ...for another player's aircraft (`--route-for-another`),
                // or once its own is a wreck (`--route-when-wrecked`).
                if (args[i] == "--route-for-another") {
                    connect_copilot.route_for_another = true;
                    continue;
                }
                if (args[i] == "--route-when-wrecked") {
                    connect_copilot.route_when_wrecked = true;
                    continue;
                }
                if (args[i] == "--send-route" && i + 1 < args.size()) {
                    connect_copilot.route_file = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--hand-over-at" && i + 1 < args.size()) {
                    hand_over_at_s = std::strtod(std::string(args[i + 1]).c_str(), nullptr);
                    ++i;
                    continue;
                }
                // **The model that plans it handed over**: anthropic, openai
                // or none, either model with `:MODEL`.
                if (args[i] == "--hand-over-model" && i + 1 < args.size()) {
                    try {
                        connect_copilot.hand_over =
                            glideslope::frontend::read_hand_over_model(args[i + 1]);
                    } catch (const std::invalid_argument& e) {
                        std::fprintf(stderr, "glideslope_cli: %s\n", e.what());
                        return 2;
                    }
                    ++i;
                    continue;
                }
                if (args[i] == "--take-over-aircraft" && i + 1 < args.size()) {
                    take_over_aircraft = std::atoi(std::string(args[i + 1]).c_str());
                    ++i;
                    continue;
                }
                if (args[i] == "--once-the-ai-flies-it") {
                    take_over_once_ai = true;
                    continue;
                }
                if (args[i] == "--take-over-at" && i + 1 < args.size()) {
                    take_over_at_s = std::strtod(std::string(args[i + 1]).c_str(), nullptr);
                    ++i;
                    continue;
                }
                if (args[i] == "--watch-ai") {
                    watch_ai = true;
                    continue;
                }
                if (args[i] == "--dive-after" && i + 1 < args.size()) {
                    dive_after_s = std::strtod(std::string(args[i + 1]).c_str(), nullptr);
                    ++i;
                    continue;
                }
                if (args[i] == "--take-back-at" && i + 1 < args.size()) {
                    take_back_at_s = std::strtod(std::string(args[i + 1]).c_str(), nullptr);
                    ++i;
                    continue;
                }
                if (args[i] == "--until-flying-again" && i + 1 < args.size()) {
                    until_flying_again = std::atoi(std::string(args[i + 1]).c_str());
                    ++i;
                    continue;
                }
                if (args[i] == "--key" && i + 1 < args.size()) {
                    secret_hex = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--heard" && i + 1 < args.size()) {
                    heard_file = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--after-ready" && i + 1 < args.size()) {
                    ready_file = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--after" && i + 1 < args.size()) {
                    after_s = std::strtod(std::string(args[i + 1]).c_str(), nullptr);
                    ++i;
                    continue;
                }
                if (args[i] == "--again") {
                    again = true;
                    continue;
                }
                if (args[i] == "--again-when-let-go") {
                    again_when_let_go = true;
                    continue;
                }
                if (args[i] == "--again-from-elsewhere" && i + 1 < args.size()) {
                    again_from_elsewhere = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--first-from-elsewhere") {
                    first_from_elsewhere = true;
                    continue;
                }
                if (args[i] == "--until-exists" && i + 1 < args.size()) {
                    until_exists = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--fly") {
                    fly = true;
                    continue;
                }
                stay_s = std::strtod(std::string(args[i]).c_str(), nullptr);
                if (!(stay_s > 0.0)) {
                    std::fprintf(stderr,
                                 "glideslope_cli: connect's seconds must be more "
                                 "than nothing\n");
                    return 2;
                }
            }
            if ((connect_copilot.route_for_another || connect_copilot.route_when_wrecked) &&
                connect_copilot.route_file.empty()) {
                std::fprintf(stderr, "glideslope_cli: --route-for-another and "
                                     "--route-when-wrecked say how --send-route FILE is sent, "
                                     "and there is none\n");
                return 2;
            }
            // **A hand-over's model is the only copilot**: its aircraft is
            // the server's to say, its words the data's, and its provider
            // the flag's - `--copilot-record`, `--copilot-playback` and
            // `--copilot-routine` say how it is asked.
            if (connect_copilot.hand_over) {
                if (hand_over_at_s < 0.0) {
                    std::fprintf(stderr, "glideslope_cli: --hand-over-model chooses what "
                                         "plans a hand-over, and there is no --hand-over-at\n");
                    return 2;
                }
                const auto& c = connect_copilot.options;
                if (connect_copilot.provider_given || (c && !c->aircraft.empty())) {
                    std::fprintf(stderr, "glideslope_cli: --hand-over-model is its copilot: "
                                         "not with --copilot, --copilot-provider or "
                                         "--copilot-model\n");
                    return 2;
                }
            } else if (connect_copilot.options) {
                if (connect_copilot.options->aircraft.empty()) {
                    std::fprintf(stderr, "glideslope_cli: --copilot-* needs --copilot AIRCRAFT TASK\n");
                    return 2;
                }
                connect_copilot.seat = std::make_unique<glideslope::frontend::PlayersCopilot>(
                    data, *connect_copilot.options);
            }
            if (again_when_let_go && (stay_s <= 0.0 || again || fly)) {
                std::fprintf(stderr, "glideslope_cli: --again-when-let-go needs "
                                     "seconds to listen for, and neither --again "
                                     "nor --fly\n");
                return 2;
            }
            if (again && stay_s <= 0.0) {
                std::fprintf(stderr, "glideslope_cli: --again needs seconds to "
                                     "stay for, or there is nothing to watch\n");
                return 2;
            }
            if (fly && stay_s <= 0.0) {
                std::fprintf(stderr, "glideslope_cli: --fly needs seconds to fly "
                                     "for\n");
                return 2;
            }
            if (stall_once_rolled && (stay_s <= 0.0 || !fly)) {
                std::fprintf(stderr, "glideslope_cli: --stall-once-rolled needs --fly "
                                     "and seconds to fly for\n");
                return 2;
            }
            if (leave_once_back && (stay_s <= 0.0 || !fly)) {
                std::fprintf(stderr, "glideslope_cli: --leave-once-back needs --fly "
                                     "and seconds to fly for\n");
                return 2;
            }
            if ((forge_leaving || !goodbye) && stay_s <= 0.0) {
                std::fprintf(stderr, "glideslope_cli: --forge-leaving and --no-goodbye "
                                     "need seconds to stay for\n");
                return 2;
            }
            const int connected = connect_to(std::string(args[1]), std::string(args[2]), stay_s,
                                             again, fly, after_s, secret_hex, heard_file,
                                             until_flying_again, ready_file, predict, track_file,
                                             hand_over_at_s, take_back_at_s, dive_after_s, watch_ai,
                                             again_when_let_go, first_from_elsewhere, until_exists,
                                             take_over_at_s, take_over_aircraft, take_over_once_ai,
                                             long_frame_after_switch, late_update_after_take_over,
                                             goodbye, forge_leaving, stall_once_rolled,
                                             again_from_elsewhere, leave_once_back);
            // **Said when it has gone** (`--done FILE`), for another client
            // in a test to wait on with `--until-exists`.
            if (!done_file.empty()) {
                std::ofstream(done_file, std::ios::app) << "done " << connected << '\n';
            }
            return connected;
        }
        if (args.size() == 1 && args[0] == "air") {
            return air();
        }
        if (args.size() == 4 && args[0] == "sky") {
            return sky(data, args[1], args[2], args[3]);
        }
        if (args.size() >= 4 && args[0] == "plan") {
            return plan_command(data, args);
        }
        if (args.size() >= 2 && args[0] == "land") {
            return land(data, args);
        }
        if ((args.size() == 2 || args.size() == 3) && args[0] == "plan-speeds") {
            return plan_speeds(data, args);
        }
        if (args.size() == 2 && args[0] == "glide-speeds") {
            return glide_speeds(data, args);
        }
        if ((args.size() == 2 || args.size() == 3) && args[0] == "takeoff-speeds") {
            return takeoff_speeds(data, args);
        }
        if (args.size() >= 2 && args[0] == "fly-plan") {
            return fly_plan(data, args);
        }
        if (args.size() >= 8 && args[0] == "fly-copilot") {
            return fly_copilot(data, args);
        }
        if (args.size() == 2 && args[0] == "weather") {
            return weather(std::string(args[1]));
        }
        if (args.size() == 3 && args[0] == "height") {
            return height(data, args[1], args[2]);
        }
        if ((args.size() == 1 || args.size() == 2) && args[0] == "selftest") {
            return selftest(data, args.size() == 2 ? std::string(args[1]) : "c172p");
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glideslope_cli: %s\n", e.what());
        return 1;
    }

    // No arguments, the wrong number, or a command it does not know: say how to
    // use it, on the error stream, and fail, so a script that typed it wrong
    // finds out.
    print_usage(stderr);
    return 2;
}

int main(int argc, char** argv) {
    // The process ends with its C runtime whole until every other thread has
    // stopped - Windows' own threads too (platform/end_process.hpp).
    glideslope::platform::end_process(run_program(argc, argv));
}
