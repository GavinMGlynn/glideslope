#include "harness.hpp"

#include "world/inflate.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::adler32;
using glideslope::world::inflate_zlib;
using glideslope::world::InflateError;

namespace {

using Bytes = std::vector<std::uint8_t>;

const std::filesystem::path fixtures = GLIDESLOPE_TEST_SOURCE_DIR "/data/inflate";

Bytes read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    check(static_cast<bool>(in), "can read " + path.string());
    return Bytes(std::istreambuf_iterator<char>(in), {});
}

std::uint64_t step(std::uint64_t& x) {
    x = x * 6364136223846793005ull + 1442695040888963407ull;
    return x >> 33;
}

// The payloads tools/make_inflate_fixtures.py compressed, made the same way.
Bytes payload(std::size_t size, std::uint64_t seed) {
    static const std::string phrase =
        "glideslope flies over the Copernicus DEM at thirty metres a sample ";
    std::uint64_t x = seed;
    Bytes out;
    while (out.size() < size) {
        const std::uint64_t r = step(x);
        if (r % 4 == 0) {
            out.push_back(static_cast<std::uint8_t>(r & 0xff));
        } else {
            const std::size_t start = (r >> 8) % phrase.size();
            const std::size_t length = 1 + (r >> 16) % 40;
            const std::string piece = phrase.substr(start, length);
            out.insert(out.end(), piece.begin(), piece.end());
        }
    }
    out.resize(size);
    return out;
}

Bytes far(std::size_t size, std::uint64_t seed) {
    std::uint64_t x = seed;
    Bytes noise;
    for (int i = 0; i < 32000; ++i) {
        noise.push_back(static_cast<std::uint8_t>(step(x) & 0xff));
    }
    Bytes out;
    while (out.size() < size) {
        out.insert(out.end(), noise.begin(), noise.end());
    }
    out.resize(size);
    return out;
}

struct Fixture {
    const char* name;
    std::size_t size;
    std::uint64_t seed;
};

// Every file tools/make_inflate_fixtures.py makes.
const std::vector<Fixture> all_fixtures{
    {"empty", 0, 1},           {"fixed", 20000, 2},        {"dynamic", 120000, 3},
    {"fast", 50000, 4},        {"huffman_only", 6000, 5},  {"runs", 20000, 6},
    {"stored", 3000, 7},       {"sync_flushes", 30000, 8}, {"full_flushes", 30000, 9},
    {"far_matches", 40000, 10}};

Bytes expected(const Fixture& f) {
    return std::string_view(f.name).starts_with("far") ? far(f.size, f.seed)
                                                       : payload(f.size, f.seed);
}

// Bits for a hand-made stream, least significant first; Huffman codes most
// significant first, as RFC 1951 packs them.
struct BitWriter {
    Bytes bytes;
    std::uint64_t buffer = 0;
    int count = 0;

    void put(std::uint32_t value, int n) {
        buffer |= std::uint64_t{value} << count;
        count += n;
        while (count >= 8) {
            bytes.push_back(static_cast<std::uint8_t>(buffer & 0xff));
            buffer >>= 8;
            count -= 8;
        }
    }
    void code(std::uint32_t value, int n) {
        for (int i = n - 1; i >= 0; --i) {
            put((value >> i) & 1u, 1);
        }
    }
    void align() {
        if (count > 0) {
            put(0, 8 - count);
        }
    }
};

// A zlib stream around raw DEFLATE bytes, with the checksum of `content`.
Bytes zlib_wrap(const Bytes& deflate, const Bytes& content) {
    Bytes out{0x78, 0x9c};
    out.insert(out.end(), deflate.begin(), deflate.end());
    const std::uint32_t a = adler32(content);
    for (const int shift : {24, 16, 8, 0}) {
        out.push_back(static_cast<std::uint8_t>(a >> shift));
    }
    return out;
}

// Throws the InflateError expected, whose message holds `says`.
void refuses(const Bytes& stream, const std::string& says,
             std::size_t max_size = 1 << 20,
             std::source_location where = std::source_location::current()) {
    try {
        inflate_zlib(stream, max_size);
    } catch (const InflateError& e) {
        if (std::string(e.what()).find(says) == std::string::npos) {
            fail("refused with \"" + std::string(e.what()) + "\", not \"" + says + "\"",
                 where);
        }
        return;
    }
    fail("accepted a stream it should refuse for \"" + says + "\"", where);
}

} // namespace

GLIDESLOPE_TEST(every_zlib_fixture_inflates_to_the_payload_it_was_made_from) {
    std::set<std::string> on_disk;
    for (const auto& entry : std::filesystem::directory_iterator(fixtures)) {
        if (entry.path().extension() == ".zz") {
            on_disk.insert(entry.path().stem().string());
        }
    }
    std::set<std::string> known;
    for (const Fixture& f : all_fixtures) {
        known.insert(f.name);
        const Bytes stream = read(fixtures / (std::string(f.name) + ".zz"));
        const Bytes out = inflate_zlib(stream, f.size);
        check(out == expected(f), std::string(f.name) + " inflates to its payload");
    }
    check(on_disk == known,
          "every fixture on disk is checked, and every one checked is on disk");
}

GLIDESLOPE_TEST(stored_blocks_of_the_largest_size_inflate) {
    const Bytes content = payload(65535 + 65535 + 7, 11);
    BitWriter w;
    std::size_t at = 0;
    for (const std::size_t size :
         {std::size_t{65535}, std::size_t{0}, std::size_t{65535}, std::size_t{7}}) {
        w.put(at + size == content.size() && size == 7 ? 1u : 0u, 1);
        w.put(0, 2);
        w.align();
        w.put(static_cast<std::uint32_t>(size), 16);
        w.put(static_cast<std::uint32_t>(size) ^ 0xffffu, 16);
        for (std::size_t i = 0; i < size; ++i) {
            w.put(content[at + i], 8);
        }
        at += size;
    }
    check(inflate_zlib(zlib_wrap(w.bytes, content), content.size()) == content,
          "stored blocks of 65535, 0, 65535 and 7 bytes inflate");
}

GLIDESLOPE_TEST(a_stream_that_fails_its_checksum_is_refused) {
    Bytes stream = read(fixtures / "dynamic.zz");
    stream.back() ^= 0x01;
    refuses(stream, "Adler-32");
}

GLIDESLOPE_TEST(a_stream_cut_short_at_any_length_is_refused) {
    for (const char* name : {"huffman_only", "stored", "fixed"}) {
        const Bytes stream = read(fixtures / (std::string(name) + ".zz"));
        for (std::size_t length = 0; length < stream.size(); ++length) {
            const Bytes cut(stream.begin(), stream.begin() + static_cast<long>(length));
            try {
                inflate_zlib(cut, 1 << 20);
                fail(std::string(name) + " cut to " + std::to_string(length) +
                     " bytes was accepted");
            } catch (const InflateError&) {
            }
        }
    }
}

GLIDESLOPE_TEST(a_stream_that_would_decompress_past_its_limit_is_refused) {
    const Bytes stream = read(fixtures / "dynamic.zz");
    check(inflate_zlib(stream, 120000).size() == 120000,
          "inflates at exactly its size");
    refuses(stream, "more than 119999 bytes", 119999);
}

GLIDESLOPE_TEST(malformed_zlib_headers_and_deflate_blocks_are_refused) {
    refuses({0x79, 0x9c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01}, "not a zlib stream");
    refuses({0x78, 0x9d, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01}, "check bits");
    refuses({0x78, 0xbb, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01}, "preset dictionary");
    refuses({0x78, 0x9c}, "at least six bytes");

    {
        BitWriter w; // a block of the reserved type
        w.put(1, 1);
        w.put(3, 2);
        w.align();
        refuses(zlib_wrap(w.bytes, {}), "reserved type 3");
    }
    {
        BitWriter w; // a stored block whose length and complement disagree
        w.put(1, 1);
        w.put(0, 2);
        w.align();
        w.put(5, 16);
        w.put(0, 16);
        refuses(zlib_wrap(w.bytes, {}), "complement");
    }
    {
        BitWriter w; // a code-length code of 19 one-bit codes
        w.put(1, 1);
        w.put(2, 2);
        w.put(0, 5);
        w.put(0, 5);
        w.put(15, 4);
        for (int i = 0; i < 19; ++i) {
            w.put(1, 3);
        }
        w.align();
        refuses(zlib_wrap(w.bytes, {}), "over-subscribed");
    }
    {
        BitWriter w; // "a", then a match two bytes back
        w.put(1, 1);
        w.put(1, 2);
        w.code(0x30 + 'a', 8); // literal
        w.code(1, 7);          // length 3, symbol 257
        w.code(1, 5);          // distance 2
        w.code(0, 7);          // end of block
        w.align();
        refuses(zlib_wrap(w.bytes, {'a'}), "refers back before its start");
    }
    {
        BitWriter w; // length symbol 286, which fixed codes have but lengths do not
        w.put(1, 1);
        w.put(1, 2);
        w.code(0xc0 + 6, 8);
        w.align();
        refuses(zlib_wrap(w.bytes, {}), "length code out of range");
    }
    {
        BitWriter w; // distance symbol 30, likewise
        w.put(1, 1);
        w.put(1, 2);
        w.code(0x30 + 'a', 8);
        w.code(1, 7);
        w.code(30, 5);
        w.align();
        refuses(zlib_wrap(w.bytes, {'a'}), "distance code out of range");
    }
}

GLIDESLOPE_TEST(
    corrupted_streams_are_refused_or_decoded_without_reading_or_writing_out_of_bounds) {
    // Deterministic: every run corrupts the same bytes the same way. Out of
    // bounds access is what the sanitized build is watching for.
    std::uint64_t x = 12345;
    int refused = 0;
    int decoded = 0;
    for (const char* name : {"fixed", "dynamic", "runs", "far_matches"}) {
        const Bytes stream = read(fixtures / (std::string(name) + ".zz"));
        for (int i = 0; i < 500; ++i) {
            Bytes corrupt = stream;
            for (int flips = 1 + static_cast<int>(step(x) % 3); flips > 0; --flips) {
                const std::size_t at = 2 + step(x) % (corrupt.size() - 6);
                corrupt[at] ^= static_cast<std::uint8_t>(1u << (step(x) % 8));
            }
            try {
                const Bytes out = inflate_zlib(corrupt, 200000);
                check(out.size() <= 200000, "never past the limit");
                ++decoded;
            } catch (const InflateError&) {
                ++refused;
            }
        }
    }
    check(refused + decoded == 2000, "all 2000 corruptions ran");
    check(refused > 1900,
          "nearly every corruption is caught, by the code or the checksum");
}

GLIDESLOPE_TEST(adler32_gives_the_published_checksums) {
    const std::string wikipedia = "Wikipedia";
    check(adler32(Bytes(wikipedia.begin(), wikipedia.end())) == 0x11e60398u,
          "Adler-32 of \"Wikipedia\" is 0x11E60398");
    check(adler32({}) == 1u, "Adler-32 of nothing is 1");
    // A million 0xff bytes: past the 5552-byte run where the sums must be reduced.
    check(adler32(Bytes(1000000, 0xff)) == 0x3843e1beu,
          "Adler-32 of a million 0xff bytes");
}
