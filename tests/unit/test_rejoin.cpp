#include "harness.hpp"

#include "net/handshake.hpp"
#include "net/inside.hpp"
#include "net/keys.hpp"
#include "net/protocol.hpp"
#include "net/rejoin.hpp"
#include "net/sealing.hpp"
#include "platform/socket.hpp"

#include <span>
#include <vector>

using glideslope::net::Rejoin;
using glideslope::test::check;

namespace {

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

// A whole `SEALED` datagram of `plain`, as the server sends one.
std::vector<std::uint8_t> sealed(glideslope::net::Sealer& sealer,
                                 const std::vector<std::uint8_t>& plain) {
    glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::sealed);
    w.bytes(sealer.seal(all_of(plain)));
    return w.take();
}

} // namespace

// **A client joining again goes back to its old session only on that
// session's answer to a knock of this attempt's own.** An update the server
// sealed before it let the session go opens under the old keys as well as a
// live one, and going back on it went back to a dead session while the
// server admitted the new initiation: a ghost holding a slot and an aircraft
// for its timeout. So: an update, a `PONG` to some other knock, and garbage
// are each heard as nothing; the `PONG` carrying the token the knock sent
// under the old keys is the old session answering; and the server's answer
// to the initiation is a new session. Every arm of `Rejoin::hear` but the
// two refusals, which session_over_a_socket's tests reach.
GLIDESLOPE_TEST(joining_again_goes_back_only_on_the_old_sessions_answer_to_its_own_knock) {
    const glideslope::net::KeyPair server_key = glideslope::net::mint_key_pair();
    const glideslope::net::KeyPair client_key = glideslope::net::mint_key_pair();
    // The old session: one key each way.
    glideslope::net::TrafficKey down, up;
    down.bytes.fill(7);
    up.bytes.fill(9);
    glideslope::net::Sealer server_sealing(down);
    glideslope::net::Unsealer server_opening(up);
    glideslope::net::Sealer old_sealing(up);
    glideslope::net::Unsealer old_opening(down);
    // Ahead of the client, as a server's numbering is when it lets go.
    for (int i = 0; i < 5; ++i) {
        (void)server_sealing.seal(all_of(std::vector<std::uint8_t>{0}));
    }
    const auto server = glideslope::platform::address_of("192.0.2.1:9");
    check(server.has_value(), "an address for the server");

    Rejoin rejoin(client_key, server_key.publik, old_sealing, old_opening);
    const auto first = rejoin.due(0.0);
    check(first.size() == 2, "the first call sends the initiation and a knock");
    check(rejoin.due(0.1).empty(), "nothing again before a quarter of a second");
    check(rejoin.due(0.25).size() == 2, "both again after a quarter of a second");
    check(first[0] == rejoin.initiation(), "the initiation is the one it began with");

    // The knock opens under the old session as a `PING` carrying its token.
    const auto knocked = server_opening.open(all_of(first[1]).subspan(glideslope::net::envelope_size));
    check(knocked.has_value(), "the knock is sealed under the old session");
    const auto token = glideslope::net::knock_token(glideslope::net::Inside::ping, all_of(*knocked));
    check(token == rejoin.token(), "the knock carries this attempt's token");

    // An update held from before the let-go: opens, and is not the answer.
    const std::vector<std::uint8_t> update{static_cast<std::uint8_t>(0x10), 1, 2, 3, 4};
    check(rejoin.hear(*server, *server, all_of(sealed(server_sealing, update))) ==
              Rejoin::Heard::nothing,
          "an update under the old session is not a reason to go back");
    const std::vector<std::uint8_t> other_pong =
        glideslope::net::knock(glideslope::net::Inside::pong, rejoin.token() + 1);
    check(rejoin.hear(*server, *server, all_of(sealed(server_sealing, other_pong))) ==
              Rejoin::Heard::nothing,
          "a PONG to another knock is not a reason to go back");
    check(rejoin.stale() == 2, "both were counted as opened and not gone back for");
    const std::vector<std::uint8_t> garbage(40, 0);
    check(rejoin.hear(*server, *server, all_of(garbage)) == Rejoin::Heard::nothing,
          "what opens under nothing is nothing");

    // The old session's own answer: back to it.
    const std::vector<std::uint8_t> pong =
        glideslope::net::knock(glideslope::net::Inside::pong, rejoin.token());
    check(rejoin.hear(*server, *server, all_of(sealed(server_sealing, pong))) ==
              Rejoin::Heard::old_session_answers,
          "the PONG to this attempt's knock is the old session answering");

    // Another attempt, answered by a server that let the old session go.
    Rejoin again(client_key, server_key.publik, old_sealing, old_opening);
    check(again.token() != rejoin.token(), "each attempt knocks with a token of its own");
    const auto sent = again.due(0.0);
    glideslope::net::Responder responder(server_key);
    const auto answer = responder.answer(all_of(sent[0]).subspan(glideslope::net::envelope_size));
    check(answer.has_value(), "the server answers the new initiation");
    glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::handshake_response);
    w.bytes(answer->message);
    check(again.hear(*server, *server, all_of(w.take())) == Rejoin::Heard::joined,
          "the answer to the initiation is a new session");
    check(again.keys().receiving == answer->session.sending,
          "the new session's keys are the server's");
}
