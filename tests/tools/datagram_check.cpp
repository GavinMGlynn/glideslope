// datagram_check - sends one datagram, written in a hex file, to an address,
// and prints the first answer that comes back, in hex.
//
//   glideslope_datagram_check 127.0.0.1:PORT FILE.hex SECONDS
//
// It says `answer <hex>` for what came back, or `no answer` if nothing did
// within SECONDS, resending once a second in case the server was not yet
// listening. It is how a test puts a stranger's bytes in front of the server -
// a gearstick client's first datagram - without a client of its own.

#include "platform/socket.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: glideslope_datagram_check HOST:PORT FILE.hex SECONDS\n");
        return 2;
    }
    const auto to = glideslope::platform::address_of(argv[1]);
    if (!to) {
        std::fprintf(stderr, "not an address: %s\n", argv[1]);
        return 2;
    }
    std::ifstream in(argv[2]);
    std::string hex;
    in >> hex;
    std::vector<std::uint8_t> datagram;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        datagram.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
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
