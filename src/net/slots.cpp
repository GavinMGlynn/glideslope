#include "net/slots.hpp"

#include <algorithm>

namespace glideslope::net {

Slots::Slots(std::uint8_t players_allowed)
    : players_allowed_(std::clamp<std::uint8_t>(players_allowed, 1,
                                                static_cast<std::uint8_t>(most_slots))) {}

std::optional<std::uint8_t> Slots::admit(const Identity& who) {
    if (const auto already = slot_of(who.key)) {
        return already;
    }
    if (full()) {
        return std::nullopt;
    }
    who_.insert({who.key, who});
    return slot_of(who.key);
}

bool Slots::release(const IdentityKey& key) {
    return who_.erase(key) > 0;
}

std::optional<std::uint8_t> Slots::slot_of(const IdentityKey& key) const {
    std::uint8_t slot = 0;
    for (const auto& [theirs, _who] : who_) {
        if (theirs == key) {
            return slot;
        }
        ++slot;
    }
    return std::nullopt;
}

Lobby Slots::lobby() const {
    Lobby out;
    out.players_allowed = players_allowed_;
    out.slots.reserve(players_allowed_);
    auto it = who_.begin();
    for (std::uint8_t slot = 0; slot < players_allowed_; ++slot) {
        Lobby::Slot s;
        s.index = slot;
        if (it != who_.end()) {
            s.controller = it->second.controller;
            s.name = it->second.name;
            ++it;
        } else {
            s.controller = Controller::nobody;
        }
        out.slots.push_back(std::move(s));
    }
    return out;
}

} // namespace glideslope::net
