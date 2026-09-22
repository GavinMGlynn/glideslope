#include "harness.hpp"

#include "net/sealing.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <numeric>
#include <set>
#include <span>
#include <string>
#include <vector>

using glideslope::net::Sealer;
using glideslope::net::TrafficKey;
using glideslope::net::Unsealer;
using glideslope::test::check;

namespace {

TrafficKey a_key(std::uint8_t of) {
    TrafficKey k;
    k.bytes.fill(of);
    return k;
}

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

std::vector<std::uint8_t> body_of(int n) {
    const std::string s = "body " + std::to_string(n);
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

} // namespace

// **What is sealed opens as itself**, and the sealed body is longer by
// exactly the number and the tag.
GLIDESLOPE_TEST(what_is_sealed_opens_as_itself) {
    Sealer sealer(a_key(0x11));
    Unsealer unsealer(a_key(0x11));
    std::size_t walked = 0;
    for (int i = 0; i < 64; ++i) {
        const std::vector<std::uint8_t> plain = body_of(i);
        const std::vector<std::uint8_t> sealed = sealer.seal(all_of(plain));
        check(sealed.size() == plain.size() + glideslope::net::sealing_overhead,
              "a sealed body is the plain one and " +
                  std::to_string(glideslope::net::sealing_overhead) + " bytes");
        const auto got = unsealer.open(all_of(sealed));
        check(got.has_value() && *got == plain, "and opens as what went in");
        ++walked;
    }
    check(walked == 64, "sixty-four bodies were sealed and opened");
    // An empty body is a body.
    const std::vector<std::uint8_t> nothing;
    const auto got = unsealer.open(all_of(sealer.seal(all_of(nothing))));
    check(got.has_value() && got->empty(), "an empty body seals and opens");
}

// **The two directions do not share a key**, so a datagram cannot be
// reflected back at the end that sent it.
GLIDESLOPE_TEST(a_sealed_body_does_not_open_under_another_key) {
    Sealer sealer(a_key(0x11));
    Unsealer other(a_key(0x22));
    const std::vector<std::uint8_t> sealed = sealer.seal(all_of(body_of(1)));
    check(!other.open(all_of(sealed)).has_value(),
          "a body sealed one way does not open the other");
    check(other.why() == Unsealer::Refused::not_ours, "and says why");
}

// **A datagram that arrives twice opens once**, which is what the replay
// window is for.
GLIDESLOPE_TEST(a_sealed_body_that_arrives_twice_opens_once) {
    Sealer sealer(a_key(0x33));
    Unsealer unsealer(a_key(0x33));
    const std::vector<std::uint8_t> sealed = sealer.seal(all_of(body_of(7)));
    check(unsealer.open(all_of(sealed)).has_value(), "it opens the first time");
    check(!unsealer.open(all_of(sealed)).has_value(), "and not the second");
    check(unsealer.why() == Unsealer::Refused::replayed, "which it calls a replay");
    check(!unsealer.open(all_of(sealed)).has_value(), "nor the third");
}

// **Every order of arrival opens every datagram exactly once**, and so does
// every order with every datagram sent twice. This is the whole space of
// orders over six datagrams - all 720 - because reordering is what UDP does
// and a window that let one through twice, or held one back for good, would
// be a fault nobody would see until a session went wrong.
GLIDESLOPE_TEST(every_order_of_arrival_opens_every_sealed_body_exactly_once) {
    constexpr int bodies = 6;
    Sealer sealer(a_key(0x44));
    std::vector<std::vector<std::uint8_t>> sealed;
    for (int i = 0; i < bodies; ++i) {
        sealed.push_back(sealer.seal(all_of(body_of(i))));
    }

    std::vector<int> order(bodies);
    std::iota(order.begin(), order.end(), 0);
    std::size_t orders = 0;
    do {
        // Once each.
        {
            Unsealer unsealer(a_key(0x44));
            std::set<int> opened;
            for (const int i : order) {
                if (unsealer.open(all_of(sealed[static_cast<std::size_t>(i)]))) {
                    check(opened.insert(i).second, "no body opened twice");
                }
            }
            check(opened.size() == bodies,
                  "every body opened in this order: " + std::to_string(opened.size()) +
                      " of " + std::to_string(bodies));
        }
        // And twice each, in the same order.
        {
            Unsealer unsealer(a_key(0x44));
            std::set<int> opened;
            for (const int i : order) {
                for (int again = 0; again < 2; ++again) {
                    if (unsealer.open(all_of(sealed[static_cast<std::size_t>(i)]))) {
                        check(opened.insert(i).second,
                              "no body opened twice, even sent twice");
                    }
                }
            }
            check(opened.size() == bodies, "and every body still opened");
        }
        ++orders;
    } while (std::next_permutation(order.begin(), order.end()));

    check(orders == 720, "all 720 orders of six were walked, not " +
                             std::to_string(orders));
    std::printf("  720 orders, each once and each twice, all opened exactly once\n");
}

// **A datagram older than the window is refused**, and the limit is stated:
// one delayed by more than the window cannot be told from a replay.
GLIDESLOPE_TEST(a_sealed_body_older_than_the_window_is_refused_and_the_limit_is_stated) {
    Sealer sealer(a_key(0x55));
    Unsealer unsealer(a_key(0x55));
    const std::vector<std::uint8_t> first = sealer.seal(all_of(body_of(0)));

    // Everything after it arrives, filling and moving the window.
    const auto window = static_cast<int>(glideslope::net::replay_window);
    for (int i = 1; i <= window; ++i) {
        const std::vector<std::uint8_t> next = sealer.seal(all_of(body_of(i)));
        check(unsealer.open(all_of(next)).has_value(),
              "datagram " + std::to_string(i) + " opens");
    }
    // Now the first is exactly the window behind, and is refused.
    check(!unsealer.open(all_of(first)).has_value(),
          "one " + std::to_string(window) + " behind the newest is refused");
    check(unsealer.why() == Unsealer::Refused::too_old, "as too old");

    // One just inside the window still opens.
    Sealer again(a_key(0x66));
    Unsealer opener(a_key(0x66));
    std::vector<std::vector<std::uint8_t>> all;
    for (int i = 0; i <= window; ++i) {
        all.push_back(again.seal(all_of(body_of(i))));
    }
    check(opener.open(all_of(all[static_cast<std::size_t>(window)])).has_value(),
          "the newest opens");
    check(opener.open(all_of(all[1])).has_value(),
          "and one " + std::to_string(window - 1) + " behind it still does");
    std::printf("  the window is %d, and its far edge was found\n", window);
}

// **Every single-byte change to a sealed body is refused**, and every
// truncation. This is the whole space of one-byte changes.
GLIDESLOPE_TEST(every_change_to_a_sealed_body_is_refused) {
    Sealer sealer(a_key(0x77));
    const std::vector<std::uint8_t> sealed = sealer.seal(all_of(body_of(3)));

    std::size_t walked = 0;
    std::size_t opened = 0;
    for (std::size_t at = 0; at < sealed.size(); ++at) {
        for (int v = 0; v < 256; ++v) {
            const auto value = static_cast<std::uint8_t>(v);
            if (value == sealed[at]) {
                continue;
            }
            std::vector<std::uint8_t> changed = sealed;
            changed[at] = value;
            Unsealer unsealer(a_key(0x77)); // fresh, so the window is not the reason
            if (unsealer.open(all_of(changed)).has_value()) {
                ++opened;
            }
            ++walked;
        }
    }
    check(walked == sealed.size() * 255,
          "every single-byte change was tried: " + std::to_string(walked));
    check(opened == 0, std::to_string(opened) +
                           " changed bodies opened, and none should");

    std::size_t cut = 0;
    for (std::size_t n = 0; n < sealed.size(); ++n) {
        Unsealer unsealer(a_key(0x77));
        check(!unsealer.open(std::span<const std::uint8_t>(sealed.data(), n))
                   .has_value(),
              "a body cut to " + std::to_string(n) + " bytes is refused");
        ++cut;
    }
    std::printf("  %zu one-byte changes and %zu truncations, all refused\n", walked,
                cut);
}
