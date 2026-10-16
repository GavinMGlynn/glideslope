### Slots, and who gets which, 2026-09-22 — item done

**What is missing first: nobody can be admitted yet.** There is no handshake,
so no key ever reaches the server and every session is empty. What is built
is the rule about slots, and it is built where it can be walked exhaustively
rather than watched over a network.

**A slot is a key's rank, not an arrival.** `REQUIREMENTS.md` 6.5 says the
server decides who is which player, "not whoever connected first", so slots
cannot be handed out in order of arrival. A player is known by a key - the
static public key the handshake will use - and their slot is that key's rank
among those in the session. The assignment is therefore a function of who is
present, not of the sequence, which is exactly what the item asks for.

**What that costs, said plainly.** A slot is a property of the set, so
somebody joining can move somebody already in: admit a key that sorts first
and everybody after it shifts down one. That is the price of an assignment
that does not depend on arrival order. The lobby is sent whole for this
reason - a client is told its slot rather than remembering it.

**Verified exhaustively**, `slots_are_the_same_whatever_order_the_players_connect_in`:
every non-empty subset of four players and every order each could arrive in -
**fifteen subsets, 64 orders** - all ending with the same person in the same
slot. The four test keys are **deliberately shuffled against their names**,
so an implementation that assigned by arrival, or alphabetically, would fail.
Five more tests hold the rank rule itself, fullness at each player count 1 to
4 with the next refused, a key admitted twice being one player, the 1 to 4
clamp, and the lobby message round-tripping through the wire format.

**The server draws its dashboard from it**, so the rows on screen are the
rows a `LOBBY` would carry rather than a count of empty seats.

**A name collision caught by the compiler, not by a test.** The class was
first called `net::Session`, which is already the `SESSION` message's struct.
It is `net::Slots` now, which is the requirement's own word.
