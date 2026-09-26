#include "net/session.hpp"

#include "net/inside.hpp"
#include "net/protocol.hpp"

#include <chrono>
#include <thread>

namespace glideslope::net {
namespace {

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

} // namespace

std::optional<ClientSession> ClientSession::connect(const std::string& where,
                                                    const std::string& key_hex,
                                                    double give_up_after_s,
                                                    double resend_every_s) {
    const auto address = platform::address_of(where);
    if (!address) {
        return std::nullopt;
    }
    const auto theirs = public_from_text(key_hex);
    if (!theirs) {
        return std::nullopt;
    }
    auto socket = platform::UdpSocket::bound(0);
    if (!socket) {
        return std::nullopt;
    }

    const KeyPair mine = mint_key_pair();
    Initiator initiator(mine, *theirs);
    Writer w = begin(Type::handshake_initiation);
    w.bytes(initiator.begin());
    const std::vector<std::uint8_t> first = w.take();
    if (!socket->send(*address, all_of(first))) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> into(platform::largest_datagram);
    const auto began = std::chrono::steady_clock::now();
    double sent_at_s = 0.0;
    for (;;) {
        platform::Address from;
        const std::size_t got = socket->receive(into, from);
        const double waited =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - began)
                .count();
        if (got > envelope_size) {
            Reader r(std::span<const std::uint8_t>(into.data(), got));
            Envelope envelope;
            Refusal why{};
            if (read_envelope(r, envelope, why)) {
                if (envelope.type == Type::refusal) {
                    return std::nullopt; // the server said no, and said why
                }
                if (envelope.type == Type::handshake_response) {
                    const auto session =
                        initiator.finish(std::span<const std::uint8_t>(into.data(), got)
                                             .subspan(envelope_size));
                    if (!session) {
                        return std::nullopt;
                    }
                    ClientSession out;
                    out.socket_ = std::make_unique<platform::UdpSocket>(
                        std::move(*socket));
                    out.server_ = *address;
                    out.theirs_ = session->theirs;
                    out.sealing_ = std::make_unique<Sealer>(session->sending);
                    out.opening_ = std::make_unique<Unsealer>(session->receiving);
                    out.initiation_ = first;
                    return out;
                }
            }
        }
        if (waited > give_up_after_s) {
            return std::nullopt;
        }
        if (waited - sent_at_s >= resend_every_s) {
            (void)socket->send(*address, all_of(first));
            sent_at_s = waited;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ClientSession::send_inputs(std::span<const std::uint8_t> packet) {
    if (!sealing_ || packet.empty()) {
        return;
    }
    std::vector<std::uint8_t> body{static_cast<std::uint8_t>(Inside::inputs)};
    body.insert(body.end(), packet.begin(), packet.end());
    Writer w = begin(Type::sealed);
    w.bytes(sealing_->seal(all_of(body)));
    const std::vector<std::uint8_t> out = w.take();
    (void)socket_->send(server_, all_of(out));
}

void ClientSession::leave() {
    if (!socket_ || !sealing_) {
        return;
    }
    const std::vector<std::uint8_t> goodbye{static_cast<std::uint8_t>(Inside::leaving)};
    for (int copy = 0; copy < leaving_copies; ++copy) {
        Writer w = begin(Type::sealed);
        w.bytes(sealing_->seal(all_of(goodbye)));
        const std::vector<std::uint8_t> out = w.take();
        (void)socket_->send(server_, all_of(out));
    }
    // Ended here: nothing more is sealed, sent or read.
    sealing_.reset();
    opening_.reset();
}

std::vector<std::uint8_t> ClientSession::sealed(std::span<const std::uint8_t> plaintext) {
    if (!sealing_) {
        return {};
    }
    Writer w = begin(Type::sealed);
    w.bytes(sealing_->seal(plaintext));
    return w.take();
}

void ClientSession::send_from_here(std::span<const std::uint8_t> datagram) {
    if (socket_) {
        (void)socket_->send(server_, datagram);
    }
}

void ClientSession::send_the_initiation_again() {
    if (socket_ && !initiation_.empty()) {
        (void)socket_->send(server_, all_of(initiation_));
    }
}

void ClientSession::poll(double now_s) {
    if (!socket_ || !opening_ || !sealing_) {
        return;
    }
    read_what_arrived();
    // **What must arrive is acknowledged**, and anything of its own that
    // must, repeated until it has.
    for (const std::vector<std::uint8_t>& datagram : reliable_.to_send(now_s)) {
        std::vector<std::uint8_t> body{static_cast<std::uint8_t>(Inside::reliable)};
        body.insert(body.end(), datagram.begin(), datagram.end());
        Writer w = begin(Type::sealed);
        w.bytes(sealing_->seal(all_of(body)));
        const std::vector<std::uint8_t> out = w.take();
        (void)socket_->send(server_, all_of(out));
    }
}

std::vector<StatePacket> ClientSession::take_states() {
    std::vector<StatePacket> out;
    out.swap(fresh_);
    return out;
}

void ClientSession::read_what_arrived() {
    std::vector<std::uint8_t> into(platform::largest_datagram);
    // Everything waiting, not one: a frame may be a long time after the last.
    for (;;) {
        platform::Address from;
        const std::size_t got = socket_->receive(into, from);
        if (got <= envelope_size) {
            return;
        }
        Reader r(std::span<const std::uint8_t>(into.data(), got));
        Envelope envelope;
        Refusal why{};
        if (!read_envelope(r, envelope, why) || envelope.type != Type::sealed) {
            continue;
        }
        const auto opened = opening_->open(
            std::span<const std::uint8_t>(into.data(), got).subspan(envelope_size));
        if (!opened) {
            continue;
        }
        const std::span<const std::uint8_t> inside(opened->data(), opened->size());
        if (const auto state = read_state(inside)) {
            ++heard_;
            clock_s_ = state->simulation_time_s;
            applied_ = state->last_input_applied;
            mine_ = state->your_aircraft;
            aircraft_ = state->aircraft;
            // Every one kept for the caller, in the order they came: an
            // interpolation wants each snapshot, not only the newest. Past
            // the most kept, the oldest go: a client that has not asked for
            // seconds wants where things are now, and keeping the first few
            // seconds instead put one right by the whole way flown since.
            if (fresh_.size() >= most_states_kept) {
                fresh_.erase(fresh_.begin());
            }
            fresh_.push_back(*state);
            continue;
        }
        if (!inside.empty() && inside[0] == static_cast<std::uint8_t>(Inside::reliable)) {
            for (const std::vector<std::uint8_t>& message : reliable_.received(inside.subspan(1))) {
                AircraftDefinition d;
                if (read(all_of(message), d)) {
                    roster_[d.aircraft] = d;
                }
            }
            continue;
        }
        if (const auto token = knock_token(Inside::ping, inside)) {
            const std::vector<std::uint8_t> pong = knock(Inside::pong, *token);
            Writer pw = begin(Type::sealed);
            pw.bytes(sealing_->seal(all_of(pong)));
            const std::vector<std::uint8_t> out = pw.take();
            (void)socket_->send(server_, all_of(out));
            ++answered_;
        }
    }
}

} // namespace glideslope::net
