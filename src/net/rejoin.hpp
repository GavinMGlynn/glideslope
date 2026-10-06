#pragma once

// **Joining again, as a client the server has let go**: one piece both
// clients use - `net::ClientSession` and `glideslope_cli connect` - so that
// a rule here is kept by both, rather than fixed in one and not the other.
//
// A fresh initiation under the same static key - a new ephemeral key, so not
// a copy of the one the server took from this address, which it drops - is
// resent until it is answered. **The old session is knocked on meanwhile**,
// under its own keys, with a token of this attempt's own: a server that still
// has it answers `PONG` with that token, and the client goes back to it - the
// refusal that ended it was forged, or a blip, and the server drops the new
// initiation from that address without a word.
//
// **Only that answer is a reason to go back.** Anything else that opens under
// the old keys may have been sealed before the server let the session go and
// held on the way: going back on it went back to a session already gone,
// while the server admitted the new initiation - a ghost session holding a
// slot and an aircraft until its timeout, and the client lost for about 13 s.
// A `PONG` carrying a token first sent after the refusal was believed was
// sealed by a server that had the session after that, which nothing held from
// before can be. What opens otherwise is counted (`stale()`) and dropped.
//
// Refusals from the server's address: `SERVER_FULL` and `DROPPED` end it.
// `BAD_HANDSHAKE` does not - sealed datagrams sent under the old session,
// knocks among them, may still be on their way to be refused.

#include "net/handshake.hpp"
#include "net/keys.hpp"
#include "net/protocol.hpp"
#include "net/sealing.hpp"
#include "platform/socket.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace glideslope::net {

// **Whether a datagram is the server's refusal, and for what**: only from the
// server's own address, and only a refusal's shape. Anything else is nothing.
std::optional<Refusal> refusal_from(const platform::Address& server,
                                    const platform::Address& from,
                                    std::span<const std::uint8_t> datagram);

class Rejoin {
public:
    // The old session's keys are the caller's, and must outlive this.
    Rejoin(const KeyPair& mine, const PublicKey& theirs, Sealer& old_sealing,
           Unsealer& old_opening);

    // **What to send now**: the initiation and a knock under the old session,
    // every `every_s` from the first call. Nothing between.
    std::vector<std::vector<std::uint8_t>> due(double now_s);
    static constexpr double every_s = 0.25;

    enum class Heard { nothing, old_session_answers, joined, server_full, dropped };
    // **What one datagram says.** `joined`: `keys()` are the new session's.
    Heard hear(const platform::Address& server, const platform::Address& from,
               std::span<const std::uint8_t> datagram);

    const SessionKeys& keys() const { return *keys_; }
    // The initiation, envelope and all: what the new session was begun by.
    const std::vector<std::uint8_t>& initiation() const { return initiation_; }
    // How many datagrams opened under the old session that were not its
    // answer: held from before, or sent before the knock arrived.
    int stale() const { return stale_; }
    std::uint64_t token() const { return token_; }

private:
    Initiator initiator_;
    std::vector<std::uint8_t> initiation_;
    Sealer& old_sealing_;
    Unsealer& old_opening_;
    std::uint64_t token_ = 0;
    std::optional<double> sent_s_;
    std::optional<SessionKeys> keys_;
    int stale_ = 0;
};

} // namespace glideslope::net
