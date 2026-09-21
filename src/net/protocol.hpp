#pragma once

// The wire format: the envelope every datagram begins with, and the reading
// and writing of the values inside one.
//
// **Everything that comes off the wire is hostile until it has been read.**
// A `Reader` never runs off the end of its buffer and never throws: it marks
// itself broken and answers zero from then on, so a caller may read a whole
// message and ask once, at the end, whether any of it was real. That is what
// lets the parsers be fuzzed without a crash being the expected outcome.
//
// **Byte order is little-endian**, and every integer is fixed-width. The
// machines this runs on are all little-endian, so nothing is swapped in
// practice; saying so is what lets a third party write a client.
//
// **Floating point is not sent.** A double is sent as its IEEE-754 bits in a
// fixed-width integer, which has one representation rather than a compiler's
// choice of one.
//
// docs/TRANSPORT.md describes all of this byte for byte, including what the
// transport does not claim. This header and that document must agree; a test
// holds them to it.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::net {

// **This project's own magic**, so that a gearstick client and a glideslope
// server refuse each other at the first four bytes rather than somewhere
// deeper and less clearly.
inline constexpr std::array<std::uint8_t, 4> magic{'G', 'L', 'D', 'S'};

// The version of the wire format. A datagram of any other version is refused.
inline constexpr std::uint8_t protocol_version = 1;

// The six bytes every datagram begins with: four of magic, one of version,
// one of type.
inline constexpr std::size_t envelope_size = 6;

enum class Type : std::uint8_t {
    // The two halves of the handshake. Their bodies are the Noise handshake's
    // own and are not read by anything here.
    handshake_initiation = 1,
    handshake_response = 2,
    // A sealed datagram: everything after the envelope is ciphertext.
    sealed = 3,
    // Refused, with a reason. Sent in the clear, because there may be no
    // session to seal it with.
    refusal = 4,
};

// Whether `type` is one this version knows.
bool known_type(std::uint8_t type);

// Why a datagram was refused. Sent as one byte; a reason this version does
// not know reads as `unknown`.
enum class Refusal : std::uint8_t {
    unknown = 0,
    not_this_protocol = 1, // the magic was someone else's
    wrong_version = 2,
    unknown_type = 3,
    too_short = 4,
    server_full = 5,
    bad_handshake = 6,
};

struct Envelope {
    std::uint8_t version = protocol_version;
    Type type = Type::sealed;
};

// **Writes the wire format.** Appends to its own buffer; it cannot fail.
class Writer {
public:
    void u8(std::uint8_t v);
    void u16(std::uint16_t v);
    void u32(std::uint32_t v);
    void u64(std::uint64_t v);
    void i32(std::int32_t v);
    void f64(double v);
    // A string as a two-byte length and that many bytes. A string longer
    // than 65,535 bytes is cut, because a length that cannot be written is
    // worse than a string that is shorter than asked.
    void text(std::string_view v);
    void bytes(std::span<const std::uint8_t> v);

    const std::vector<std::uint8_t>& out() const { return out_; }
    std::vector<std::uint8_t> take() { return std::move(out_); }
    std::size_t size() const { return out_.size(); }

private:
    std::vector<std::uint8_t> out_;
};

// **Reads the wire format, and never runs off the end.** Every read that
// cannot be satisfied marks the reader broken and returns zero; once broken
// it stays broken. `ok()` at the end is the only thing worth believing.
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> in) : in_(in) {}

    std::uint8_t u8();
    std::uint16_t u16();
    std::uint32_t u32();
    std::uint64_t u64();
    std::int32_t i32();
    double f64();
    // At most `most` bytes; a longer string breaks the reader rather than
    // being cut, because a caller asking for a bounded string has said what
    // it will accept.
    std::string text(std::size_t most = 4096);
    std::vector<std::uint8_t> bytes(std::size_t count);

    bool ok() const { return ok_; }
    std::size_t left() const { return ok_ ? in_.size() - at_ : 0; }
    // Nothing unread, and nothing went wrong.
    bool done() const { return ok_ && at_ == in_.size(); }

private:
    bool take(std::size_t count);

    std::span<const std::uint8_t> in_;
    std::size_t at_ = 0;
    bool ok_ = true;
};

// Writes the envelope to the front of a new buffer.
Writer begin(Type type);

// Reads an envelope. Returns the reason it is not one on failure; the reader
// is left positioned after the envelope on success.
//
// **The magic is checked before the version**, so that a datagram from
// another protocol is told what it is rather than being told its version is
// wrong.
bool read_envelope(Reader& reader, Envelope& out, Refusal& why);

} // namespace glideslope::net
