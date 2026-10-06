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
                    out.mine_key_ = mine;
                    out.prove_at_once();
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
    if (!sealing_ || packet.empty() || standing_ != Standing::joined || stalling_) {
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
    if (!socket_) {
        return;
    }
    if (!last_opened_s_) {
        last_opened_s_ = now_s;
    }
    if (standing_ == Standing::joining_again) {
        keep_joining_again(now_s);
        return;
    }
    if (standing_ != Standing::joined || !opening_ || !sealing_) {
        return;
    }
    read_what_arrived(now_s);
    if (standing_ != Standing::joined) {
        return;
    }
    if (stalling_) {
        // Stalled: nothing is sent until the server's knocks have stopped,
        // and then only a knock a second, whose refusal is the let-go.
        if (!stall_heard_s_) {
            stall_heard_s_ = now_s;
        }
        if (now_s - *stall_heard_s_ >= quiet_before_believing_s &&
            now_s - knocked_s_ >= 1.0) {
            knock_on(++knock_token_);
            knocked_s_ = now_s;
        }
        return;
    }
    // **Knocked until anything opens** (see `prove_every_s`): the `PONG`
    // sent at `connect()` may have been lost, and nothing else would be
    // sent until the caller has heard something.
    if (!opened_any_ && (!proved_at_s_ || now_s - *proved_at_s_ >= prove_every_s)) {
        proved_at_s_ = now_s;
        const std::vector<std::uint8_t> ping = knock(Inside::ping, ++proving_token_);
        Writer w = begin(Type::sealed);
        w.bytes(sealing_->seal(all_of(ping)));
        const std::vector<std::uint8_t> out = w.take();
        (void)socket_->send(server_, all_of(out));
    }
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
    // **A session gone quiet is knocked on from this end**, once a second:
    // a server that has it answers `PONG`, one that has let it go refuses
    // it - so a client that sends nothing else still hears which.
    if (now_s - *last_opened_s_ >= knock_after_quiet_s && now_s - knocked_s_ >= 1.0) {
        knock_on(++knock_token_);
        knocked_s_ = now_s;
    }
}

// **Something sealed at once**: a server sends a session nothing but its
// answer until something sealed under it has opened (TRANSPORT.md). A pong
// nobody pinged for costs the server nothing. A session joined again proves
// itself the same way, and is knocked on every `prove_every_s` until
// anything opens, as a first one is.
void ClientSession::prove_at_once() {
    opened_any_ = false;
    proved_at_s_.reset();
    const std::vector<std::uint8_t> pong = knock(Inside::pong, 0);
    Writer pw = begin(Type::sealed);
    pw.bytes(sealing_->seal(all_of(pong)));
    const std::vector<std::uint8_t> proof = pw.take();
    (void)socket_->send(server_, all_of(proof));
}

void ClientSession::knock_on(std::uint64_t token) {
    const std::vector<std::uint8_t> ping = knock(Inside::ping, token);
    Writer w = begin(Type::sealed);
    w.bytes(sealing_->seal(all_of(ping)));
    const std::vector<std::uint8_t> out = w.take();
    (void)socket_->send(server_, all_of(out));
}

// **Let go**: joining again (net::Rejoin), the old session's keys kept to
// knock on, in case it was not gone after all.
void ClientSession::let_go_at(double now_s) {
    ++let_go_;
    quiet_when_let_go_s_ = now_s - *last_opened_s_;
    stalling_ = false;
    standing_ = Standing::joining_again;
    old_sealing_ = std::move(sealing_);
    old_opening_ = std::move(opening_);
    rejoin_ = std::make_unique<Rejoin>(mine_key_, theirs_, *old_sealing_, *old_opening_);
    again_began_s_ = now_s;
}

// **Joining again**: the initiation resent until it is answered, a minute the
// most, and the old session knocked on meanwhile - back to it only on its
// answer to that knock (net::Rejoin says why). `SERVER_FULL` or `DROPPED`
// from the server's address ends it.
void ClientSession::keep_joining_again(double now_s) {
    if (now_s - again_began_s_ > give_up_joining_again_s) {
        standing_ = Standing::gave_up;
        return;
    }
    for (const std::vector<std::uint8_t>& out : rejoin_->due(now_s)) {
        (void)socket_->send(server_, all_of(out));
    }
    std::vector<std::uint8_t> into(platform::largest_datagram);
    for (;;) {
        platform::Address from;
        const std::size_t got = socket_->receive(into, from);
        if (got <= envelope_size) {
            return;
        }
        const Rejoin::Heard heard =
            rejoin_->hear(server_, from, std::span<const std::uint8_t>(into.data(), got));
        stale_ = rejoin_->stale();
        if (heard == Rejoin::Heard::dropped) {
            standing_ = Standing::dropped;
            return;
        }
        if (heard == Rejoin::Heard::server_full) {
            standing_ = Standing::refused;
            return;
        }
        if (heard == Rejoin::Heard::old_session_answers) {
            rejoin_.reset();
            sealing_ = std::move(old_sealing_);
            opening_ = std::move(old_opening_);
            standing_ = Standing::joined;
            last_opened_s_ = now_s;
            ++went_back_;
            return;
        }
        if (heard == Rejoin::Heard::joined) {
            // **A new session is a new start**: its own reliable stream,
            // and every aircraft introduced again - which may include this
            // client's own under its old number.
            sealing_ = std::make_unique<Sealer>(rejoin_->keys().sending);
            opening_ = std::make_unique<Unsealer>(rejoin_->keys().receiving);
            initiation_ = rejoin_->initiation();
            rejoin_.reset();
            old_sealing_.reset();
            old_opening_.reset();
            reliable_ = Reliable{};
            // Nothing of the old session is handed up as if of this one:
            // an update it left waiting would be taken for the new
            // aircraft.
            fresh_.clear();
            roster_.clear();
            told_ = Told{};
            mine_ = no_aircraft;
            applied_ = 0;
            aircraft_.clear();
            standing_ = Standing::joined;
            last_opened_s_ = now_s;
            knocked_s_ = now_s;
            ++joined_again_;
            prove_at_once();
            return;
        }
    }
}

std::vector<StatePacket> ClientSession::take_states() {
    std::vector<StatePacket> out;
    out.swap(fresh_);
    return out;
}

void ClientSession::read_what_arrived(double now_s) {
    std::vector<std::uint8_t> into(platform::largest_datagram);
    // Everything waiting, not one: a frame may be a long time after the last.
    for (;;) {
        platform::Address from;
        const std::size_t got = socket_->receive(into, from);
        if (got <= envelope_size) {
            return;
        }
        const std::span<const std::uint8_t> datagram(into.data(), got);
        // **A refusal is believed only of a session gone quiet**, only from
        // the server's address, and only for `BAD_HANDSHAKE` - "no session
        // here". It is sent in the clear, so anybody can forge one; heard
        // while the session works, it is nothing.
        if (now_s - *last_opened_s_ >= quiet_before_believing_s &&
            refusal_from(server_, from, datagram) == Refusal::bad_handshake) {
            let_go_at(now_s);
            return;
        }
        if (stalling_) {
            // Read and thrown away, as a stopped process never read it.
            stall_heard_s_ = now_s;
            continue;
        }
        Reader r(datagram);
        Envelope envelope;
        Refusal why{};
        if (!read_envelope(r, envelope, why) || envelope.type != Type::sealed) {
            continue;
        }
        const auto opened = opening_->open(datagram.subspan(envelope_size));
        if (!opened) {
            continue;
        }
        opened_any_ = true;
        last_opened_s_ = now_s;
        const std::span<const std::uint8_t> inside(opened->data(), opened->size());
        // **The server's goodbye**, sealed under this session and so the
        // server's own: the operator dropped this client. It does not come
        // back, and says nothing more.
        if (is_leaving(inside)) {
            standing_ = Standing::dropped;
            sealing_.reset();
            opening_.reset();
            return;
        }
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
                (void)told_.hear(all_of(message));
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
