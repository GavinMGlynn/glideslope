### The state stream, over a socket, 2026-09-22

**What is missing first: a client still cannot fly.** The server sends state
25 times a second and reads nothing back from a client but a `PONG`. Inputs
are defined and encoded and reach no aircraft. A client is a spectator with a
slot.

**But it can see.** Run as a test and by hand: a server flying two AI Cessnas
over Sydney Harbour, a client connecting over the loopback, and **73 state
updates in three seconds** - 25 Hz is seventy-five - each carrying both
aircraft, which the client turned back into latitude, longitude and height:
-33.905, 151.290 at 914 m and 1,067 m.

**Why the test checks the places and not the count.** A packet of the right
size full of the wrong place is what a wrong frame, a wrong sign, or a
latitude and longitude the wrong way round all produce, and a count would pass
every one of them. So the client turns the ECEF metres back into a position
and the test holds them to the harbour the flight plan starts over.
**And to the gap between them**: the server stacks AI aircraft 500 ft apart,
which is 152 m, and a height dropped on the way out would leave them both in
the right place horizontally. The measured gap is 153 m.

**25 Hz, and why that number.** `REQUIREMENTS.md` 6.6 asks for 20 to 30. 25 is
the middle of it and divides 120 exactly, so a state update always lands on a
simulation step rather than between two.

**`ned_to_ecef` had to be written first.** A flight model gives a velocity in
north, east and down; a position on this wire is Earth-centred. The rotation
between them is four lines and is now in `world/geodesy`, held by
`a_local_velocity_becomes_the_same_velocity_in_the_earths_frame`: **361 points
over the whole Earth** - every ten degrees of latitude and twenty of longitude
- each checked for three unit vectors at right angles to each other and for a
speed that survives the rotation, plus the three places at 0, 0 where the
answer can be written down by hand. **And that down is towards the ellipsoid
and not towards the centre**, which is the only reason the function takes a
geodetic latitude at all: at every latitude but 0 and the poles the two must
differ, and the test fails if they do not.
