#pragma once

// The server's view of who is in a session, and which slot each has.
//
// **The server decides who is which player, not whoever connected first**
// (`REQUIREMENTS.md` 6.5). So a slot is not handed out on arrival: it is
// worked out from who is present. Every identity has a key, the keys have an
// order, and a player's slot is their rank among the keys in the session.
// Connect the same three people in any of the six orders and each gets the
// same slot, which is what the item asks for.
//
// **What that costs, said plainly.** A slot is a property of the set, so
// somebody joining can move somebody already in: admit a key that sorts
// first and the players after it each shift down one. That is the price of
// an assignment that does not depend on arrival order, and it is a lobby
// event either way - the lobby is sent whole, so a client is told its slot
// rather than remembering it.
//
// **Nothing here touches a socket or a flight model.** It is the rule about
// slots and nothing else, which is what lets it be walked over every order
// and every set.

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "net/messages.hpp"

namespace glideslope::net {

// A player is known by a key, out of band: the static public key the
// handshake will use. Thirty-two bytes, as X25519 has.
inline constexpr std::size_t identity_key_bytes = 32;
using IdentityKey = std::array<std::uint8_t, identity_key_bytes>;

struct Identity {
    IdentityKey key{};
    std::string name;
    // Whether this is a person flying or an AI pilot the server runs.
    Controller controller = Controller::person;
};

class Slots {
public:
    // `players_allowed` is 1 to 4; anything else is held to that range,
    // because a session outside it is not one this project has.
    explicit Slots(std::uint8_t players_allowed);

    // **Admits `who`.** Returns the slot they now have, or nothing if the
    // session is full - which is what a `SERVER_FULL` refusal is sent for.
    // Admitting a key already in the session returns its slot and changes
    // nothing else, so a client that asks twice is not two players.
    std::optional<std::uint8_t> admit(const Identity& who);

    // Lets a key go. False if it was not in.
    bool release(const IdentityKey& key);

    // Which slot a key has, or nothing if it is not in.
    std::optional<std::uint8_t> slot_of(const IdentityKey& key) const;

    std::uint8_t players_allowed() const { return players_allowed_; }
    std::size_t size() const { return who_.size(); }
    bool full() const { return who_.size() >= players_allowed_; }

    // **The lobby as it stands**, ready to be sent: every slot the server
    // has, in slot order, with the open ones named as open.
    Lobby lobby() const;

private:
    std::uint8_t players_allowed_ = 1;
    // Sorted by key, which is what makes a slot a rank rather than an
    // arrival.
    std::map<IdentityKey, Identity> who_;
};

} // namespace glideslope::net
