#include "harness.hpp"

#include "platform/paths.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using glideslope::platform::read_default_server;
using glideslope::test::check;

namespace {

const char* a_key() {
    return "49cf887bbb3a100176a197ad23dd9ddf835f42a985cd7e3bebc5421b8305fb75";
}

} // namespace

// **A `server.txt` names a host, a port and a key**, in that order, on one
// line. That is what `--online` reads and all a client needs: the key in it is
// the half a server prints at startup, so the file may be passed around.
GLIDESLOPE_TEST(a_server_txt_names_a_host_a_port_and_a_key) {
    const std::string line = std::string("glideslope.example.org 47801 ") + a_key();
    const auto got = read_default_server(line);
    check(got.has_value(), "a plain line is read");
    check(got->host == "glideslope.example.org", "the host is kept");
    check(got->port == 47801, "and the port, not " + std::to_string(got->port));
    check(got->key_hex == a_key(), "and the key");

    // A trailing newline, a carriage return, and leading comment lines: all
    // things a file that was e-mailed or checked in will have.
    const auto crlf = read_default_server(line + "\r\n");
    check(crlf.has_value() && crlf->port == 47801, "a CRLF line ending is read");
    const auto commented = read_default_server(
        std::string("# the club's server, from Gavin\n\n") + line + "\n");
    check(commented.has_value() && commented->host == "glideslope.example.org",
          "comments and blank lines are skipped");

    // The first line that is one wins, so a file may keep old servers below.
    const auto first = read_default_server(
        line + "\nother.example.org 47802 " + a_key() + "\n");
    check(first.has_value() && first->host == "glideslope.example.org",
          "the first line that is a server is the one used");

    // An upper-case key is read, because somebody will paste one.
    std::string shouted = a_key();
    for (char& c : shouted) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    check(read_default_server("h 1 " + shouted).has_value(),
          "an upper-case key is still a key");
}

// **Every way a line can fail to be one is refused**, walked and counted, so
// a `--online` that found a file would never connect somewhere it was not
// told to.
GLIDESLOPE_TEST(anything_that_is_not_a_server_line_is_refused) {
    const std::string key = a_key();
    const std::vector<std::pair<std::string, std::string>> wrong = {
        {"nothing at all", ""},
        {"only a host", "host\n"},
        {"a host and a port", "host 47801\n"},
        {"a fourth word", "host 47801 " + key + " extra\n"},
        {"a port that is not a number", "host forty " + key + "\n"},
        {"a port of nought", "host 0 " + key + "\n"},
        {"a port past 65535", "host 70000 " + key + "\n"},
        {"a negative port", "host -1 " + key + "\n"},
        {"a key that is too short", "host 47801 " + key.substr(0, 63) + "\n"},
        {"a key that is too long", "host 47801 " + key + "a\n"},
        {"a key that is not hexadecimal", "host 47801 " + key.substr(0, 63) + "z\n"},
        {"only comments", "# a comment\n# another\n"},
        {"only blank lines", "\n\n   \n"},
    };
    std::size_t refused = 0;
    for (const auto& [what, text] : wrong) {
        check(!read_default_server(text).has_value(),
              std::string("a file with ") + what + " names no server");
        ++refused;
    }
    check(refused == wrong.size(), "every way was walked");
    check(refused == 13, "thirteen ways, and the list above holds thirteen");

    // And a file whose good line comes after a bad one still works: a broken
    // line is skipped, not fatal.
    check(read_default_server("host 0 " + key + "\nhost 47801 " + key + "\n")
              .has_value(),
          "a line that is not one does not spoil the file");
}

namespace {

// Sets or clears an environment variable, on every platform.
void set_environment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str()); // an empty value removes it
#else
    if (value.empty()) {
        unsetenv(name);
    } else {
        setenv(name, value.c_str(), 1);
    }
#endif
}

} // namespace

// **The user's token goes wherever GLIDESLOPE_CESIUM_ION_API names**, in the
// query of each request, so only https - or http no further than this machine -
// may be named. A host that merely begins like the loopback is not it.
GLIDESLOPE_TEST(a_cesium_ion_api_that_would_send_the_token_in_the_clear_is_refused) {
    using glideslope::platform::cesium_ion_api;
    // Each test is a process of its own, so what this sets goes with it.
    const char* variable = "GLIDESLOPE_CESIUM_ION_API";

    set_environment(variable, "");
    check(cesium_ion_api() == "https://api.cesium.com", "unset, it is Cesium's own");

    const std::vector<std::pair<std::string, std::string>> taken{
        {"https://ion.example.org/", "https://ion.example.org"},
        {"http://127.0.0.1:4711", "http://127.0.0.1:4711"},
        {"http://127.0.0.1/", "http://127.0.0.1"},
        {"http://localhost:8080/", "http://localhost:8080"},
        {"http://[::1]:9", "http://[::1]:9"},
    };
    const std::vector<std::string> refused{
        "http://api.cesium.com",
        "http://127.0.0.1.example.org",
        "http://localhost.example.org:80",
        "ftp://127.0.0.1",
        "api.cesium.com",
    };
    std::size_t walked = 0;
    for (const auto& [named, is] : taken) {
        set_environment(variable, named);
        check(cesium_ion_api() == is, named + " is taken, as " + is);
        ++walked;
    }
    for (const std::string& named : refused) {
        set_environment(variable, named);
        bool threw = false;
        try {
            (void)cesium_ion_api();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, named + " is refused");
        ++walked;
    }
    set_environment(variable, "");
    check(walked == taken.size() + refused.size() && walked == 10,
          "ten names walked, five taken and five refused");
}
