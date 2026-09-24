// glideslope_cli - the simulation with no window.
//
// It links the simulation and nothing presentational, which is what makes it
// the proof that the simulation can run where there is no window: in CI, in a
// test, and inside the server.

#include "net/handshake.hpp"
#include "platform/end_process.hpp"
#include "platform/no_crash_dialogs.hpp"
#include "net/inputs.hpp"
#include "net/inside.hpp"
#include "net/keys.hpp"
#include "net/protocol.hpp"
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
        "                            --key HEX connects as the player whose secret\n"
        "                            key that is, rather than a new one\n"
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

int stay(glideslope::platform::UdpSocket& socket,
         const glideslope::platform::Address& server, glideslope::net::Sealer& sealer,
         glideslope::net::Unsealer& unsealer, double seconds,
         std::span<const std::uint8_t> initiation_again, bool fly, const std::string& me) {
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
    for (;;) {
        const double up_s =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
                .count();
        if (up_s >= seconds) {
            break;
        }
        if (fly && up_s - sent_inputs_at_s >= inputs_every_s) {
            sent_inputs_at_s = up_s;
            ++sequence;
            sending.add(sequence, glideslope::net::as_sent(stick.as_list()));
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

        glideslope::platform::Address from;
        const std::size_t got = socket.receive(into, from);
        if (got <= glideslope::net::envelope_size) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
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
            applied = state->last_input_applied;
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
                        std::fprintf(stderr, "client %s: aircraft %u is a wreck\n", me.c_str(),
                                     static_cast<unsigned>(a.index));
                    } else if (was != heard_as.end()) {
                        std::fprintf(stderr, "client %s: aircraft %u flies again\n",
                                     me.c_str(), static_cast<unsigned>(a.index));
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
    return answered > 0 ? 0 : 1;
}

int connect_to(const std::string& where, const std::string& key_hex, double stay_s,
               bool again, bool fly, double after_s, const std::string& secret_hex = "") {
    // **A test flag's work**: join a session that is already running. A
    // client that connects the instant the server does learns nothing about
    // whether the server was flying before it arrived.
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
                    std::fprintf(stderr, "glideslope_cli: refused, reason %u\n",
                                 static_cast<unsigned>(into[glideslope::net::envelope_size]));
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
                                fly, mine.publik.text().substr(0, 8));
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
        if (args.size() >= 3 && args.size() <= 10 && args[0] == "connect") {
            double stay_s = 0.0;
            bool again = false;
            bool fly = false;
            double after_s = 0.0;
            std::string secret_hex;
            for (std::size_t i = 3; i < args.size(); ++i) {
                if (args[i] == "--key" && i + 1 < args.size()) {
                    secret_hex = std::string(args[i + 1]);
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
                              again, fly, after_s, secret_hex);
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
