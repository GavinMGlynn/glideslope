#include "net/rejoin.hpp"

#include "net/inside.hpp"

#include <sodium.h>

namespace glideslope::net {

std::optional<Refusal> refusal_from(const platform::Address& server,
                                    const platform::Address& from,
                                    std::span<const std::uint8_t> datagram) {
    if (!(from == server) || datagram.size() != envelope_size + 1) {
        return std::nullopt;
    }
    Reader r(datagram);
    Envelope envelope;
    Refusal why{};
    if (!read_envelope(r, envelope, why) || envelope.type != Type::refusal) {
        return std::nullopt;
    }
    return static_cast<Refusal>(datagram[envelope_size]);
}

bool lets_go(const platform::Address& server, const platform::Address& from,
             std::span<const std::uint8_t> datagram, double quiet_s) {
    return quiet_s >= quiet_before_believing_s &&
           refusal_from(server, from, datagram) == Refusal::bad_handshake;
}

std::vector<std::uint8_t> sealed_knock(Sealer& sealing, std::uint64_t token) {
    const std::vector<std::uint8_t> ping = knock(Inside::ping, token);
    Writer w = begin(Type::sealed);
    w.bytes(sealing.seal(std::span<const std::uint8_t>(ping.data(), ping.size())));
    return w.take();
}

Rejoin::Rejoin(const KeyPair& mine, const PublicKey& theirs, Sealer& old_sealing,
               Unsealer& old_opening, std::span<const std::uint8_t> payload)
    : initiator_(mine, theirs), old_sealing_(old_sealing), old_opening_(old_opening) {
    Writer w = begin(Type::handshake_initiation);
    w.bytes(initiator_.begin(payload));
    initiation_ = w.take();
    // **A token nothing sent before can have carried**: random, so that a
    // `PONG` held from an earlier attempt to join again is not this one's.
    randombytes_buf(&token_, sizeof token_);
}

std::vector<std::vector<std::uint8_t>> Rejoin::due(double now_s) {
    if (sent_s_ && now_s - *sent_s_ < every_s) {
        return {};
    }
    sent_s_ = now_s;
    const std::vector<std::uint8_t> ping = knock(Inside::ping, token_);
    Writer w = begin(Type::sealed);
    w.bytes(old_sealing_.seal(std::span<const std::uint8_t>(ping.data(), ping.size())));
    return {initiation_, w.take()};
}

Rejoin::Heard Rejoin::hear(const platform::Address& server, const platform::Address& from,
                           std::span<const std::uint8_t> datagram) {
    if (datagram.size() <= envelope_size) {
        return Heard::nothing;
    }
    const auto refused = refusal_from(server, from, datagram);
    if (refused == Refusal::dropped) {
        return Heard::dropped;
    }
    if (refused == Refusal::server_full) {
        return Heard::server_full;
    }
    Reader r(datagram);
    Envelope envelope;
    Refusal why{};
    if (!read_envelope(r, envelope, why)) {
        return Heard::nothing;
    }
    const std::span<const std::uint8_t> body = datagram.subspan(envelope_size);
    if (envelope.type == Type::sealed) {
        const auto opened = old_opening_.open(body);
        if (!opened) {
            return Heard::nothing;
        }
        const std::span<const std::uint8_t> inside(opened->data(), opened->size());
        if (knock_token(Inside::pong, inside) == token_) {
            return Heard::old_session_answers;
        }
        ++stale_;
        return Heard::nothing;
    }
    if (envelope.type == Type::handshake_response) {
        if (auto session = initiator_.finish(body)) {
            keys_ = std::move(session);
            return Heard::joined;
        }
    }
    return Heard::nothing;
}

} // namespace glideslope::net
