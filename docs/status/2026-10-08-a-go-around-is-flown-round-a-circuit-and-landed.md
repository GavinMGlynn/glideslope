### A go-around is flown round a circuit and landed, 2026-10-08 — tail still open

**What is missing first.** The AI still goes around by itself only from a
balloon: an approach arriving long (past the touchdown zone in the air) or
fast (more than the stabilised approach's 20 knots over the reference) is
landed anyway. **The circuit knows no traffic**: nothing sequences it with
another aircraft in the circuit or on the runway; only the separation
monitor's floor or ceiling on the autopilot's height (sim/separation.hpp)
acts, and no run measures two going around together. It is a fixed
left-hand circuit, with no wind correction to its legs beyond the
autopilot's heading hold and no right-hand pattern where a runway's side
asks for one. Its terrain is a height, not a path: it is flown over the
highest ground of the circuit's box, and the final approach from the raised
height is not checked against the ground under it. The circuit lessons'
AI still flies its own copy of these rules (a tail).

**Over the ground** (from #123's review): the circuit samples her terrain -
the collision DEM, on a server or a client - every 250 m over a box from
eight miles before the threshold to four past the runway and from 500 m
right of the centreline to two turns and 2 km left, and is raised by the
highest ground there above the runway; the downwind leg is then left where
the glidepath meets the raised height. `Aircraft::terrain()` gives the
terrain to it. `an_aeroplane_going_around_beside_rising_ground_flies_its_circuit_height_over_it_and_lands`:
every landplane but the F-35B over ground rising six metres in a hundred
from 400 m left of the centreline flies downwind at least 0.85 of its
circuit height over the ground (1,264 to 2,016 ft) and lands, within thirty
minutes. **The F-35B is named and left out**: on the raised downwind leg
she climbs from 2,900 to 6,000 ft and flies on past base, cause not found
(a tail). On flat ground the bound is the same; she settles to 1,309 ft of
her 1,500. **Seen to fail** with the circuit not raised:
13 wrong, the 737-300 downwind 538 ft over the ground, the A380 532.

**What changed.** At the go-around's 500 ft the controller no longer hands
her to the plain autopilot: `sim::GoAroundCircuit` (`sim/circuit.hpp`) flies
the climb straight ahead on the runway's heading and a left-hand circuit
through the plain autopilot - upwind, crosswind, downwind, base and a
thirty-degree intercept - and gives her to a fresh `Lander` on the same
runway on the intercept, which puts the landing flap and gear out again.
The configuration is cleaned up in order: half the landing flap from the
go-around on, and the gear up at a positive climb where it retracts. Its
sizes are the circuit lessons' (`fly_a_circuit` in `test_lesson.cpp`):
1,000 to 1,500 ft by the reference speed, the downwind leg left where the
glidepath meets that height, base at 1.4 times the landing stall; the climb
about ten feet a minute a knot of the reference speed (500 to 1,500).
`Lander::go_around()` and `Controller::go_around()` tell the AI to go
around now, from the approach or the flare; the go-around is flown as one
from a balloon is. The lessons' circuit code is not yet moved onto this
class; the two are the same rules written twice.

**Verification.**
`an_aeroplane_that_goes_around_is_flown_round_and_landed`: every landplane
taught the approach (13; the flying boat named, having no runway), flown
down the approach and told to go around at 200 ft, flies all six legs in
order, has half its landing flap and its gear up (where it retracts) on the
downwind leg, and stops on the runway unwrecked: the C172P 631 m past the
threshold, the F-35B 2,628, none more than 3.4 m off the centreline.
**Seen to fail** with the circuit not started (as before): 48 things wrong,
every one "did not fly every leg", the A380 not stopped 62 km on; and with
the flap not cleaned up: "737-300 flew downwind at flap 1.0". Every lesson,
landing, take-back, flare and gust test, the learnt landing's and the
selftest pass (95 tests, linux-release).
