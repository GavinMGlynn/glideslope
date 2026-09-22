#include "harness.hpp"

#include "net/slots.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

using glideslope::net::Controller;
using glideslope::net::Identity;
using glideslope::net::IdentityKey;
using glideslope::net::Slots;
using glideslope::test::check;

namespace {

// Four people, whose keys deliberately do not sort the way their names do:
// if the server handed out slots by name or by arrival these tests would
// still pass, so the keys are what decides and they are shuffled against the
// names on purpose.
//
//   name     first byte of key   so the slot is
//   Delta    0x10                0
//   Bravo    0x40                1
//   Alpha    0x80                2
//   Charlie  0xC0                3
Identity person(std::uint8_t first, const std::string& name) {
    Identity who;
    who.key.fill(0);
    who.key[0] = first;
    who.name = name;
    who.controller = Controller::person;
    return who;
}

const std::vector<Identity>& four() {
    static const std::vector<Identity> people{
        person(0x80, "Alpha"), person(0x40, "Bravo"), person(0xC0, "Charlie"),
        person(0x10, "Delta")};
    return people;
}

// Who has which slot, after admitting `people` in this order.
std::map<std::string, int> slots_after(const std::vector<Identity>& people,
                                       std::uint8_t players_allowed) {
    Slots session(players_allowed);
    std::map<std::string, int> out;
    for (const Identity& who : people) {
        const auto slot = session.admit(who);
        out[who.name] = slot ? static_cast<int>(*slot) : -1;
    }
    // Read them back at the end, because a slot is a property of the set and
    // an earlier answer may have been overtaken by a later arrival.
    for (const Identity& who : people) {
        const auto slot = session.slot_of(who.key);
        out[who.name] = slot ? static_cast<int>(*slot) : -1;
    }
    return out;
}

} // namespace

// **Slots are assigned by the server, the same whatever order players
// connect in.** This is the item's own verification, and it is walked
// exhaustively: every subset of the four, and every order each subset could
// arrive in - 64 of them - must end with the same person in the same slot.
GLIDESLOPE_TEST(slots_are_the_same_whatever_order_the_players_connect_in) {
    const std::vector<Identity>& people = four();
    std::size_t walked = 0;
    std::size_t subsets = 0;
    for (unsigned mask = 1; mask < (1u << 4); ++mask) {
        std::vector<Identity> subset;
        for (unsigned i = 0; i < 4; ++i) {
            if ((mask & (1u << i)) != 0) {
                subset.push_back(people[i]);
            }
        }
        // The order they are written down in, as the answer to compare to.
        std::sort(subset.begin(), subset.end(),
                  [](const Identity& a, const Identity& b) { return a.name < b.name; });
        const std::map<std::string, int> expected = slots_after(subset, 4);
        std::size_t orders = 0;
        do {
            const std::map<std::string, int> got = slots_after(subset, 4);
            if (got != expected) {
                std::string said;
                for (const auto& [name, slot] : got) {
                    said += " " + name + "=" + std::to_string(slot);
                }
                glideslope::test::fail(
                    "connecting " + std::to_string(subset.size()) +
                    " players in another order gave different slots:" + said);
            }
            ++orders;
            ++walked;
        } while (std::next_permutation(
            subset.begin(), subset.end(),
            [](const Identity& a, const Identity& b) { return a.name < b.name; }));
        // k players have k! orders, and every one of them was tried.
        std::size_t factorial = 1;
        for (std::size_t i = 2; i <= subset.size(); ++i) {
            factorial *= i;
        }
        check(orders == factorial, "every order of " + std::to_string(subset.size()) +
                                       " players was tried: " + std::to_string(orders) +
                                       " of " + std::to_string(factorial));
        ++subsets;
    }
    // **The space this walked, stated**: fifteen non-empty subsets of four,
    // and 4 + 12 + 24 + 24 = 64 orders across them.
    check(subsets == 15, "every non-empty subset of the four was walked, not " +
                             std::to_string(subsets));
    check(walked == 64, "sixty-four orders in all, not " + std::to_string(walked));
    std::printf("  %zu subsets, %zu connection orders, all agreeing\n", subsets, walked);
}

// **The slot is the key's rank, not the arrival.** The four keys are
// shuffled against their names on purpose, so an implementation that handed
// out slots in arrival order, or by name, would fail here.
GLIDESLOPE_TEST(a_players_slot_is_their_keys_rank_among_those_in_the_session) {
    Slots session(4);
    // Admitted worst-first by key: Charlie, Alpha, Bravo, Delta.
    const std::vector<Identity>& people = four();
    for (const char* const name : {"Charlie", "Alpha", "Bravo", "Delta"}) {
        const auto it = std::find_if(people.begin(), people.end(),
                                     [&](const Identity& i) { return i.name == name; });
        check(it != people.end(), "the person is one of the four");
        check(session.admit(*it).has_value(), std::string(name) + " is admitted");
    }
    const auto slot = [&](const std::string& name) {
        const auto it = std::find_if(people.begin(), people.end(),
                                     [&](const Identity& i) { return i.name == name; });
        const auto got = session.slot_of(it->key);
        return got ? static_cast<int>(*got) : -1;
    };
    check(slot("Delta") == 0, "Delta's key sorts first, so Delta is slot 0");
    check(slot("Bravo") == 1, "Bravo is slot 1");
    check(slot("Alpha") == 2, "Alpha is slot 2");
    check(slot("Charlie") == 3, "Charlie is slot 3");
}

// **A session is full at its player count, and a fifth is refused.** Every
// count 1 to 4 is tried, and in each the one past it is refused.
GLIDESLOPE_TEST(a_session_is_full_at_its_player_count_and_the_next_is_refused) {
    const std::vector<Identity>& people = four();
    std::size_t walked = 0;
    for (std::uint8_t allowed = 1; allowed <= 4; ++allowed) {
        Slots session(allowed);
        for (std::uint8_t i = 0; i < allowed; ++i) {
            check(session.admit(people[i]).has_value(),
                  "player " + std::to_string(i) + " is admitted into a session of " +
                      std::to_string(allowed));
        }
        check(session.full(), "a session of " + std::to_string(allowed) + " is full");
        check(session.size() == allowed, "and holds that many");
        // A fifth person, whose key is not one of the four.
        const Identity extra = person(0xFF, "Echo");
        check(!session.admit(extra).has_value(),
              "one more than " + std::to_string(allowed) + " is refused");
        check(session.size() == allowed, "and the session is unchanged");
        ++walked;
    }
    check(walked == 4, "every player count from one to four was tried");
}

// **Asking twice is not two players**, and letting go frees the slot.
GLIDESLOPE_TEST(admitting_the_same_key_twice_is_one_player_and_releasing_frees_it) {
    const std::vector<Identity>& people = four();
    Slots session(4);
    const auto first = session.admit(people[0]);
    const auto again = session.admit(people[0]);
    check(first.has_value() && again.has_value(), "both answers are a slot");
    check(*first == *again, "and it is the same slot");
    check(session.size() == 1, "one key admitted twice is one player");

    check(session.release(people[0].key), "the key is let go");
    check(session.size() == 0, "and the session is empty");
    check(!session.slot_of(people[0].key).has_value(), "it has no slot now");
    check(!session.release(people[0].key), "letting go twice says it was not in");
}

// **A player count outside 1 to 4 is held to that range**, because a session
// outside it is not one this project has.
GLIDESLOPE_TEST(a_session_is_never_fewer_than_one_player_or_more_than_four) {
    std::size_t walked = 0;
    for (int asked = 0; asked <= 10; ++asked) {
        const Slots session(static_cast<std::uint8_t>(asked));
        const int got = static_cast<int>(session.players_allowed());
        check(got >= 1 && got <= 4,
              "asking for " + std::to_string(asked) + " gives " + std::to_string(got) +
                  ", which is inside 1 to 4");
        if (asked >= 1 && asked <= 4) {
            check(got == asked, "and inside the range it is what was asked");
        }
        ++walked;
    }
    check(walked == 11, "every count from nought to ten was tried");
}

// **The lobby has a row for every slot the server has**, in slot order, with
// the ones nobody is in named as open. It is what goes on the wire.
GLIDESLOPE_TEST(the_lobby_has_a_row_for_every_slot_with_the_open_ones_named) {
    const std::vector<Identity>& people = four();
    Slots session(4);
    check(session.admit(people[0]).has_value(), "Alpha is admitted"); // key 0x80
    check(session.admit(people[3]).has_value(), "Delta is admitted"); // key 0x10

    const glideslope::net::Lobby lobby = session.lobby();
    check(lobby.players_allowed == 4, "the lobby says how many may fly");
    check(lobby.slots.size() == 4, "and has a row per slot, not " +
                                       std::to_string(lobby.slots.size()));
    for (std::size_t i = 0; i < lobby.slots.size(); ++i) {
        check(lobby.slots[i].index == i, "the rows are in slot order");
    }
    check(lobby.slots[0].name == "Delta", "Delta's key sorts first, so slot 0 is Delta");
    check(lobby.slots[0].controller == Controller::person, "and a person is flying it");
    check(lobby.slots[1].name == "Alpha", "slot 1 is Alpha");
    check(lobby.slots[2].controller == Controller::nobody, "slot 2 is open");
    check(lobby.slots[2].name.empty(), "and has nobody's name in it");
    check(lobby.slots[3].controller == Controller::nobody, "slot 3 is open too");

    // And it goes on the wire and comes back the same.
    const std::vector<std::uint8_t> bytes = glideslope::net::write(lobby);
    glideslope::net::Lobby back;
    check(glideslope::net::read(bytes, back), "the lobby reads back");
    check(back.players_allowed == lobby.players_allowed, "with the same player count");
    check(back.slots.size() == lobby.slots.size(), "and the same rows");
    for (std::size_t i = 0; i < back.slots.size(); ++i) {
        check(back.slots[i].index == lobby.slots[i].index, "each row's slot");
        check(back.slots[i].controller == lobby.slots[i].controller, "each row's controller");
        check(back.slots[i].name == lobby.slots[i].name, "each row's name");
    }
}
