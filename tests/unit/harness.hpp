#pragma once

// The unit-test harness: a named list of functions, one executable, and one
// ctest per name.
//
// Kept this small on purpose - a registry, a failure that says where it
// happened, and a main that runs one test by name - because the project prefers
// no dependency to a small one, and a test framework is a dependency every
// platform has to build.

#include <functional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace glideslope::test {

struct TestCase {
    std::string_view name;
    std::function<void()> body;
};

std::vector<TestCase>& registry();

struct Register {
    Register(std::string_view name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

// Thrown by a failed check; the harness reports it and fails the test.
struct Failure {
    std::string message;
};

[[noreturn]] void fail(const std::string& message,
                       std::source_location where = std::source_location::current());

inline void check(bool condition, std::string_view what,
                  std::source_location where = std::source_location::current()) {
    if (!condition) {
        fail(std::string(what), where);
    }
}

} // namespace glideslope::test

// Defines a test. The name is a sentence stating the fact the test pins.
#define GLIDESLOPE_TEST(name)                                                          \
    static void name();                                                                \
    static const ::glideslope::test::Register name##_registration(#name, &name);       \
    static void name()
