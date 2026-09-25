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
#include <utility>
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
    // Headers to send, name and value. A terrain provider's own asks for
    // them: Cesium ion authorises a tile request with one, and a language
    // model's API another. A name or value holding a control character is
    // refused - the request is not made, and HttpError says why - so that
    // nothing can be smuggled into the request by splitting a header across
    // lines (`refuse_unsafe_headers`).
    std::vector<std::pair<std::string, std::string>> headers;
    std::uint64_t max_body = std::uint64_t{64} << 20;
    int connect_timeout_seconds = 30;
    // A transfer stalled this long, with no bytes arriving, is abandoned.
    int stall_timeout_seconds = 60;
};

struct HttpResponse {
    int status = 0;
    // Header names in lower case. Of the final response, after any redirects.
    //
    // **`content-encoding` is not here, and `content-length` is the body's.**
    // The body below is what was sent, undone: every backend asks for and
    // undoes whatever compression it understands, because a server may
    // compress a body whether it was asked to or not - Cesium ion serves its
    // layer.json gzipped either way. A `content-encoding` naming what was
    // already undone would be a lie, so it is taken off; a `content-length`
    // counting the compressed bytes would be another, so it is made to count
    // what `body` actually holds.
    std::map<std::string, std::string> headers;
    std::vector<std::uint8_t> body;
};

// Throws HttpError if a header's name or value holds a control character.
// Every backend calls it before a request is made.
inline void refuse_unsafe_headers(const HttpRequest& request) {
    const auto unsafe = [](const std::string& s) {
        for (const char c : s) {
            const auto u = static_cast<unsigned char>(c);
            if (u < 0x20 || u == 0x7F) {
                return true;
            }
        }
        return false;
    };
    for (const auto& [name, value] : request.headers) {
        if (unsafe(name) || unsafe(value)) {
            // The value is not repeated: it may be a key.
            throw HttpError(request.url + ": the header " +
                            (unsafe(name) ? std::string("named with a control character")
                                          : name) +
                            " holds a control character");
        }
    }
}

// A GET, following redirects. Throws HttpError.
HttpResponse http_get(const HttpRequest& request);

// A POST of `body`, as it is: its Content-Type is one of the request's
// headers. What a language model's API is asked with. Throws HttpError.
//
// **A POST follows no redirect**: a redirect is the response. A request that
// carries a key in a header would carry it to wherever it was sent on, and
// not every client drops a header of the caller's own when the host changes -
// libcurl drops only the Authorization it made itself.
HttpResponse http_post(const HttpRequest& request, const std::string& body);

// What carries the requests here, for diagnostics: "WinHTTP", "NSURLSession",
// or "libcurl" and its version. Throws HttpError if there is nothing.
std::string http_client();

} // namespace glideslope::platform
