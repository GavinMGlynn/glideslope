### The store, and the timeout shown to work, 2026-09-22

**What is missing first: what the store keeps is one key and nothing else.**
There is no session in it - no slots, no aircraft, no history - because there
is no session state yet worth surviving a restart. It is a table of names and
values with one row in it.

**But that row is the one that matters.** A client is given the server's
public key out of band: written in a `server.txt`, pasted into a command line.
A server that minted a fresh key at every start would lock out every client it
had at each restart, which is exactly what `REQUIREMENTS.md` 6.6 means by
"minted once and stored if not given". `--store FILE` is an SQLite file, made
if it is not there, and `a_server_given_a_store_keeps_its_key_across_restarts`
walks **all four ways a server comes by a key** - given on the command line,
read from a store, minted into a store, minted with no store - and counts
them, so a way that stopped being walked fails the test. Across three restarts
against one store the key held; two starts with no store gave two different
keys, which is the thing the store exists to prevent, shown to be real; and
`--key` wins over the store without overwriting what the store holds, because
a key given on a command line is the operator's to manage.

**A secret is one member access away from a log line, unless it is not.**
`SecretKey` still has no `text()`; writing one down goes through
`net::secret_for_keeping()`, which is named to be greppable and has one
caller. `PublicKey::text()` is the only thing a stray `printf` can reach.

**The store was watched failing.** Opened on `:memory:` instead of its file,
three of its five tests went red - the three that claim persistence - and the
two that only claim a round trip inside one store stayed green, which is
right. Against the server, making the key lookup use a name nothing writes
turned `a_server_given_a_store...` red with "the second start should read the
key, not minted it".

**`--timeout` was taken and checked and drove nothing that any test watched.**
Its parsing had tests; its effect had none, because until this week no client
could connect to go quiet. Now one can:
`a_server_lets_go_a_client_it_has_not_heard_from_for_its_timeout` runs the
client and the server **at the same time**, the server admitting the client to
slot 0 and letting it go 0.5 s after it exited, once - not once round every
loop, which the test also holds.

**Two processes at once, out of CMake, which has no way to start one in the
background.** The commands of a single `execute_process` are a pipeline, so
they run together: the client goes first and its output into the server's
standard input, which the server never reads, and `OUTPUT_VARIABLE` catches
the last command's output, which is the server's. Nothing else in this project
needed two live processes before.

**A comment that described code that was not there.** `connect_to` said
"resending while nothing comes" above a loop that sent once and waited five
seconds. It resends every 250 ms now, which the timeout test needs - both
processes start at the same instant, so the first initiation goes out before
the server has bound - and which a client over UDP owes anyway.

**A line on the dashboard that had become a lie.** It said "nobody can connect
yet: the handshake is not built". It now says what is still true: ping and
traffic are not counted per player.

**The dashboard shows ping and traffic now.** The server knocks on each
connection once a second - a `PING` inside the seal carrying eight bytes, sent
straight back as a `PONG` - and draws the round trip and the bytes each way.
That knock is also the keepalive, which is why it is the server's job: the
server is the one deciding who has gone. `--plain` draws the same rows without
the escape codes that clear the screen, so a test can read them, and
`the_dashboard_shows_the_round_trip_and_the_traffic_of_a_client_that_stays`
holds a real round trip off a real socket.

**What is inside a sealed body is now decided**, because more than one thing
has to go in there. The plaintext begins with a byte saying which of five kinds
it is; `PING` and `PONG` are built, `RELIABLE`, `INPUTS` and `STATE` are
numbered and nothing sends them. The numbering is fixed before anything uses it
so that it cannot move later, `TRANSPORT.md` writes it down, and a test holds
the document and the code to each other.

**And the server item still does not tick.** `REQUIREMENTS.md` 6.6 asks for "a
live dashboard with connected clients, ping, traffic and a drop control". Ping
and traffic are there; **the dashboard is drawn in a terminal rather than a
window, and there is no way to drop a client from it.** Both are named in the
item.

**A flag that was documented and silently ignored.** `--plain` went into the
usage text and the settings block and not into the parser, and passed both of
its own tests, because neither of them looked at what the flag did. The lesson
generalises, so the test does:
`every_flag_the_server_prints_in_its_usage_is_one_it_takes` reads the usage
text itself, runs the server with each flag it finds, and fails if the parser
has never heard of one. Sixteen flags, counted against a written list, so a
flag that stopped being listed fails too.
