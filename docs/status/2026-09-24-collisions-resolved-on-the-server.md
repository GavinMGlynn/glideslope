### Collisions, resolved on the server, 2026-09-24 — item done

**An aircraft that crashes is a wreck for five seconds, then flies again from
where it started** - the project owner's choice (`REQUIREMENTS.md` 6.4): a
crash costs the flight, not the session. The server decides it
(`sim/crash.hpp`), and every client is told.

- **The ground**: touching down sinking faster than 10 ft/s - 14 CFR 25.473's
  limit descent velocity, and the most the old 23.473 formula gave a light
  aeroplane - or any part of the airframe that is not a wheel touching, or a
  landplane on water. A flying boat's hull on water is none of these. **A
  wheel is a leg of the undercarriage** - it retracts, steers or brakes:
  JSBSim's BOGEY type alone is not enough, since the A320 model gives its
  nose, tail, engines and wingtips rolling contacts too, and a belly landing
  on them read as a landing on wheels.
- **Each other**: centres closer than the mean of the two wingspans, each the
  flight model's own (`metrics/bw-ft`). The least reach between any two of the
  sixteen is 9.1 m; two aircraft closing at 1,000 knots move 4.3 m a step, so
  nothing passes through anything between steps.
- **The wire**: every aircraft's line in a state update carries a condition,
  `00` flying or `01` wrecked (`TRANSPORT.md`); a reader refuses any other. A
  full packet is now 1,035 bytes, 1,065 sealed. The client written from the
  document read it as soon as the document said so.
- **The server**: a wreck stays where it hit, not stepped, for five simulated
  seconds, then is set back to its start - an AI's plan begun again, a
  player's inputs flying it as before. Aircraft placed with `--fly` now hold
  their start's heading, height and speed on the autopilot, and take a
  heading (`--fly ID@LAT,LON,HEADING`): with only enough power held to stay
  up, a Cessna rolled off into a slow turn, and two set head-on circled apart.
- **The client** (`glideslope_cli connect`) says on standard error, with its
  name, each aircraft it hears become a wreck and fly again.

**Verification.** Unit, every aircraft walked: all fifteen landplanes dropped
25 ft onto the ground (about 2,300 ft/min) are wrecked and set down from half
a foot (5.7 ft/s) are not; all ten whose wheels retract, put down with them
up, are wrecked by the airframe; a Cessna on water is wrecked and the flying
boat afloat is not; and every one of the 256 pairs of wingspans collides
inside its reach and not beyond it. Each seen to fail on a deliberate bug: a
gear limit of 50 ft/s, the airframe ignored, the ditch ignored, the reach
halved. On the server, `two_aircraft_on_a_collision_course_collide_on_the_server_and_every_client_is_told`
puts two 737s head-on over the sea off Sydney with two clients connected; the
server must say both are wrecks, having hit each other, and that both fly
again, and both clients must have heard all four - seen to fail with the
condition never sent. Two Cessnas set the same way passed 20 m apart, which
is a miss, and why the test flies 737s. A state packet carries a wreck and
refuses the 254 conditions it does not know.

**Not here**: the graphical client does not draw the server's aircraft at
all yet, wrecks or not - "Four machines in one sky". The protocol's version
byte stays `01` although the state packet grew, as it did for the handshake:
nothing deployed depends on the old one.
