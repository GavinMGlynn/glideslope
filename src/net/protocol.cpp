#include "net/protocol.hpp"

#include <algorithm>
#include <cstring>

namespace glideslope::net {

bool known_type(std::uint8_t type) {
    switch (type) {
    case static_cast<std::uint8_t>(Type::handshake_initiation):
    case static_cast<std::uint8_t>(Type::handshake_response):
    case static_cast<std::uint8_t>(Type::sealed):
    case static_cast<std::uint8_t>(Type::refusal):
        return true;
    default:
        return false;
    }
}

void Writer::u8(std::uint8_t v) {
    out_.push_back(v);
}

void Writer::u16(std::uint16_t v) {
    out_.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
}

void Writer::u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

void Writer::u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

void Writer::i32(std::int32_t v) {
    u32(static_cast<std::uint32_t>(v));
}

void Writer::f64(double v) {
    // Its IEEE-754 bits, which have one representation. std::bit_cast would
    // say the same thing; memcpy says it without a header.
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(bits));
    u64(bits);
}

void Writer::text(std::string_view v) {
    const std::size_t count = std::min<std::size_t>(v.size(), 0xFFFFu);
    u16(static_cast<std::uint16_t>(count));
    out_.insert(out_.end(), v.begin(), v.begin() + static_cast<std::ptrdiff_t>(count));
}

void Writer::bytes(std::span<const std::uint8_t> v) {
    out_.insert(out_.end(), v.begin(), v.end());
}

bool Reader::take(std::size_t count) {
    if (!ok_ || in_.size() - at_ < count) {
        ok_ = false;
        return false;
    }
    at_ += count;
    return true;
}

std::uint8_t Reader::u8() {
    if (!take(1)) {
        return 0;
    }
    return in_[at_ - 1];
}

std::uint16_t Reader::u16() {
    if (!take(2)) {
        return 0;
    }
    return static_cast<std::uint16_t>(in_[at_ - 2] |
                                      (static_cast<std::uint16_t>(in_[at_ - 1]) << 8));
}

std::uint32_t Reader::u32() {
    if (!take(4)) {
        return 0;
    }
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(in_[at_ - 4 + static_cast<std::size_t>(i)])
             << (8 * i);
    }
    return v;
}

std::uint64_t Reader::u64() {
    if (!take(8)) {
        return 0;
    }
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(in_[at_ - 8 + static_cast<std::size_t>(i)])
             << (8 * i);
    }
    return v;
}

std::int32_t Reader::i32() {
    return static_cast<std::int32_t>(u32());
}

double Reader::f64() {
    const std::uint64_t bits = u64();
    double v = 0.0;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

std::string Reader::text(std::size_t most) {
    const std::uint16_t count = u16();
    if (!ok_) {
        return {};
    }
    // A string longer than the caller will accept breaks the reader rather
    // than being cut: the caller said what it would take.
    if (count > most || !take(count)) {
        ok_ = false;
        return {};
    }
    const auto* from = in_.data() + at_ - count;
    return std::string(reinterpret_cast<const char*>(from), count);
}

std::vector<std::uint8_t> Reader::bytes(std::size_t count) {
    if (!take(count)) {
        return {};
    }
    const auto* from = in_.data() + at_ - count;
    return std::vector<std::uint8_t>(from, from + count);
}

Writer begin(Type type) {
    Writer w;
    w.bytes(std::span<const std::uint8_t>(magic.data(), magic.size()));
    w.u8(protocol_version);
    w.u8(static_cast<std::uint8_t>(type));
    return w;
}

bool read_envelope(Reader& reader, Envelope& out, Refusal& why) {
    if (reader.left() < envelope_size) {
        why = Refusal::too_short;
        return false;
    }
    const std::vector<std::uint8_t> got = reader.bytes(magic.size());
    if (!std::equal(got.begin(), got.end(), magic.begin())) {
        why = Refusal::not_this_protocol;
        return false;
    }
    const std::uint8_t version = reader.u8();
    const std::uint8_t type = reader.u8();
    if (!reader.ok()) {
        why = Refusal::too_short;
        return false;
    }
    // **The magic is checked first**, so another protocol's datagram is told
    // what it is rather than told its version is wrong.
    if (version != protocol_version) {
        why = Refusal::wrong_version;
        return false;
    }
    if (!known_type(type)) {
        why = Refusal::unknown_type;
        return false;
    }
    out.version = version;
    out.type = static_cast<Type>(type);
    return true;
}

} // namespace glideslope::net
