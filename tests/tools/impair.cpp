// impair - a UDP relay that makes a network worse, for the network checks.
//
//   glideslope_impair LISTEN_PORT SERVER_HOST:PORT --delay MS --jitter MS
//                     --loss PERCENT --seed N [--until-input-ends] [--seconds S]
//
// Clients send to LISTEN_PORT as if it were the server; each client gets a
// socket of its own towards the server, so the server still sees each one at
// an address of its own - which is how it tells sessions apart. Every datagram,
// either way, is dropped with probability PERCENT, and otherwise delivered
// after DELAY milliseconds plus up to JITTER more, drawn evenly - so datagrams
// can arrive out of order, as they do. The draws come from one generator seeded
// with N, so a run can be repeated.
//
// It stops after S seconds (600 unless given) or, with `--until-input-ends`,
// when its standard input ends - which, last in a test's pipeline, is when the
// server before it has gone - and says what it did: how many datagrams each
// way, and how many it dropped.
//
// It is what `tc netem` does, without needing to be root or on Linux, so that
// the same check runs on every CI platform.

#include "platform/socket.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Held {
    std::chrono::steady_clock::time_point due;
    bool to_server = false;
    glideslope::platform::Address to;     // the client, when going back
    std::string client;                   // which client's upstream socket
    std::vector<std::uint8_t> bytes;
};

double number(const char* text) {
    return std::strtod(text, nullptr);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: glideslope_impair LISTEN_PORT SERVER_HOST:PORT --delay MS "
                             "--jitter MS --loss PERCENT --seed N [--quiet-for S] [--seconds S]\n");
        return 2;
    }
    const auto listen_port = static_cast<std::uint16_t>(std::atoi(argv[1]));
    const auto server = glideslope::platform::address_of(argv[2]);
    if (!server) {
        std::fprintf(stderr, "impair: not an address: %s\n", argv[2]);
        return 2;
    }
    double delay_ms = 0.0, jitter_ms = 0.0, loss = 0.0, seconds = 600.0;
    unsigned seed = 1;
    bool until_input_ends = false;
    for (int i = 3; i < argc; i += 2) {
        const std::string flag = argv[i];
        if (flag == "--until-input-ends") {
            until_input_ends = true;
            --i;
            continue;
        }
        if (i + 1 >= argc) {
            std::fprintf(stderr, "impair: %s wants a value\n", flag.c_str());
            return 2;
        }
        if (flag == "--delay") delay_ms = number(argv[i + 1]);
        else if (flag == "--jitter") jitter_ms = number(argv[i + 1]);
        else if (flag == "--loss") loss = number(argv[i + 1]) / 100.0;
        else if (flag == "--seed") seed = static_cast<unsigned>(std::strtoul(argv[i + 1], nullptr, 10));
        else if (flag == "--seconds") seconds = number(argv[i + 1]);
        else {
            std::fprintf(stderr, "impair: no option %s\n", flag.c_str());
            return 2;
        }
    }
    auto front = glideslope::platform::UdpSocket::bound(listen_port);
    if (!front) {
        std::fprintf(stderr, "impair: cannot listen on port %u\n", static_cast<unsigned>(listen_port));
        return 1;
    }
    std::printf("impair: %u -> %s, delay %.0f ms, jitter %.0f ms, loss %.1f%%, seed %u\n",
                static_cast<unsigned>(listen_port), argv[2], delay_ms, jitter_ms, loss * 100.0, seed);
    std::fflush(stdout);

    std::mt19937_64 random(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    // One socket towards the server for each client, and who it is for.
    std::map<std::string, std::unique_ptr<glideslope::platform::UdpSocket>> upstream;
    std::map<std::string, glideslope::platform::Address> client_of;
    std::vector<Held> held;
    std::uint64_t up = 0, down = 0, dropped_up = 0, dropped_down = 0;

    // Standard input read to its end on a thread of its own, because reading
    // it waits and the relaying must not.
    std::atomic<bool> input_ended{false};
    if (until_input_ends) {
        std::thread([&input_ended] {
            while (std::fgetc(stdin) != EOF) {
            }
            input_ended = true;
        }).detach();
    }
    const auto began = std::chrono::steady_clock::now();
    std::vector<std::uint8_t> buffer(glideslope::platform::largest_datagram);

    const auto hold = [&](bool to_server, const glideslope::platform::Address& to,
                          const std::string& client, const std::uint8_t* data, std::size_t n) {
        if (unit(random) < loss) {
            ++(to_server ? dropped_up : dropped_down);
            return;
        }
        const double after_ms = delay_ms + jitter_ms * unit(random);
        held.push_back({std::chrono::steady_clock::now() +
                            std::chrono::microseconds(static_cast<long long>(after_ms * 1000.0)),
                        to_server, to, client, std::vector<std::uint8_t>(data, data + n)});
    };

    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        const double up_s = std::chrono::duration<double>(now - began).count();
        if (up_s >= seconds || input_ended) {
            break;
        }
        bool busy = false;
        glideslope::platform::Address from;
        // From the clients, towards the server.
        for (std::size_t got; (got = front->receive(buffer, from)) > 0;) {
            busy = true;
            const std::string client = from.text();
            if (!upstream.count(client)) {
                upstream[client] = std::make_unique<glideslope::platform::UdpSocket>(
                    std::move(*glideslope::platform::UdpSocket::bound(0)));
                client_of[client] = from;
            }
            hold(true, *server, client, buffer.data(), got);
        }
        // From the server, back towards each client.
        for (auto& [client, socket] : upstream) {
            for (std::size_t got; (got = socket->receive(buffer, from)) > 0;) {
                busy = true;
                hold(false, client_of[client], client, buffer.data(), got);
            }
        }
        // What is due, delivered - in the order it falls due, not the order it came.
        for (auto it = held.begin(); it != held.end();) {
            if (it->due > now) {
                ++it;
                continue;
            }
            const std::span<const std::uint8_t> bytes(it->bytes.data(), it->bytes.size());
            if (it->to_server) {
                (void)upstream[it->client]->send(*server, bytes);
                ++up;
            } else {
                (void)front->send(it->to, bytes);
                ++down;
            }
            it = held.erase(it);
            busy = true;
        }
        if (!busy) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
    std::printf("impair: forwarded %llu to the server and %llu back; dropped %llu and %llu\n",
                static_cast<unsigned long long>(up), static_cast<unsigned long long>(down),
                static_cast<unsigned long long>(dropped_up),
                static_cast<unsigned long long>(dropped_down));
    return 0;
}
