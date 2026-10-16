### Every player flies their own aircraft, whatever order they join in, 2026-09-24 — tail done

**Two players could be given the same aircraft number**, found by the client
written from `TRANSPORT.md`. A slot is a key's rank among the players present,
so that slots come out the same whatever order players arrive in - and so a
player whose key sorts first moves everybody after them. The server numbered
a player's aircraft by the slot it had on arrival and kept it, so the next to
arrive could be handed a number already flying: two clients then each took
one line of the state update for their own, and one aeroplane flew by both.
A player's aircraft now has the lowest number below `most_slots` no other
player's has, which is its own for as long as it flies; the dashboard finds
each connection's slot from its key when it draws, rather than remembering it.

**Verification.** `four_players_joining_in_the_reverse_of_their_keys_order_each_fly_their_own_aircraft`
joins four clients with fixed keys (`glideslope_cli connect ... --key HEX`, a
new test flag) a second apart, from the key that sorts last to the one that
sorts first, so every arrival moves everybody already in - built, not hoped
for. Each holds full aileron; the server, which now says how far each
aircraft banked, must report four players' aircraft under four numbers, each
over past 90 degrees. Seen to fail with the old numbering: two players'
aircraft both number 0.
