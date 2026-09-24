#include "harness.hpp"

#include "net/handshake.hpp"
#include "net/keys.hpp"

#include <sodium.h>

#include <algorithm>
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

namespace {

std::vector<std::uint8_t> from_hex(const std::string& hex) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

KeyPair pair_from_hex(const std::string& secret_hex) {
    KeyPair out;
    const std::vector<std::uint8_t> secret = from_hex(secret_hex);
    std::copy(secret.begin(), secret.end(), out.secret.bytes.begin());
    out.publik = glideslope::net::public_from_secret(out.secret);
    return out;
}

// A transport message as Noise sends one after the handshake: ChaCha20-Poly1305
// under a split key, the nonce four bytes of nought then the counter, and no
// additional data.
std::vector<std::uint8_t> noise_transport(const glideslope::net::TrafficKey& key,
                                          std::uint64_t n,
                                          const std::vector<std::uint8_t>& plain) {
    std::array<std::uint8_t, crypto_aead_chacha20poly1305_ietf_NPUBBYTES> nonce{};
    for (std::size_t i = 0; i < 8; ++i) {
        nonce[4 + i] = static_cast<std::uint8_t>((n >> (8 * i)) & 0xFF);
    }
    std::vector<std::uint8_t> out(plain.size() + crypto_aead_chacha20poly1305_ietf_ABYTES);
    unsigned long long wrote = 0;
    crypto_aead_chacha20poly1305_ietf_encrypt(out.data(), &wrote, plain.data(), plain.size(),
                                              nullptr, 0, nullptr, nonce.data(),
                                              key.bytes.data());
    out.resize(static_cast<std::size_t>(wrote));
    return out;
}

} // namespace

// **The handshake is Noise's own, byte for byte.** The Noise community's
// `cacophony` known-answer vector for `Noise_IK_25519_ChaChaPoly_BLAKE2b`
// (haskell-cryptography/cacophony, vectors/cacophony.txt, public domain) fixes
// every key, the prologue and the payloads, and gives the exact bytes of both
// handshake messages and of four transport messages sealed under the keys
// the handshake splits into. Both ends here must write exactly those bytes.
// Until 2026-09-24 this suite cut BLAKE2b to 32 bytes and mixed in no
// prologue, and no standard Noise could talk to it: this is the test that
// says it now can. Seen to fail, in writing it, with each DH output mixed
// in padded out to the 64-byte hash.
GLIDESLOPE_TEST(the_handshake_is_noise_ik_byte_for_byte_as_the_cacophony_vector_has_it) {
    const std::vector<std::uint8_t> prologue = from_hex("4a6f686e2047616c74");
    const KeyPair init_static =
        pair_from_hex("e61ef9919cde45dd5f82166404bd08e38bceb5dfdfded0a34c8df7ed542214d1");
    const KeyPair init_ephemeral =
        pair_from_hex("893e28b9dc6ca8d611ab664754b8ceb7bac5117349a4439a6b0569da977c464a");
    const KeyPair resp_static =
        pair_from_hex("4a3acbfdb163dec651dfa3194dece676d437029c62a408b4c5ea9114246e4893");
    const KeyPair resp_ephemeral =
        pair_from_hex("bbdb4cdbd309f1a1f2e1456967fe288cadd6f712d65dc7b7793d5e63da6b375b");
    check(resp_static.publik.text() ==
              "31e0303fd6418d2f8c0e78b91f22e8caed0fbe48656dcf4767e4834f701b8f62",
          "the responder's static public key is the vector's init_remote_static");

    struct Message {
        const char* payload;
        const char* ciphertext;
    };
    const std::array<Message, 6> messages{{
        {"4c756477696720766f6e204d69736573",
         "ca35def5ae56cec33dc2036731ab14896bc4c75dbb07a61f879f8e3afa4c7944ba83a447b38c83e327"
         "ad936929812f624884847b7831e95e197b2f797088efdd2f88f1db7e1fb0e99c64419097af91cee64e"
         "470f4b6fcd9298ce0b56fe20f86e13bf70439c538e3602a7127af71a29cc"},
        {"4d757272617920526f746862617264",
         "95ebc60d2b1fa672c1f46a8aa265ef51bfe38e7ccb39ec5be34069f1448088439f069b267a06b3de3e"
         "cb1043bcb098e9af91d9c64748d998c7b47890871571"},
        {"462e20412e20486179656b", "cd54383060e7a28434cca27fb1cc524cfbabeb18181589df219d07"},
        {"4361726c204d656e676572", "a856d3bf0246bfc476c655009cd1ed677b8dcc5b349ae8ef2a05f2"},
        {"4a65616e2d426170746973746520536179",
         "49063084b2c51f098337cb8a13739ac848f907e67cfb2cc8a8b60586467aa02fc7"},
        {"457567656e2042f6686d20766f6e2042617765726b",
         "8b9709d23b47e4639df7678d7a21741eba4ef1e9c60383001c7435549c20f9d56f30e935d3"},
    }};
    std::size_t checked = 0;

    Initiator initiator(init_static, resp_static.publik, all_of(prologue), init_ephemeral);
    const std::vector<std::uint8_t> first = initiator.begin(all_of(from_hex(messages[0].payload)));
    check(first == from_hex(messages[0].ciphertext),
          "the initiation is the vector's first message, byte for byte");
    ++checked;

    Responder responder(resp_static, all_of(prologue), resp_ephemeral);
    std::vector<std::uint8_t> heard;
    const auto answer =
        responder.answer(all_of(first), all_of(from_hex(messages[1].payload)), &heard);
    check(answer.has_value(), "the responder reads the vector's first message");
    check(heard == from_hex(messages[0].payload), "and finds the first payload in it");
    check(answer->message == from_hex(messages[1].ciphertext),
          "the answer is the vector's second message, byte for byte");
    check(answer->session.theirs == init_static.publik, "the responder learns who it was");
    ++checked;

    std::vector<std::uint8_t> answered;
    const auto session = initiator.finish(all_of(answer->message), &answered);
    check(session.has_value(), "the initiator reads the answer");
    check(answered == from_hex(messages[1].payload), "and finds the second payload in it");

    // Then the transport: initiator and responder in turn, each counting its
    // own nonces from nought.
    for (std::size_t i = 2; i < messages.size(); ++i) {
        const bool from_initiator = i % 2 == 0;
        const glideslope::net::TrafficKey& key =
            from_initiator ? session->sending : answer->session.sending;
        const std::uint64_t n = (i - 2) / 2;
        check(noise_transport(key, n, from_hex(messages[i].payload)) ==
                  from_hex(messages[i].ciphertext),
              "transport message " + std::to_string(i + 1) +
                  " seals to the vector's bytes under the split key");
        ++checked;
    }
    check(session->receiving == answer->session.sending &&
              session->sending == answer->session.receiving,
          "and the two ends' keys are each other's");
    check(checked == messages.size(),
          "all six of the vector's messages were checked: " + std::to_string(checked));
}
