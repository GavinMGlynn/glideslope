// glideslope_cli - the simulation with no window.
//
// It links the simulation and nothing presentational, which is what makes it
// the proof that the simulation can run where there is no window: in CI, in a
// test, and inside the server.

#include "net/handshake.hpp"
#include "platform/end_process.hpp"
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
#include "net/state.hpp"
#include "platform/http.hpp"
#include "platform/socket.hpp"
#include "platform/paths.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/figures.hpp"
#include "sim/fixed_step.hpp"
#include "sim/selftest.hpp"
#include "sim/version.hpp"
#include "world/dem.hpp"
#include "world/geodesy.hpp"
#include "world/download.hpp"
#include "world/metar.hpp"
#include "world/sky.hpp"
#include "world/weather.hpp"
#include "world/winds_aloft.hpp"

#include <algorithm>
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
        "                            duplicates a datagram would. --fly sends\n"
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
        "                            --take-back-at S for it back; predicting, it says\n"
        "                            how far what it showed of its own stepped then\n"
        "                            --track FILE writes where every aircraft was\n"
        "                            heard to be, and, predicting, where each other\n"
        "                            one was drawn, to FILE\n"
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

    // Flies its own aircraft forward to `local_s`, on the input most recently
    // sent, which is what the server will fly too.
    void advance(double local_s, std::uint32_t sequence, const glideslope::sim::Controls& c) {
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
        for (; stepped_ < due; ++stepped_) {
            prediction_->step(sequence, c);
        }
        if (stepped_ == due && sequence > 0) {
            // Where it was flown to on this input, for the server's word on
            // the same input to be held against - once it has had the
            // server's word on any input of its. Before that it flew on from
            // an update a trip old, which it could not carry forward over
            // the time the server flew before any input of its arrived: it
            // is joining, not predicting.
            // Taken back from the AI, likewise, until the server has
            // applied an input sent since and the controls have met the
            // pilot's (sim::Controller: at a hand's pace, full travel in a
            // second).
            if (answered_ && !resuming_ && local_s >= settled_at_s_) {
                predicted_at_[sequence] = aircraft_->motion().location_ecef_m;
            } else if (joining_.empty() || joining_.back() != sequence) {
                joining_.push_back(sequence);
            }
        }
    }

    void heard(const glideslope::net::StatePacket& state, double local_s) {
        // The session clock, at the rate it runs (net::SessionClock).
        clock_.heard(state.simulation_time_s, local_s);
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
                    start(m, model->second, state.last_input_applied);
                }
            } else {
                // **The prediction error**: where the server says the
                // aircraft was when it had got to an input, against where
                // this client had flown it to on that input. A client that
                // lagged the server - put back to each update and never
                // flown forward again - would be corrected by little at a
                // time, and it is this, not the corrections, that shows it.
                const auto at = predicted_at_.find(state.last_input_applied);
                if (at != predicted_at_.end()) {
                    const double error = std::hypot(at->second[0] - m.location_ecef_m[0],
                                                    at->second[1] - m.location_ecef_m[1],
                                                    at->second[2] - m.location_ecef_m[2]);
                    if (error > worst_error_m_) {
                        worst_error_m_ = error;
                        worst_error_at_s_ = local_s;
                    }
                    ++compared_;
                }
                predicted_at_.erase(predicted_at_.begin(),
                                    predicted_at_.upper_bound(state.last_input_applied));
                const auto c = prediction_->reconcile(m, state.last_input_applied);
                answered_ = answered_ || state.last_input_applied > 0;
                if (resuming_ && state.last_input_applied >= resumed_from_) {
                    resuming_ = false;
                    settled_at_s_ = local_s + 1.0;
                }
                ++corrections_;
                worst_correction_m_ = std::max(worst_correction_m_, c.moved_m);
                if (c.snapped) {
                    ++snapped_;
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
            // Its own is kept too: while the AI flies it, it is drawn from
            // the updates like any other.
            if (a.index == state.your_aircraft) {
                own_shown_.received(r);
            } else {
                others_[a.index].received(r);
            }
        }
    }

    // **Its own aircraft handed to the AI pilot, or taken back**, as the
    // server said (`CONTROLLER_SWAP`). Handed over, it is no longer
    // predicted - nothing sent here flies it - and is drawn from the updates;
    // taken back, it is predicted again from the next update with its motion.
    // Either way what is shown of it moves across over `blend_s` rather than
    // jumping from where it was shown to where it now is.
    // `sequence` is the newest input sent: the next one is the first the
    // server can apply after a take-back.
    void handed(bool to_ai, double local_s, std::uint32_t sequence) {
        (void)local_s;
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
        switching_ = true;
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
        for (auto& [index, shown] : others_) {
            if (!shown.known()) continue;
            const glideslope::net::RemoteState got = shown.at(now);
            ++shown_;
            if (shown.extrapolating()) {
                ++extrapolated_;
            }
            if (track_) {
                // Where it was drawn, back in the Earth-centred frame, and the
                // session time it was drawn as being at.
                std::array<double, 3> at = ecef(got.north_m, got.east_m, got.down_m);
                *track_ << "shown " << now - glideslope::net::shown_behind_s << ' '
                        << static_cast<unsigned>(index) << ' ' << at[0] << ' ' << at[1]
                        << ' ' << at[2] << '\n';
            }
        }
    }

    void track_to(std::ostream& out) { track_ = &out; }

    // **Where its own aircraft is shown this frame**: predicted while this
    // client flies it, drawn from the updates while the AI does, and blended
    // across a switch. The step measured is the second difference of what is
    // shown from frame to frame - nought for an aircraft moving smoothly, and
    // the size of any jump - at a switch and everywhere else.
    void own_frame(double now, double local_s) {
        std::optional<std::array<double, 3>> at;
        if (!ai_flying_ && prediction_) {
            at = aircraft_->motion().location_ecef_m;
        } else if (ai_flying_ && own_shown_.known() && origin_) {
            const glideslope::net::RemoteState r = own_shown_.at(now);
            at = ecef(r.north_m, r.east_m, r.down_m);
        }
        if (!at) {
            return;
        }
        if (switching_ && shown_before_) {
            // What was shown, less where it now is: taken up over blend_s.
            for (std::size_t i = 0; i < 3; ++i) {
                blend_[i] = (*shown_before_)[i] - (*at)[i];
            }
            blend_from_s_ = local_s;
            switching_ = false;
            frames_since_switch_ = 0;
        }
        const double left = std::max(0.0, 1.0 - (local_s - blend_from_s_) / blend_s);
        std::array<double, 3> shown{};
        for (std::size_t i = 0; i < 3; ++i) {
            shown[i] = (*at)[i] + blend_[i] * left;
        }
        if (shown_before_ && shown_before_before_) {
            double step = 0.0;
            for (std::size_t i = 0; i < 3; ++i) {
                const double d = shown[i] - 2.0 * (*shown_before_)[i] + (*shown_before_before_)[i];
                step += d * d;
            }
            step = std::sqrt(step);
            if (frames_since_switch_ <= 2) {
                worst_step_at_switch_m_ = std::max(worst_step_at_switch_m_, step);
            } else {
                worst_step_otherwise_m_ = std::max(worst_step_otherwise_m_, step);
            }
        }
        ++frames_since_switch_;
        shown_before_before_ = shown_before_;
        shown_before_ = shown;
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
        if (handed_over_ + taken_back_ > 0) {
            std::snprintf(line, sizeof line,
                          "own aircraft: handed to the AI %zu times and taken back %zu; the "
                          "largest step at a switch %.3f m, and otherwise %.3f m",
                          handed_over_, taken_back_, worst_step_at_switch_m_,
                          worst_step_otherwise_m_);
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
               std::uint32_t last_applied) {
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
        aircraft_->set_motion(m);
        prediction_ = std::make_unique<glideslope::sim::Prediction>(*aircraft_);
        // From where the server said it was to where it is now: every step
        // flown since on inputs the server had not yet applied.
        for (const auto& [sequence, controls] : before_) {
            if (sequence > last_applied) {
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

    // The other way: north-east-down about the origin back to Earth-centred.
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
    // Handing over and taking back (`handed`), and what is shown of its own.
    static constexpr double blend_s = 0.5;
    bool ai_flying_ = false;
    bool resuming_ = false;
    std::uint32_t resumed_from_ = 0;
    double settled_at_s_ = 0.0;
    bool switching_ = false;
    glideslope::net::Interpolated own_shown_;
    std::optional<std::array<double, 3>> shown_before_;
    std::optional<std::array<double, 3>> shown_before_before_;
    std::array<double, 3> blend_{};
    double blend_from_s_ = -1.0e9;
    int frames_since_switch_ = 1000;
    std::size_t handed_over_ = 0;
    std::size_t taken_back_ = 0;
    double worst_step_at_switch_m_ = 0.0;
    double worst_step_otherwise_m_ = 0.0;
    double longest_between_frames_s_ = 0.0;
    std::optional<double> reconciled_s_;
    std::map<std::uint32_t, std::array<double, 3>> predicted_at_;
    std::size_t compared_ = 0;
    std::vector<std::uint32_t> joining_;
    bool answered_ = false;
    double worst_error_m_ = 0.0;
    double worst_error_at_s_ = 0.0;
    std::size_t corrections_ = 0;
    std::size_t snapped_ = 0;
    double worst_correction_m_ = 0.0;
};

int stay(glideslope::platform::UdpSocket& socket,
         const glideslope::platform::Address& server, glideslope::net::Sealer& sealer,
         glideslope::net::Unsealer& unsealer, double seconds,
         std::span<const std::uint8_t> initiation_again, bool fly, const std::string& me,
         const std::string& heard_file, int until_flying_again,
         bool predict, const std::string& track_file, double hand_over_at_s,
         double take_back_at_s) {
    // A client that predicts flies a pilot of its own (Predicting::pilot).
    std::optional<Predicting> predicting;
    if (predict) {
        predicting.emplace();
        fly = true;
    }
    // **What must arrive**: the server's reliable messages, acknowledged,
    // and what each aircraft is, said as it is heard.
    glideslope::net::Reliable reliable;
    std::uint8_t mine = glideslope::net::no_aircraft;
    bool asked_to_hand_over = false;
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
    glideslope::net::InputSender sending;
    std::uint32_t sequence = 0;
    double sent_inputs_at_s = -1.0;
    std::uint32_t applied = 0;
    double roll_seen_deg = 0.0;
    double last_heard_s = 0.0;
    bool drained = true;
    for (;;) {
        const double up_s =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
                .count();
        if (until_flying_again > 0 && flown_again >= until_flying_again) {
            break;
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
        if (mine != glideslope::net::no_aircraft) {
            if (hand_over_at_s >= 0.0 && up_s >= hand_over_at_s && !asked_to_hand_over) {
                asked_to_hand_over = true;
                ask(glideslope::net::Controller::ai);
            }
            if (take_back_at_s >= 0.0 && up_s >= take_back_at_s && !asked_to_take_back) {
                asked_to_take_back = true;
                ask(glideslope::net::Controller::person);
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
        if (!glideslope::net::read_envelope(r, envelope, why) ||
            envelope.type != glideslope::net::Type::sealed) {
            continue;
        }
        const auto opened = unsealer.open(
            std::span<const std::uint8_t>(into.data(), got)
                .subspan(glideslope::net::envelope_size));
        if (!opened) {
            continue;
        }
        const std::span<const std::uint8_t> inside(opened->data(), opened->size());
        // **Where everybody is.** Nothing is done with it here beyond
        // counting and printing it: this is the command-line tool, and it has
        // no sky to draw them in.
        if (const auto state = glideslope::net::read_state(inside)) {
            ++heard;
            if (predicting && !finishing) {
                predicting->heard(*state, up_s);
            }
            if (track_out) {
                for (const glideslope::net::AircraftState& a : state->aircraft) {
                    track_out << "heard " << state->simulation_time_s << ' '
                              << static_cast<unsigned>(a.index) << ' ' << a.x_m << ' '
                              << a.y_m << ' ' << a.z_m << '\n';
                }
            }
            applied = state->last_input_applied;
            mine = state->your_aircraft;
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
                    if (a.condition == glideslope::net::Condition::wrecked) {
                        say_heard("aircraft " + std::to_string(a.index) + " is a wreck");
                    } else if (was != heard_as.end()) {
                        say_heard("aircraft " + std::to_string(a.index) + " flies again");
                        ++flown_again;
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
                glideslope::net::AircraftDefinition d;
                if (glideslope::net::read(
                        std::span<const std::uint8_t>(message.data(), message.size()), d)) {
                    say_heard("aircraft " + std::to_string(d.aircraft) + " is " + d.id);
                    if (predicting) {
                        predicting->introduced(d.aircraft, d.model);
                    }
                }
                glideslope::net::ControllerSwap swap;
                if (glideslope::net::read(
                        std::span<const std::uint8_t>(message.data(), message.size()), swap)) {
                    const bool to_ai = swap.to == glideslope::net::Controller::ai;
                    say_heard("aircraft " + std::to_string(swap.aircraft) +
                              (to_ai ? " handed to the AI" : " handed to its pilot"));
                    if (predicting && swap.aircraft == mine) {
                        predicting->handed(to_ai, up_s, sequence);
                    }
                }
            }
            continue;
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
    std::printf("stayed %.1f s, answered %d ping%s and heard %d state update%s\n",
                seconds, answered, answered == 1 ? "" : "s", heard,
                heard == 1 ? "" : "s");
    if (heard > 0) {
        std::printf("state updates from step %lld to step %lld of the simulation\n",
                    first_step, last_step);
    }
    if (fly) {
        std::printf("sent %u input frames, the server applied %u\n", sequence,
                    applied);
        std::printf("my aircraft rolled to %.0f degrees\n", roll_seen_deg);
    }
    if (predicting) {
        for (const std::string& line : predicting->report()) {
            say_heard(line);
        }
    }
    return answered > 0 ? 0 : 1;
}

int connect_to(const std::string& where, const std::string& key_hex, double stay_s,
               bool again, bool fly, double after_s, const std::string& secret_hex = "",
               const std::string& heard_file = "", int until_flying_again = 0,
               const std::string& ready_file = "", bool predict = false,
               const std::string& track_file = "", double hand_over_at_s = -1.0,
               double take_back_at_s = -1.0) {
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
                        std::fprintf(stderr,
                                     "glideslope_cli: the answer did not open\n");
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
                    if (stay_s <= 0.0) {
                        return 0;
                    }
                    return stay(*socket, *address, sealer, unsealer, stay_s,
                                again ? std::span<const std::uint8_t>(first.data(),
                                                                     first.size())
                                      : std::span<const std::uint8_t>(),
                                fly, mine.publik.text().substr(0, 8), heard_file,
                                until_flying_again, predict, track_file, hand_over_at_s,
                                take_back_at_s);
                }
            }
        }
        if (waited > give_up_after_s) {
            std::fprintf(stderr, "glideslope_cli: no answer from %s\n", where.c_str());
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
    std::vector<std::string_view> args(argv + 1, argv + argc);
    try {
        std::filesystem::path data;
        if (args.size() >= 2 && args[0] == "--data") {
            data = std::filesystem::path(std::string(args[1]));
            args.erase(args.begin(), args.begin() + 2);
        } else {
            data = glideslope::platform::data_directory();
        }

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
        if (args.size() >= 3 && args.size() <= 24 && args[0] == "connect") {
            double stay_s = 0.0;
            bool again = false;
            bool fly = false;
            double after_s = 0.0;
            std::string secret_hex;
            std::string heard_file;
            int until_flying_again = 0;
            std::string ready_file;
            bool predict = false;
            double hand_over_at_s = -1.0;
            double take_back_at_s = -1.0;
            std::string track_file;
            for (std::size_t i = 3; i < args.size(); ++i) {
                if (args[i] == "--track" && i + 1 < args.size()) {
                    track_file = std::string(args[i + 1]);
                    ++i;
                    continue;
                }
                if (args[i] == "--predict") {
                    predict = true;
                    continue;
                }
                if (args[i] == "--hand-over-at" && i + 1 < args.size()) {
                    hand_over_at_s = std::strtod(std::string(args[i + 1]).c_str(), nullptr);
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
            return connect_to(std::string(args[1]), std::string(args[2]), stay_s,
                              again, fly, after_s, secret_hex, heard_file,
                              until_flying_again, ready_file, predict, track_file,
                              hand_over_at_s, take_back_at_s);
        }
        if (args.size() == 1 && args[0] == "air") {
            return air();
        }
        if (args.size() == 4 && args[0] == "sky") {
            return sky(data, args[1], args[2], args[3]);
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
