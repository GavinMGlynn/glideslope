### A client refused by a forger while its session is quiet goes back to it, 2026-10-01 — tail done

**What is still not covered first.** A client goes back to its old session on
*anything* that opens under the old keys, and a datagram the server sealed
before it let the session go opens as well as a new one. So a client whose
session really was let go, and whose held-back updates arrive while it is
joining again, goes back to a dead session - and it costs more than it looks
(server `take()`, the initiation arm): the fresh initiation join_again() had
already sent is **admitted** - a slot, and an aircraft from the fleet - and
the client, gone back, ignores the answer. Its datagrams under the old keys
open under nothing the server has at that address, and are dropped in
silence, not refused, so the client sits quiet until that orphan session is
let go for silence (`--timeout`, 10 s by default), and only then is refused,
believes it three seconds later, and joins again: about 13 s lost, and **a
ghost session holding a slot and an aircraft for the timeout** - on a full
server, a real `SERVER_FULL` for somebody else. It is a tail in the plan.
This was seen in the window test's first form (below), not changed. Nothing
tests the window client refused `DROPPED` with its goodbyes lost (its own
tail). The window client builds its flight without reading its socket: a
debug build under load was silent to the server for more than five seconds
just after it was given its aircraft (below) - a tail. The client's half of
a session is written twice, `stay()`/`join_again()` in `glideslope_cli` and
`net::ClientSession` - which is how the one bug fixed here was in one and
not the other - a tail.

**What was wrong.** The going back - `join_again()` in `glideslope_cli`,
`net::ClientSession::keep_joining_again()` in the client with the window - had
been reviewed and never run. Running it found the command-line client's half
broken after the going back: it began a fresh `stay()` in the old session,
numbering its inputs from 1 again with a new `InputSender` and a new
`Reliable`. The server's count of that session's inputs had not started again,
so it dropped every input as older than the newest it had applied - "sent 1
input frames, the server applied 55" - until the count passed where it had
been; and the reliable stream started again under a session whose other end
had not. The client with the window was right: its `net::ClientSession` keeps
both, and `client::Online` numbers its inputs on.

**What it does now.**
- **`glideslope_cli` carries a session's streams across stays**
  (`SessionStreams`: the input count, the `InputSender`, the `Reliable`): going
  back keeps them, joining a new session starts them again. It says so: "the
  old session answered; staying in it, its inputs numbered on from N".
  `--leave-once-back` (with `--fly`): back in a session after a refusal it
  believed - its old one or a new one - it leaves once the server has applied
  an input it sent there.
- **The relay forges** (`glideslope_impair --forge-refusal-after N`): once N
  datagrams have come from the server it holds - not drops - everything the
  server sends, so the session goes quiet while the server still hears the
  client, and answers every datagram a client sends with a `BAD_HANDSHAKE` from
  its own port, the server's address as the client sees it. The first
  handshake initiation from a client ends the hold - the event, not a time -
  and what was held is delivered in order, and nothing more forged. It says on
  standard error how many it forged and whether the hold ended so. It is for
  one client: the hold is of everything, and any client's initiation ends it.
  It holds at most 4,096 datagrams and says how many it dropped past that;
  both tests require none. It links `glideslope_net` now, for the envelope.
- **The client with the window** says, at a `--shot-once-back` shot, whether it
  has gone back and been flown by an input sent since, with the input sent,
  the input applied and the input sent when it went back
  (`client::Online::gone_back()`, `flown_since_going_back()`). It refuses
  `--shot-once-back` without a `--shot` on a server
  (`the_client_refuses_to_hold_a_shot_for_going_back_with_no_server`, seen to
  fail with the refusal taken out).
- **Docs**: `docs/TRANSPORT.md` says both clients join again (it still said
  only the command-line one, in "Refusals"), and that back in the old session
  nothing starts again: inputs numbered on, the reliable streams where they
  were. `docs/THREATS.md` says the same and that both clients are tested.

**Verification.** `a_client_refused_by_a_forger_while_its_session_is_quiet_goes_back_to_it`
(`tests/cmake/server_forged_refusal.cmake`, port 24708, relay 24709): server
(`--until-empty`, `--timeout 30`) | relay `--forge-refusal-after 50` |
`glideslope_cli connect --fly --leave-once-back`. It holds that the relay
forged and its hold ended on the client's initiation; the client believed a
refusal after at least 3 s of nothing, went back, joined nothing new, numbered
its inputs on (from 141 here), and the server applied one sent since and none
past what it numbered; the server admitted it once, let it go once for its
goodbye and never for silence, and flew one player's aircraft, past 90
degrees. 8.7 s here (Linux debug). **Seen to fail three ways**: with the going
back disabled, "the client did not go back to its old session" (the server let
it go after its timeout and stopped, and the client could not join again);
with the streams started again on going back, "the client numbered its inputs
from 1 again" - and, that check switched off, "the server applied input 56 of
the 1 the client numbered". All reverted.

`the_client_with_the_window_refused_by_a_forger_while_its_session_is_quiet_goes_back_to_it`
(`tests/cmake/client_forged_refusal.cmake`, port 24710, relay 24711, Vulkan
platforms only): server | relay `--forge-refusal-after 50` | the client with
the window, `--shot-once-back`. It holds the same of the relay, that the client
believed a refusal after at least 3 s, went back and did not join again, that
the shot was drawn gone back and flown by an input sent since, with the pilot
flying it, and the server's one admission, no let-go for silence, one goodbye.
45 to 95 s here, five runs in a row beside the command-line test. **Seen to
fail**: with `ClientSession`'s going back disabled, "the client did not go back
to its old session" ("let go, and could not join again in a minute", the server
letting it go for 30 s of silence); reverted. **Its first form failed by
itself**, once in five, with `--timeout 5`: the window client was silent to the
server for five seconds while building its flight, the server let the session
go before the hold ended, and the client went back to the dead session on an
update held from before ("input 2024 sent, 86 applied, 86 when it went back") -
the ghost case above, with the server stopping as everybody had gone.
The session in this test must be merely quiet, so the server's timeout is now
30 s; the client's own three seconds are what is tested.

**On Windows** (MSVC, `windows-debug`, through `tools/windows_build.sh`):
both tests pass, the command-line one in 11 to 14 s and the window one in
109 to 221 s. In one run of four tests at the preset's four jobs,
`a_client_stalled_past_the_timeout_joins_again_by_itself` - not one of these -
timed out at 900 s with no output; alone it passed in 15 s. That run's build
had also made the stall's second session wait for an applied input before
leaving; that was taken out again, so the stall test is as it was on `main`.
After the rebase onto `main` (4c88ba3) all four pass on Windows again, with
both relay tests. **The client with the window can crash on Windows as it
exits after refusing its arguments**: `the_client_refuses_to_hold_a_shot_for_going_back_with_no_server`
failed there once with exit 0xc0000409 and no output. Run by hand, the same
arguments refused and exited 2; but `glideslope.exe --view nonesuch` - the
long-standing view refusal - printed its refusal and then "first-chance
exception 0xc0000005 at ntdll.dll+0x1a12d ... in thread N, started at
KERNEL32.DLL+0x3ee90, after the program began to exit" and exited 9, and so
did `--shot-once-back` alone. Not this branch's; a tail.

**Once failed unexplained**: in the first `-j4` run of the 137 network
tests here, the command-line test failed after 302 s - the server's whole
`--seconds` - and that run kept no output (no `--output-on-failure`). It did
not come back in the next `-j4` run of 139 or in six runs of the forger,
stall, drop and relay tests together at `-j4`; on failure the test prints
both programs' words, so a repeat will say what happened.
