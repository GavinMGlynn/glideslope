#pragma once

// The handshake: `Noise_IK_25519_ChaChaPoly_BLAKE2b`.
//
// **The suite is not quite the one `REQUIREMENTS.md` 6.7 names, and this is
// the reason.** It asks for `..._BLAKE2s`. libsodium - the library that
// section chooses for the primitives - provides BLAKE2b and no BLAKE2s at
// all: its headers offer `crypto_generichash_blake2b` and nothing else of
// that family. BLAKE2b is a hash the Noise specification itself defines, so
// `Noise_IK_25519_ChaChaPoly_BLAKE2b` is a real suite rather than an
// invention; the alternatives were SHA-256, also native to libsodium, or
// carrying a BLAKE2s of our own, which would mean hand-written cryptography
// in a project that has none. **The project owner has not ruled on it**, and
// the choice is one line: `hash_name` below, and the hash functions it names.
//
// **What Noise IK is for.** The initiator knows the responder's static public
// key before it says anything - the server prints it at startup and the
// client is given it out of band - so the first message is already encrypted
// to the right server, and a client that has the wrong key gets nowhere. The
// server learns who the client is inside that first message.
//
//   IK:
//     <- s
//     ...
//     -> e, es, s, ss
//     <- e, ee, se
//
// **What this does not claim.** It is not reviewed cryptography. It is a
// careful reading of the Noise specification (revision 34) built on
// libsodium's primitives, held to the Noise community's own known-answer
// vector for this exact suite - the `cacophony` vectors, byte for byte, the
// handshake and the transport keys it splits into - and by tests that say a
// wrong key, a changed byte, a replayed message and a truncated one all fail.
// `docs/THREATS.md` says what is defended.
//
// **It is Noise as the specification writes it**, since 2026-09-24. Before,
// BLAKE2b was cut to 32 bytes where Noise's is 64, the 33-byte protocol name
// was cut to fit, and no prologue was mixed in, so a client built on any
// standard Noise library could not complete it.

#include "net/keys.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace glideslope::net {

// The name the handshake is bound to. Both ends hash it before anything
// else, so two builds that disagree about it cannot talk at all.
inline constexpr std::string_view handshake_name =
    "Noise_IK_25519_ChaChaPoly_BLAKE2b";

// Noise's BLAKE2b is 64 bytes, as is its chaining key; its cipher keys are
// the first 32 bytes of what the HKDF gives.
inline constexpr std::size_t hash_bytes = 64;
inline constexpr std::size_t cipher_key_bytes = 32;
// ChaCha20-Poly1305's tag.
inline constexpr std::size_t tag_bytes = 16;

// A key for sealing traffic once the handshake is done.
struct TrafficKey {
    std::array<std::uint8_t, cipher_key_bytes> bytes{};
    bool operator==(const TrafficKey&) const = default;
};

// What each end has when the handshake completes: a key to send under and a
// key to receive under, and who the other end turned out to be.
//
// **It is `SessionKeys` and not `Session`** because `net::Session` is already
// the `SESSION` message's own structure - the one that says what a session is
// called and when it began. The compiler caught the clash; the two are
// different things and now read as different things.
struct SessionKeys {
    TrafficKey sending;
    TrafficKey receiving;
    PublicKey theirs;
};

// **The initiator**, which is the client. It must already know the server's
// static public key.
class Initiator {
public:
    // `prologue` is Noise's: data both ends must agree on, mixed into the
    // hash first. The protocol uses none. `ephemeral` is for a known-answer
    // test only, which must fix the key a real handshake makes fresh.
    Initiator(const KeyPair& mine, const PublicKey& theirs,
              std::span<const std::uint8_t> prologue = {},
              std::optional<KeyPair> ephemeral = std::nullopt);

    // The first message, to be sent as a HANDSHAKE_INITIATION body.
    std::vector<std::uint8_t> begin(std::span<const std::uint8_t> payload = {});

    // The responder's answer. Nothing if it is not one - a wrong key, a
    // changed byte, a message that is too short - and the session if it is.
    std::optional<SessionKeys> finish(std::span<const std::uint8_t> response,
                                  std::vector<std::uint8_t>* payload = nullptr);

private:
    KeyPair mine_;
    PublicKey theirs_;
    std::vector<std::uint8_t> prologue_;
    std::optional<KeyPair> fixed_ephemeral_;
    KeyPair ephemeral_;
    std::array<std::uint8_t, hash_bytes> h_{};
    std::array<std::uint8_t, hash_bytes> ck_{};
    std::array<std::uint8_t, cipher_key_bytes> k_{};
    std::uint64_t n_ = 0;
    bool have_key_ = false;
    bool begun_ = false;
};

// **The responder**, which is the server.
class Responder {
public:
    // `prologue` and `ephemeral` as for the initiator.
    explicit Responder(const KeyPair& mine, std::span<const std::uint8_t> prologue = {},
                       std::optional<KeyPair> ephemeral = std::nullopt);

    // Reads the first message and writes the answer. Nothing if the message
    // is not one; the session and the answer if it is.
    struct Answer {
        std::vector<std::uint8_t> message;
        SessionKeys session;
    };
    std::optional<Answer> answer(std::span<const std::uint8_t> initiation,
                                 std::span<const std::uint8_t> payload = {},
                                 std::vector<std::uint8_t>* theirs_payload = nullptr);

private:
    KeyPair mine_;
    std::vector<std::uint8_t> prologue_;
    std::optional<KeyPair> fixed_ephemeral_;
};

} // namespace glideslope::net
