#include "harness.hpp"

#include "net/keys.hpp"

#include <cstdio>
#include <set>
#include <string>

using glideslope::net::KeyPair;
using glideslope::net::PublicKey;
using glideslope::net::SecretKey;
using glideslope::test::check;

// **A key written down and read back is the same key**, in either case of
// hexadecimal, and nothing that is not 64 hexadecimal digits is one.
GLIDESLOPE_TEST(a_key_written_down_and_read_back_is_the_same_key) {
    const KeyPair pair = glideslope::net::mint_key_pair();
    const std::string text = pair.publik.text();
    check(text.size() == glideslope::net::key_hex_digits,
          "a key is 64 digits, not " + std::to_string(text.size()));
    for (const char c : text) {
        check((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'),
              std::string("and lower-case hexadecimal, not '") + c + "'");
    }
    const auto back = glideslope::net::public_from_text(text);
    check(back.has_value() && *back == pair.publik, "and reads back as itself");

    // Upper case is read too, because somebody will type it that way.
    std::string shouted = text;
    for (char& c : shouted) {
        if (c >= 'a' && c <= 'f') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    const auto loud = glideslope::net::public_from_text(shouted);
    check(loud.has_value() && *loud == pair.publik, "in upper case as well");

    // **Everything that is not a key is refused.** Too short, too long, and
    // every character that is not a hexadecimal digit in the first place.
    std::size_t refused = 0;
    for (const std::string& bad :
         {std::string(""), std::string("0"), text.substr(0, 63), text + "0",
          text + "00"}) {
        check(!glideslope::net::public_from_text(bad).has_value(),
              "a key of " + std::to_string(bad.size()) + " digits is refused");
        ++refused;
    }
    std::size_t walked = 0;
    for (int c = 0; c < 256; ++c) {
        const auto ch = static_cast<char>(c);
        const bool hex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
                         (ch >= 'A' && ch <= 'F');
        std::string one = text;
        one[0] = ch;
        const bool read = glideslope::net::public_from_text(one).has_value();
        check(read == hex, std::string("a key beginning '") + ch +
                               "' is read only if that is a hexadecimal digit");
        ++walked;
        if (!hex) {
            ++refused;
        }
    }
    check(walked == 256, "every possible first character was tried");
    std::printf("  %zu things that are not keys were refused\n", refused);
}

// **A secret's public half is the one X25519 says it is.** The pair is held
// against RFC 7748's own test vector, so this fails if the key agreement
// underneath ever stops being X25519 - which no round trip of our own could
// tell us.
GLIDESLOPE_TEST(a_secrets_public_half_is_the_one_rfc_7748_gives_for_it) {
    // RFC 7748, section 6.1: Alice's private and public keys.
    const std::string alices_secret =
        "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a";
    const std::string alices_public =
        "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a";

    const auto secret = glideslope::net::secret_from_text(alices_secret);
    check(secret.has_value(), "the vector's secret reads");
    const PublicKey got = glideslope::net::public_from_secret(*secret);
    check(got.text() == alices_public,
          "RFC 7748 says that secret's public half is\n    " + alices_public +
              "\n  and this gives\n    " + got.text());
    std::printf("  RFC 7748's vector agrees\n");
}

// **A minted pair's halves go together**, and two mints are not the same key.
GLIDESLOPE_TEST(every_minted_key_is_its_own_and_its_halves_go_together) {
    std::set<std::string> seen;
    std::size_t walked = 0;
    for (int i = 0; i < 64; ++i) {
        const KeyPair pair = glideslope::net::mint_key_pair();
        check(glideslope::net::public_from_secret(pair.secret) == pair.publik,
              "the public half is the one the secret gives");
        check(seen.insert(pair.publik.text()).second,
              "and no two mints gave the same key");
        // A key of nothing but noughts would mean the minting never ran.
        check(pair.publik != PublicKey{}, "a minted key is not all noughts");
        check(pair.secret != SecretKey{}, "nor is its secret");
        ++walked;
    }
    check(walked == 64, "sixty-four keys were minted");
    check(seen.size() == 64, "and all sixty-four differ");
}
