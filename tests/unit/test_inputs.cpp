#include "harness.hpp"

#include "net/inputs.hpp"
#include "sim/aircraft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <vector>

using glideslope::net::ControlList;
using glideslope::net::InputFrame;
using glideslope::net::InputReceiver;
using glideslope::net::InputSender;
using glideslope::test::check;

namespace {

// The controls for frame `n`, every one of them different from every other
// frame's, so that a frame delivered in the wrong place is caught.
ControlList controls_of(std::uint32_t n) {
    ControlList out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        // Spread over -1..1 and never repeating between frames.
        const double t = static_cast<double>(n * out.size() + i);
        out[i] = std::sin(t * 0.37);
    }
    return out;
}

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

// Whether `f` carries the controls frame `f.sequence` was sent with, as they
// went on the wire.
bool carries_its_own_controls(const InputFrame& f) {
    const ControlList sent = glideslope::net::as_sent(controls_of(f.sequence));
    for (std::size_t i = 0; i < sent.size(); ++i) {
        if (f.controls[i] != sent[i]) {
            return false;
        }
    }
    return true;
}

// Sends `frames` frames, losing the packets `lose` names by bit, and answers
// which sequences arrived.
std::vector<std::uint32_t> run(int frames, std::uint32_t lose) {
    InputSender sender;
    InputReceiver receiver;
    std::vector<std::uint32_t> arrived;
    for (int i = 0; i < frames; ++i) {
        const auto sequence = static_cast<std::uint32_t>(i + 1);
        sender.add(sequence, controls_of(sequence));
        const std::vector<std::uint8_t> packet = sender.packet();
        if ((lose & (1u << i)) != 0) {
            continue; // this datagram never arrives
        }
        for (const InputFrame& f : receiver.received(all_of(packet))) {
            check(carries_its_own_controls(f),
                  "frame " + std::to_string(f.sequence) + " carries its own controls");
            arrived.push_back(f.sequence);
        }
    }
    return arrived;
}

} // namespace

// **Every control goes on the wire and comes back within a step of itself,
// and the ends are exact.** A control held hard over must arrive hard over,
// not a hair short of it.
GLIDESLOPE_TEST(every_control_goes_on_the_wire_and_comes_back_within_one_step) {
    // The step: the range -1..1 across a signed 16-bit integer.
    const double step = 1.0 / 32767.0;
    check(glideslope::net::unquantise(glideslope::net::quantise(1.0)) == 1.0,
          "hard over one way is exact");
    check(glideslope::net::unquantise(glideslope::net::quantise(-1.0)) == -1.0,
          "and the other");
    check(glideslope::net::unquantise(glideslope::net::quantise(0.0)) == 0.0,
          "and the middle");

    // Every value in the range, at a fine spacing, and beyond both ends.
    std::size_t walked = 0;
    double worst = 0.0;
    for (int i = -20000; i <= 20000; ++i) {
        const double asked = static_cast<double>(i) / 10000.0; // -2 .. 2
        const double got = glideslope::net::unquantise(glideslope::net::quantise(asked));
        const double held = std::clamp(asked, -1.0, 1.0);
        worst = std::max(worst, std::abs(got - held));
        check(std::abs(got - held) <= step,
              "a control of " + std::to_string(asked) + " came back as " +
                  std::to_string(got));
        ++walked;
    }
    check(walked == 40001, "the whole range and past both ends was walked");
    std::printf("  40,001 values, worst %.3e against a step of %.3e\n", worst, step);

    // What the client must fly is what it sent, and sending it again changes
    // nothing.
    const ControlList once = glideslope::net::as_sent(controls_of(7));
    const ControlList twice = glideslope::net::as_sent(once);
    check(once == twice, "what was sent, sent again, is itself");
}

// **With nothing lost, every frame arrives once and in order.**
GLIDESLOPE_TEST(with_nothing_lost_every_input_frame_arrives_once_and_in_order) {
    const std::vector<std::uint32_t> arrived = run(12, 0);
    check(arrived.size() == 12, "twelve frames arrived, not " +
                                    std::to_string(arrived.size()));
    for (std::size_t i = 0; i < arrived.size(); ++i) {
        check(arrived[i] == i + 1, "in order");
    }
}

// **No input frame is lost under injected loss**, which is the item's own
// verification - stated exactly, and walked over every pattern there is.
//
// A packet carries the last `redundancy` frames, so frame *n* rides in
// packets *n* to *n + redundancy - 1*, as far as those exist. **A frame
// arrives if and only if at least one packet carrying it arrives**, and that
// is what is held here, frame by frame, for all 4,096 patterns of loss over
// twelve packets.
//
// **The first shape of this test was wrong**, and the test caught it: it
// claimed nothing is lost unless `redundancy` packets go in a row, which is
// true in the middle of a stream and false at its end - losing only the last
// packet loses the last frame, because no later packet carries it. The rule
// below has no such edge.
GLIDESLOPE_TEST(an_input_frame_arrives_exactly_when_a_packet_carrying_it_arrives) {
    constexpr int frames = 12;
    constexpr std::uint32_t patterns = 1u << frames;
    const auto cover = static_cast<int>(glideslope::net::redundancy);

    std::uint32_t walked = 0;
    std::uint32_t whole = 0;   // patterns where every frame got through
    std::uint32_t partial = 0; // patterns where at least one did not
    for (std::uint32_t lose = 0; lose < patterns; ++lose) {
        const std::vector<std::uint32_t> arrived = run(frames, lose);
        const std::set<std::uint32_t> got(arrived.begin(), arrived.end());
        check(got.size() == arrived.size(),
              "no frame arrives twice under pattern " + std::to_string(lose));
        for (std::size_t i = 1; i < arrived.size(); ++i) {
            check(arrived[i] > arrived[i - 1],
                  "frames arrive in order under pattern " + std::to_string(lose));
        }

        for (int n = 1; n <= frames; ++n) {
            // The packets that carry frame n: the nth and the `cover - 1`
            // after it, as far as there are any. Packet k is bit k - 1.
            bool any_arrived = false;
            for (int k = n; k <= std::min(n + cover - 1, frames); ++k) {
                if ((lose & (1u << (k - 1))) == 0) {
                    any_arrived = true;
                    break;
                }
            }
            const bool here = got.contains(static_cast<std::uint32_t>(n));
            if (here != any_arrived) {
                glideslope::test::fail(
                    "with loss pattern " + std::to_string(lose) + ", frame " +
                    std::to_string(n) + (here ? " arrived" : " did not arrive") +
                    " but a packet carrying it " +
                    (any_arrived ? "did" : "did not") + " get through");
            }
        }
        if (got.size() == frames) {
            ++whole;
        } else {
            ++partial;
        }
        ++walked;
    }
    check(walked == patterns, "every pattern of loss was walked: " +
                                  std::to_string(walked));
    check(patterns == 4096, "there are 4,096 patterns over twelve packets");
    check(whole + partial == patterns, "every pattern fell on one side or the other");
    check(whole > 0 && partial > 0, "and both sides have patterns in them");
    std::printf("  4,096 patterns: %u lose no input at all, %u lose at least one\n",
                whole, partial);
}

// **The bound is real.** Losing a whole packet's worth in a row does lose an
// input, and saying so is what makes the guarantee above mean anything.
GLIDESLOPE_TEST(losing_a_whole_packets_worth_of_datagrams_in_a_row_does_lose_an_input) {
    const auto cover = static_cast<int>(glideslope::net::redundancy);
    // Lose packets 1 to `cover` (bits 1..cover), keeping the first and the
    // rest: frame 2 was carried by packets 2 to cover+1, all of which are
    // gone.
    std::uint32_t lose = 0;
    for (int i = 1; i <= cover; ++i) {
        lose |= 1u << i;
    }
    const std::vector<std::uint32_t> arrived = run(12, lose);
    const std::set<std::uint32_t> got(arrived.begin(), arrived.end());
    check(got.size() < 12, "something was lost, as the bound says it must be");
    check(!got.contains(2), "frame 2 is the one gone: every packet carrying it "
                            "was lost");
    std::printf("  losing %d packets in a row lost %zu of 12 frames\n", cover,
                12 - got.size());
}

// **A packet that arrives twice hands up nothing the second time.**
GLIDESLOPE_TEST(an_input_packet_that_arrives_twice_hands_up_nothing_the_second_time) {
    InputSender sender;
    InputReceiver receiver;
    for (std::uint32_t i = 1; i <= 3; ++i) {
        sender.add(i, controls_of(i));
    }
    const std::vector<std::uint8_t> packet = sender.packet();
    check(receiver.received(all_of(packet)).size() == 3, "three frames the first time");
    check(receiver.received(all_of(packet)).empty(), "and none the second");
    check(receiver.newest() == 3, "the newest is still three");
}

// **Nothing that is not an input packet is taken for one**: every truncation
// of one, and a packet with anything after it.
GLIDESLOPE_TEST(an_input_receiver_refuses_anything_that_is_not_an_input_packet) {
    InputSender sender;
    for (std::uint32_t i = 1; i <= glideslope::net::redundancy; ++i) {
        sender.add(i, controls_of(i));
    }
    const std::vector<std::uint8_t> packet = sender.packet();
    check(!packet.empty(), "there is a packet to cut up");

    std::size_t walked = 0;
    for (std::size_t n = 0; n < packet.size(); ++n) {
        InputReceiver receiver;
        check(receiver.received(std::span<const std::uint8_t>(packet.data(), n)).empty(),
              "a packet cut to " + std::to_string(n) + " bytes hands up nothing");
        ++walked;
    }
    std::vector<std::uint8_t> longer = packet;
    longer.push_back(0);
    InputReceiver after;
    check(after.received(all_of(longer)).empty(),
          "and one with a byte after it hands up nothing");
    check(walked > 100, "every truncation was tried: " + std::to_string(walked));

    // An empty sender makes an empty packet, which is not one.
    const InputSender nothing;
    InputReceiver receiver;
    check(nothing.packet().empty(), "a sender with nothing sends nothing");
    check(receiver.received(all_of(nothing.packet())).empty(), "and it is not a packet");
}

// **Every control the flight model has goes on the wire.** There is no
// reflection in C++, so this holds the structure's size: a control added to
// `sim::Controls` without being added to `as_list` changes the size and
// fails here, rather than being quietly left out of every flight.
GLIDESLOPE_TEST(every_control_the_flight_model_has_is_one_the_wire_carries) {
    check(glideslope::sim::Controls::control_count ==
              glideslope::net::controls_on_the_wire,
          "the flight model's list and the wire's are the same length");
    // Thirteen doubles and two arrays of two: seventeen doubles in all.
    check(sizeof(glideslope::sim::Controls) == 17 * sizeof(double),
          "sim::Controls is seventeen doubles, not " +
              std::to_string(sizeof(glideslope::sim::Controls) / sizeof(double)) +
              "; a control was added without being put in as_list()");

    // And a filled-in Controls survives the list and the wire.
    glideslope::sim::Controls c;
    c.elevator = 0.25;
    c.aileron = -0.5;
    c.rudder = 0.125;
    c.throttle = 0.75;
    c.mixture = 0.9;
    c.flaps = 0.3333;
    c.left_brake = 0.1;
    c.right_brake = 0.2;
    c.pitch_trim = -0.05;
    c.propeller = 0.8;
    c.gear = 0.0;
    c.supercharger = 0.0;
    c.speedbrake = 0.6;
    c.throttle_offset = {0.02, -0.02};
    c.cooling_flaps = {0.4, 0.45};
    const glideslope::sim::Controls back =
        glideslope::sim::Controls::from_list(glideslope::net::as_sent(c.as_list()));
    const std::array<double, 17> was = c.as_list();
    const std::array<double, 17> now = back.as_list();
    const double step = 1.0 / 32767.0;
    for (std::size_t i = 0; i < was.size(); ++i) {
        check(std::abs(now[i] - was[i]) <= step,
              "control " + std::to_string(i) + " survived: " + std::to_string(was[i]) +
                  " became " + std::to_string(now[i]));
    }
}

// **The document and the code say the same thing about the inputs.**
GLIDESLOPE_TEST(the_transport_document_and_the_code_agree_about_the_inputs) {
    const std::filesystem::path doc =
        std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "docs" / "TRANSPORT.md";
    std::ifstream in(doc);
    check(in.good(), "docs/TRANSPORT.md is there");
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto says = [&](const std::string& what) {
        return text.find(what) != std::string::npos;
    };

    check(says("carries the last\n**" + std::to_string(glideslope::net::redundancy) +
               "** frames") ||
              says("last **" + std::to_string(glideslope::net::redundancy) +
                   "** frames"),
          "the document says how many frames ride in a packet");
    check(says("1 to " + std::to_string(glideslope::net::redundancy)),
          "and the count a packet may carry");
    check(says("`i16` x" + std::to_string(glideslope::net::controls_on_the_wire)),
          "and how many controls there are");
    check(says("`-32767` is -1 and `32767` is 1"),
          "and what the ends of the range are");
    // The ends really are those numbers.
    check(glideslope::net::quantise(1.0) == 32767, "one is 32767");
    check(glideslope::net::quantise(-1.0) == -32767, "and minus one is -32767");
    std::printf("  the document holds the redundancy, the count and the range\n");
}
