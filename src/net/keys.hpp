#pragma once

// The server's static key, and a client's knowledge of it.
//
// **The handshake this project specifies is `Noise_IK`**, whose whole point
// is that the initiator knows the responder's static public key before it
// says anything. So the server has a long-lived X25519 key pair: it keeps the
// secret half and prints the public half at startup, and a client is given
// that out of band - `--server-key` on its command line, or a line in
// `server.txt` (REQUIREMENTS.md 6.6, 6.7).
//
// **This is the key handling and nothing else.** It mints a pair, reads one
// written down, and says what a public half is. It performs no handshake and
// seals nothing; those wait on the decision recorded against the transport
// item in `docs/COMPLETION_PLAN.md`.
//
// **A secret read from a file or a command line is still a secret.** Nothing
// here writes one to a log, and `PublicKey` is the only half with a `text()`.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace glideslope::net {

// X25519's keys are 32 bytes, and are written down as 64 hexadecimal digits.
inline constexpr std::size_t key_bytes = 32;
inline constexpr std::size_t key_hex_digits = key_bytes * 2;

struct PublicKey {
    std::array<std::uint8_t, key_bytes> bytes{};
    // Lower-case hexadecimal, 64 digits.
    std::string text() const;
    bool operator==(const PublicKey&) const = default;
};

struct SecretKey {
    std::array<std::uint8_t, key_bytes> bytes{};
    bool operator==(const SecretKey&) const = default;
};

struct KeyPair {
    SecretKey secret;
    PublicKey publik; // `public` is a keyword
};

// A fresh pair from the system's randomness.
KeyPair mint_key_pair();

// The public half a secret goes with.
PublicKey public_from_secret(const SecretKey& secret);

// A secret written down: 64 hexadecimal digits, in either case. Nothing if it
// is not one.
std::optional<SecretKey> secret_from_text(std::string_view text);
// The same for a public half, which is what a client is given.
std::optional<PublicKey> public_from_text(std::string_view text);

} // namespace glideslope::net
