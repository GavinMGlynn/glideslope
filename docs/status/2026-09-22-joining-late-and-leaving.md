### Joining late, and leaving, 2026-09-22

**What is missing first: a player cannot hand their own aircraft to an AI
pilot and take it back.** That is the controller swap, it is the thing the
whole project is named around, and it needs a `CONTROLLER_SWAP` message to
travel - which needs the reliable layer inside a sealed body, which is not
built. What works is only what the *server* does when somebody goes.

**AI traffic with nobody connected.** A client that waits five seconds before
connecting finds a session at **4.200 s** with both AI Cessnas away from where
the plan starts - `-33.90301` and `-33.90298` against a start of `-33.905` -
and its own aircraft, made the moment it joined, still on the start point.
That last one is the control: if everything in the packet had moved, the test
would be measuring the plan's start point rather than the passage of time.

**A player leaving.** There is no goodbye message and a crashed client could
not send one, so leaving is going quiet and the `--timeout` sweep is what
notices. `--on-leave remove` takes the aircraft out of the sky; `--on-leave
ai` hands it to an AI pilot flying the server's plan, renumbers it - a
player's aircraft is numbered by their slot, and that slot is about to be
given to somebody else - and says so. Both settings walked and counted.

**Under `ai` the test makes the aircraft prove it is being flown.** Handing it
to a pilot that then flew nothing would leave it hanging where its owner left
it, and a check that counted aeroplanes would pass. So the handed-over
aircraft must have moved from the start: it ends at `-33.902101, 151.289744`,
which it could only reach by being flown there.

**`--after N` on the client is a test flag and says so.** A client that
connects the instant the server does can learn nothing about whether the
server was flying before it arrived.
