#include "net/session.hpp"

#include "net/inputs.hpp"
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
                                                    std::uint32_t layout,
                                                    double give_up_after_s,
                                                    double resend_every_s,
                                                    const std::string& aircraft,
                                                    const std::string& loading,
                                                    const std::string& secret_hex,
                                                    std::optional<Refusal>* refused) {
    const auto address = platform::address_of(where);
    if (!address) {
        return std::nullopt;
    }
    const auto theirs = public_from_text(key_hex);
    if (!theirs) {
        return std::nullopt;
    }
    KeyPair mine = mint_key_pair();
    if (!secret_hex.empty()) {
        const auto secret = secret_from_text(secret_hex);
        if (!secret) {
            return std::nullopt;
        }
        mine.secret = *secret;
        mine.publik = public_from_secret(*secret);
    }
    const std::vector<std::uint8_t> asked =
        aircraft.empty() ? std::vector<std::uint8_t>{} : write_asked_aircraft(aircraft, loading);
    if ((!aircraft.empty() && asked.empty()) || (aircraft.empty() && !loading.empty())) {
        return std::nullopt; // not an aircraft's id, or a loading's name, or a loading of nothing
    }
    Asking asking;
    // **The layout first** (net::write_joining): a server of another refuses
    // it `WRONG_LAYOUT`, by name, rather than drop every input it sends.
    asking.payload = write_joining(layout, all_of(asked));
    asking.give_up_after_s = give_up_after_s;
    asking.resend_every_s = resend_every_s;
    return join(*address, *theirs, mine, asking, nullptr, refused);
}

std::optional<ClientSession> ClientSession::join(const platform::Address& server,
                                                 const PublicKey& theirs, const KeyPair& mine,
                                                 const Asking& asking, NotJoined* why,
                                                 std::optional<Refusal>* refused) {
    const auto fail = [why](NotJoined how) -> std::optional<ClientSession> {
        if (why != nullptr) {
            *why = how;
        }
        return std::nullopt;
    };
    auto socket = platform::UdpSocket::bound(0);
    if (!socket) {
        return fail(NotJoined::no_socket);
    }
    Initiator initiator(mine, theirs);
    Writer w = begin(Type::handshake_initiation);
    w.bytes(initiator.begin(all_of(asking.payload)));
    const std::vector<std::uint8_t> first = w.take();
    if (asking.before_sending && !asking.before_sending(all_of(first))) {
        return fail(NotJoined::held_back);
    }
    if (!socket->send(server, all_of(first))) {
        return fail(NotJoined::cannot_send);
    }

    // **Resent while nothing comes**: a handshake over UDP has to expect its
    // first datagram to be lost, and a server that is not listening yet
    // answers nothing at all. The same initiation each time - `Noise_IK`
    // makes one, and a second would be a second handshake.
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
            Refusal reason{};
            if (read_envelope(r, envelope, reason)) {
                if (envelope.type == Type::refusal) {
                    // The server said no, and said why.
                    if (refused != nullptr) {
                        *refused = refusal_of(into[envelope_size]);
                    }
                    return fail(NotJoined::refused);
                }
                if (envelope.type == Type::handshake_response) {
                    const auto session =
                        initiator.finish(std::span<const std::uint8_t>(into.data(), got)
                                             .subspan(envelope_size));
                    if (!session) {
                        return fail(NotJoined::did_not_open);
                    }
                    ClientSession out;
                    out.socket_ = std::make_unique<platform::UdpSocket>(
                        std::move(*socket));
                    out.server_ = server;
                    out.theirs_ = session->theirs;
                    out.sealing_ = std::make_unique<Sealer>(session->sending);
                    out.opening_ = std::make_unique<Unsealer>(session->receiving);
                    out.initiation_ = first;
                    out.answer_.assign(into.begin(),
                                       into.begin() + static_cast<std::ptrdiff_t>(got));
                    out.mine_key_ = mine;
                    out.asked_ = asking.payload;
                    out.prove_at_once();
                    return out;
                }
            }
        }
        if (waited > asking.give_up_after_s) {
            return fail(NotJoined::no_answer);
        }
        if (waited - sent_at_s >= asking.resend_every_s) {
            (void)socket->send(server, all_of(first));
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
    tend(now_s);
}

bool ClientSession::read_one(double now_s) {
    if (!socket_ || standing_ != Standing::joined || !opening_ || !sealing_) {
        return false;
    }
    if (!last_opened_s_) {
        last_opened_s_ = now_s;
    }
    return read_datagram(now_s);
}

void ClientSession::tend(double now_s) {
    if (!socket_ || standing_ != Standing::joined || !opening_ || !sealing_) {
        return;
    }
    if (!last_opened_s_) {
        last_opened_s_ = now_s;
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
    proof_size_ = proof.size();
    (void)socket_->send(server_, all_of(proof));
}

void ClientSession::knock_on(std::uint64_t token) {
    const std::vector<std::uint8_t> out = sealed_knock(*sealing_, token);
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
    rejoin_ = std::make_unique<Rejoin>(mine_key_, theirs_, *old_sealing_, *old_opening_,
                                       all_of(asked_));
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
        if (heard == Rejoin::Heard::server_full || heard == Rejoin::Heard::wrong_layout) {
            standing_ = Standing::refused;
            refused_by_ = heard == Rejoin::Heard::server_full ? Refusal::server_full
                                                               : Refusal::wrong_layout;
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
            arrived_.clear();
            roster_.clear();
            refusals_.clear();
            learnt_refusals_.clear();
            told_ = Told{};
            air_said_.clear();
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
    // Everything waiting, not one: a frame may be a long time after the last.
    while (standing_ == Standing::joined && read_datagram(now_s)) {
    }
}

bool ClientSession::read_datagram(double now_s) {
    std::vector<std::uint8_t>& into = into_;
    platform::Address from;
    const std::size_t got = socket_->receive(into, from);
    if (got == 0) {
        return false;
    }
    if (got <= envelope_size) {
        return true;
    }
    const std::span<const std::uint8_t> datagram(into.data(), got);
    // **A refusal is believed only of a session gone quiet**, only from
    // the server's address, and only for `BAD_HANDSHAKE` - "no session
    // here". It is sent in the clear, so anybody can forge one; heard
    // while the session works, it is nothing. Nor by a session about to
    // leave (`leaving_soon`).
    if (!leaving_soon_ && lets_go(server_, from, datagram, now_s - *last_opened_s_)) {
        let_go_at(now_s);
        return true;
    }
    if (stalling_) {
        // Read and thrown away, as a stopped process never read it.
        stall_heard_s_ = now_s;
        return true;
    }
    Reader r(datagram);
    Envelope envelope;
    Refusal why{};
    if (!read_envelope(r, envelope, why) || envelope.type != Type::sealed) {
        return true;
    }
    const auto opened = opening_->open(datagram.subspan(envelope_size));
    if (!opened) {
        return true;
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
        return true;
    }
    if (const auto state = read_state(inside)) {
        ++heard_;
        clock_s_ = state->simulation_time_s;
        applied_ = state->last_input_applied;
        mine_ = state->your_aircraft;
        aircraft_ = state->aircraft;
        if (in_order_) {
            Arrived a;
            a.what = Arrived::What::state;
            a.state = *state;
            arrived_.push_back(std::move(a));
            return true;
        }
        // Every one kept for the caller, in the order they came: an
        // interpolation wants each snapshot, not only the newest. Past
        // the most kept, the oldest go: a client that has not asked for
        // seconds wants where things are now, and keeping the first few
        // seconds instead put one right by the whole way flown since.
        if (fresh_.size() >= most_states_kept) {
            fresh_.erase(fresh_.begin());
        }
        fresh_.push_back(*state);
        return true;
    }
    if (!inside.empty() && inside[0] == static_cast<std::uint8_t>(Inside::reliable)) {
        for (std::vector<std::uint8_t>& message : reliable_.received(inside.subspan(1))) {
            AircraftDefinition d;
            if (read(all_of(message), d)) {
                roster_[d.aircraft] = d;
            }
            TakeOverRefused refused;
            if (read(all_of(message), refused)) {
                refusals_.push_back(refused.aircraft);
            }
            LearntLandingRefused learnt_refused;
            if (read(all_of(message), learnt_refused)) {
                learnt_refusals_.push_back(std::move(learnt_refused));
            }
            TrafficInstruction instruction;
            if (read(all_of(message), instruction)) {
                instructions_.push_back(std::move(instruction));
            }
            ControllerSwap swap;
            if (read(all_of(message), swap)) {
                swaps_[swap.aircraft] = swap.to;
            }
            if (in_order_) {
                // Told by the caller itself, which says each as it hears it.
                Arrived a;
                a.what = Arrived::What::message;
                a.message = std::move(message);
                arrived_.push_back(std::move(a));
            } else if (auto air = told_.air_said(told_.hear(all_of(message)))) {
                air_said_.push_back(std::move(*air));
            }
        }
        return true;
    }
    if (const auto token = knock_token(Inside::ping, inside)) {
        const std::vector<std::uint8_t> pong = knock(Inside::pong, *token);
        Writer pw = begin(Type::sealed);
        pw.bytes(sealing_->seal(all_of(pong)));
        const std::vector<std::uint8_t> out = pw.take();
        (void)socket_->send(server_, all_of(out));
        ++answered_;
        return true;
    }
    if (const auto token = knock_token(Inside::pong, inside); token && in_order_) {
        Arrived a;
        a.what = Arrived::What::pong;
        a.token = *token;
        arrived_.push_back(std::move(a));
    }
    return true;
}

} // namespace glideslope::net
