#include "harness.hpp"

#include "net/handshake.hpp"
#include "net/inside.hpp"
#include "net/keys.hpp"
#include "net/protocol.hpp"
#include "net/sealing.hpp"
#include "net/session.hpp"
#include "net/state.hpp"
#include "platform/socket.hpp"

#include <chrono>
#include <cstdio>
#include <functional>
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
    // Counted over the next 0.9 s of its clock: proving, it would knock three
    // times in it; past a second of nothing opening it knocks once a second
    // again, as any client does to hear whether it has been let go
    // (`knock_after_quiet_s`), and that is not proving.
    int after = 0;
    const double until_s = now_s + 0.9;
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

namespace {

// **A stand-in server** for a `net::ClientSession`: answers initiations on its
// socket with the responder's keys, and seals to the client under the newest
// session it made.
struct StandIn {
    UdpSocket socket = *UdpSocket::bound(0);
    KeyPair key = glideslope::net::mint_key_pair();
    Address client;
    std::optional<glideslope::net::Sealer> sealing;
    std::optional<glideslope::net::Unsealer> opening;
    int sessions = 0;

    // Waits for an initiation, skipping whatever else comes, and answers it.
    bool answer_one() {
        const auto began = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - began < std::chrono::seconds(5)) {
            Address from;
            const std::vector<std::uint8_t> got = wait_for(socket, from);
            if (got.size() <= glideslope::net::envelope_size ||
                got[glideslope::net::envelope_size - 1] !=
                    static_cast<std::uint8_t>(glideslope::net::Type::handshake_initiation)) {
                continue;
            }
            Responder responder(key);
            const auto answer = responder.answer(
                std::span<const std::uint8_t>(got).subspan(glideslope::net::envelope_size));
            if (!answer) {
                return false;
            }
            glideslope::net::Writer w =
                glideslope::net::begin(glideslope::net::Type::handshake_response);
            w.bytes(answer->message);
            const std::vector<std::uint8_t> out = w.take();
            (void)socket.send(from, all_of(out));
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
        w.bytes(sealing->seal(all_of(*body)));
        const std::vector<std::uint8_t> out = w.take();
        (void)socket.send(client, all_of(out));
    }

    void refuse(glideslope::net::Refusal why) {
        glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::refusal);
        w.u8(static_cast<std::uint8_t>(why));
        const std::vector<std::uint8_t> out = w.take();
        (void)socket.send(client, all_of(out));
    }
};

} // namespace

// **A session joined again hands up nothing of the one let go**: an update
// the old session left waiting is not handed up as the new one's - it would
// be taken for the aircraft given on joining again - and a message asked to
// be sent while joining again is refused, not queued into a reliable stream
// the new session throws away. The client's clock is handed to `poll()`, so
// the three seconds of nothing before a refusal is believed are simulated,
// not waited for. The new session's update comes from a clock started again,
// as a server restarted with its key would send.
GLIDESLOPE_TEST(a_session_joined_again_hands_up_nothing_of_the_one_let_go) {
    StandIn server;
    const std::string where = "127.0.0.1:" + std::to_string(server.socket.port());
    std::optional<glideslope::net::ClientSession> session;
    std::thread connecting([&] {
        if (auto made = glideslope::net::ClientSession::connect(where, server.key.publik.text())) {
            session.emplace(std::move(*made));
        }
    });
    const bool answered = server.answer_one();
    connecting.join();
    check(answered && session.has_value(), "the client has a session with the stand-in");

    const auto polled_until = [&](double now_s, const std::function<bool()>& done) {
        const auto began = std::chrono::steady_clock::now();
        while (!done() && std::chrono::steady_clock::now() - began < std::chrono::seconds(5)) {
            session->poll(now_s);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return done();
    };
    // An update of the old session, arrived and read but not yet asked for.
    server.send_state(42.0);
    check(polled_until(0.0, [&] { return session->heard() == 1; }), "the update was read");

    // Let go: refused after more than three seconds of nothing.
    server.refuse(glideslope::net::Refusal::bad_handshake);
    check(polled_until(10.0,
                       [&] {
                           return session->standing() ==
                                  glideslope::net::ClientSession::Standing::joining_again;
                       }),
          "the refusal of a session gone quiet is believed");
    const std::vector<std::uint8_t> watch{1, 2, 3};
    check(!session->send_message(all_of(watch)),
          "a message asked for while joining again is refused, not queued");

    bool answered_again = false;
    std::thread answering([&] { answered_again = server.answer_one(); });
    const bool joined = polled_until(10.0, [&] { return session->joined_again() == 1; });
    answering.join();
    check(joined && answered_again && server.sessions == 2,
          "and it joined again, with a new session");

    // **The session joined again proves itself as a first one does**: its
    // first sealed datagram is lost - read here and thrown away - and it
    // knocks every quarter of a second of its own clock until one opens.
    Address whence;
    const std::vector<std::uint8_t> lost = wait_for(server.socket, whence);
    check(lost.size() > glideslope::net::envelope_size &&
              server.opening->open(std::span<const std::uint8_t>(lost).subspan(
                  glideslope::net::envelope_size)),
          "the new session sealed something at once");
    bool proved = false;
    for (double now_s = 10.0; now_s < 10.9 && !proved; now_s += 0.01) {
        session->poll(now_s);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
        const std::size_t got = server.socket.receive(into, whence);
        proved = got > glideslope::net::envelope_size &&
                 server.opening->open(std::span<const std::uint8_t>(into.data(), got)
                                          .subspan(glideslope::net::envelope_size));
    }
    check(proved, "with that lost, it sealed another within 0.9 s of its clock, before "
                  "any knock of a session gone quiet was due");
    check(session->take_states().empty(),
          "no update of the old session is handed up as the new one's");
    server.send_state(0.5);
    check(polled_until(10.5, [&] { return session->heard() == 2; }),
          "the new session's own update is heard");
    const std::vector<glideslope::net::StatePacket> fresh = session->take_states();
    check(fresh.size() == 1 && fresh[0].simulation_time_s == 0.5,
          "and it is the only one handed up");
}
