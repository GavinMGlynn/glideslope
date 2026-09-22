#include "harness.hpp"

#include "net/handshake.hpp"
#include "net/keys.hpp"

#include <array>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

using glideslope::net::Initiator;
using glideslope::net::KeyPair;
using glideslope::net::Responder;
using glideslope::net::SessionKeys;
using glideslope::test::check;

namespace {

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

std::vector<std::uint8_t> bytes_of(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

} // namespace

// **Two honest ends complete the handshake and agree.** Each ends up sending
// under the key the other receives under, and each learns who the other is.
GLIDESLOPE_TEST(two_honest_ends_complete_the_handshake_and_agree_on_their_keys) {
    const KeyPair server = glideslope::net::mint_key_pair();
    const KeyPair client = glideslope::net::mint_key_pair();

    Initiator initiator(client, server.publik);
    const std::vector<std::uint8_t> hello = bytes_of("glideslope client");
    const std::vector<std::uint8_t> first = initiator.begin(all_of(hello));
    check(!first.empty(), "the initiation is written");

    Responder responder(server);
    std::vector<std::uint8_t> heard;
    const std::vector<std::uint8_t> welcome = bytes_of("glideslope server");
    const auto answer = responder.answer(all_of(first), all_of(welcome), &heard);
    check(answer.has_value(), "the server reads it and answers");
    check(heard == hello, "and heard what the client said");
    check(answer->session.theirs == client.publik,
          "and learned which client it was");

    std::vector<std::uint8_t> back;
    const auto session = initiator.finish(all_of(answer->message), &back);
    check(session.has_value(), "the client reads the answer");
    check(back == welcome, "and heard what the server said");
    check(session->theirs == server.publik, "and knows which server it was");

    // **The keys match, crosswise.** What one sends under, the other
    // receives under.
    check(session->sending == answer->session.receiving,
          "the client sends under the key the server receives under");
    check(session->receiving == answer->session.sending,
          "and the other way round");
    check(!(session->sending == session->receiving),
          "and the two directions are not the same key");
    std::printf("  a session agreed, and the two directions differ\n");
}

// **Every handshake is its own.** Two runs with the same keys must not give
// the same session, or a recording of one would be a recording of all.
GLIDESLOPE_TEST(two_handshakes_with_the_same_keys_give_different_sessions) {
    const KeyPair server = glideslope::net::mint_key_pair();
    const KeyPair client = glideslope::net::mint_key_pair();
    std::vector<SessionKeys> sessions;
    for (int i = 0; i < 8; ++i) {
        Initiator initiator(client, server.publik);
        Responder responder(server);
        const auto answer = responder.answer(all_of(initiator.begin()));
        check(answer.has_value(), "it completes");
        const auto session = initiator.finish(all_of(answer->message));
        check(session.has_value(), "both ways");
        for (const SessionKeys& before : sessions) {
            check(!(before.sending == session->sending),
                  "no two handshakes gave the same key");
        }
        sessions.push_back(*session);
    }
    check(sessions.size() == 8, "eight handshakes were run");
}

// **A client that has the wrong server key gets nowhere**, which is the whole
// point of IK: the first message is already encrypted to a particular server.
GLIDESLOPE_TEST(a_client_with_the_wrong_server_key_gets_nowhere) {
    const KeyPair server = glideslope::net::mint_key_pair();
    const KeyPair impostor = glideslope::net::mint_key_pair();
    const KeyPair client = glideslope::net::mint_key_pair();

    Initiator initiator(client, impostor.publik); // told the wrong key
    Responder responder(server);
    check(!responder.answer(all_of(initiator.begin())).has_value(),
          "the server cannot read an initiation meant for somebody else");

    // And a server that is not the one addressed cannot read it either.
    Initiator honest(client, server.publik);
    Responder wrong(impostor);
    check(!wrong.answer(all_of(honest.begin())).has_value(),
          "nor can a server with another key read one meant for this one");
}

// **Every single-byte change to an initiation is refused**, and every
// truncation of one. This is the whole space of one-byte changes: each byte,
// each of the 255 other values it could hold.
GLIDESLOPE_TEST(every_change_to_an_initiation_is_refused) {
    const KeyPair server = glideslope::net::mint_key_pair();
    const KeyPair client = glideslope::net::mint_key_pair();
    Initiator initiator(client, server.publik);
    const std::vector<std::uint8_t> first = initiator.begin(bytes_of("hello"));

    std::size_t walked = 0;
    std::size_t accepted = 0;
    for (std::size_t at = 0; at < first.size(); ++at) {
        for (int v = 0; v < 256; ++v) {
            const auto value = static_cast<std::uint8_t>(v);
            if (value == first[at]) {
                continue;
            }
            std::vector<std::uint8_t> changed = first;
            changed[at] = value;
            Responder responder(server);
            if (responder.answer(all_of(changed)).has_value()) {
                ++accepted;
            }
            ++walked;
        }
    }
    check(walked == first.size() * 255,
          "every single-byte change was tried: " + std::to_string(walked));
    check(accepted == 0, std::to_string(accepted) +
                             " changed initiations were accepted, and none should be");

    std::size_t cut = 0;
    for (std::size_t n = 0; n < first.size(); ++n) {
        Responder responder(server);
        check(!responder.answer(std::span<const std::uint8_t>(first.data(), n))
                   .has_value(),
              "an initiation cut to " + std::to_string(n) + " bytes is refused");
        ++cut;
    }
    // And one with anything after it.
    std::vector<std::uint8_t> longer = first;
    longer.push_back(0);
    Responder responder(server);
    check(!responder.answer(all_of(longer)).has_value(),
          "and one with a byte after it");
    std::printf("  %zu one-byte changes and %zu truncations, all refused\n", walked,
                cut);
}

// **Every change to an answer is refused too**, so that a server's reply
// cannot be tampered with on the way back.
GLIDESLOPE_TEST(every_change_to_a_handshake_answer_is_refused) {
    const KeyPair server = glideslope::net::mint_key_pair();
    const KeyPair client = glideslope::net::mint_key_pair();
    Initiator initiator(client, server.publik);
    const std::vector<std::uint8_t> first = initiator.begin();
    Responder responder(server);
    const auto answer = responder.answer(all_of(first));
    check(answer.has_value(), "there is an answer to change");

    std::size_t walked = 0;
    std::size_t accepted = 0;
    for (std::size_t at = 0; at < answer->message.size(); ++at) {
        for (const std::uint8_t value : std::array<std::uint8_t, 4>{0x00, 0x01, 0x7F,
                                                                   0xFF}) {
            // A fresh handshake each time: finishing one twice is not the
            // case under test.
            Initiator again(client, server.publik);
            const std::vector<std::uint8_t> theirs = again.begin();
            Responder fresh(server);
            const auto fresh_answer = fresh.answer(all_of(theirs));
            check(fresh_answer.has_value(), "the fresh handshake got an answer");
            // **Skip on this answer's own byte, not the first one's.** The
            // ephemeral key differs every run, so a value that changes the
            // first answer may leave this one untouched - and an untouched
            // answer is rightly accepted, which looked like a hole until it
            // was read.
            if (value == fresh_answer->message[at]) {
                continue;
            }
            std::vector<std::uint8_t> broken = fresh_answer->message;
            broken[at] = value;
            if (again.finish(all_of(broken)).has_value()) {
                ++accepted;
            }
            ++walked;
        }
    }
    check(walked > 100, "changes across the whole answer were tried: " +
                            std::to_string(walked));
    check(accepted == 0, std::to_string(accepted) +
                             " changed answers were accepted, and none should be");
    std::printf("  %zu changes to an answer, all refused\n", walked);
}

// **A replayed initiation buys nothing.** The responder will answer it - it
// has no memory, and cannot tell - but the session it makes is a new one that
// the replayer cannot read, because it has not got the initiator's ephemeral
// secret. **It is still work the server did for a stranger**, which is a
// denial of service and is named in `docs/THREATS.md` rather than defended
// here.
GLIDESLOPE_TEST(a_replayed_initiation_makes_a_session_the_replayer_cannot_read) {
    const KeyPair server = glideslope::net::mint_key_pair();
    const KeyPair client = glideslope::net::mint_key_pair();
    Initiator initiator(client, server.publik);
    const std::vector<std::uint8_t> first = initiator.begin();

    Responder once(server);
    Responder twice(server);
    const auto a = once.answer(all_of(first));
    const auto b = twice.answer(all_of(first));
    check(a.has_value() && b.has_value(), "the server answers a replay too");
    check(!(a->session.sending == b->session.sending),
          "but the two sessions differ, so the replay gained nothing");
    check(a->session.theirs == client.publik && b->session.theirs == client.publik,
          "both name the client the initiation really came from");
}
