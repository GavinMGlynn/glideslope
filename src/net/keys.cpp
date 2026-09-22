#include "net/keys.hpp"

#include <sodium.h>

#include <cctype>

namespace glideslope::net {
namespace {

// libsodium wants starting once. It is safe to call more than once and from
// more than one thread, and every entry point here goes through this.
bool started() {
    static const bool ok = sodium_init() >= 0;
    return ok;
}

std::string to_hex(const std::array<std::uint8_t, key_bytes>& bytes) {
    static const char* const digits = "0123456789abcdef";
    std::string out;
    out.reserve(key_hex_digits);
    for (const std::uint8_t b : bytes) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

std::optional<std::array<std::uint8_t, key_bytes>> from_hex(std::string_view text) {
    if (text.size() != key_hex_digits) {
        return std::nullopt;
    }
    std::array<std::uint8_t, key_bytes> out{};
    for (std::size_t i = 0; i < key_bytes; ++i) {
        int value = 0;
        for (std::size_t half = 0; half < 2; ++half) {
            const char c = text[i * 2 + half];
            int digit = 0;
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                digit = c - 'A' + 10;
            } else {
                return std::nullopt;
            }
            value = value * 16 + digit;
        }
        out[i] = static_cast<std::uint8_t>(value);
    }
    return out;
}

} // namespace

std::string secret_for_keeping(const SecretKey& secret) {
    return to_hex(secret.bytes);
}

std::string PublicKey::text() const {
    return to_hex(bytes);
}

KeyPair mint_key_pair() {
    KeyPair out;
    if (!started()) {
        return out;
    }
    // X25519's own key generation: a random secret, clamped, and the public
    // half from the base point.
    crypto_box_curve25519xchacha20poly1305_keypair(out.publik.bytes.data(),
                                                   out.secret.bytes.data());
    return out;
}

PublicKey public_from_secret(const SecretKey& secret) {
    PublicKey out;
    if (!started()) {
        return out;
    }
    crypto_scalarmult_curve25519_base(out.bytes.data(), secret.bytes.data());
    return out;
}

std::optional<SecretKey> secret_from_text(std::string_view text) {
    const auto bytes = from_hex(text);
    if (!bytes) {
        return std::nullopt;
    }
    SecretKey out;
    out.bytes = *bytes;
    return out;
}

std::optional<PublicKey> public_from_text(std::string_view text) {
    const auto bytes = from_hex(text);
    if (!bytes) {
        return std::nullopt;
    }
    PublicKey out;
    out.bytes = *bytes;
    return out;
}

} // namespace glideslope::net
