// glideslope_cli - the simulation with no window.
//
// It links the simulation and nothing presentational, which is what makes it
// the proof that the simulation can run where there is no window: in CI, in a
// test, and inside the server. Today it only reports its version; Phase 1 gives
// it a Cessna 172 to fly.

#include "sim/version.hpp"

#include <cstdio>
#include <string_view>

namespace {

void print_usage(std::FILE* out) {
    std::fputs("usage: glideslope_cli --version\n"
               "       glideslope_cli --help\n",
               out);
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string_view arg = argv[1];
        if (arg == "--version") {
            const std::string_view v = glideslope::sim::version();
            std::printf("glideslope_cli %.*s\n", static_cast<int>(v.size()), v.data());
            return 0;
        }
        if (arg == "--help") {
            print_usage(stdout);
            return 0;
        }
    }

    // No arguments, too many, or one it does not know: say how to use it, on
    // the error stream, and fail, so a script that typed it wrong finds out.
    print_usage(stderr);
    return 2;
}
