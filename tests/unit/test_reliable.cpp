#include "harness.hpp"

#include "net/protocol.hpp"
#include "net/reliable.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

using glideslope::net::Reliable;
using glideslope::test::check;

namespace {

std::vector<std::uint8_t> body_of(int n) {
    const std::string text = "message " + std::to_string(n);
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

// **Two endpoints and a channel that loses what it is told to.** `lose` is a
// bit per datagram, in the order they are put on the wire counting both
// directions: bit k set loses the kth. Datagrams past the mask's width all
// arrive, so every exchange finishes and the test can say what arrived.
struct Exchange {
    std::vector<std::vector<std::uint8_t>> arrived;
    std::uint64_t put_on_the_wire = 0;
    std::uint64_t lost = 0;
    bool finished = false;
    double took_s = 0.0;
};

Exchange run(int messages, std::uint32_t lose, int mask_width) {
    Reliable a;
    Reliable b;
    Exchange out;
    for (int i = 0; i < messages; ++i) {
        const std::vector<std::uint8_t> body = body_of(i);
        check(a.send(all_of(body)), "a message is queued");
    }

    int on_the_wire = 0;
    const auto carry = [&](Reliable& from, Reliable& to, double now_s, bool collect) {
        for (const std::vector<std::uint8_t>& datagram : from.to_send(now_s)) {
            const bool drop = on_the_wire < mask_width &&
                              (lose & (1u << on_the_wire)) != 0;
            ++on_the_wire;
            ++out.put_on_the_wire;
            if (drop) {
                ++out.lost;
                continue;
            }
            for (std::vector<std::uint8_t>& got : to.received(all_of(datagram))) {
                if (collect) {
                    out.arrived.push_back(std::move(got));
                }
            }
        }
    };

    double now_s = 0.0;
    for (int round = 0; round < 400; ++round) {
        carry(a, b, now_s, true);
        carry(b, a, now_s, false);
        if (a.in_flight() == 0 &&
            out.arrived.size() == static_cast<std::size_t>(messages)) {
            out.finished = true;
            out.took_s = now_s;
            break;
        }
        // Far enough for every retransmission that is due.
        now_s += glideslope::net::retry_after_s;
    }
    return out;
}

// Whether `arrived` is exactly messages 0 to n-1, in order and once each.
bool exactly_once_in_order(const std::vector<std::vector<std::uint8_t>>& arrived,
                           int n) {
    if (arrived.size() != static_cast<std::size_t>(n)) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        if (arrived[static_cast<std::size_t>(i)] != body_of(i)) {
            return false;
        }
    }
    return true;
}

} // namespace

// **With nothing lost, everything arrives once, in order, at once.**
GLIDESLOPE_TEST(with_nothing_lost_every_reliable_message_arrives_once_and_in_order) {
    const Exchange e = run(8, 0, 0);
    check(e.finished, "the exchange finished");
    check(exactly_once_in_order(e.arrived, 8),
          "all eight arrived exactly once and in order");
    check(e.lost == 0, "nothing was lost");
}

// **The item's own verification**: every message arrives exactly once and in
// order under injected loss. Every pattern of loss over the first twelve
// datagrams, in both directions - 4,096 of them, which is all there are.
GLIDESLOPE_TEST(under_every_pattern_of_loss_every_message_arrives_exactly_once_and_in_order) {
    constexpr int messages = 6;
    constexpr int mask_width = 12;
    constexpr std::uint32_t patterns = 1u << mask_width;

    std::uint32_t walked = 0;
    std::uint32_t with_loss = 0;
    std::uint64_t worst_datagrams = 0;
    std::uint32_t worst_pattern = 0;
    for (std::uint32_t lose = 0; lose < patterns; ++lose) {
        const Exchange e = run(messages, lose, mask_width);
        if (!e.finished || !exactly_once_in_order(e.arrived, messages)) {
            glideslope::test::fail(
                "with loss pattern " + std::to_string(lose) + " of " +
                std::to_string(patterns) + ", " + std::to_string(e.arrived.size()) +
                " of " + std::to_string(messages) +
                " messages arrived" + (e.finished ? "" : " and it never finished"));
        }
        if (e.lost > 0) {
            ++with_loss;
        }
        if (e.put_on_the_wire > worst_datagrams) {
            worst_datagrams = e.put_on_the_wire;
            worst_pattern = lose;
        }
        ++walked;
    }
    check(walked == patterns,
          "every pattern of loss was walked: " + std::to_string(walked) + " of " +
              std::to_string(patterns));
    check(patterns == 4096, "there are 4,096 patterns over twelve datagrams");
    // **Not every pattern loses something**, and the number that do is
    // stated rather than guessed. An exchange of six messages with nothing
    // lost is over in seven datagrams - six out and one acknowledgement back
    // - so a pattern whose set bits all lie at 7 to 11 never touches
    // anything. There are 2^5 = 32 such patterns, including the empty one,
    // which leaves 4,064 that do lose something.
    check(with_loss == 4064,
          "4,064 of the patterns lost something, not " + std::to_string(with_loss) +
              "; the rest set only bits past the end of the exchange");
    // The worst case is stated so that a change making delivery far more
    // expensive shows up here rather than nowhere.
    check(worst_datagrams <= 40,
          "the worst pattern (" + std::to_string(worst_pattern) + ") took " +
              std::to_string(worst_datagrams) + " datagrams, which is more than 40");
}

// **A message that arrives twice is handed up once.**
GLIDESLOPE_TEST(a_reliable_message_that_arrives_twice_is_handed_up_once) {
    Reliable a;
    Reliable b;
    for (int i = 0; i < 4; ++i) {
        const std::vector<std::uint8_t> body = body_of(i);
        check(a.send(all_of(body)), "a message is queued");
    }
    std::vector<std::vector<std::uint8_t>> arrived;
    // Everything a sends, delivered twice over.
    for (const std::vector<std::uint8_t>& datagram : a.to_send(0.0)) {
        for (int twice = 0; twice < 2; ++twice) {
            for (std::vector<std::uint8_t>& got : b.received(all_of(datagram))) {
                arrived.push_back(std::move(got));
            }
        }
    }
    check(exactly_once_in_order(arrived, 4),
          "each of the four arrived once, though each was delivered twice");
}

// **A message that arrives early waits for its predecessors**, and they all
// come up together when the missing one lands.
GLIDESLOPE_TEST(a_reliable_message_that_arrives_early_waits_for_the_ones_before_it) {
    Reliable a;
    Reliable b;
    for (int i = 0; i < 4; ++i) {
        const std::vector<std::uint8_t> body = body_of(i);
        check(a.send(all_of(body)), "a message is queued");
    }
    const std::vector<std::vector<std::uint8_t>> datagrams = a.to_send(0.0);
    check(datagrams.size() == 4, "four datagrams went out");

    // The last three first, then the first.
    std::vector<std::vector<std::uint8_t>> arrived;
    for (std::size_t i = 1; i < 4; ++i) {
        const auto got = b.received(all_of(datagrams[i]));
        check(got.empty(), "nothing is handed up while the first is missing");
    }
    check(b.held_back() == 3, "three are held back");
    check(b.delivered() == 0, "and none has been handed up");

    for (std::vector<std::uint8_t>& got : b.received(all_of(datagrams[0]))) {
        arrived.push_back(std::move(got));
    }
    check(exactly_once_in_order(arrived, 4),
          "all four come up together, in order, when the missing one lands");
    check(b.held_back() == 0, "and nothing is held back any more");
}

// **An endpoint with nothing to say still answers**, or the far end
// retransmits for ever at a receiver that already has everything.
GLIDESLOPE_TEST(an_endpoint_with_nothing_to_say_still_acknowledges_what_it_has) {
    Reliable a;
    Reliable b;
    const std::vector<std::uint8_t> body = body_of(1);
    check(a.send(all_of(body)), "a message is queued");

    const auto out = a.to_send(0.0);
    check(out.size() == 1, "one datagram went out");
    check(b.received(all_of(out[0])).size() == 1, "and arrived");

    // b has nothing of its own to send, and must still answer.
    const auto answer = b.to_send(0.0);
    check(answer.size() == 1, "b answers though it has nothing to say");
    check(answer[0].size() == glideslope::net::reliable_header_size,
          "and its answer is a header and no body");
    check(a.received(all_of(answer[0])).empty(),
          "an acknowledgement is not a message and is handed up as nothing");
    check(a.in_flight() == 0, "and it clears what a was holding");

    // With nothing owed and nothing to say, it says nothing at all.
    check(b.to_send(1.0).empty(), "and then it goes quiet");
}

// **Rubbish off the wire is ignored**, not trusted and not fatal.
GLIDESLOPE_TEST(a_reliable_endpoint_ignores_a_datagram_that_is_not_one_of_its_own) {
    Reliable b;
    const std::vector<std::vector<std::uint8_t>> rubbish{
        {},
        {1},
        {1, 2, 3},
        {1, 2, 3, 4, 5, 6, 7}, // one byte short of a header
    };
    std::size_t walked = 0;
    for (const std::vector<std::uint8_t>& one : rubbish) {
        check(b.received(all_of(one)).empty(),
              "a datagram of " + std::to_string(one.size()) +
                  " bytes is handed up as nothing");
        ++walked;
    }
    check(walked == rubbish.size(), "every kind of rubbish was walked");
    check(b.delivered() == 0, "and none of it was taken for a message");
}

// **A sender that gets far enough behind refuses rather than queueing for
// ever.**
GLIDESLOPE_TEST(a_reliable_sender_refuses_once_too_many_are_waiting) {
    Reliable a;
    const std::vector<std::uint8_t> body = body_of(1);
    std::size_t queued = 0;
    while (a.send(all_of(body))) {
        ++queued;
        if (queued > glideslope::net::most_in_flight + 10) {
            break;
        }
    }
    check(queued == glideslope::net::most_in_flight,
          "it queued " + std::to_string(glideslope::net::most_in_flight) +
              " and then refused, not " + std::to_string(queued));
    check(a.in_flight() == glideslope::net::most_in_flight, "and holds them all");
}

namespace {

// **A datagram that is an acknowledgement and nothing else**: the reliable
// layer's header with a message number of 0, then the number it claims to
// have received everything up to. This is what a forger would send, because
// it is the smallest datagram the layer will read.
std::vector<std::uint8_t> an_acknowledgement_of(std::uint32_t acknowledges) {
    glideslope::net::Writer w;
    w.u32(0);
    w.u32(acknowledges);
    return w.take();
}

// An endpoint that has put `sent` messages on the wire and has `later` more
// queued behind them that have not gone out yet.
Reliable a_sender(std::uint32_t sent, std::uint32_t later) {
    Reliable a;
    for (std::uint32_t i = 0; i < sent; ++i) {
        const std::vector<std::uint8_t> body = body_of(static_cast<int>(i));
        check(a.send(all_of(body)), "a message is queued");
    }
    a.to_send(0.0); // those, and only those, are now on the wire
    for (std::uint32_t i = 0; i < later; ++i) {
        const std::vector<std::uint8_t> body = body_of(static_cast<int>(sent + i));
        check(a.send(all_of(body)), "a later message is queued");
    }
    return a;
}

// What an acknowledgement of `ack` may let go of, for a sender that has put
// `sent` messages on the wire: those numbered at or below it, and nothing at
// all if it claims more than was ever sent.
std::size_t may_let_go(std::uint32_t ack, std::uint32_t sent) {
    return ack <= sent ? ack : 0;
}

// Feeds `a` one forged acknowledgement and says whether the queue is what it
// should be afterwards.
void feed_one(std::uint32_t sent, std::uint32_t later, std::uint32_t ack) {
    Reliable a = a_sender(sent, later);
    const std::vector<std::uint8_t> forged = an_acknowledgement_of(ack);
    check(a.received(all_of(forged)).empty(),
          "an acknowledgement is not a message and is handed up as nothing");
    const std::size_t expected =
        static_cast<std::size_t>(sent) + later - may_let_go(ack, sent);
    check(a.in_flight() == expected,
          "a sender that had put " + std::to_string(sent) + " on the wire with " +
              std::to_string(later) + " behind them, told it was acknowledged up to " +
              std::to_string(ack) + ", should hold " + std::to_string(expected) +
              " and holds " + std::to_string(a.in_flight()));
}

} // namespace

// **An acknowledgement of a message that was never sent is not believed.**
// The far end cannot have received what this end never put on the wire, so a
// datagram saying it did is either mangled or forged; believing one empties
// the send queue of messages that have never been delivered, and nothing
// sends them again. Nothing seals these datagrams yet, so the layer refuses
// it on its own rather than trusting a caller to have checked.
//
// **The space, stated.** An acknowledgement is a `u32`, so there are
// 4,294,967,296 a datagram could carry, which is too many to feed one at a
// time. Three parts are walked and counted instead:
//
//   - every acknowledgement from 0 to two past the end, for every sender
//     that has put 0 to 4 messages on the wire with 0 to 4 more queued
//     behind them - 175 of them, which is every case where the answer
//     changes as the number crosses what was sent;
//   - the 64 far numbers a forger reaches for: every power of two, every
//     power of two less one, and `0xFFFFFFFF`;
//   - and one whole exchange, forged part-way through.
//
// Every acknowledgement not walked is strictly greater than the four
// messages the sender had put on the wire, which is the case `0xFFFFFFFF`
// walks: the layer compares against what it has sent and does no arithmetic
// on the number itself, so there is nothing for a larger one to do
// differently.
GLIDESLOPE_TEST(an_acknowledgement_of_a_message_that_was_never_sent_is_not_believed) {
    std::size_t fed = 0;

    // The boundary, walked exhaustively.
    std::size_t at_the_boundary = 0;
    for (std::uint32_t sent = 0; sent <= 4; ++sent) {
        for (std::uint32_t later = 0; later <= 4; ++later) {
            for (std::uint32_t ack = 0; ack <= sent + later + 2; ++ack) {
                feed_one(sent, later, ack);
                ++at_the_boundary;
                ++fed;
            }
        }
    }
    check(at_the_boundary == 175,
          "every acknowledgement at the boundary was walked: " +
              std::to_string(at_the_boundary) + " of 175");

    // The far numbers, with four on the wire and two behind them.
    std::vector<std::uint32_t> far;
    for (int k = 0; k < 32; ++k) {
        far.push_back(1u << k);
        far.push_back((1u << k) - 1u);
    }
    far.push_back(0xFFFFFFFFu);
    std::sort(far.begin(), far.end());
    far.erase(std::unique(far.begin(), far.end()), far.end());
    check(far.size() == 64,
          "sixty-four distinct far acknowledgements, not " + std::to_string(far.size()));
    for (const std::uint32_t ack : far) {
        feed_one(4, 2, ack);
        ++fed;
    }

    // And the whole of it: a forgery part-way through an exchange changes
    // nothing about what still arrives.
    Reliable a;
    Reliable b;
    for (int i = 0; i < 4; ++i) {
        const std::vector<std::uint8_t> body = body_of(i);
        check(a.send(all_of(body)), "a message is queued");
    }
    const std::vector<std::uint8_t> forged = an_acknowledgement_of(0xFFFFFFFFu);
    check(a.received(all_of(forged)).empty(), "the forgery is handed up as nothing");
    ++fed;
    check(a.in_flight() == 4, "and the sender still holds all four, not " +
                                  std::to_string(a.in_flight()));

    std::vector<std::vector<std::uint8_t>> arrived;
    double now_s = 0.0;
    for (int round = 0; round < 20; ++round) {
        for (const std::vector<std::uint8_t>& datagram : a.to_send(now_s)) {
            for (std::vector<std::uint8_t>& got : b.received(all_of(datagram))) {
                arrived.push_back(std::move(got));
            }
        }
        for (const std::vector<std::uint8_t>& datagram : b.to_send(now_s)) {
            check(a.received(all_of(datagram)).empty(), "b answers with acknowledgements");
        }
        if (a.in_flight() == 0) {
            break;
        }
        now_s += glideslope::net::retry_after_s;
    }
    check(exactly_once_in_order(arrived, 4),
          "all four still arrive, once each and in order, after the forgery");
    check(a.in_flight() == 0, "and the real acknowledgements still clear the queue");

    check(fed == 240, "240 acknowledgements were fed: " + std::to_string(fed));
    std::printf("  240 acknowledgements fed: 175 at the boundary, 64 far ones, "
                "and one mid-exchange\n");
}
