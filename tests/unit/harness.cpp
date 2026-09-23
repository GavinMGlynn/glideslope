#include "harness.hpp"
#include "platform/no_crash_dialogs.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>

namespace glideslope::test {

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

std::string environment(const char* name) {
#if defined(_MSC_VER)
    // MSVC's getenv is deprecated in favour of this.
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
        return {};
    }
    std::string out(value);
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
#endif
}

void fail(const std::string& message, std::source_location where) {
    throw Failure{std::string(where.file_name()) + ":" + std::to_string(where.line()) +
                  ": " + message};
}

} // namespace glideslope::test

// glideslope_tests --list        every test's name, one per line
// glideslope_tests NAME          run that test; exit 0 if it passes
int main(int argc, char** argv) {
    // First: a failed assert prints and ends the program rather than
    // waiting on a dialog nobody will answer (platform/no_crash_dialogs.hpp).
    glideslope::platform::no_crash_dialogs();
    using glideslope::test::registry;
    if (argc == 2 && std::string_view(argv[1]) == "--list") {
        for (const auto& t : registry()) {
            std::printf("%.*s\n", static_cast<int>(t.name.size()), t.name.data());
        }
        return 0;
    }
    if (argc != 2) {
        std::fputs("usage: glideslope_tests --list | NAME\n", stderr);
        return 2;
    }
    const std::string_view wanted = argv[1];
    for (const auto& t : registry()) {
        if (t.name != wanted) {
            continue;
        }
        try {
            t.body();
            return 0;
        } catch (const glideslope::test::Skip& s) {
            std::fprintf(stderr, "SKIPPED %s\n  %s\n", argv[1], s.reason.c_str());
            return 77;
        } catch (const glideslope::test::Failure& f) {
            std::fprintf(stderr, "FAILED %s\n  %s\n", argv[1], f.message.c_str());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "FAILED %s\n  threw: %s\n", argv[1], e.what());
        }
        return 1;
    }
    std::fprintf(stderr, "no test named %s\n", argv[1]);
    return 2;
}
