#pragma once

// HTTPS requests, through what each operating system already trusts: WinHTTP on
// Windows, NSURLSession on macOS, and on Linux the system's libcurl, loaded
// when first needed so that nothing about it is needed to build or to start.
// Each uses the system's certificate store and proxy settings, and is updated
// with the system.

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace glideslope::platform {

// The request could not be made or completed: no network, no such host, a TLS
// failure, a timeout, a body over its limit, or no HTTP client on this system.
// An HTTP error status is not an HttpError; it is a response.
struct HttpError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct HttpRequest {
    std::string url;
    std::string user_agent = "glideslope";
    std::uint64_t max_body = std::uint64_t{64} << 20;
    int connect_timeout_seconds = 30;
    // A transfer stalled this long, with no bytes arriving, is abandoned.
    int stall_timeout_seconds = 60;
};

struct HttpResponse {
    int status = 0;
    // Header names in lower case. Of the final response, after any redirects.
    std::map<std::string, std::string> headers;
    std::vector<std::uint8_t> body;
};

// A GET, following redirects. Throws HttpError.
HttpResponse http_get(const HttpRequest& request);

// What carries the requests here, for diagnostics: "WinHTTP", "NSURLSession",
// or "libcurl" and its version. Throws HttpError if there is nothing.
std::string http_client();

} // namespace glideslope::platform
