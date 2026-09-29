// glideslope_http_stub - a web server on the loopback that answers every
// request with one status, for a test to point a service at.
//
//   glideslope_http_stub [--retry-after SECONDS] [--files-for N] [--stall]
//                        STATUS PORTFILE [PREFIX FILE]...
//
// Listens on 127.0.0.1, on a port the system picks, and writes that port to
// PORTFILE - whole, by renaming it into place - once it is listening. Every
// request is answered with STATUS and an empty body, and the connection
// closed, until a request for the path /stop, which is answered 200 and ends
// it. A 429 is sent with a Retry-After, as a rate limit says how long:
// --retry-after's seconds, or 1. With PREFIX and FILE pairs, a GET whose path
// begins with a PREFIX is answered 200 with that FILE's bytes instead: one
// service answered, and another not. With --files-for N, only the first N
// requests a FILE would answer are answered with it, in all; every one after
// them is answered STATUS - a service that answered, and then did not.
//
// **--stall** answers STATUS with a transfer under way that never ends: its
// head, a Content-Length of a megabyte, and the first few bytes of the body,
// and then nothing, the connection held open until /stop. It says, as each is
// begun, that it is holding a transfer under way. Whether the other end let
// it go is not said: by the time /stop is asked, the program that was sent it
// has usually ended, and the system has closed its end whether it gave the
// transfer up or not - how soon it ended is the test's to measure.
//
// Says on standard error how many requests it answered with STATUS, and how
// many with FILE. Exits 0, or 2 on bad arguments or a socket it could not
// open.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
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
    std::fprintf(stderr, "glideslope_http_stub: %s\n", why);
    return 2;
}

// The request's head, up to its blank line; its body, if any, is not read.
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

void send_all(Socket client, const std::string& text) {
    std::size_t sent = 0;
    while (sent < text.size()) {
        const auto n =
            ::send(client, text.data() + sent, static_cast<Length>(text.size() - sent), 0);
        if (n <= 0) {
            return;
        }
        sent += static_cast<std::size_t>(n);
    }
}

} // namespace

int main(int argc, char** argv) {
#ifndef _WIN32
    // It runs in a test's pipeline, whose reader may go first; and a client
    // may hang up before its answer is written. Either write must fail, not
    // end it (src/platform/closed_pipes.hpp).
    std::signal(SIGPIPE, SIG_IGN);
#endif
    const char* usage = "usage: glideslope_http_stub [--retry-after SECONDS] "
                        "[--files-for N] [--stall] STATUS PORTFILE [PREFIX FILE]...";
    int retry_after = 1;
    long files_for = -1;
    bool stall = false;
    int arg = 1;
    for (; arg < argc && std::string(argv[arg]).rfind("--", 0) == 0; ++arg) {
        const std::string option = argv[arg];
        if (option == "--stall") {
            stall = true;
        } else if (option == "--retry-after" && arg + 1 < argc) {
            retry_after = std::atoi(argv[++arg]);
            if (retry_after < 0) {
                return refuse(usage);
            }
        } else if (option == "--files-for" && arg + 1 < argc) {
            files_for = std::atol(argv[++arg]);
            if (files_for < 0) {
                return refuse(usage);
            }
        } else {
            return refuse(usage);
        }
    }
    if (argc - arg < 2 || (argc - arg) % 2 != 0) {
        return refuse(usage);
    }
    const int status = std::atoi(argv[arg]);
    if (status < 100 || status > 599) {
        return refuse("STATUS must be an HTTP status, 100 to 599");
    }
    const std::filesystem::path port_file = argv[arg + 1];
    // Each PREFIX as the start of a request line, and the answer its FILE makes.
    std::vector<std::pair<std::string, std::string>> files;
    for (int i = arg + 2; i + 1 < argc; i += 2) {
        std::ifstream in(argv[i + 1], std::ios::binary);
        if (!in) {
            return refuse("cannot read FILE");
        }
        const std::string body((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        files.emplace_back(std::string("GET ") + argv[i],
                           "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                           "Content-Length: " +
                               std::to_string(body.size()) +
                               "\r\nConnection: close\r\n\r\n" + body);
    }
#ifdef _WIN32
    WSADATA data;
    if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return refuse("Winsock would not start");
    }
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
        ::listen(listening, 16) != 0) {
        return refuse("cannot listen on the loopback address");
    }
    socklen_t size = sizeof at;
    ::getsockname(listening, reinterpret_cast<sockaddr*>(&at), &size);
    const std::filesystem::path part = port_file.string() + ".part";
    {
        std::ofstream out(part, std::ios::binary);
        out << ntohs(at.sin_port) << "\n";
    }
    std::filesystem::rename(part, port_file);

    // A 429 asks for its wait, as a rate limit says how long.
    const std::string retry =
        status == 429 ? "Retry-After: " + std::to_string(retry_after) + "\r\n" : std::string();
    const std::string head_of = "HTTP/1.1 " + std::to_string(status) + " Stubbed\r\n" + retry;
    const std::string answer = head_of + "Content-Length: 0\r\nConnection: close\r\n\r\n";
    // A megabyte promised, and a few bytes of it sent.
    const std::string stalled_answer =
        head_of + "Content-Length: 1048576\r\nConnection: close\r\n\r\n{\"stalled\": ";
    std::vector<Socket> held;
    long answered = 0;
    long answered_file = 0;
    for (;;) {
        const Socket client = ::accept(listening, nullptr, nullptr);
        if (client == no_socket) {
            break;
        }
        const std::string head = read_head(client);
        const bool stop = head.rfind("GET /stop ", 0) == 0;
        const std::string* file_answer = nullptr;
        for (const auto& [prefix, file] : files) {
            if (head.rfind(prefix, 0) == 0) {
                file_answer = &file;
            }
        }
        if (files_for >= 0 && answered_file >= files_for) {
            file_answer = nullptr;
        }
        if (stop) {
            send_all(client, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        } else if (file_answer != nullptr) {
            send_all(client, *file_answer);
            ++answered_file;
        } else if (stall) {
            send_all(client, stalled_answer);
            ++answered;
            held.push_back(client);
            std::fprintf(stderr, "glideslope_http_stub: holding a transfer under way\n");
            continue;
        } else {
            send_all(client, answer);
            ++answered;
        }
        close_socket(client);
        if (stop) {
            break;
        }
    }
    for (const Socket s : held) {
        close_socket(s);
    }
    close_socket(listening);
    std::fprintf(stderr, "glideslope_http_stub: answered %ld requests %d, and %ld with FILE\n",
                 answered, status, answered_file);
    return 0;
}
