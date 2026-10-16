### The server flies aircraft anywhere on Earth, 2026-09-22 — item done

**What is missing first: nobody is flying them.** The server owns the
aircraft and steps them, but no client can connect and no input reaches a
control surface, so they fly at a fixed throttle and nothing else. What is
proved is that the server simulates at a fixed rate against the real
collision terrain wherever an aircraft is, not that anybody can fly one.

**Two aircraft, Sydney and Denver, in one session.** `--fly ID@LAT,LON` gives
the server an aeroplane and where it starts, up to the four a session holds;
`--seconds N` stops it after a set time, which is what lets a test watch a
whole run rather than kill one. `--port 0` asks the system for a free port and
the server prints the one it got, so two tests never fight over a number.

**One DEM, one thread.** A `Fleet` owns a single `world::Dem` and an aircraft
per `--fly`, each with terrain over that DEM. The DEM caches tiles as it goes,
so aircraft on opposite sides of the world each pull their own and neither
waits for the other's; every aircraft is stepped on the one thread, because
`PROJECT_STATUS.md` has said since the DEM was written that it is not
thread-safe. Stepping is `sim::FixedStep`: every step that has become due is
taken, so the rate is the simulation's and not the loop's.

**Verified**,
`the_server_flies_aircraft_on_opposite_sides_of_the_world_each_over_its_own_terrain`:
the two flew **2.003 s in 240 steps - 120 Hz exactly** - and found ground at
**77 ft** and **5,183 ft** above the ellipsoid. Those are the right numbers:
Sydney Airport is a few feet above the sea with the geoid some 70 ft above the
ellipsoid there, and Denver is 5,280 ft above the sea with the geoid about
56 ft below it. Each is held to a band that place really has, so a server
flying both over one terrain fails both bands at once.

**The test was watched failing.** Given a flat terrain at the ellipsoid, it
reports "the aeroplane over Denver found ground at 0 ft, which is outside the
4500 to 6500 ft that place has: it is not over its own terrain". Reverting the
bug makes it pass.

**A contradiction of my own making.** The server refused `--port 0` - "name a
port" - while the test that flies it asked for 0, so the verification skipped
itself rather than running. A skipped verification is not one, so `--port 0`
is now allowed and documented as what a test wants.
