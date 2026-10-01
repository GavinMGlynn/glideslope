#include "harness.hpp"

#include "platform/http.hpp"
#include "platform/paths.hpp"
#include "world/digest.hpp"
#include "world/json.hpp"

#include <atomic>
#include <cstring>
#include <functional>
#include <string>
#include <thread>

#ifdef __linux__
#include <csignal>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using glideslope::platform::http_get;
using glideslope::platform::http_post;
using glideslope::platform::HttpError;
using glideslope::platform::HttpRequest;
using glideslope::platform::HttpResponse;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

// The first request of a test: without the network the test cannot run, and is
// skipped - unless the network is required, as in CI.
HttpResponse first_get(const HttpRequest& request) {
    try {
        return http_get(request);
    } catch (const HttpError& e) {
        if (!glideslope::test::network_required()) {
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

    HttpRequest limited = get("https://copernicus-dem-30m.s3.amazonaws.com/tileList.txt");
    limited.max_body = 1000;
    try {
        http_get(limited);
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

GLIDESLOPE_TEST(a_download_cannot_be_killed_by_a_server_hanging_up) {
#ifdef __linux__
    // libcurl's requests are made with CURLOPT_NOSIGNAL, so it leaves SIGPIPE
    // - raised by TLS writing to a connection the server has closed - to the
    // program. Once downloading, the program ignores it.
    try {
        http_get(get("https://glideslope-no-such-host.invalid/"));
    } catch (const HttpError&) {
    }
    struct sigaction now{};
    sigaction(SIGPIPE, nullptr, &now);
    check(now.sa_handler == SIG_IGN, "SIGPIPE is ignored once downloads are made");
#else
    glideslope::test::skip("SIGPIPE is libcurl's on Linux; elsewhere the system's "
                           "HTTP does not raise it");
#endif
}

namespace {

#ifdef _WIN32
using Socket = SOCKET;
using Length = int; // what Winsock's send and recv count in
constexpr Socket no_socket = INVALID_SOCKET;
void close_socket(Socket s) {
    ::closesocket(s);
}
#else
using Socket = int;
using Length = std::size_t;
constexpr Socket no_socket = -1;
void close_socket(Socket s) {
    ::close(s);
}
#endif

// **A server for one request**, on the loopback address: it keeps every byte
// of the request it is sent and answers it with `answer`, then closes. What
// the client sent can be checked byte for byte, and nothing leaves the
// machine. The answer may instead be made from the request's head, as a
// server choosing an encoding from Accept-Encoding makes it.
class OneRequestServer {
public:
    using Answer = std::function<std::string(const std::string& head)>;
    explicit OneRequestServer(std::string answer)
        : OneRequestServer(Answer([answer](const std::string&) { return answer; })) {}
    explicit OneRequestServer(Answer answer) : answer_(std::move(answer)) {
#ifdef _WIN32
        WSADATA data;
        ::WSAStartup(MAKEWORD(2, 2), &data);
#endif
        listening_ = ::socket(AF_INET, SOCK_STREAM, 0);
        check(listening_ != no_socket, "a socket to listen on");
        sockaddr_in at{};
        at.sin_family = AF_INET;
        at.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        at.sin_port = 0;
        check(::bind(listening_, reinterpret_cast<const sockaddr*>(&at), sizeof at) == 0 &&
                  ::listen(listening_, 1) == 0,
              "listening on the loopback address");
        socklen_t size = sizeof at;
        ::getsockname(listening_, reinterpret_cast<sockaddr*>(&at), &size);
        port_ = ntohs(at.sin_port);
        thread_ = std::thread([this] { serve(); });
    }
    ~OneRequestServer() {
        // A server never asked is still waiting to accept: shut down, the
        // wait ends, where closing alone would not end it on Linux.
#ifndef _WIN32
        ::shutdown(listening_, SHUT_RDWR);
#endif
        close_socket(listening_);
        thread_.join();
    }
    bool asked() const {
        return !head_.empty();
    }
    OneRequestServer(const OneRequestServer&) = delete;
    OneRequestServer& operator=(const OneRequestServer&) = delete;

    std::string url(const std::string& path) const {
        return "http://127.0.0.1:" + std::to_string(port_) + path;
    }
    // The request as it arrived: its head, and its body.
    std::string head() const {
        return head_;
    }
    std::string body() const {
        return body_;
    }

private:
    void serve() {
        const Socket client = ::accept(listening_, nullptr, nullptr);
        if (client == no_socket) {
            return;
        }
        std::string got;
        char buffer[4096];
        std::size_t wanted = std::string::npos;
        for (;;) {
            const auto n = ::recv(client, buffer, static_cast<Length>(sizeof buffer), 0);
            if (n <= 0) {
                break;
            }
            got.append(buffer, static_cast<std::size_t>(n));
            const std::size_t end = got.find("\r\n\r\n");
            if (end != std::string::npos && wanted == std::string::npos) {
                head_ = got.substr(0, end);
                wanted = end + 4 + content_length(head_);
            }
            if (wanted != std::string::npos && got.size() >= wanted) {
                break;
            }
        }
        if (wanted != std::string::npos) {
            body_ = got.substr(head_.size() + 4);
        }
        const std::string answer = answer_(head_);
        std::size_t sent = 0;
        while (sent < answer.size()) {
            const auto n = ::send(client, answer.data() + sent,
                                  static_cast<Length>(answer.size() - sent), 0);
            if (n <= 0) {
                break;
            }
            sent += static_cast<std::size_t>(n);
        }
        close_socket(client);
    }

    static std::size_t content_length(const std::string& head) {
        std::string lower;
        for (const char c : head) {
            lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        const std::size_t at = lower.find("\r\ncontent-length:");
        return at == std::string::npos
                   ? 0
                   : static_cast<std::size_t>(std::strtoull(lower.c_str() + at + 17, nullptr, 10));
    }

    Answer answer_;
    Socket listening_ = no_socket;
    unsigned short port_ = 0;
    std::thread thread_;
    std::string head_;
    std::string body_;
};

} // namespace

GLIDESLOPE_TEST(a_post_sends_its_body_and_headers_byte_for_byte_and_reads_the_answer) {
    // A body of every byte value but none of the HTTP layer's business - a
    // JSON request as a language model's API is asked, with a zero byte and
    // a character outside ASCII in it for good measure.
    std::string body = "{\"input\":\"take off, climb to 3,000 ft and orbit the CBD\"}";
    body += std::string(1, '\0');
    body += "\xC3\xA9";
    const std::string answer_body = "{\"output\":\"a plan\"}";
    OneRequestServer server("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                            "Content-Length: " +
                            std::to_string(answer_body.size()) +
                            "\r\nConnection: close\r\n\r\n" + answer_body);
    HttpRequest request = get(server.url("/v1/responses"));
    request.headers = {{"Content-Type", "application/json"},
                       {"Authorization", "Bearer not-a-real-key"}};
    const HttpResponse r = http_post(request, body);

    check(server.head().rfind("POST /v1/responses HTTP/1.1\r\n", 0) == 0,
          "sent as a POST to its path:\n" + server.head());
    for (const char* header : {"\r\nContent-Type: application/json",
                               "\r\nAuthorization: Bearer not-a-real-key"}) {
        check(server.head().find(header) != std::string::npos,
              std::string("sent with its header") + header + ":\n" + server.head());
    }
    check(server.body() == body, "the body arrived byte for byte: " +
                                     std::to_string(server.body().size()) + " bytes of " +
                                     std::to_string(body.size()));
    check(r.status == 200 &&
              std::string(r.body.begin(), r.body.end()) == answer_body,
          "the answer read back");
}

GLIDESLOPE_TEST(a_post_follows_no_redirect_so_its_key_goes_nowhere_else) {
    // Asked with a key, the server answers "try over there", pointing at a
    // second server. The key must not go there: the redirect is the answer.
    OneRequestServer elsewhere("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    for (const int status : {301, 302, 303, 307, 308}) {
        OneRequestServer asked("HTTP/1.1 " + std::to_string(status) + " Moved\r\nLocation: " +
                               elsewhere.url("/stolen") +
                               "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        HttpRequest request = get(asked.url("/v1/messages"));
        request.headers = {{"x-api-key", "not-a-real-key"}};
        const HttpResponse r = http_post(request, "{}");
        check(r.status == status, "a " + std::to_string(status) + " is the answer, not followed: " +
                                      std::to_string(r.status));
    }
    check(!elsewhere.asked(), "nothing reached the server redirected to: five redirects, none followed");
}

GLIDESLOPE_TEST(a_header_holding_a_control_character_is_refused_before_anything_is_sent) {
    // Nothing listens at this address, so a request that were made would fail
    // for that; refused, it fails first, and says why.
    std::size_t refused = 0;
    for (const auto& [name, value] :
         std::vector<std::pair<std::string, std::string>>{
             {"Authorization", "Bearer key\r\nX-Smuggled: yes"},
             {"X-Line\nBreak", "value"},
             {"Authorization", std::string("Bearer key") + '\x7F'}}) {
        HttpRequest request = get("http://127.0.0.1:9/");
        request.headers = {{name, value}};
        for (const bool post : {false, true}) {
            try {
                if (post) {
                    (void)http_post(request, "{}");
                } else {
                    (void)http_get(request);
                }
                fail("a header holding a control character was sent");
            } catch (const HttpError& e) {
                const std::string why = e.what();
                check(why.find("holds a control character") != std::string::npos,
                      "refused for the header, not: " + why);
                check(why.find("key") == std::string::npos || name != "Authorization",
                      "and without repeating its value: " + why);
                ++refused;
            }
        }
    }
    check(refused == 6, "three headers, each by GET and by POST: six refused");
}

namespace {

// What the compressed answers below hold, before they were compressed.
const std::string plain_body =
    "{\"said\":\"the same bytes, however they travelled\",\"levels\":[1000,975,950,925,"
    "900,850,800,700,600,500]}";

// `plain_body` gzipped (RFC 1952), made by Python's gzip.compress(mtime=0).
const std::uint8_t gzipped_body[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0x0d, 0x88,
    0x41, 0x0a, 0x80, 0x30, 0x0c, 0x04, 0xbf, 0x12, 0x72, 0xee, 0x21, 0x0a,
    0xb5, 0xb5, 0x5f, 0x11, 0x0f, 0x15, 0x03, 0x0a, 0x11, 0xc1, 0x96, 0x8a,
    0x88, 0x7f, 0x77, 0x0f, 0xc3, 0xec, 0xce, 0xcb, 0x25, 0xef, 0x2b, 0x27,
    0xae, 0x9b, 0x52, 0xc9, 0x87, 0xd2, 0xf2, 0x54, 0x2d, 0x8e, 0xb6, 0xf3,
    0xd6, 0xa6, 0x17, 0xa1, 0x3f, 0x54, 0xaf, 0xdc, 0xd4, 0x4c, 0x57, 0x76,
    0x6c, 0xc8, 0x56, 0x38, 0x4d, 0x9d, 0x88, 0xb8, 0x31, 0x78, 0x37, 0x7a,
    0xb8, 0x87, 0xf1, 0x23, 0x76, 0x84, 0x03, 0x18, 0x80, 0x17, 0x99, 0xbf,
    0x1f, 0x05, 0x40, 0x13, 0xae, 0x65, 0x00, 0x00, 0x00};

// `plain_body` as HTTP's "deflate" is defined - the zlib format, RFC 1950,
// header and Adler-32 around the deflate data - made by Python's
// zlib.compress at level 9. Open-Meteo sends this kind of stream.
const std::uint8_t deflated_body[] = {
    0x78, 0xda, 0x0d, 0x88, 0x41, 0x0a, 0x80, 0x30, 0x0c, 0x04, 0xbf, 0x12,
    0x72, 0xee, 0x21, 0x0a, 0xb5, 0xb5, 0x5f, 0x11, 0x0f, 0x15, 0x03, 0x0a,
    0x11, 0xc1, 0x96, 0x8a, 0x88, 0x7f, 0x77, 0x0f, 0xc3, 0xec, 0xce, 0xcb,
    0x25, 0xef, 0x2b, 0x27, 0xae, 0x9b, 0x52, 0xc9, 0x87, 0xd2, 0xf2, 0x54,
    0x2d, 0x8e, 0xb6, 0xf3, 0xd6, 0xa6, 0x17, 0xa1, 0x3f, 0x54, 0xaf, 0xdc,
    0xd4, 0x4c, 0x57, 0x76, 0x6c, 0xc8, 0x56, 0x38, 0x4d, 0x9d, 0x88, 0xb8,
    0x31, 0x78, 0x37, 0x7a, 0xb8, 0x87, 0xf1, 0x23, 0x76, 0x84, 0x03, 0x18,
    0x80, 0x17, 0x99, 0xbf, 0x1f, 0x86, 0x7f, 0x1d, 0x60};

template <std::size_t N>
std::string answer_encoded(const char* encoding, const std::uint8_t (&body)[N]) {
    return std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Encoding: ") +
           encoding + "\r\nContent-Length: " + std::to_string(N) +
           "\r\nConnection: close\r\n\r\n" +
           std::string(reinterpret_cast<const char*>(body), N);
}

// The value of a request header, by its name in lower case, itself lowered;
// empty if the request has none.
std::string request_header(const std::string& head, const std::string& name) {
    std::string lower;
    for (const char c : head) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const std::size_t at = lower.find("\r\n" + name + ":");
    if (at == std::string::npos) {
        return {};
    }
    std::size_t from = at + 2 + name.size() + 1;
    while (from < lower.size() && lower[from] == ' ') {
        ++from;
    }
    return lower.substr(from, lower.find("\r\n", from) - from);
}

void check_plain(const HttpResponse& r, const std::string& what) {
    check(r.status == 200, what + ": status 200, not " + std::to_string(r.status));
    check(std::string(r.body.begin(), r.body.end()) == plain_body,
          what + ": the body as it was before it was compressed, not " +
              std::to_string(r.body.size()) + " bytes beginning with byte " +
              (r.body.empty() ? std::string("none")
                              : std::to_string(static_cast<int>(r.body[0]))));
    check(r.headers.count("content-encoding") == 0,
          what + ": no content-encoding for what was already undone");
    check(r.headers.count("content-length") == 1 &&
              r.headers.at("content-length") == std::to_string(plain_body.size()),
          what + ": a content-length that counts the body handed up");
}

} // namespace

GLIDESLOPE_TEST(a_body_gzipped_unasked_arrives_as_it_was_before_it_was_gzipped) {
    // Cesium ion's layer.json is kept gzipped and served with
    // `Content-Encoding: gzip` whatever the request said, so this server does
    // the same: whatever the client offered, or did not, it gets gzip.
    OneRequestServer server(answer_encoded("gzip", gzipped_body));
    const HttpResponse r = http_get(get(server.url("/layer.json")));
    std::printf("asked with Accept-Encoding \"%s\"\n",
                request_header(server.head(), "accept-encoding").c_str());
    check_plain(r, "gzipped unasked");
}

GLIDESLOPE_TEST(a_server_that_prefers_deflate_when_offered_it_is_still_read) {
    // Open-Meteo's way: offered deflate, it sends deflate - the zlib format,
    // as HTTP defines it - and gzip only when gzip is all that is offered,
    // and nothing compressed when nothing is. Whatever a client offers it
    // must be able to undo. WinHTTP's own deflate decoder cannot undo the
    // zlib format, and fails the read with E_ABORT: that is what made every
    // Open-Meteo fetch fail on Windows when decompression was first turned on.
    OneRequestServer server(OneRequestServer::Answer([](const std::string& head) {
        const std::string offered = request_header(head, "accept-encoding");
        if (offered.find("deflate") != std::string::npos) {
            return answer_encoded("deflate", deflated_body);
        }
        if (offered.find("gzip") != std::string::npos) {
            return answer_encoded("gzip", gzipped_body);
        }
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
               std::to_string(plain_body.size()) + "\r\nConnection: close\r\n\r\n" +
               plain_body;
    }));
    HttpResponse r;
    try {
        r = http_get(get(server.url("/v1/forecast")));
    } catch (const HttpError& e) {
        fail("offered \"" + request_header(server.head(), "accept-encoding") +
             "\", the answer could not be read: " + e.what());
    }
    const std::string offered = request_header(server.head(), "accept-encoding");
    std::printf("offered \"%s\"\n", offered.c_str());
    check_plain(r, "offered \"" + offered + "\"");
}

GLIDESLOPE_TEST(cesium_ions_layer_json_is_fetched_and_read) {
    // The real thing, with the user's token: where ion keeps Cesium World
    // Terrain (asset 1), and its layer.json - which ion's CDN serves gzipped
    // whether or not it was asked to - read as the JSON it is. Without a
    // token this is skipped, never passed. Nothing below repeats a URL or an
    // error that could hold the token.
    const std::string token = glideslope::platform::cesium_ion_token();
    if (token.empty()) {
        glideslope::test::skip("no Cesium ion token (GLIDESLOPE_CESIUM_ION_TOKEN, or "
                               "cesium-ion-token in the config directory)");
    }
    HttpResponse endpoint;
    try {
        endpoint = http_get(get(glideslope::platform::cesium_ion_api() +
                                "/v1/assets/1/endpoint?access_token=" + token));
    } catch (const HttpError&) {
        if (!glideslope::test::network_required()) {
            glideslope::test::skip("Cesium ion could not be reached");
        }
        fail("Cesium ion could not be reached");
    }
    check(endpoint.status == 200,
          "ion said where asset 1 is: status " + std::to_string(endpoint.status));
    const glideslope::world::Json said = glideslope::world::parse_json(
        std::string(endpoint.body.begin(), endpoint.body.end()));
    const glideslope::world::Json* url = said.find("url");
    const glideslope::world::Json* access = said.find("accessToken");
    check(url != nullptr && access != nullptr, "ion's answer has a url and a token");
    std::string layer = url->string();
    if (layer.empty() || layer.back() != '/') {
        layer += '/';
    }
    layer += "layer.json";

    HttpRequest request = get(layer);
    request.headers = {{"Authorization", "Bearer " + access->string()}};
    HttpResponse r;
    try {
        r = http_get(request);
    } catch (const HttpError&) {
        fail("ion's layer.json could not be fetched");
    }
    std::printf("fetched %zu bytes of layer.json through %s\n", r.body.size(),
                glideslope::platform::http_client().c_str());
    check(r.status == 200, "layer.json: status " + std::to_string(r.status));
    check(r.headers.count("content-encoding") == 0,
          "no content-encoding left on it: " +
              (r.headers.count("content-encoding") != 0 ? r.headers.at("content-encoding")
                                                        : std::string()));
    check(!r.body.empty() && r.body[0] != 0x1f, "not gzip's magic: it was undone");
    check(r.headers.count("content-length") == 1 &&
              r.headers.at("content-length") == std::to_string(r.body.size()),
          "a content-length that counts the body handed up");
    glideslope::world::Json doc;
    try {
        doc = glideslope::world::parse_json(std::string(r.body.begin(), r.body.end()));
    } catch (const glideslope::world::JsonError& e) {
        fail(std::string("layer.json is not JSON: ") + e.what());
    }
    const glideslope::world::Json* format = doc.find("format");
    check(format != nullptr && format->kind() == glideslope::world::Json::Kind::string &&
              format->string() == "quantized-mesh-1.0",
          "a quantized-mesh terrain's layer.json");
    const glideslope::world::Json* tiles = doc.find("tiles");
    check(tiles != nullptr && tiles->kind() == glideslope::world::Json::Kind::array &&
              !tiles->array().empty(),
          "naming where its tiles are");
    const glideslope::world::Json* available = doc.find("available");
    check(available != nullptr &&
              available->kind() == glideslope::world::Json::Kind::array &&
              available->array().size() > 10,
          "and which are available, level by level");
}
