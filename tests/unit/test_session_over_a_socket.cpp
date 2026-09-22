#include "harness.hpp"

#include "net/handshake.hpp"
#include "net/keys.hpp"
#include "net/protocol.hpp"
#include "net/sealing.hpp"
#include "platform/socket.hpp"

#include <chrono>
#include <cstdio>
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
