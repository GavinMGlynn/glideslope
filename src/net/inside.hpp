#pragma once

// What a sealed body carries.
//
// **Everything that is not a handshake or a refusal goes inside a `SEALED`
// datagram** (`REQUIREMENTS.md` 6.7), and there are several kinds of thing to
// send: the reliable layer's datagrams carrying the seven messages, a
// client's input packets, a server's state updates, the keepalive that
// measures a round trip, and a client's goodbye. They share one envelope type, so the plaintext
// inside begins with a byte saying which of them it is.
//
// **This byte is inside the seal, not in front of it.** It is not in the
// envelope, because the envelope is in the clear and nothing outside a
// session needs to know which of these a datagram is.
//
// Every kind is written up in `docs/TRANSPORT.md`, "What is inside a sealed
// body".

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace glideslope::net {

enum class Inside : std::uint8_t {
    // The reliable layer's datagram, carrying one of the seven messages.
    reliable = 1,
    // A client's input packet.
    inputs = 2,
    // A server's state update.
    state = 3,
    // A keepalive, and how a round trip is measured: eight bytes the other
    // end sends straight back.
    ping = 4,
    pong = 5,
    // **A client saying it is leaving**: the kind's byte and nothing else. The
    // server lets its session go at once, as `--timeout` would have. It is
    // sealed like everything here, so only the session's own client can send
    // one; it is not reliable, because the client is going away and will not
    // wait to hear it acknowledged, so it is sent `leaving_copies` times.
    leaving = 6,
};

// **How many times a client sends its goodbye**, each sealed afresh - a
// datagram sealed once and sent twice would be the replay the window refuses.
// The first that arrives lets the session go; if every one is lost, the
// server's `--timeout` still does, later.
inline constexpr int leaving_copies = 3;
// Whether `body` is a goodbye: exactly the one byte. A longer one is not a
// goodbye with something after it; it is not a goodbye.
bool is_leaving(std::span<const std::uint8_t> body);

// Whether `kind` is one this version knows. A kind it does not know is not a
// sealed body it can read, and the datagram is dropped.
bool known_inside(std::uint8_t kind);

// A `ping` or a `pong` carrying `token`: the kind's byte then the token as a
// little-endian `u64`, which is nine bytes.
inline constexpr std::size_t knock_size = 9;
std::vector<std::uint8_t> knock(Inside kind, std::uint64_t token);

// The token out of a `ping` or a `pong`. Nothing if `body` is not one of
// exactly that shape - a longer one is not a knock with something after it,
// it is not a knock.
std::optional<std::uint64_t> knock_token(Inside kind,
                                         std::span<const std::uint8_t> body);

} // namespace glideslope::net
