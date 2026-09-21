#include "net/reliable.hpp"

#include "net/protocol.hpp"

namespace glideslope::net {

bool Reliable::send(std::span<const std::uint8_t> body) {
    if (waiting_.size() >= most_in_flight) {
        return false;
    }
    Waiting one;
    one.number = next_number_++;
    one.body.assign(body.begin(), body.end());
    waiting_.push_back(std::move(one));
    return true;
}

std::vector<std::vector<std::uint8_t>> Reliable::to_send(double now_s) {
    std::vector<std::vector<std::uint8_t>> out;

    // Everything not yet acknowledged whose turn has come round again. The
    // queue is in number order, so this sends in number order too.
    for (Waiting& one : waiting_) {
        if (one.last_sent_s >= 0.0 && now_s - one.last_sent_s < retry_after_s) {
            continue;
        }
        Writer w;
        w.u32(one.number);
        w.u32(delivered_);
        w.bytes(std::span<const std::uint8_t>(one.body.data(), one.body.size()));
        out.push_back(w.take());
        one.last_sent_s = now_s;
        ++sent_;
    }

    // **An endpoint with nothing to say still has to answer.** Otherwise the
    // far end retransmits for ever against a receiver that has everything.
    if (out.empty() && owe_acknowledgement_) {
        Writer w;
        w.u32(0); // not a message: an acknowledgement and nothing else
        w.u32(delivered_);
        out.push_back(w.take());
        ++sent_;
    }
    if (!out.empty()) {
        owe_acknowledgement_ = false;
    }
    return out;
}

std::vector<std::vector<std::uint8_t>> Reliable::received(
    std::span<const std::uint8_t> datagram) {
    std::vector<std::vector<std::uint8_t>> out;
    Reader r(datagram);
    const std::uint32_t number = r.u32();
    const std::uint32_t acknowledges = r.u32();
    if (!r.ok()) {
        return out; // not one of these; being fed rubbish is not a reason to stop
    }

    // What the far end says it has. Everything at or below it can be let go.
    if (acknowledges > acknowledged_) {
        acknowledged_ = acknowledges;
    }
    while (!waiting_.empty() && waiting_.front().number <= acknowledged_) {
        waiting_.pop_front();
    }

    if (number == 0) {
        return out; // an acknowledgement and nothing else
    }
    // Something arrived, so something is owed back even if there is nothing
    // to say.
    owe_acknowledgement_ = true;

    if (number <= delivered_) {
        return out; // it came twice; the first one was handed up already
    }
    std::vector<std::uint8_t> body = r.bytes(r.left());
    if (!r.ok()) {
        return out;
    }
    if (number != delivered_ + 1) {
        // Early. Hold it for its predecessors, unless too many are already
        // held - an endpoint stuck behind one missing message has no reason
        // to hold an unbounded number behind it.
        if (early_.size() < most_held_back) {
            early_.emplace(number, std::move(body));
        }
        return out;
    }

    // Its turn: hand it up, and everything held behind it that is now in
    // order too.
    out.push_back(std::move(body));
    delivered_ = number;
    for (auto at = early_.find(delivered_ + 1); at != early_.end();
         at = early_.find(delivered_ + 1)) {
        out.push_back(std::move(at->second));
        delivered_ = at->first;
        early_.erase(at);
    }
    return out;
}

} // namespace glideslope::net
