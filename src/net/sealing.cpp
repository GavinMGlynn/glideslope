#include "net/sealing.hpp"

#include <sodium.h>

namespace glideslope::net {
namespace {

bool started() {
    static const bool ok = sodium_init() >= 0;
    return ok;
}

// The same shape of nonce the handshake uses: four bytes of nought, then the
// number, least significant byte first.
std::array<std::uint8_t, crypto_aead_chacha20poly1305_ietf_NPUBBYTES> nonce_of(
    std::uint64_t n) {
    std::array<std::uint8_t, crypto_aead_chacha20poly1305_ietf_NPUBBYTES> out{};
    for (std::size_t i = 0; i < 8; ++i) {
        out[4 + i] = static_cast<std::uint8_t>((n >> (8 * i)) & 0xFF);
    }
    return out;
}

void write_sequence(std::vector<std::uint8_t>& out, std::uint64_t n) {
    for (std::size_t i = 0; i < sequence_bytes; ++i) {
        out.push_back(static_cast<std::uint8_t>((n >> (8 * i)) & 0xFF));
    }
}

std::uint64_t read_sequence(std::span<const std::uint8_t> in) {
    std::uint64_t n = 0;
    for (std::size_t i = 0; i < sequence_bytes; ++i) {
        n |= static_cast<std::uint64_t>(in[i]) << (8 * i);
    }
    return n;
}

} // namespace

std::vector<std::uint8_t> Sealer::seal(std::span<const std::uint8_t> plain) {
    if (!started()) {
        return {};
    }
    const std::uint64_t n = next_++;
    std::vector<std::uint8_t> out;
    out.reserve(sequence_bytes + plain.size() + tag_bytes);
    write_sequence(out, n);
    // The number is the additional data, so a changed number does not open.
    const std::size_t at = out.size();
    out.resize(at + plain.size() + tag_bytes);
    unsigned long long wrote = 0;
    const auto nonce = nonce_of(n);
    crypto_aead_chacha20poly1305_ietf_encrypt(out.data() + at, &wrote, plain.data(),
                                              plain.size(), out.data(), sequence_bytes,
                                              nullptr, nonce.data(), key_.bytes.data());
    out.resize(at + static_cast<std::size_t>(wrote));
    return out;
}

std::optional<std::vector<std::uint8_t>> Unsealer::open(
    std::span<const std::uint8_t> sealed) {
    why_ = Refused::nothing;
    if (!started() || sealed.size() < sealing_overhead) {
        why_ = Refused::too_short;
        return std::nullopt;
    }
    const std::uint64_t n = read_sequence(sealed);

    // **The window, before the cipher**: a replay is cheaper to refuse here
    // than after doing the work of opening it.
    if (any_) {
        if (n > highest_) {
            // New and ahead: nothing to check.
        } else if (highest_ - n >= replay_window) {
            why_ = Refused::too_old;
            return std::nullopt;
        } else if ((seen_ >> (highest_ - n)) & 1u) {
            why_ = Refused::replayed;
            return std::nullopt;
        }
    }

    std::vector<std::uint8_t> plain(sealed.size() - sealing_overhead, 0);
    unsigned long long wrote = 0;
    const auto nonce = nonce_of(n);
    const int ok = crypto_aead_chacha20poly1305_ietf_decrypt(
        plain.data(), &wrote, nullptr, sealed.data() + sequence_bytes,
        sealed.size() - sequence_bytes, sealed.data(), sequence_bytes, nonce.data(),
        key_.bytes.data());
    if (ok != 0) {
        why_ = Refused::not_ours;
        return std::nullopt;
    }
    plain.resize(static_cast<std::size_t>(wrote));

    // Only once it has opened is it remembered: a datagram that did not open
    // is not one of ours and must not shift the window.
    if (!any_ || n > highest_) {
        const std::uint64_t moved = any_ ? n - highest_ : 0;
        seen_ = moved >= replay_window ? 0 : (seen_ << moved);
        seen_ |= 1u;
        highest_ = n;
        any_ = true;
    } else {
        seen_ |= std::uint64_t{1} << (highest_ - n);
    }
    return plain;
}

} // namespace glideslope::net
