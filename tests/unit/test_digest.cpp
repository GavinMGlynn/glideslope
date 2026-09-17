#include "harness.hpp"

#include "world/digest.hpp"

#include <cstdint>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::world::md5_hex;
using glideslope::world::sha256_hex;

namespace {

std::vector<std::uint8_t> bytes(const std::string& s) {
    return {s.begin(), s.end()};
}

// (i * 7) & 255 for each byte: a pattern that is not a string.
std::vector<std::uint8_t> pattern(std::size_t n) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(static_cast<std::uint8_t>((i * 7) & 255));
    }
    return out;
}

} // namespace

GLIDESLOPE_TEST(sha256_and_md5_give_the_published_digests) {
    // FIPS 180-2's SHA-256 examples, and RFC 1321's MD5 test suite.
    check(sha256_hex({}) ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA-256 of nothing");
    check(sha256_hex(bytes("abc")) ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA-256 of abc");
    check(
        sha256_hex(bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "SHA-256 of the 448-bit message");
    check(sha256_hex(std::vector<std::uint8_t>(1000000, 'a')) ==
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "SHA-256 of a million a");
    check(md5_hex({}) == "d41d8cd98f00b204e9800998ecf8427e", "MD5 of nothing");
    check(md5_hex(bytes("abc")) == "900150983cd24fb0d6963f7d28e17f72", "MD5 of abc");
    check(md5_hex(bytes("The quick brown fox jumps over the lazy dog")) ==
              "9e107d9d372bb6826bd81d3542a419d6",
          "MD5 of the quick brown fox");
}

GLIDESLOPE_TEST(
    sha256_and_md5_pad_every_length_around_a_block_boundary_and_hash_in_pieces) {
    // Python's hashlib, for (i * 7) & 255: the lengths where padding needs one
    // block or two.
    struct Expected {
        std::size_t length;
        const char* sha256;
        const char* md5;
    };
    const std::vector<Expected> lengths{
        {55, "576a1bf8d4478657e6dc4af9398544765c2a92cde28478b019235cfed315fc09",
         "8d24280288a696559fd8d5aa1b6d8c6e"},
        {56, "9b20501dfd1d99161c257950f3444f3e49230c351c5c8e0943ef369f85f5205d",
         "ef2c72b7254c92459e498eddd4ace573"},
        {63, "30b345906b493f06f69444b6521113511c242f30e29840462950035043682f1e",
         "c4c8c6d513f4e1604eb18508a1769364"},
        {64, "d8bc63b4fc1156e5e7d95a418b9bf54cd3174bedbc2db40f74895349b229b3c0",
         "a2fcb39a253b9b785b1f97518fa37683"},
        {65, "1ee23b0fbcaecc1aff4a9e8f1645f35ab2c8e13609cd73b68df8b5e3f63ce073",
         "e49fe82d0bb12967a196c85de313e446"},
        {119, "7a6589821178918ca8d9edaba5abfc1e9b2669564f4469b66885379c1530b2c8",
         "1640deea49ebb258ec6ede18d4b2d2d7"},
        {120, "655250427d56b1b0eeb8497d21428704273458a01772d6881b65c0abac0f8a98",
         "05f879f7b542a7ebf0605adff67d4423"}};
    for (const Expected& e : lengths) {
        const auto data = pattern(e.length);
        check(sha256_hex(data) == e.sha256,
              "SHA-256 of " + std::to_string(e.length) + " bytes");
        check(md5_hex(data) == e.md5, "MD5 of " + std::to_string(e.length) + " bytes");
        // The same digests fed a byte, then 13, then the rest.
        for (const std::size_t first : {std::size_t{1}, std::size_t{13}, e.length}) {
            glideslope::world::Sha256 sha;
            glideslope::world::Md5 md5;
            const std::size_t split = std::min(first, e.length);
            sha.update(std::span(data).first(split));
            sha.update(std::span(data).subspan(split));
            md5.update(std::span(data).first(split));
            md5.update(std::span(data).subspan(split));
            check(sha.hex() == e.sha256 && md5.hex() == e.md5,
                  "in pieces split at " + std::to_string(split));
        }
    }
}
