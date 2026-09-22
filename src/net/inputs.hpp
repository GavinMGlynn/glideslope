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
#include <span>
#include <vector>

#include <array>

#include "net/protocol.hpp"

namespace glideslope::net {

// How many frames ride in each packet. At a 60 Hz input rate this is 67 ms of
// cover, which is more than a typical loss burst and small enough that a
// packet stays well inside a datagram.
inline constexpr std::size_t redundancy = 4;

// The controls, as they go on the wire: seventeen 16-bit fractions of -1..1.
// **The network does not know what they are.** `sim::Controls::as_list` hands
// them over as a flat list and `from_list` takes them back, so the list of
// controls lives with the flight model and `src/net/` keeps including
// nothing but the standard library - which is what lets the whole transport
// be tested without a flight model.
inline constexpr std::size_t controls_on_the_wire = 17;
using ControlList = std::array<double, controls_on_the_wire>;

// One frame of a client's input: which frame it is, and what the controls
// were.
struct InputFrame {
    std::uint32_t sequence = 0;
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
    // Adds the frame for `sequence`. Sequences are the client's own count,
    // from 1, and are expected to arrive here in order.
    void add(std::uint32_t sequence, const ControlList& controls);

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
