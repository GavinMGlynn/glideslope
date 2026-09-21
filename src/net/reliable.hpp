#pragma once

// Reliable delivery over an unreliable channel.
//
// **UDP loses, duplicates and reorders.** Inputs and state updates are sent
// expecting that: the latest wins, and losing one costs a frame. But the
// lobby, the session, the weather, an aircraft's definition, the terrain
// dataset and a controller swap must each arrive, exactly once, in the order
// they were sent. This is the small layer that makes that true.
//
// **How it works**, in four sentences. Every reliable message is numbered,
// from one, and carries its number. The sender keeps a message until it has
// been acknowledged and sends it again if it has not been, no faster than
// once every `retry_after_s`. The receiver hands messages up in number
// order, holding a message that arrives early until its predecessors have
// come, and throwing away one that arrives twice. The receiver answers with
// the highest number below which nothing is missing, which acknowledges that
// one and every one before it at once.
//
// **It is not a window.** There is no congestion control and no flight
// limit: these messages are few, small and occasional, and the channel below
// is a game's, not a file transfer's. `docs/TRANSPORT.md` says so where it
// says what the transport does not claim.
//
// **Nothing here touches a socket.** It turns messages into datagrams and
// datagrams back into messages, which is what lets it be tested against
// every pattern of loss rather than against a network that happens to be
// working.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <span>
#include <vector>

namespace glideslope::net {

// How long to wait for an acknowledgement before sending a message again.
inline constexpr double retry_after_s = 0.25;

// The most messages held unacknowledged before `send` refuses. A sender that
// has got this far behind is not going to catch up by queueing more.
inline constexpr std::size_t most_in_flight = 256;

// The most messages held out of order before the rest are dropped. An
// endpoint that cannot deliver because one message is missing has no reason
// to hold an unbounded number behind it.
inline constexpr std::size_t most_held_back = 256;

// One reliable message on the wire: its number, then its body.
//
//   u32  number, from 1
//   u32  the highest number below which the sender is missing nothing
//   ...  the body
//
// The acknowledgement rides on every message, so an endpoint that is sending
// anything at all is acknowledging at the same time. An endpoint with
// nothing to send sends a message with an empty body and number 0, which is
// an acknowledgement and nothing else.
inline constexpr std::size_t reliable_header_size = 8;

class Reliable {
public:
    // Queues a message. False if too many are already waiting, in which case
    // the caller has a problem this layer cannot solve for it.
    bool send(std::span<const std::uint8_t> body);

    // The datagrams to put on the wire now: each message not yet
    // acknowledged whose turn has come round again, and an acknowledgement
    // of its own if there is anything to acknowledge and nothing to say.
    std::vector<std::vector<std::uint8_t>> to_send(double now_s);

    // Takes a datagram off the wire. Returns the message bodies now ready to
    // be handed up, in the order they were sent - which may be none, or
    // several at once when a missing message finally arrives.
    //
    // A datagram that is not one of these is ignored and answers with
    // nothing: this layer is fed by the transport above it, and being fed
    // rubbish is not a reason to stop.
    std::vector<std::vector<std::uint8_t>> received(std::span<const std::uint8_t> datagram);

    // How many messages are waiting to be acknowledged.
    std::size_t in_flight() const { return waiting_.size(); }
    // How many have arrived early and are being held for their predecessors.
    std::size_t held_back() const { return early_.size(); }
    // The number of the last message handed up.
    std::uint32_t delivered() const { return delivered_; }
    // How many datagrams this has sent, retransmissions and all.
    std::uint64_t sent_datagrams() const { return sent_; }

private:
    struct Waiting {
        std::uint32_t number = 0;
        std::vector<std::uint8_t> body;
        double last_sent_s = -1.0; // never
    };

    std::deque<Waiting> waiting_;
    std::uint32_t next_number_ = 1;

    // What has been received: the highest number below which nothing is
    // missing, and the ones that arrived before their turn.
    std::uint32_t delivered_ = 0;
    std::map<std::uint32_t, std::vector<std::uint8_t>> early_;

    // The last acknowledgement seen from the far end.
    std::uint32_t acknowledged_ = 0;
    // Whether anything has arrived that has not been acknowledged back yet.
    bool owe_acknowledgement_ = false;

    std::uint64_t sent_ = 0;
};

} // namespace glideslope::net
