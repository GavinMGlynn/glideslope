#include "harness.hpp"

#include "net/inside.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using glideslope::net::Inside;
using glideslope::net::knock;
using glideslope::net::knock_token;
using glideslope::net::known_inside;
using glideslope::test::check;

namespace {

// Every kind there is, named here so that the tests below can say how big the
// space is and fail if it grows without them.
const std::vector<std::pair<Inside, const char*>> every_kind = {
    {Inside::reliable, "reliable"}, {Inside::inputs, "inputs"},
    {Inside::state, "state"},       {Inside::ping, "ping"},
    {Inside::pong, "pong"},
};

} // namespace

// **The kinds a sealed body can hold are exactly five**, numbered 1 to 5, and
// every other byte is not one. All 256 are walked, so a sixth kind added
// without a test fails here.
GLIDESLOPE_TEST(the_kinds_inside_a_sealed_body_are_the_five_the_document_names) {
    check(every_kind.size() == 5, "five kinds, not " + std::to_string(every_kind.size()));
    std::set<std::uint8_t> theirs;
    for (const auto& [kind, name] : every_kind) {
        theirs.insert(static_cast<std::uint8_t>(kind));
    }
    check(theirs.size() == 5, "and no two share a number");

    int known = 0;
    int unknown = 0;
    for (int byte = 0; byte <= 255; ++byte) {
        const auto b = static_cast<std::uint8_t>(byte);
        const bool ours = theirs.count(b) != 0;
        check(known_inside(b) == ours,
              "byte " + std::to_string(byte) + " is " + (ours ? "" : "not ") +
                  "a kind, and known_inside disagrees");
        (ours ? known : unknown) += 1;
    }
    check(known == 5 && unknown == 251,
          "all 256 bytes walked: " + std::to_string(known) + " known, " +
              std::to_string(unknown) + " not");

    // The numbers themselves, because the document writes them down and a
    // client written from it will send these and no others.
    check(static_cast<std::uint8_t>(Inside::reliable) == 1, "reliable is 01");
    check(static_cast<std::uint8_t>(Inside::inputs) == 2, "inputs is 02");
    check(static_cast<std::uint8_t>(Inside::state) == 3, "state is 03");
    check(static_cast<std::uint8_t>(Inside::ping) == 4, "ping is 04");
    check(static_cast<std::uint8_t>(Inside::pong) == 5, "pong is 05");
}

// **A knock written down and read back carries the same token**, for both
// kinds that have one and for the values that break a careless encoder.
GLIDESLOPE_TEST(a_knock_written_and_read_back_carries_the_same_token) {
    const std::vector<std::uint64_t> tokens = {
        0u,
        1u,
        0xFFu,
        0x100u,
        0xFFFFFFFFu,          // where a 32-bit field would run out
        0x100000000u,         // and just past it
        0x0123456789ABCDEFu,  // every byte different, so an order mistake shows
        0xFFFFFFFFFFFFFFFFu,  // all ones
    };
    std::size_t walked = 0;
    for (const Inside kind : {Inside::ping, Inside::pong}) {
        for (const std::uint64_t token : tokens) {
            const std::vector<std::uint8_t> body = knock(kind, token);
            check(body.size() == glideslope::net::knock_size,
                  "a knock is nine bytes, not " + std::to_string(body.size()));
            check(body[0] == static_cast<std::uint8_t>(kind),
                  "and begins with its kind");
            const auto back = knock_token(
                kind, std::span<const std::uint8_t>(body.data(), body.size()));
            check(back.has_value() && *back == token,
                  "and gives back " + std::to_string(token));
            ++walked;
        }
    }
    check(walked == 2 * tokens.size(),
          "both kinds against all " + std::to_string(tokens.size()) + " tokens");

    // The token is little-endian, which is what the document says and what
    // every other number on this wire is.
    const std::vector<std::uint8_t> one = knock(Inside::ping, 1);
    check(one[1] == 1 && one[8] == 0, "the least significant byte goes first");
}

// **A knock of the wrong kind, the wrong length or nothing at all is not
// read.** Every length from nothing to twice a knock is walked, and every one
// but the right one is refused.
GLIDESLOPE_TEST(a_knock_that_is_not_one_is_not_read) {
    const std::vector<std::uint8_t> ping = knock(Inside::ping, 12345);

    // Read as the other kind: the byte says ping, so it is not a pong.
    check(!knock_token(Inside::pong,
                       std::span<const std::uint8_t>(ping.data(), ping.size()))
               .has_value(),
          "a ping is not a pong");

    std::size_t refused = 0;
    std::size_t taken = 0;
    for (std::size_t length = 0; length <= 2 * glideslope::net::knock_size; ++length) {
        std::vector<std::uint8_t> body(length, 0);
        if (length > 0) {
            body[0] = static_cast<std::uint8_t>(Inside::ping);
        }
        const auto got =
            knock_token(Inside::ping, std::span<const std::uint8_t>(body.data(), length));
        if (got.has_value()) {
            ++taken;
            check(length == glideslope::net::knock_size,
                  "only nine bytes is a knock, and " + std::to_string(length) +
                      " was taken for one");
        } else {
            ++refused;
        }
    }
    check(taken == 1, "exactly one length is a knock");
    check(taken + refused == 2 * glideslope::net::knock_size + 1,
          "every length from 0 to 18 was walked");

    // Every byte of the kind, against both readers: only its own is taken.
    std::size_t wrong = 0;
    for (int byte = 0; byte <= 255; ++byte) {
        std::vector<std::uint8_t> body(glideslope::net::knock_size, 0);
        body[0] = static_cast<std::uint8_t>(byte);
        const bool as_ping =
            knock_token(Inside::ping,
                        std::span<const std::uint8_t>(body.data(), body.size()))
                .has_value();
        const bool as_pong =
            knock_token(Inside::pong,
                        std::span<const std::uint8_t>(body.data(), body.size()))
                .has_value();
        check(as_ping == (byte == static_cast<int>(Inside::ping)), "only 04 is a ping");
        check(as_pong == (byte == static_cast<int>(Inside::pong)), "only 05 is a pong");
        if (!as_ping && !as_pong) {
            ++wrong;
        }
    }
    check(wrong == 254, "254 of the 256 first bytes are neither");
}

// **The document and the code agree about what a sealed body holds**, by
// value and by name, so that a client written from `TRANSPORT.md` sends the
// bytes this code reads.
GLIDESLOPE_TEST(the_transport_document_and_the_code_agree_about_a_sealed_body) {
    const std::filesystem::path doc =
        std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "docs" / "TRANSPORT.md";
    std::ifstream in(doc);
    check(in.good(), "docs/TRANSPORT.md is there");
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto says = [&](const std::string& what) {
        return text.find(what) != std::string::npos;
    };

    const std::vector<std::pair<Inside, std::string>> named = {
        {Inside::reliable, "RELIABLE"}, {Inside::inputs, "INPUTS"},
        {Inside::state, "STATE"},       {Inside::ping, "PING"},
        {Inside::pong, "PONG"},
    };
    check(named.size() == every_kind.size(), "one name per kind");
    std::size_t walked = 0;
    for (const auto& [kind, name] : named) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "`%02X`", static_cast<unsigned>(kind));
        check(says(std::string("| ") + buf + " | `" + name + "` |"),
              "the document gives " + name + " as " + buf);
        ++walked;
    }
    check(walked == 5, "every kind was walked");

    // And it says the two things about them that are not in the table.
    check(says("A kind this version does not know is **ignored, not refused**"),
          "the document says an unknown kind is ignored");
    check(says("`u64`, a token"), "and that a knock carries a token");
}
