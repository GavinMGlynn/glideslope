#pragma once

// A client's inputs, streamed to the server with redundancy.
//
// **Inputs are not sent reliably, they are sent repeatedly.** A lost input
// frame is worth nothing a moment later - the aircraft has moved on - so
// retransmitting one until it is acknowledged would deliver it too late to
// use and cost a round trip to find out. Instead every packet carries the
// last `redundancy` frames, so losing a datagram loses nothing as long as the
// next one arrives.
//
// **The bound is stated, because it is not "no loss ever".** A packet carries
// `redundancy` frames, so an input survives up to `redundancy - 1` lost
// packets in a row. Lose `redundancy` in a row and the oldest of them is gone
// for good. `docs/TRANSPORT.md` says so, and the tests walk both sides of it.
//
// **Every control is sent quantised, and both ends fly the quantised value.**
// A control is a number from -1 to 1; it goes on the wire as a 16-bit
// fraction of that range, which is about 3e-5 - far finer than any stick, and
// a quarter the size of a double. The client predicts on the quantised value
// rather than on what its stick actually said, so that client and server run
// the same numbers: a prediction fed different inputs would drift for a
// reason no measurement could explain, which is exactly the drift this
// project says it will bound.
//
// **Nothing here touches a socket.** It turns inputs into packets and packets
// back into inputs, which is what lets it be walked against every pattern of
// loss rather than against a network that happens to be working.

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <vector>

#include <array>

#include "net/protocol.hpp"

namespace glideslope::net {

// How many frames ride in each packet. At a 60 Hz input rate this is 67 ms of
// cover, which is more than a typical loss burst and small enough that a
// packet stays well inside a datagram.
inline constexpr std::size_t redundancy = 4;

// The controls, as they go on the wire: twenty-one 16-bit fractions of -1..1.
// **The network does not know what they are.** `sim::Controls::as_list` hands
// them over as a flat list and `from_list` takes them back, so the list of
// controls lives with the flight model and `src/net/` keeps including
// nothing but the standard library - which is what lets the whole transport
// be tested without a flight model.
inline constexpr std::size_t controls_on_the_wire = 21;
using ControlList = std::array<double, controls_on_the_wire>;
static_assert(controls_on_the_wire <= 255, "the count goes on the wire in one byte");

// **The layout a client sends, said as it joins** (since protocol 20): the
// initiation's payload begins with one byte, how many controls ride in each
// of its input frames - `controls_on_the_wire`, the number the frame's own
// layout is made from, so it is never written down twice - and the aeroplane
// asked for follows it (net::write_asked_aircraft), or nothing does. A server
// whose count is another refuses the initiation `WRONG_LAYOUT`, so a client
// of another day's controls is told why at once, rather than let in and its
// every input dropped as the wrong length (docs/TRANSPORT.md, "Starting a
// session"). `controls` is for a test, to be a client of another layout.
std::vector<std::uint8_t> write_joining(std::span<const std::uint8_t> asked,
                                        std::size_t controls = controls_on_the_wire);
struct Joining {
    std::size_t controls = 0;
    // The rest of the payload: the aeroplane asked for, or nothing.
    std::vector<std::uint8_t> asked;
};
// Nothing for an empty payload, which says no layout at all.
std::optional<Joining> read_joining(std::span<const std::uint8_t> payload);

// **A frame's step not said**: a client not yet counting its steps - before
// its prediction begins, or while the AI flies her - sends this.
inline constexpr std::uint32_t step_not_said = 0xFFFFFFFFu;

// One frame of a client's input: which frame it is, **the step of the
// client's own it was sent at**, and what the controls were.
//
// **An input names its step** (since protocol `13`) so that a server held up
// can fly the inputs that came in meanwhile as far apart as the client sent
// them, rather than as one: read together after a stall, only the newest was
// flown, and none of the stick's moves in those seconds was flown on the
// server - its client put right 4.9 m at worst after a 3 s stop
// (docs/status, 2026-10-28). The count is the client's and only its
// differences mean anything to the server, as Overwatch's command frames
// and Source's user commands carry the client's tick.
struct InputFrame {
    std::uint32_t sequence = 0;
    std::uint32_t step = step_not_said;
    ControlList controls{};
};

// A control put on the wire and taken off it again. Exposed because the
// client must fly what it sends, not what its stick said.
std::int16_t quantise(double control);
double unquantise(std::int16_t wire);

// The same for a whole list: what a client must fly locally, which is what
// it sent rather than what its stick said.
ControlList as_sent(const ControlList& controls);

// **The sending end.** Keeps the last `redundancy` frames and writes them
// into every packet.
class InputSender {
public:
    // Adds the frame for `sequence`, sent at the client's step `step`.
    // Sequences are the client's own count, from 1, and are expected to
    // arrive here in order.
    void add(std::uint32_t sequence, const ControlList& controls,
             std::uint32_t step = step_not_said);

    // The packet to send now: the frames it has, oldest first. Empty before
    // anything has been added.
    std::vector<std::uint8_t> packet() const;

    std::size_t held() const { return recent_.size(); }

private:
    std::deque<InputFrame> recent_;
};

// **The receiving end.** Hands up the frames it has not seen, in order, and
// says nothing about the ones it has.
class InputReceiver {
public:
    // Takes a packet off the wire. Returns the frames that are new, oldest
    // first - which may be none, or several at once after a loss.
    //
    // A packet that is not one of these is ignored and answers with nothing:
    // this is fed by the transport above it, and being fed rubbish is not a
    // reason to stop.
    std::vector<InputFrame> received(std::span<const std::uint8_t> packet);

    // The highest sequence handed up so far.
    std::uint32_t newest() const { return newest_; }

private:
    std::uint32_t newest_ = 0;
};

} // namespace glideslope::net
