#pragma once

// **A stand-in server for a `net::ClientSession`**, over the loopback: what
// the unit tests that put a session's bytes through a real socket share
// (test_session_over_a_socket.cpp, test_same_air.cpp).

#include "net/handshake.hpp"
#include "net/inside.hpp"
#include "net/keys.hpp"
#include "net/protocol.hpp"
#include "net/reliable.hpp"
#include "net/sealing.hpp"
#include "net/state.hpp"
#include "platform/socket.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace glideslope::test {

inline std::span<const std::uint8_t> bytes_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

// Waits for a datagram, up to a second, which is a very long time on
// loopback and short enough that a broken test fails rather than hangs.
inline std::vector<std::uint8_t> wait_for(platform::UdpSocket& socket, platform::Address& from) {
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    const auto began = std::chrono::steady_clock::now();
    for (;;) {
        const std::size_t got = socket.receive(into, from);
        if (got > 0) {
            into.resize(got);
            return into;
        }
        if (std::chrono::steady_clock::now() - began > std::chrono::seconds(1)) {
            return {};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// **A stand-in server** for a `net::ClientSession`: answers initiations on its
// socket with the responder's keys, and seals to the client under the newest
// session it made.
struct StandIn {
    platform::UdpSocket socket = *platform::UdpSocket::bound(0);
    net::KeyPair key = glideslope::net::mint_key_pair();
    platform::Address client;
    std::optional<glideslope::net::Sealer> sealing;
    std::optional<glideslope::net::Unsealer> opening;
    int sessions = 0;

    // Waits for an initiation, skipping whatever else comes, and answers it.
    bool answer_one() {
        const auto began = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - began < std::chrono::seconds(5)) {
            platform::Address from;
            const std::vector<std::uint8_t> got = wait_for(socket, from);
            if (got.size() <= glideslope::net::envelope_size ||
                got[glideslope::net::envelope_size - 1] !=
                    static_cast<std::uint8_t>(glideslope::net::Type::handshake_initiation)) {
                continue;
            }
            net::Responder responder(key);
            const auto answer = responder.answer(
                std::span<const std::uint8_t>(got).subspan(glideslope::net::envelope_size));
            if (!answer) {
                return false;
            }
            glideslope::net::Writer w =
                glideslope::net::begin(glideslope::net::Type::handshake_response);
            w.bytes(answer->message);
            const std::vector<std::uint8_t> out = w.take();
            (void)socket.send(from, bytes_of(out));
            client = from;
            sealing.emplace(answer->session.sending);
            opening.emplace(answer->session.receiving);
            ++sessions;
            return true;
        }
        return false;
    }

    void send_state(double at_s) {
        glideslope::net::StatePacket state;
        state.simulation_time_s = at_s;
        const auto body = glideslope::net::write_state(state);
        glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::sealed);
        w.bytes(sealing->seal(bytes_of(*body)));
        const std::vector<std::uint8_t> out = w.take();
        (void)socket.send(client, bytes_of(out));
    }

    // **Messages that must arrive** (`net::write` of each), sent through
    // `reliable` as a server sends them, at `now_s` on its clock: those not
    // yet acknowledged, again when their turn comes round.
    void send_reliable(glideslope::net::Reliable& reliable, double now_s) {
        for (const std::vector<std::uint8_t>& datagram : reliable.to_send(now_s)) {
            std::vector<std::uint8_t> body{
                static_cast<std::uint8_t>(glideslope::net::Inside::reliable)};
            body.insert(body.end(), datagram.begin(), datagram.end());
            glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::sealed);
            w.bytes(sealing->seal(bytes_of(body)));
            const std::vector<std::uint8_t> out = w.take();
            (void)socket.send(client, bytes_of(out));
        }
    }

    // **What the client says back of them**, read without waiting: each
    // reliable datagram it has sent - its acknowledgements among them - fed
    // to `reliable` as a server feeds it. Without it a sender with more than
    // most_in_flight messages to say never says the rest.
    void hear_reliable(glideslope::net::Reliable& reliable) {
        std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
        platform::Address from;
        for (std::size_t got = socket.receive(into, from); got > 0;
             got = socket.receive(into, from)) {
            if (got <= glideslope::net::envelope_size ||
                into[glideslope::net::envelope_size - 1] !=
                    static_cast<std::uint8_t>(glideslope::net::Type::sealed)) {
                continue;
            }
            const auto opened = opening->open(std::span<const std::uint8_t>(into).subspan(
                glideslope::net::envelope_size, got - glideslope::net::envelope_size));
            if (!opened || opened->empty() ||
                (*opened)[0] != static_cast<std::uint8_t>(glideslope::net::Inside::reliable)) {
                continue;
            }
            (void)reliable.received(std::span<const std::uint8_t>(*opened).subspan(1));
        }
    }

    void refuse(glideslope::net::Refusal why) {
        glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::refusal);
        w.u8(static_cast<std::uint8_t>(why));
        const std::vector<std::uint8_t> out = w.take();
        (void)socket.send(client, bytes_of(out));
    }
};

} // namespace glideslope::test
