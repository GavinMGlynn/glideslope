// glideslope_ion_stall - a Cesium ion on the loopback that says where its
// terrain is and then never sends it; and a check that a cache file can be
// written at once.
//
//   glideslope_ion_stall serve PORTFILE
//   glideslope_ion_stall write CACHEFILE
//
// **serve** listens on 127.0.0.1, on a port the system picks, and writes that
// port to PORTFILE - whole, by renaming it into place - once it is listening.
// Asked where ion asset 1 or 2 is (`/v1/assets/N/endpoint`), it answers as
// Cesium ion does, naming itself as where the terrain and the imagery are.
// Every other request is answered with a status of 200 and a length of a
// hundred megabytes, and then a byte every quarter of a second, for ever:
// a transfer that no stall timeout ends, because it never stalls. When the
// first of those begins it writes PORTFILE.stalled, so that a test can wait
// for the moment a client is stuck in one. A request for /stop ends it, saying
// on standard error how many endpoints it answered and how many transfers it
// held.
//
// **write** opens CACHEFILE, which must exist, with SQLite and no busy
// timeout, and takes and commits a write transaction. A process still holding
// the file would make that fail at once with "database is locked". Exits 0 if
// the write was taken, 1 if not.
//
// Exits 2 on bad arguments or a socket it could not open.

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
using Socket = SOCKET;
using Length = int;
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

int refuse(const char* why) {
    std::fprintf(stderr, "glideslope_ion_stall: %s\n", why);
    return 2;
}

std::string read_head(Socket client) {
    std::string got;
    char buffer[4096];
    while (got.find("\r\n\r\n") == std::string::npos) {
        const auto n = ::recv(client, buffer, static_cast<Length>(sizeof buffer), 0);
        if (n <= 0) {
            break;
        }
        got.append(buffer, static_cast<std::size_t>(n));
    }
    return got;
}

bool send_all(Socket client, const std::string& text) {
    std::size_t sent = 0;
    while (sent < text.size()) {
        const auto n =
            ::send(client, text.data() + sent, static_cast<Length>(text.size() - sent), 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

void write_whole(const std::filesystem::path& file, const std::string& text) {
    const std::filesystem::path part = file.string() + ".part";
    {
        std::ofstream out(part, std::ios::binary);
        out << text;
    }
    std::filesystem::rename(part, file);
}

std::string json_answer(const std::string& body) {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

std::atomic<int> endpoints{0};
std::atomic<int> held{0};
std::atomic<bool> first_held{false};

void answer(Socket client, std::string head, unsigned short port,
            std::filesystem::path stalled_file) {
    const std::string here = "http://127.0.0.1:" + std::to_string(port);
    if (head.rfind("GET /v1/assets/1/endpoint", 0) == 0) {
        ++endpoints;
        send_all(client, json_answer(
                             R"({"type":"TERRAIN","url":")" + here +
                             R"(/terrain/","accessToken":"stand-in",)"
                             R"("attributions":[{"html":"A stand-in for Cesium ion",)"
                             R"("collapsible":false}]})"));
    } else if (head.rfind("GET /v1/assets/2/endpoint", 0) == 0) {
        ++endpoints;
        send_all(client, json_answer(
                             R"({"type":"IMAGERY","externalType":"BING","options":{"url":")" +
                             here + R"(/bing","key":"stand-in","mapStyle":"Aerial"},)"
                             R"("attributions":[]})"));
    } else {
        // Answered, and then fed for ever: never quiet long enough for a
        // stall timeout, and never finished.
        if (send_all(client, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                             "Content-Length: 100000000\r\nConnection: close\r\n\r\n")) {
            ++held;
            if (!first_held.exchange(true)) {
                write_whole(stalled_file, "held\n");
            }
            while (send_all(client, " ")) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
    }
    close_socket(client);
}

int serve(const std::filesystem::path& port_file) {
#ifdef _WIN32
    WSADATA data;
    if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return refuse("Winsock would not start");
    }
#else
    // A client that hangs up mid-body is expected; it must not end this.
    std::signal(SIGPIPE, SIG_IGN);
#endif
    const Socket listening = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listening == no_socket) {
        return refuse("no socket to listen on");
    }
    sockaddr_in at{};
    at.sin_family = AF_INET;
    at.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    at.sin_port = 0;
    if (::bind(listening, reinterpret_cast<const sockaddr*>(&at), sizeof at) != 0 ||
        ::listen(listening, 64) != 0) {
        return refuse("cannot listen on the loopback address");
    }
    socklen_t size = sizeof at;
    ::getsockname(listening, reinterpret_cast<sockaddr*>(&at), &size);
    const unsigned short port = ntohs(at.sin_port);
    const std::filesystem::path stalled_file = port_file.string() + ".stalled";
    std::error_code ignored;
    std::filesystem::remove(stalled_file, ignored);
    write_whole(port_file, std::to_string(port) + "\n");

    for (;;) {
        const Socket client = ::accept(listening, nullptr, nullptr);
        if (client == no_socket) {
            break;
        }
        std::string head = read_head(client);
        if (head.rfind("GET /stop ", 0) == 0) {
            send_all(client,
                     "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            close_socket(client);
            break;
        }
        std::thread(answer, client, std::move(head), port, stalled_file).detach();
    }
    std::fprintf(stderr, "glideslope_ion_stall: answered %d endpoints and held %d transfers\n",
                 endpoints.load(), held.load());
    std::fflush(stderr);
    // The transfers still being fed are on threads of their own, which are
    // simply ended with the process.
    std::_Exit(0);
}

int write_to(const std::filesystem::path& cache) {
    if (!std::filesystem::exists(cache)) {
        std::fprintf(stderr, "glideslope_ion_stall: there is no %s to write to\n",
                     cache.string().c_str());
        return 1;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(cache.string().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) !=
        SQLITE_OK) {
        std::fprintf(stderr, "glideslope_ion_stall: cannot open %s: %s\n",
                     cache.string().c_str(), db ? sqlite3_errmsg(db) : "no memory");
        sqlite3_close(db);
        return 1;
    }
    // No waiting: a lock held elsewhere is reported at once.
    sqlite3_busy_timeout(db, 0);
    char* error = nullptr;
    const int rc = sqlite3_exec(db, "BEGIN IMMEDIATE; COMMIT;", nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr, "glideslope_ion_stall: %s could not be written at once: %s\n",
                     cache.string().c_str(), error ? error : sqlite3_errstr(rc));
        sqlite3_free(error);
        sqlite3_close(db);
        return 1;
    }
    sqlite3_close(db);
    std::printf("glideslope_ion_stall: %s took a write at once\n", cache.string().c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        return refuse("usage: glideslope_ion_stall serve PORTFILE | write CACHEFILE");
    }
    const std::string mode = argv[1];
    if (mode == "serve") {
        return serve(argv[2]);
    }
    if (mode == "write") {
        return write_to(argv[2]);
    }
    return refuse("the first argument is serve or write");
}
