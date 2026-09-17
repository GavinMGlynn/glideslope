#include "harness.hpp"

#include "platform/http.hpp"
#include "world/digest.hpp"

#include <cstdlib>
#include <string>

using glideslope::platform::http_get;
using glideslope::platform::HttpError;
using glideslope::platform::HttpRequest;
using glideslope::platform::HttpResponse;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

bool network_required() {
    const char* v = std::getenv("GLIDESLOPE_REQUIRE_NETWORK");
    return v != nullptr && v[0] != '\0';
}

// The first request of a test: without the network the test cannot run, and is
// skipped - unless the network is required, as in CI.
HttpResponse first_get(const HttpRequest& request) {
    try {
        return http_get(request);
    } catch (const HttpError& e) {
        if (!network_required()) {
            glideslope::test::skip(std::string("no network here: ") + e.what());
        }
        throw;
    }
}

HttpRequest get(const std::string& url) {
    HttpRequest r;
    r.url = url;
    return r;
}

} // namespace

GLIDESLOPE_TEST(an_https_get_fetches_a_pinned_file_byte_for_byte) {
    const HttpResponse r =
        first_get(get("https://copernicus-dem-30m.s3.amazonaws.com/tileList.txt"));
    std::printf("fetched through %s\n", glideslope::platform::http_client().c_str());
    check(r.status == 200, "status 200, not " + std::to_string(r.status));
    check(r.body.size() == 1110900,
          "1110900 bytes, not " + std::to_string(r.body.size()));
    check(glideslope::world::sha256_hex(r.body) ==
              "10604e3052c98a09e9216f1a8f0a555a04148419757575f783d4937fd44316dc",
          "the pinned SHA-256");
    check(r.headers.count("content-length") == 1 &&
              r.headers.at("content-length") == "1110900",
          "headers are read, with lower-case names");
    // S3's ETag for a file uploaded whole is its MD5 - what a DEM download is
    // checked against.
    check(r.headers.count("etag") == 1 &&
              r.headers.at("etag") == "\"" + glideslope::world::md5_hex(r.body) + "\"",
          "the ETag is the body's MD5");
}

GLIDESLOPE_TEST(an_https_get_follows_redirects_to_the_file) {
    // GitHub answers a raw file with a redirect to another host. This is the
    // commit that added the licence, which is unchanged since.
    const HttpResponse r =
        first_get(get("https://github.com/GavinMGlynn/glideslope/raw/"
                      "966bb9f5e826bd653f05ae97f10d5bcd432596fa/LICENSE"));
    check(r.status == 200,
          "status 200 after the redirect, not " + std::to_string(r.status));
    check(glideslope::world::sha256_hex(r.body) ==
              "3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986",
          "the licence, byte for byte");
}

GLIDESLOPE_TEST(an_http_error_status_is_a_response_and_a_failed_request_is_an_error) {
    const HttpResponse missing =
        first_get(get("https://copernicus-dem-30m.s3.amazonaws.com/no-such-file-here"));
    check(missing.status == 404,
          "a missing file is status 404, not " + std::to_string(missing.status));

    HttpRequest small = get("https://copernicus-dem-30m.s3.amazonaws.com/tileList.txt");
    small.max_body = 1000;
    try {
        http_get(small);
        fail("a body over its limit was accepted");
    } catch (const HttpError& e) {
        check(std::string(e.what()).find("more than 1000 bytes") != std::string::npos,
              std::string("refused for its size, not: ") + e.what());
    }

    try {
        http_get(get("https://glideslope-no-such-host.invalid/"));
        fail("a host that cannot exist answered");
    } catch (const HttpError&) {
    }
}
