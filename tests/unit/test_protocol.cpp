#include "harness.hpp"

#include "net/protocol.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <limits>
#include <span>
#include <string>
#include <vector>

using glideslope::net::Envelope;
using glideslope::net::Reader;
using glideslope::net::Refusal;
using glideslope::net::Type;
using glideslope::net::Writer;
using glideslope::test::check;

namespace {

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

// A whole, well-formed datagram of every kind of value this writes.
std::vector<std::uint8_t> a_datagram() {
    Writer w = glideslope::net::begin(Type::sealed);
    w.u8(0x7F);
    w.u16(0xBEEF);
    w.u32(0xDEADBEEF);
    w.u64(0x0123456789ABCDEFull);
    w.i32(-123456);
    w.f64(-3.5);
    w.text("the Cessna");
    return w.take();
}

} // namespace

// **What goes on the wire comes back off it.** Every width, at its extremes
// as well as in the middle, because a value that only round-trips in the
// middle is a value that has not been tested.
GLIDESLOPE_TEST(every_value_written_to_the_wire_reads_back_as_itself) {
    Writer w;
    const std::vector<std::uint8_t> u8s{0, 1, 0x7F, 0x80, 0xFF};
    const std::vector<std::uint16_t> u16s{0, 1, 0x7FFF, 0x8000, 0xFFFF};
    const std::vector<std::uint32_t> u32s{0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF};
    const std::vector<std::uint64_t> u64s{0, 1, 0x7FFFFFFFFFFFFFFFull,
                                          0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull};
    const std::vector<std::int32_t> i32s{0, 1, -1, std::numeric_limits<std::int32_t>::min(),
                                         std::numeric_limits<std::int32_t>::max()};
    const std::vector<double> f64s{0.0,
                                   -0.0,
                                   1.0,
                                   -1.0,
                                   0.1,
                                   -33.946100000000001,
                                   std::numeric_limits<double>::min(),
                                   std::numeric_limits<double>::max(),
                                   std::numeric_limits<double>::lowest(),
                                   std::numeric_limits<double>::infinity(),
                                   -std::numeric_limits<double>::infinity()};
    const std::vector<std::string> texts{"", "a", "the Short S.23 Empire flying boat",
                                         std::string(4096, 'x')};

    for (const auto v : u8s) {
        w.u8(v);
    }
    for (const auto v : u16s) {
        w.u16(v);
    }
    for (const auto v : u32s) {
        w.u32(v);
    }
    for (const auto v : u64s) {
        w.u64(v);
    }
    for (const auto v : i32s) {
        w.i32(v);
    }
    for (const auto v : f64s) {
        w.f64(v);
    }
    for (const auto& v : texts) {
        w.text(v);
    }

    const std::vector<std::uint8_t> wire = w.out();
    Reader r(all_of(wire));
    std::size_t walked = 0;
    for (const auto v : u8s) {
        check(r.u8() == v, "a byte reads back");
        ++walked;
    }
    for (const auto v : u16s) {
        check(r.u16() == v, "two bytes read back");
        ++walked;
    }
    for (const auto v : u32s) {
        check(r.u32() == v, "four bytes read back");
        ++walked;
    }
    for (const auto v : u64s) {
        check(r.u64() == v, "eight bytes read back");
        ++walked;
    }
    for (const auto v : i32s) {
        check(r.i32() == v, "a signed four bytes reads back");
        ++walked;
    }
    for (const auto v : f64s) {
        const double got = r.f64();
        // Bit-for-bit, so that -0.0 and the infinities count.
        check(std::signbit(got) == std::signbit(v) && (got == v || (std::isnan(got) && std::isnan(v))),
              "a double reads back exactly");
        ++walked;
    }
    for (const auto& v : texts) {
        check(r.text() == v, "a string reads back");
        ++walked;
    }
    check(r.done(), "the whole datagram was read and nothing was left over");
    const std::size_t expected =
        u8s.size() + u16s.size() + u32s.size() + u64s.size() + i32s.size() +
        f64s.size() + texts.size();
    check(walked == expected, "every value was walked: " + std::to_string(walked) +
                                  " of " + std::to_string(expected));
    check(walked == 40, "there are 40 values in this walk, not " + std::to_string(walked));
}

// **Little-endian, and said so.** A third party writing a client from
// docs/TRANSPORT.md has to know which way round the bytes go, so the bytes
// themselves are pinned here rather than only their round trip.
GLIDESLOPE_TEST(the_wire_puts_the_least_significant_byte_first) {
    Writer w;
    w.u16(0x0201);
    w.u32(0x04030201);
    w.u64(0x0807060504030201ull);
    const std::vector<std::uint8_t> wire = w.out();
    check(wire.size() == 14, "two, four and eight bytes were written");
    for (std::size_t i = 0; i < wire.size(); ++i) {
        const std::uint8_t want = static_cast<std::uint8_t>(
            i < 2 ? i + 1 : (i < 6 ? i - 2 + 1 : i - 6 + 1));
        check(wire[i] == want, "byte " + std::to_string(i) + " is " +
                                   std::to_string(want) + ", not " +
                                   std::to_string(wire[i]));
    }

    // And a double is its IEEE-754 bits: 1.0 is 0x3FF0000000000000.
    Writer d;
    d.f64(1.0);
    const std::vector<std::uint8_t> bits = d.out();
    const std::vector<std::uint8_t> want{0, 0, 0, 0, 0, 0, 0xF0, 0x3F};
    check(bits == want, "1.0 goes on the wire as its IEEE-754 bits, least first");
}

// **The envelope is six bytes**, and says what it is before it says anything
// else.
GLIDESLOPE_TEST(the_envelope_is_the_magic_then_the_version_then_the_type) {
    const std::vector<std::uint8_t> wire = glideslope::net::begin(Type::sealed).take();
    check(wire.size() == glideslope::net::envelope_size, "the envelope is six bytes");
    check(wire[0] == 'G' && wire[1] == 'L' && wire[2] == 'D' && wire[3] == 'S',
          "the magic comes first");
    check(wire[4] == glideslope::net::protocol_version, "then the version");
    check(wire[5] == static_cast<std::uint8_t>(Type::sealed), "then the type");

    Reader r(all_of(wire));
    Envelope got;
    Refusal why = Refusal::unknown;
    check(glideslope::net::read_envelope(r, got, why), "and it reads back");
    check(got.type == Type::sealed, "as the type it was");
    check(r.done(), "with nothing left over");
}

// **Every way an envelope can be wrong is refused, and says which way.**
GLIDESLOPE_TEST(an_envelope_that_is_wrong_is_refused_with_the_reason_it_is_wrong) {
    const auto refuse = [](std::vector<std::uint8_t> wire) {
        Reader r(all_of(wire));
        Envelope got;
        Refusal why = Refusal::unknown;
        const bool ok = glideslope::net::read_envelope(r, got, why);
        check(!ok, "this should have been refused");
        return why;
    };

    std::vector<std::uint8_t> wire = glideslope::net::begin(Type::sealed).take();

    // Someone else's protocol: the magic is checked before anything else, so
    // a gearstick datagram is told it is not this protocol rather than told
    // its version is wrong.
    std::vector<std::uint8_t> theirs = wire;
    theirs[0] = 'G';
    theirs[1] = 'E';
    theirs[2] = 'A';
    theirs[3] = 'R';
    theirs[4] = 99;
    check(refuse(theirs) == Refusal::not_this_protocol,
          "another protocol's magic is refused as another protocol's");

    std::vector<std::uint8_t> old = wire;
    old[4] = static_cast<std::uint8_t>(glideslope::net::protocol_version + 1);
    check(refuse(old) == Refusal::wrong_version, "a version there is none of is refused");

    std::vector<std::uint8_t> odd = wire;
    odd[5] = 200;
    check(refuse(odd) == Refusal::unknown_type, "a type there is none of is refused");

    // Every length short of six.
    for (std::size_t n = 0; n < glideslope::net::envelope_size; ++n) {
        check(refuse(std::vector<std::uint8_t>(wire.begin(), wire.begin() + static_cast<std::ptrdiff_t>(n))) ==
                  Refusal::too_short,
              "a datagram of " + std::to_string(n) + " bytes is refused as too short");
    }
}

// **Every truncation of a whole datagram is refused and nothing runs off the
// end.** This walks every length there is, not a sample of them.
GLIDESLOPE_TEST(every_truncation_of_a_datagram_is_refused_without_running_off_the_end) {
    const std::vector<std::uint8_t> whole = a_datagram();
    check(whole.size() > glideslope::net::envelope_size, "the datagram has a body");

    std::size_t walked = 0;
    for (std::size_t n = 0; n < whole.size(); ++n) {
        const std::vector<std::uint8_t> cut(whole.begin(),
                                            whole.begin() + static_cast<std::ptrdiff_t>(n));
        Reader r(all_of(cut));
        Envelope got;
        Refusal why = Refusal::unknown;
        if (glideslope::net::read_envelope(r, got, why)) {
            // The envelope fitted; the body cannot, so reading it must break
            // the reader rather than answering with rubbish.
            (void)r.u8();
            (void)r.u16();
            (void)r.u32();
            (void)r.u64();
            (void)r.i32();
            (void)r.f64();
            (void)r.text();
            check(!r.ok(), "a body cut short breaks the reader at " + std::to_string(n));
        }
        ++walked;
    }
    check(walked == whole.size(),
          "every length short of the whole was walked: " + std::to_string(walked) +
              " of " + std::to_string(whole.size()));

    // And the whole thing reads.
    Reader r(all_of(whole));
    Envelope got;
    Refusal why = Refusal::unknown;
    check(glideslope::net::read_envelope(r, got, why), "the whole datagram's envelope reads");
    check(r.u8() == 0x7F && r.u16() == 0xBEEF && r.u32() == 0xDEADBEEF, "and its body");
    check(r.u64() == 0x0123456789ABCDEFull && r.i32() == -123456, "and the rest of it");
    check(r.f64() == -3.5 && r.text() == "the Cessna", "and the end of it");
    check(r.done(), "with nothing left over");
}

// **Every single-byte change to a whole datagram is survived.** Not a sample:
// every position, every value it could take. A parser that reads what comes
// off a socket has to be safe against all of them, and this is what says so.
GLIDESLOPE_TEST(no_single_byte_changed_anywhere_in_a_datagram_can_break_the_reader) {
    const std::vector<std::uint8_t> whole = a_datagram();
    std::size_t walked = 0;
    std::size_t accepted = 0;
    std::size_t refused = 0;
    for (std::size_t at = 0; at < whole.size(); ++at) {
        for (int value = 0; value < 256; ++value) {
            std::vector<std::uint8_t> bent = whole;
            bent[at] = static_cast<std::uint8_t>(value);
            Reader r(all_of(bent));
            Envelope got;
            Refusal why = Refusal::unknown;
            if (!glideslope::net::read_envelope(r, got, why)) {
                ++refused;
            } else {
                // Read the whole body. Whatever it says, it must not reach
                // past the buffer - which the sanitizers would catch - and
                // ok() must be an honest answer.
                (void)r.u8();
                (void)r.u16();
                (void)r.u32();
                (void)r.u64();
                (void)r.i32();
                (void)r.f64();
                const std::string text = r.text();
                check(text.size() <= bent.size(),
                      "a string read from the wire is no longer than the wire");
                ++accepted;
            }
            ++walked;
        }
    }
    const std::size_t space = whole.size() * 256;
    check(walked == space, "every byte at every value was walked: " +
                               std::to_string(walked) + " of " + std::to_string(space));
    check(refused > 0, "some of them are refused");
    check(accepted > 0, "and some of them are read");
}

// **The document and the code say the same thing.** `docs/TRANSPORT.md` is
// written so that a third party could build a client from it alone, which is
// worth nothing if it drifts from what this end actually sends. Every number
// in its tables is read back out of it and held against the code.
GLIDESLOPE_TEST(the_transport_document_and_the_code_agree_byte_for_byte) {
    const std::filesystem::path doc =
        std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "docs" / "TRANSPORT.md";
    std::ifstream in(doc);
    check(in.good(), "docs/TRANSPORT.md is there");
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    check(!text.empty(), "and has something in it");

    const auto says = [&](const std::string& what) {
        return text.find(what) != std::string::npos;
    };

    // The magic, as the document spells it out in hex and in ASCII.
    std::string hex;
    for (const std::uint8_t b : glideslope::net::magic) {
        char buf[4];
        std::snprintf(buf, sizeof(buf), "%02X", b);
        hex += (hex.empty() ? "" : " ") + std::string(buf);
    }
    check(says(hex), "the document gives the magic as " + hex);
    const std::string ascii(glideslope::net::magic.begin(), glideslope::net::magic.end());
    check(says("`" + ascii + "`"), "and as the ASCII " + ascii);

    // The version and the envelope's size.
    check(says("| 4 | 1 | version | `0" +
               std::to_string(glideslope::net::protocol_version) + "` |"),
          "the document gives the version as " +
              std::to_string(glideslope::net::protocol_version));
    check(says(std::to_string(glideslope::net::envelope_size) + " bytes"),
          "and says the envelope is " + std::to_string(glideslope::net::envelope_size) +
              " bytes");

    // Every type, by value and by name.
    const std::vector<std::pair<Type, std::string>> types{
        {Type::handshake_initiation, "HANDSHAKE_INITIATION"},
        {Type::handshake_response, "HANDSHAKE_RESPONSE"},
        {Type::sealed, "SEALED"},
        {Type::refusal, "REFUSAL"}};
    std::size_t walked = 0;
    for (const auto& [type, name] : types) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "`%02X`", static_cast<unsigned>(type));
        check(says(std::string("| ") + buf + " | `" + name + "`"),
              "the document gives " + name + " as " + buf);
        check(glideslope::net::known_type(static_cast<std::uint8_t>(type)),
              name + " is a type the code knows");
        ++walked;
    }
    check(walked == types.size(), "every type was walked");
    // The code knows these four and no more: a fifth would be a type the
    // document does not describe.
    std::size_t knows = 0;
    for (int v = 0; v < 256; ++v) {
        knows += glideslope::net::known_type(static_cast<std::uint8_t>(v)) ? 1u : 0u;
    }
    check(knows == types.size(),
          "the code knows " + std::to_string(types.size()) + " types, not " +
              std::to_string(knows));

    // Every refusal, by value and by name.
    const std::vector<std::pair<Refusal, std::string>> refusals{
        {Refusal::unknown, "UNKNOWN"},
        {Refusal::not_this_protocol, "NOT_THIS_PROTOCOL"},
        {Refusal::wrong_version, "WRONG_VERSION"},
        {Refusal::unknown_type, "UNKNOWN_TYPE"},
        {Refusal::too_short, "TOO_SHORT"},
        {Refusal::server_full, "SERVER_FULL"},
        {Refusal::bad_handshake, "BAD_HANDSHAKE"}};
    std::size_t reasons = 0;
    for (const auto& [why, name] : refusals) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "`%02X`", static_cast<unsigned>(why));
        check(says(std::string("| ") + buf + " | `" + name + "`"),
              "the document gives " + name + " as " + buf);
        ++reasons;
    }
    check(reasons == 7, "there are seven reasons to refuse, not " + std::to_string(reasons));

    // **What it does not claim is in it.** A transport document that lists
    // only what works is the kind this project does not want.
    check(says("## What the transport does not claim"),
          "the document says what the transport does not claim");
    check(says("## What is not here yet"),
          "and what is not built yet, so nothing here is mistaken for working");
    check(says("little-endian"), "and which way round the bytes go");
    check(says("00 00 00 00 00 00 F0 3F"),
          "and pins a double's bits, so a client can check its own");
}
