#pragma once

// Sealing: what a `SEALED` datagram's body actually is.
//
// **Everything after the envelope is ciphertext**, under the keys the
// handshake agreed (net/handshake.hpp). Each end seals under the key it sends
// with and opens under the key it receives with, so the two directions never
// share a key and a datagram cannot be reflected back at its sender.
//
// **Each sealed body carries its own sequence number**, because UDP reorders
// and duplicates and the cipher needs a nonce that is never used twice. The
// number is that nonce, and it is sent in the clear: it is not a secret, and
// the tag over the body covers it, so changing it only makes the body fail to
// open.
//
// **A replay window, because a number alone is not enough.** An attacker who
// records a datagram can send it again. The opener remembers the highest
// number it has seen and a window of `window_bytes * 8` behind it, and
// refuses anything it has already opened or anything older than the window.
// That is the standard sliding window - the same shape IPsec and WireGuard
// use - and its limit is stated rather than implied: a datagram delayed by
// more than the window is indistinguishable from a replay and is refused.
//
// **Nothing here touches a socket.** It turns a body into a sealed body and
// back, which is what lets every order and every duplication be walked.

#include "net/handshake.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace glideslope::net {

// The sequence number in front of every sealed body.
inline constexpr std::size_t sequence_bytes = 8;
// How far behind the newest a datagram may arrive and still be opened.
inline constexpr std::size_t replay_window = 64;

// What a sealed body costs over the plaintext inside it.
inline constexpr std::size_t sealing_overhead = sequence_bytes + tag_bytes;

// **The sending end.** One per direction; it must not be shared, because two
// bodies sealed under one key with one number would be a broken cipher
// rather than a bug in a game.
class Sealer {
public:
    explicit Sealer(const TrafficKey& key) : key_(key) {}

    // Seals `plain` into a body ready to follow a `SEALED` envelope.
    std::vector<std::uint8_t> seal(std::span<const std::uint8_t> plain);

    // How many have been sealed, which is the next number to be used.
    std::uint64_t sealed() const { return next_; }

private:
    TrafficKey key_;
    std::uint64_t next_ = 0;
};

// **The receiving end.**
class Unsealer {
public:
    explicit Unsealer(const TrafficKey& key) : key_(key) {}

    // Opens a sealed body. Nothing if it is not one, if it has been opened
    // before, or if it is older than the window.
    std::optional<std::vector<std::uint8_t>> open(std::span<const std::uint8_t> sealed);

    // Why the last `open` answered nothing, for saying so in a log.
    enum class Refused : std::uint8_t {
        nothing = 0, // it opened
        too_short,
        replayed,   // this exact number has been opened already
        too_old,    // older than the window
        not_ours,   // it did not open under this key
    };
    Refused why() const { return why_; }

    std::uint64_t highest() const { return highest_; }

private:
    TrafficKey key_;
    // The newest number opened, and a window of the ones before it.
    std::uint64_t highest_ = 0;
    std::uint64_t seen_ = 0;
    bool any_ = false;
    Refused why_ = Refused::nothing;
};

} // namespace glideslope::net
