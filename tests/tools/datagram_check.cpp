// datagram_check - sends one datagram, written in a hex file, to an address,
// and prints the first answer that comes back, in hex; or answers one.
//
//   glideslope_datagram_check 127.0.0.1:PORT FILE.hex SECONDS
//   glideslope_datagram_check --answer PORT FILE.hex SECONDS
//
// It says `answer <hex>` for what came back, or `no answer` if nothing did
// within SECONDS, resending once a second in case the server was not yet
// listening. It is how a test puts a stranger's bytes in front of the server -
// a gearstick client's first datagram - without a client of its own.
//
// With `--answer` it stands in for a server that answers once and goes: it
// listens on PORT, answers the first datagram that comes with FILE's bytes,
// says `answered`, and exits 0 - its output ending as it does, which is how a
// relay last in a pipeline learns that the server has gone
// (tests/cmake/impair_last_words.cmake). With nothing come within SECONDS, it
// says `nobody asked` and exits 1.

#include "platform/closed_pipes.hpp"
#include "platform/socket.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<std::uint8_t> read_hex(const char* path) {
    std::ifstream in(path);
    std::string hex;
    in >> hex;
    std::vector<std::uint8_t> datagram;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        datagram.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return datagram;
}

int answer(const char* port_text, const char* path, double seconds) {
    const std::vector<std::uint8_t> datagram = read_hex(path);
    if (datagram.empty()) {
        std::fprintf(stderr, "no datagram in %s\n", path);
        return 2;
    }
    auto socket = glideslope::platform::UdpSocket::bound(
        static_cast<std::uint16_t>(std::strtoul(port_text, nullptr, 10)));
    if (!socket) {
        std::fprintf(stderr, "cannot listen on port %s\n", port_text);
        return 2;
    }
    const auto start = std::chrono::steady_clock::now();
    std::vector<std::uint8_t> buffer(2048);
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() <
           seconds) {
        glideslope::platform::Address from;
        if (socket->receive(buffer, from) > 0) {
            (void)socket->send(from, datagram);
            std::printf("answered\n");
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::printf("nobody asked\n");
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    // Its reader in a pipeline may be gone first (platform/closed_pipes.hpp).
    glideslope::platform::outlive_closed_pipes();
    if (argc == 5 && std::string(argv[1]) == "--answer") {
        return answer(argv[2], argv[3], std::stod(argv[4]));
    }
    if (argc != 4) {
        std::fprintf(stderr, "usage: glideslope_datagram_check HOST:PORT FILE.hex SECONDS\n"
                             "       glideslope_datagram_check --answer PORT FILE.hex "
                             "SECONDS\n");
        return 2;
    }
    const auto to = glideslope::platform::address_of(argv[1]);
    if (!to) {
        std::fprintf(stderr, "not an address: %s\n", argv[1]);
        return 2;
    }
    const std::vector<std::uint8_t> datagram = read_hex(argv[2]);
    if (datagram.empty()) {
        std::fprintf(stderr, "no datagram in %s\n", argv[2]);
        return 2;
    }
    auto socket = glideslope::platform::UdpSocket::bound(0);
    if (!socket) {
        std::fprintf(stderr, "cannot open a socket\n");
        return 2;
    }
    const double seconds = std::stod(argv[3]);
    const auto start = std::chrono::steady_clock::now();
    auto last_sent = start - std::chrono::seconds(2);
    std::vector<std::uint8_t> buffer(2048);
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() <
           seconds) {
        if (std::chrono::steady_clock::now() - last_sent >= std::chrono::seconds(1)) {
            socket->send(*to, datagram);
            last_sent = std::chrono::steady_clock::now();
        }
        glideslope::platform::Address from;
        const std::size_t got = socket->receive(buffer, from);
        if (got > 0) {
            std::printf("answer ");
            for (std::size_t i = 0; i < got; ++i) {
                std::printf("%02x", buffer[i]);
            }
            std::printf("\n");
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::printf("no answer\n");
    return 0;
}
