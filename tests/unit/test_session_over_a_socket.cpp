#include "harness.hpp"

#include "net/handshake.hpp"
#include "net/inside.hpp"
#include "net/keys.hpp"
#include "net/protocol.hpp"
#include "net/sealing.hpp"
#include "net/session.hpp"
#include "platform/socket.hpp"

#include <chrono>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

using glideslope::net::Initiator;
using glideslope::net::KeyPair;
using glideslope::net::Responder;
using glideslope::platform::Address;
using glideslope::platform::UdpSocket;
using glideslope::test::check;

namespace {

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

// Waits for a datagram, up to a second, which is a very long time on
// loopback and short enough that a broken test fails rather than hangs.
std::vector<std::uint8_t> wait_for(UdpSocket& socket, Address& from) {
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

} // namespace

// **A client and a server complete a session over a socket, and seal to each
// other.** Every other test of the transport hands bytes from one object to
// another; this one puts them through the loopback, in the envelopes they
// really travel in, so that nothing is proved about a wire that was never
// used.
GLIDESLOPE_TEST(a_client_and_a_server_complete_a_session_over_a_socket) {
    auto server_socket = UdpSocket::bound(0);
    auto client_socket = UdpSocket::bound(0);
    check(server_socket.has_value() && client_socket.has_value(),
          "both ends have a socket");
    const Address server_at = glideslope::platform::loopback_v4(server_socket->port());

    const KeyPair server_key = glideslope::net::mint_key_pair();
    const KeyPair client_key = glideslope::net::mint_key_pair();

    // The client knows the server's public key out of band, which is what
    // `IK` is for and what the server prints at startup.
    Initiator initiator(client_key, server_key.publik);
    glideslope::net::Writer w =
        glideslope::net::begin(glideslope::net::Type::handshake_initiation);
    w.bytes(initiator.begin());
    const std::vector<std::uint8_t> first = w.take();
    check(client_socket->send(server_at, all_of(first)), "the initiation is sent");

    // The server side.
    Address from;
    const std::vector<std::uint8_t> arrived = wait_for(*server_socket, from);
    check(!arrived.empty(), "the server heard it");
    glideslope::net::Reader r(all_of(arrived));
    glideslope::net::Envelope envelope;
    glideslope::net::Refusal why{};
    check(glideslope::net::read_envelope(r, envelope, why), "and it is one of ours");
    check(envelope.type == glideslope::net::Type::handshake_initiation,
          "and says it is a handshake");

    Responder responder(server_key);
    const auto answer = responder.answer(
        std::span<const std::uint8_t>(arrived).subspan(glideslope::net::envelope_size));
    check(answer.has_value(), "the server answers it");
    check(answer->session.theirs == client_key.publik,
          "and knows which client it was");

    glideslope::net::Writer aw =
        glideslope::net::begin(glideslope::net::Type::handshake_response);
    aw.bytes(answer->message);
    const std::vector<std::uint8_t> reply = aw.take();
    check(server_socket->send(from, all_of(reply)), "and sends its answer");

    // Back at the client.
    Address whence;
    const std::vector<std::uint8_t> back = wait_for(*client_socket, whence);
    check(!back.empty(), "the client heard the answer");
    const auto session = initiator.finish(
        std::span<const std::uint8_t>(back).subspan(glideslope::net::envelope_size));
    check(session.has_value(), "and completed the session");
    check(session->theirs == server_key.publik, "with the server it meant to");

    // **And they seal to each other, both ways.**
    glideslope::net::Sealer client_seals(session->sending);
    glideslope::net::Unsealer server_opens(answer->session.receiving);
    const std::string said = "a client's inputs would go here";
    const std::vector<std::uint8_t> plain(said.begin(), said.end());
    glideslope::net::Writer sw = glideslope::net::begin(glideslope::net::Type::sealed);
    sw.bytes(client_seals.seal(all_of(plain)));
    const std::vector<std::uint8_t> sealed = sw.take();
    check(client_socket->send(server_at, all_of(sealed)), "the client seals and sends");

    const std::vector<std::uint8_t> got = wait_for(*server_socket, from);
    check(!got.empty(), "the server heard it");
    const auto opened = server_opens.open(
        std::span<const std::uint8_t>(got).subspan(glideslope::net::envelope_size));
    check(opened.has_value(), "and it opened");
    check(*opened == plain, "as what the client sealed");

    // And the other way, under the other key.
    glideslope::net::Sealer server_seals(answer->session.sending);
    glideslope::net::Unsealer client_opens(session->receiving);
    const std::string told = "and the server's state would come back";
    const std::vector<std::uint8_t> theirs(told.begin(), told.end());
    glideslope::net::Writer tw = glideslope::net::begin(glideslope::net::Type::sealed);
    tw.bytes(server_seals.seal(all_of(theirs)));
    const std::vector<std::uint8_t> sealed_back = tw.take();
    check(server_socket->send(from, all_of(sealed_back)), "the server seals and sends");
    const std::vector<std::uint8_t> came = wait_for(*client_socket, whence);
    check(!came.empty(), "the client heard it");
    const auto theirs_opened = client_opens.open(
        std::span<const std::uint8_t>(came).subspan(glideslope::net::envelope_size));
    check(theirs_opened.has_value() && *theirs_opened == theirs,
          "and it opened as what the server sealed");

    std::printf("  a session over loopback: %zu-byte initiation, %zu-byte answer, "
                "sealed both ways\n",
                first.size(), reply.size());
}

// **A client that does not know the server's key gets nowhere over a socket
// either**, which is the property that matters on a real port: anybody can
// send to it, and only somebody holding the right key is answered.
GLIDESLOPE_TEST(a_stranger_on_the_port_cannot_complete_a_session) {
    auto server_socket = UdpSocket::bound(0);
    auto client_socket = UdpSocket::bound(0);
    check(server_socket.has_value() && client_socket.has_value(), "two sockets");
    const Address server_at = glideslope::platform::loopback_v4(server_socket->port());

    const KeyPair server_key = glideslope::net::mint_key_pair();
    const KeyPair someone_else = glideslope::net::mint_key_pair();
    const KeyPair client_key = glideslope::net::mint_key_pair();

    Initiator initiator(client_key, someone_else.publik); // the wrong key
    glideslope::net::Writer w =
        glideslope::net::begin(glideslope::net::Type::handshake_initiation);
    w.bytes(initiator.begin());
    const std::vector<std::uint8_t> first = w.take();
    check(client_socket->send(server_at, all_of(first)), "it is sent anyway");

    Address from;
    const std::vector<std::uint8_t> arrived = wait_for(*server_socket, from);
    check(!arrived.empty(), "the server heard it");
    Responder responder(server_key);
    check(!responder
               .answer(std::span<const std::uint8_t>(arrived).subspan(
                   glideslope::net::envelope_size))
               .has_value(),
          "and could make nothing of it");
}

// **A client whose first sealed datagram is lost still proves its session.**
// A server sends a session nothing but its handshake answer until something
// sealed under it has opened (docs/TRANSPORT.md), so a client that sealed one
// thing and then waited to hear would wait for ever if that one thing were
// lost - the window client's `Online::join` waits for a state update before it
// sends anything. A stand-in server here answers the handshake, throws away
// the first sealed datagram the client sends, and then waits, on the client's
// own clock, for another: the client knocks until anything opens. And once
// something has opened, it stops.
//
// Counted in the client's time, not the wall clock's: `poll()` is given a
// clock stepped a hundredth of a second at a time, two seconds of it - eight
// knocks' worth - with a millisecond's real wait for loopback between steps.
GLIDESLOPE_TEST(a_client_whose_first_sealed_datagram_is_lost_still_proves_its_session) {
    auto server_socket = UdpSocket::bound(0);
    check(server_socket.has_value(), "the stand-in server has a socket");
    const KeyPair server_key = glideslope::net::mint_key_pair();
    const std::string where = "127.0.0.1:" + std::to_string(server_socket->port());

    // The client connects on a thread of its own, since `connect` waits for
    // its answer; the stand-in server answers it from here.
    std::optional<glideslope::net::ClientSession> client;
    std::thread connecting([&] {
        if (auto made =
                glideslope::net::ClientSession::connect(where, server_key.publik.text(), 5.0)) {
            client.emplace(std::move(*made));
        }
    });
    Address from;
    const std::vector<std::uint8_t> arrived = wait_for(*server_socket, from);
    check(!arrived.empty(), "the stand-in server heard the initiation");
    Responder responder(server_key);
    const auto answer = responder.answer(
        std::span<const std::uint8_t>(arrived).subspan(glideslope::net::envelope_size));
    check(answer.has_value(), "and answers it");
    glideslope::net::Writer aw =
        glideslope::net::begin(glideslope::net::Type::handshake_response);
    aw.bytes(answer->message);
    const std::vector<std::uint8_t> reply = aw.take();
    check(server_socket->send(from, all_of(reply)), "the answer is sent");
    connecting.join();
    check(client.has_value(), "the client completed its session");

    glideslope::net::Unsealer server_opens(answer->session.receiving);
    glideslope::net::Sealer server_seals(answer->session.sending);
    const auto opened_one = [&]() -> std::optional<std::vector<std::uint8_t>> {
        std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
        Address whence;
        const std::size_t got = server_socket->receive(into, whence);
        if (got <= glideslope::net::envelope_size) {
            return std::nullopt;
        }
        return server_opens.open(std::span<const std::uint8_t>(into.data(), got)
                                     .subspan(glideslope::net::envelope_size));
    };

    // **The first sealed datagram is lost**: read and thrown away.
    const std::vector<std::uint8_t> first_sealed = wait_for(*server_socket, from);
    check(!first_sealed.empty(), "the client sealed something at once");

    double now_s = 0.0;
    bool proved = false;
    for (; now_s < 2.0 && !proved; now_s += 0.01) {
        client->poll(now_s);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        while (opened_one()) {
            proved = true;
        }
    }
    check(proved, "with its first sealed datagram lost, the client sealed another, which "
                  "opened, within two seconds of its own clock");
    std::printf("  proved again %.2f s after its first sealed datagram was lost\n", now_s);

    // **Something opens at the client: it stops knocking.** The stand-in
    // server seals a pong nobody pinged for - it opens, and asks nothing.
    glideslope::net::Writer sw = glideslope::net::begin(glideslope::net::Type::sealed);
    const std::vector<std::uint8_t> pong =
        glideslope::net::knock(glideslope::net::Inside::pong, 0);
    sw.bytes(server_seals.seal(all_of(pong)));
    const std::vector<std::uint8_t> sealed = sw.take();
    check(server_socket->send(from, all_of(sealed)), "the stand-in server seals to it");
    int after = 0;
    const double until_s = now_s + 2.0;
    for (; now_s < until_s; now_s += 0.01) {
        client->poll(now_s);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        while (opened_one()) {
            ++after;
        }
    }
    // One knock may already have been on its way when the pong arrived.
    check(after <= 1, "once something had opened, the client stopped knocking (" +
                          std::to_string(after) + " sealed after)");
}
