### A client flies, 2026-09-22

**What is missing first: a client is never told anything.** None of the seven
reliable messages travels, so a connected client never learns the lobby, the
session, the weather, what aeroplane anybody is in or what the terrain dataset
is. It also does not choose its aeroplane: a player flies whatever the
server's flight plan flies and starts where it starts.

**But it flies.** A client sends inputs and nothing else; the server owns the
aircraft and flies it by them; the client learns what happened only from the
state updates coming back. Measured over the loopback: **177 input frames sent
in six seconds, 176 applied**, and the aeroplane rolled to 180 degrees of bank
- inverted - on full left aileron.

**A player joining is given an aircraft numbered by their slot.** That is why
a state update can say `your_aircraft` and a client can pick its own line out
of twenty: slots 0 to 3 are the players, and everything else is numbered from
4 upwards and never moves. A player leaving takes their aircraft with them;
handing it to an AI pilot instead is a session setting and its own item.

**A slot is not an aeroplane.** A server with nothing to fly has loaded no
terrain and has nowhere to put one, so a client connecting to one gets a slot,
no aircraft, and state updates that say `no_aircraft`. That is what every
server test that passes `--ai 0` is doing, and it is why they need no network.

**The bug worth writing down.** The first run flew: the server applied the
inputs, the client saw its aeroplane roll - and only to 13 degrees in four
seconds. Full aileron on a Cessna does far better than three degrees a second,
so it went back for a look. `Fleet::step()` set the controls itself on every
step for an aircraft with no AI pilot - throttle 0.6 and everything else at
rest - so a client's inputs were applied when the datagram arrived and wiped
120 times a second afterwards. **A control is a position, not an event**: a
stick held over stays over. The aircraft holds its controls now, and the same
run gives 180 degrees where it gave 34.

**It would have passed a careless test.** "Rolls more than ten degrees" is
what a test written to the first run's output would have said, and it would
have been green on a server that threw away 119 of every 120 inputs. The bar
is 90 degrees - past it the aeroplane is inverted - which neither a server
that drops the inputs nor one that forgets them can reach.
