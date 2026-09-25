// glideslope_http_stub - a web server on the loopback that answers every
// request with one status, for a test to point a service at.
//
//   glideslope_http_stub STATUS PORTFILE
//
// Listens on 127.0.0.1, on a port the system picks, and writes that port to
// PORTFILE - whole, by renaming it into place - once it is listening. Every
// request is answered with STATUS and an empty body, and the connection
// closed, until a request for the path /stop, which is answered 200 and ends
// it. Says on standard error how many requests it answered with STATUS.
// Exits 0, or 2 on bad arguments or a socket it could not open.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

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
    if (argc != 3) {
        return refuse("usage: glideslope_http_stub STATUS PORTFILE");
    }
    const int status = std::atoi(argv[1]);
    if (status < 100 || status > 599) {
        return refuse("STATUS must be an HTTP status, 100 to 599");
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
    const std::filesystem::path port_file = argv[2];
    const std::filesystem::path part = port_file.string() + ".part";
    {
        std::ofstream out(part, std::ios::binary);
        out << ntohs(at.sin_port) << "\n";
    }
    std::filesystem::rename(part, port_file);

    const std::string answer = "HTTP/1.1 " + std::to_string(status) +
                               " Stubbed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    int answered = 0;
    for (;;) {
        const Socket client = ::accept(listening, nullptr, nullptr);
        if (client == no_socket) {
            break;
        }
        const std::string head = read_head(client);
        const bool stop = head.rfind("GET /stop ", 0) == 0;
        if (stop) {
            send_all(client, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        } else {
            send_all(client, answer);
            ++answered;
        }
        close_socket(client);
        if (stop) {
            break;
        }
    }
    close_socket(listening);
    std::fprintf(stderr, "glideslope_http_stub: answered %d requests %d\n", answered, status);
    return 0;
}
