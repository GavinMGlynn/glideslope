#include "net/inputs.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace glideslope::net {
namespace {

// A control runs from -1 to 1, and goes on the wire as a 16-bit fraction of
// that range. The extremes are exact: -1 is the lowest value a signed 16-bit
// integer has and 1 is the highest, so a control held hard over arrives hard
// over rather than a hair short of it.
constexpr double wire_scale = 32767.0;

} // namespace

std::int16_t quantise(double control) {
    const double held = std::clamp(control, -1.0, 1.0);
    return static_cast<std::int16_t>(std::lround(held * wire_scale));
}

double unquantise(std::int16_t wire) {
    return static_cast<double>(wire) / wire_scale;
}

ControlList as_sent(const ControlList& controls) {
    ControlList out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = unquantise(quantise(controls[i]));
    }
    return out;
}

void InputSender::add(std::uint32_t sequence, const ControlList& controls) {
    recent_.push_back({sequence, controls});
    while (recent_.size() > redundancy) {
        recent_.pop_front();
    }
}

std::vector<std::uint8_t> InputSender::packet() const {
    Writer w;
    if (recent_.empty()) {
        return w.take();
    }
    // The newest sequence and how many frames follow. The frames are
    // consecutive, so the oldest is the newest less one fewer than the
    // count; nothing needs a sequence of its own.
    w.u32(recent_.back().sequence);
    w.u8(static_cast<std::uint8_t>(recent_.size()));
    for (const InputFrame& f : recent_) {
        for (const double control : f.controls) {
            w.u16(std::bit_cast<std::uint16_t>(quantise(control)));
        }
    }
    return w.take();
}

std::vector<InputFrame> InputReceiver::received(std::span<const std::uint8_t> packet) {
    Reader r(packet);
    const std::uint32_t newest = r.u32();
    const std::uint8_t count = r.u8();
    if (!r.ok() || count == 0 || count > redundancy || newest < count) {
        return {};
    }
    std::vector<InputFrame> frames;
    frames.reserve(count);
    const std::uint32_t oldest = newest - count + 1;
    for (std::uint8_t i = 0; i < count; ++i) {
        InputFrame f;
        f.sequence = oldest + i;
        for (double& control : f.controls) {
            control = unquantise(std::bit_cast<std::int16_t>(r.u16()));
        }
        frames.push_back(f);
    }
    // Nothing may be left over, so that a packet cannot hide anything.
    if (!r.done()) {
        return {};
    }
    std::vector<InputFrame> fresh;
    for (InputFrame& f : frames) {
        if (f.sequence > newest_) {
            fresh.push_back(std::move(f));
        }
    }
    if (!fresh.empty()) {
        newest_ = fresh.back().sequence;
    }
    return fresh;
}

} // namespace glideslope::net
