// glideslope_cli - the simulation with no window.
//
// It links the simulation and nothing presentational, which is what makes it
// the proof that the simulation can run where there is no window: in CI, in a
// test, and inside the server.

#include "platform/http.hpp"
#include "platform/paths.hpp"
#include "sim/aircraft.hpp"
#include "sim/figures.hpp"
#include "sim/selftest.hpp"
#include "sim/version.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"

#include <algorithm>
#include <cinttypes>
#include <cmath>
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
        "  aircraft NAME             print what NAME's model files say\n"
        "  figures NAME [FIGURE]     fly NAME's published figures, or one, and exit 1\n"
        "                            if any lands out of range\n"
        "  selftest [NAME]           fly NAME's selftest input log (default c172p) "
        "and\n"
        "                            print the hash of its state\n"
        "  height LAT LON            the ground's height there, from the Copernicus "
        "DEM,\n"
        "                            fetching what it needs into the cache directory\n"
        "\n"
        "  --data DIR                read data from DIR instead of data/ beside the\n"
        "                            program\n",
        out);
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
        if (args.size() == 2 && args[0] == "aircraft") {
            return print_aircraft(data, std::string(args[1]));
        }
        if ((args.size() == 2 || args.size() == 3) && args[0] == "figures") {
            return fly_figures(data, std::string(args[1]),
                               args.size() == 3 ? std::string(args[2]) : "");
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
