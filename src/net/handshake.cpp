#include "net/handshake.hpp"

#include <sodium.h>

#include <algorithm>
#include <cstring>

namespace glideslope::net {
namespace {

using Hash = std::array<std::uint8_t, hash_bytes>;
using CipherKey = std::array<std::uint8_t, cipher_key_bytes>;
// X25519's output, which is 32 bytes whatever the hash is: held in a `Hash`
// once that was 64 bytes, a DH mixed its 32 and 32 zeros into the key.
using Shared = std::array<std::uint8_t, key_bytes>;

bool started() {
    static const bool ok = sodium_init() >= 0;
    return ok;
}

// BLAKE2b, with its full 64-byte output, as Noise's HASHLEN for it is 64.
Hash hash_of(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b = {}) {
    Hash out{};
    crypto_generichash_blake2b_state state;
    crypto_generichash_blake2b_init(&state, nullptr, 0, out.size());
    if (!a.empty()) {
        crypto_generichash_blake2b_update(&state, a.data(), a.size());
    }
    if (!b.empty()) {
        crypto_generichash_blake2b_update(&state, b.data(), b.size());
    }
    crypto_generichash_blake2b_final(&state, out.data(), out.size());
    return out;
}

// HMAC over the hash, as Noise's HKDF needs. BLAKE2b's block is 128 bytes.
constexpr std::size_t block_bytes = 128;

Hash hmac(const Hash& key, std::span<const std::uint8_t> data,
          std::span<const std::uint8_t> more = {}) {
    std::array<std::uint8_t, block_bytes> pad{};
    std::copy(key.begin(), key.end(), pad.begin());
    std::array<std::uint8_t, block_bytes> inner{};
    std::array<std::uint8_t, block_bytes> outer{};
    for (std::size_t i = 0; i < block_bytes; ++i) {
        inner[i] = static_cast<std::uint8_t>(pad[i] ^ 0x36);
        outer[i] = static_cast<std::uint8_t>(pad[i] ^ 0x5C);
    }
    crypto_generichash_blake2b_state state;
    Hash first{};
    crypto_generichash_blake2b_init(&state, nullptr, 0, first.size());
    crypto_generichash_blake2b_update(&state, inner.data(), inner.size());
    if (!data.empty()) {
        crypto_generichash_blake2b_update(&state, data.data(), data.size());
    }
    if (!more.empty()) {
        crypto_generichash_blake2b_update(&state, more.data(), more.size());
    }
    crypto_generichash_blake2b_final(&state, first.data(), first.size());

    Hash out{};
    crypto_generichash_blake2b_init(&state, nullptr, 0, out.size());
    crypto_generichash_blake2b_update(&state, outer.data(), outer.size());
    crypto_generichash_blake2b_update(&state, first.data(), first.size());
    crypto_generichash_blake2b_final(&state, out.data(), out.size());
    return out;
}

// Noise's HKDF: two outputs from a chaining key and some input material.
void hkdf2(const Hash& ck, std::span<const std::uint8_t> ikm, Hash& out1, Hash& out2) {
    const Hash temp = hmac(ck, ikm);
    const std::array<std::uint8_t, 1> one{0x01};
    out1 = hmac(temp, one);
    const std::array<std::uint8_t, 1> two{0x02};
    out2 = hmac(temp, std::span<const std::uint8_t>(out1), std::span<const std::uint8_t>(two));
}

// X25519. False if the far end's key is one that gives nothing away.
bool agree(const SecretKey& mine, const PublicKey& theirs, Shared& out) {
    return crypto_scalarmult_curve25519(out.data(), mine.bytes.data(),
                                        theirs.bytes.data()) == 0;
}

// Noise's nonce in ChaCha20-Poly1305's twelve bytes: four of nought, then the
// counter, least significant byte first.
std::array<std::uint8_t, crypto_aead_chacha20poly1305_ietf_NPUBBYTES> nonce_of(
    std::uint64_t n) {
    std::array<std::uint8_t, crypto_aead_chacha20poly1305_ietf_NPUBBYTES> out{};
    for (std::size_t i = 0; i < 8; ++i) {
        out[4 + i] = static_cast<std::uint8_t>((n >> (8 * i)) & 0xFF);
    }
    return out;
}

// The symmetric state Noise keeps: the hash, the chaining key, the cipher key
// and its counter.
struct Symmetric {
    Hash h{};
    Hash ck{};
    CipherKey k{};
    std::uint64_t n = 0;
    bool keyed = false;

    // Noise's InitializeSymmetric: the name, which at 33 bytes is shorter
    // than the 64-byte hash, padded with zeros to it; the chaining key the
    // same. (Until 2026-09-24 the hash was cut to 32 bytes and so was the
    // name, and no standard Noise could talk to it.)
    void initialize() {
        h = {};
        const auto* name = reinterpret_cast<const std::uint8_t*>(handshake_name.data());
        static_assert(handshake_name.size() <= hash_bytes, "the name fits the hash");
        std::copy(name, name + handshake_name.size(), h.begin());
        ck = h;
    }
    void mix_hash(std::span<const std::uint8_t> data) {
        h = hash_of(std::span<const std::uint8_t>(h), data);
    }
    void mix_key(std::span<const std::uint8_t> ikm) {
        Hash next_ck{};
        Hash next_k{};
        hkdf2(ck, ikm, next_ck, next_k);
        ck = next_ck;
        // A 64-byte hash's output is cut to the cipher's 32-byte key.
        std::copy(next_k.begin(), next_k.begin() + cipher_key_bytes, k.begin());
        n = 0;
        keyed = true;
    }
    std::vector<std::uint8_t> encrypt_and_hash(std::span<const std::uint8_t> plain) {
        std::vector<std::uint8_t> out(plain.size() + tag_bytes);
        unsigned long long wrote = 0;
        const auto nonce = nonce_of(n++);
        crypto_aead_chacha20poly1305_ietf_encrypt(
            out.data(), &wrote, plain.data(), plain.size(), h.data(), h.size(), nullptr,
            nonce.data(), k.data());
        out.resize(static_cast<std::size_t>(wrote));
        mix_hash(out);
        return out;
    }
    bool decrypt_and_hash(std::span<const std::uint8_t> sealed,
                          std::vector<std::uint8_t>& plain) {
        if (sealed.size() < tag_bytes) {
            return false;
        }
        plain.assign(sealed.size() - tag_bytes, 0);
        unsigned long long wrote = 0;
        const auto nonce = nonce_of(n++);
        const int ok = crypto_aead_chacha20poly1305_ietf_decrypt(
            plain.data(), &wrote, nullptr, sealed.data(), sealed.size(), h.data(),
            h.size(), nonce.data(), k.data());
        if (ok != 0) {
            return false;
        }
        plain.resize(static_cast<std::size_t>(wrote));
        mix_hash(sealed);
        return true;
    }
    void split(TrafficKey& first, TrafficKey& second) const {
        Hash a{};
        Hash b{};
        hkdf2(ck, {}, a, b);
        std::copy(a.begin(), a.begin() + cipher_key_bytes, first.bytes.begin());
        std::copy(b.begin(), b.begin() + cipher_key_bytes, second.bytes.begin());
    }
};

} // namespace

// ---- the initiator ------------------------------------------------------

Initiator::Initiator(const KeyPair& mine, const PublicKey& theirs,
                     std::span<const std::uint8_t> prologue, std::optional<KeyPair> ephemeral)
    : mine_(mine), theirs_(theirs), prologue_(prologue.begin(), prologue.end()),
      fixed_ephemeral_(ephemeral) {}

std::vector<std::uint8_t> Initiator::begin(std::span<const std::uint8_t> payload) {
    if (!started() || begun_) {
        return {};
    }
    Symmetric s;
    s.initialize();
    // The prologue is mixed in whether or not there is one: an empty one
    // is still hashed, as Noise has it.
    s.mix_hash(prologue_);
    // The pre-message: the initiator already knows the responder's static.
    s.mix_hash(std::span<const std::uint8_t>(theirs_.bytes));

    ephemeral_ = fixed_ephemeral_ ? *fixed_ephemeral_ : mint_key_pair();
    s.mix_hash(std::span<const std::uint8_t>(ephemeral_.publik.bytes));

    Shared shared{};
    if (!agree(ephemeral_.secret, theirs_, shared)) {
        return {};
    }
    s.mix_key(shared); // es

    std::vector<std::uint8_t> out(ephemeral_.publik.bytes.begin(),
                                  ephemeral_.publik.bytes.end());
    const std::vector<std::uint8_t> sealed_static =
        s.encrypt_and_hash(std::span<const std::uint8_t>(mine_.publik.bytes));
    out.insert(out.end(), sealed_static.begin(), sealed_static.end());

    if (!agree(mine_.secret, theirs_, shared)) {
        return {};
    }
    s.mix_key(shared); // ss

    const std::vector<std::uint8_t> sealed_payload = s.encrypt_and_hash(payload);
    out.insert(out.end(), sealed_payload.begin(), sealed_payload.end());

    h_ = s.h;
    ck_ = s.ck;
    k_ = s.k;
    n_ = s.n;
    have_key_ = s.keyed;
    begun_ = true;
    return out;
}

std::optional<SessionKeys> Initiator::finish(std::span<const std::uint8_t> response,
                                         std::vector<std::uint8_t>* payload) {
    if (!started() || !begun_ || response.size() < key_bytes + tag_bytes) {
        return std::nullopt;
    }
    Symmetric s;
    s.h = h_;
    s.ck = ck_;
    s.k = k_;
    s.n = n_;
    s.keyed = have_key_;

    PublicKey their_ephemeral;
    std::copy(response.begin(), response.begin() + key_bytes,
              their_ephemeral.bytes.begin());
    s.mix_hash(std::span<const std::uint8_t>(their_ephemeral.bytes));

    Shared shared{};
    if (!agree(ephemeral_.secret, their_ephemeral, shared)) {
        return std::nullopt;
    }
    s.mix_key(shared); // ee
    if (!agree(mine_.secret, their_ephemeral, shared)) {
        return std::nullopt;
    }
    s.mix_key(shared); // se

    std::vector<std::uint8_t> got;
    if (!s.decrypt_and_hash(response.subspan(key_bytes), got)) {
        return std::nullopt;
    }
    if (payload != nullptr) {
        *payload = got;
    }

    SessionKeys session;
    // The initiator sends under the first key and receives under the second.
    s.split(session.sending, session.receiving);
    session.theirs = theirs_;
    return session;
}

// ---- the responder ------------------------------------------------------

Responder::Responder(const KeyPair& mine, std::span<const std::uint8_t> prologue,
                     std::optional<KeyPair> ephemeral)
    : mine_(mine), prologue_(prologue.begin(), prologue.end()), fixed_ephemeral_(ephemeral) {}

std::optional<Responder::Answer> Responder::answer(
    std::span<const std::uint8_t> initiation, std::span<const std::uint8_t> payload,
    std::vector<std::uint8_t>* theirs_payload) {
    // e (32) + sealed static (32 + 16) + sealed payload (at least 16).
    constexpr std::size_t least = key_bytes + key_bytes + tag_bytes + tag_bytes;
    if (!started() || initiation.size() < least) {
        return std::nullopt;
    }
    Symmetric s;
    s.initialize();
    s.mix_hash(prologue_);
    s.mix_hash(std::span<const std::uint8_t>(mine_.publik.bytes));

    PublicKey their_ephemeral;
    std::copy(initiation.begin(), initiation.begin() + key_bytes,
              their_ephemeral.bytes.begin());
    s.mix_hash(std::span<const std::uint8_t>(their_ephemeral.bytes));

    Shared shared{};
    if (!agree(mine_.secret, their_ephemeral, shared)) {
        return std::nullopt;
    }
    s.mix_key(shared); // es

    std::vector<std::uint8_t> their_static_bytes;
    if (!s.decrypt_and_hash(initiation.subspan(key_bytes, key_bytes + tag_bytes),
                            their_static_bytes) ||
        their_static_bytes.size() != key_bytes) {
        return std::nullopt;
    }
    PublicKey their_static;
    std::copy(their_static_bytes.begin(), their_static_bytes.end(),
              their_static.bytes.begin());

    if (!agree(mine_.secret, their_static, shared)) {
        return std::nullopt;
    }
    s.mix_key(shared); // ss

    std::vector<std::uint8_t> got;
    if (!s.decrypt_and_hash(initiation.subspan(key_bytes + key_bytes + tag_bytes),
                            got)) {
        return std::nullopt;
    }
    if (theirs_payload != nullptr) {
        *theirs_payload = got;
    }

    // And the answer.
    const KeyPair ephemeral = fixed_ephemeral_ ? *fixed_ephemeral_ : mint_key_pair();
    s.mix_hash(std::span<const std::uint8_t>(ephemeral.publik.bytes));
    if (!agree(ephemeral.secret, their_ephemeral, shared)) {
        return std::nullopt;
    }
    s.mix_key(shared); // ee
    if (!agree(ephemeral.secret, their_static, shared)) {
        return std::nullopt;
    }
    s.mix_key(shared); // se

    Answer out;
    out.message.assign(ephemeral.publik.bytes.begin(), ephemeral.publik.bytes.end());
    const std::vector<std::uint8_t> sealed = s.encrypt_and_hash(payload);
    out.message.insert(out.message.end(), sealed.begin(), sealed.end());

    // The responder receives under the initiator's sending key and sends
    // under the other, so the two are the other way round here.
    s.split(out.session.receiving, out.session.sending);
    out.session.theirs = their_static;
    return out;
}

} // namespace glideslope::net
