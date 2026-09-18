// glideslope_cli - the simulation with no window.
//
// It links the simulation and nothing presentational, which is what makes it
// the proof that the simulation can run where there is no window: in CI, in a
// test, and inside the server.

#include "platform/http.hpp"
#include "platform/paths.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/figures.hpp"
#include "sim/selftest.hpp"
#include "sim/version.hpp"
#include "world/dem.hpp"
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
#include <stdexcept>
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
        std::printf("%-12s %-28s model %s, starting at %.0f KCAS%s%s\n", e.id.c_str(),
                    e.name.c_str(), e.model.c_str(), e.start_airspeed_kts,
                    figures ? ", published figures" : "",
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
            std::exit(2);
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
            std::exit(2);
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

int main(int argc, char** argv) {
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
