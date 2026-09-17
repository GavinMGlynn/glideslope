// glideslope_cli - the simulation with no window.
//
// It links the simulation and nothing presentational, which is what makes it
// the proof that the simulation can run where there is no window: in CI, in a
// test, and inside the server.

#include "platform/paths.hpp"
#include "sim/aircraft.hpp"
#include "sim/figures.hpp"
#include "sim/version.hpp"

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>

namespace {

void print_usage(std::FILE* out) {
    std::fputs(
        "usage: glideslope_cli --version\n"
        "       glideslope_cli --help\n"
        "       glideslope_cli aircraft NAME   print what NAME's model files say\n"
        "       glideslope_cli figures NAME [FIGURE]\n"
        "                                      fly NAME's published figures, or one,\n"
        "                                      and exit 1 if any lands out of range\n",
        out);
}

int print_aircraft(const std::string& model) {
    const auto root = glideslope::platform::data_directory() / "jsbsim";
    const glideslope::sim::Aircraft aircraft(root, model);
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

int fly_figures(const std::string& model, const std::string& only) {
    const auto data = glideslope::platform::data_directory();
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

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            const std::string_view arg = argv[1];
            if (arg == "--version") {
                const std::string_view v = glideslope::sim::version();
                std::printf("glideslope_cli %.*s\n", static_cast<int>(v.size()),
                            v.data());
                return 0;
            }
            if (arg == "--help") {
                print_usage(stdout);
                return 0;
            }
        }
        if (argc == 3 && std::string_view(argv[1]) == "aircraft") {
            return print_aircraft(argv[2]);
        }
        if ((argc == 3 || argc == 4) && std::string_view(argv[1]) == "figures") {
            return fly_figures(argv[2], argc == 4 ? argv[3] : "");
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
