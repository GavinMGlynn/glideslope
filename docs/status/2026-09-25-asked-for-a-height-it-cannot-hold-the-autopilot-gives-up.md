### Asked for a height it cannot hold, the autopilot gives up height, 2026-09-25 — tail done

**What is still missing.** The AI still flies with the mixture full rich, so
the ceiling it gives up height at is about 8,500 ft rather than the handbook's.
That is the next tail, "The AI never leans the mixture".

**Only light aeroplanes have the floor, and only clean.** A light
aeroplane's climb speed is its handbook's best rate of climb, and the floor
stands on that. Every other class's climb speed is a different thing:
- the jets' were measured on the model at a chosen speed;
- the Mosquito's is one climb test's speed;
- the S.23's is its sea-level climb at one boost.

None of them is a speed below which no height can be held, so the other
twelve aircraft have no floor, as before. That includes the 747-400 and the
F-22, which publish no climb speed at all.
`every_aircraft_the_data_holds_has_a_speed_floor_or_is_named_without_one`
names each of the twelve with its reason and counts all sixteen. The floor
is read once, when the model loads (`Aircraft::climb_floor_kts`). A light
aeroplane that publishes no climb speed fails to load rather than fly
without the floor unnoticed.

**Before**, asked for a height it could not reach, the altitude hold went on
pitching up with the throttle at its stop and paid for the height with speed.
Asked for 3,000 ft above its ceiling, a Cessna 172P slowed to 44 knots, a
Cessna 182 and a Cherokee to 50, and a Cub to 5, which then spun down to the
ground.

**Now** (`sim/autopilot.cpp`) the altitude hold has a floor on the airspeed.
- **The least speed** is the aeroplane's best-climb speed, or 5 knots below a
  slower speed asked for. The best-climb speed is `departure_speeds`', read
  from the catalogue and figures beside the JSBSim root when the model
  loads.
- **When it acts:** only clean (flaps up, and gear up where it retracts), and
  when the throttle has no more to give - at its stop, or no speed held. It
  acts when the airspeed, five seconds ahead on its trend, would come within a
  knot of the least.
- **What it does:** the climb the altitude hold may ask for is limited by an
  integral on the airspeed (30 ft/min a second per knot above the least, 500
  ft/min per knot it moves). The limit is kept between the climb asked for and
  the descent the pitch envelope's least pitch (-10 degrees) gives at the
  speed, so it cannot wind up. Review asked for the vertical speed the
  altitude hold captures heights at as the lower bound. That was tried and
  failed: on a fifth of its throttle the Cherokee needs more sink than that to
  keep its speed, and it slowed to 63 knots. While the limit
  binds, the throttle opens to its stop.
- **Flaps or gear out:** there is no floor, and a hold already running is let
  go. The airspeed's trend is kept current even so, so nothing jumps when the
  aeroplane is clean again.
  The aeroplane climbs what it can at that speed, or comes down. The limit
  lets go when it no longer binds.
- **In a turn** the least is 3 knots lower: the bank limit's own allowance
  (2026-09-24). It stays lower until the speed is back, so the bank limit
  keeps a turn within its allowance and the floor catches only what gets past
  it.

It is the underspeed protection of total energy control (Lambregts, AIAA
83-2239), as a floor only. The code's comment records each version that was
tried and why it failed.

**What the new tests verify.**
`every_light_aeroplane_the_data_holds_is_asked_for_a_height_it_cannot_hold`
checks that the four light aeroplanes are the ones the catalogue holds. One
test per aeroplane,
`a_<aeroplane>_asked_for_a_height_it_cannot_hold_gives_up_height_not_airspeed`,
flies 2 situations x 2 speeds - its best-climb speed and the speed a flight
starts at - which is 16 flights, counted.
- **The situations.**
  - Level near its ceiling, settled there, then asked for 3,000 ft more; ten
    minutes.
  - At 6,000 ft on a fifth of its throttle with no speed held, asked to hold
    that height; five minutes.
- **The checks.**
  - None may fall more than 2 knots below its best-climb speed: the calm-air
    airspeed band.
  - Each must end within 2 knots of that speed.
  - With a speed held, each must end no lower than it began.
  - On little power, each must end lower than it began and clear of the
    ground.
  - The engine must run throughout.
- **Measured.**
  - The slowest any flight went was 1.5 knots under its best-climb speed: the
    Cherokee and the Cub near their ceilings, handed over at that speed.
  - Near their ceilings all four climbed on at their best-climb speeds, to
    between 8,300 and 8,580 ft.
  - On little power all four came down at that speed.

**Seen to fail.**
- **Against the autopilot before this change**, all four were red, with the
  speeds given above.
- **Holding the speed without opening the throttle.** The Cherokee came down
  1,250 ft on part throttle when it could have climbed. That is why a flight
  with a speed held must end no lower than it began.

**Not handed over above the ceiling**, which would be the plainer situation.
At full rich the engine stops about 1,000 ft above the ceiling. JSBSim's
piston engine meters fuel as the mixture times sea-level pressure over
ambient pressure, and above an equivalence ratio of about 1.8 it will not
fire. The test says so.

**Three regressions found and fixed on the way.** Each was seen red and
then green.
- **The near-ceiling turns** (C172P, C182, Cherokee) lost 21-32 ft against
  their 20 ft band, because the floor took the turn's spend as height. The
  fix is the turn allowance above.
- **The Learjet 35A in both stall-lesson tests** lost 235 ft before the entry,
  because it dived for its clean best-climb speed with the gear and flaps
  down. The fix is the rule that the floor acts only clean.
- **The Cessna 182 in the circuit lesson** touched down 12.7 m off the
  centreline. The floor was acting on base with the flaps out. With the clean
  rule it passes, so the change caused it, and the clean rule fixed it.

**The hand-over is pinned on its own.** Handed over at its best-climb speed
with the throttle it starts a flight at, an aeroplane sinks while the
throttle comes up.
- **At 3,000 ft** it sinks 21-31 ft and is back within the 20 ft band in
  13-18 s. That is the same with the floor or without it, and on `main`.
- **Near the ceiling** the floor will not buy the height back with speed. The
  aeroplane climbs back at the little climb it has there: 26-39 ft, back in
  35-50 s. Without the floor it is 22-32 ft, back in 15-22 s.

So the near-ceiling turn test now checks, as its own named failure, two
things. Near the ceiling a hand-over may cost no more than 15 ft beyond the
same aeroplane's hand-over at 3,000 ft (measured: 2-8 ft more). And every
hand-over is back within the band inside 90 s (measured: 50 at most). The
room is for drift between machines. Seen to fail: allowed no more than at
3,000 ft, the C172P's 26.5 ft against 21.2 turned it red. Level flight before each turn also counts as
settled only once the height has stopped moving (under 5 ft in the half
minute) as well as the speed, so a turn is not charged with the hand-over.
The turns' own bands are unchanged.

**From review.**
- `a_light_aeroplane_whose_flaps_go_out_while_the_floor_holds_it_flies_on_its_throttle`
  holds the C172P, C182 and Cherokee on the floor near their ceilings, puts
  the flaps out, and then asks each to descend. Each throttle comes off its
  stop in 40-67 s; the limit is two minutes.
  - Seen to fail: with a hold left standing when the flaps went out, all
    three throttles stayed at their stops for the whole three minutes.
  - The Cub is named as left out: it has no flaps and fixed gear, so it is
    always clean.

**Elsewhere.** The stall lesson's own flight now asks for a speed 10 knots
below the stall as it closes the throttle, as the instructor's demonstration
already did. The autopilot, lander, departure, lesson, navigator, circuit,
stall and controller tests, with the two added from review, pass: 65 of 65. The selftest replays a pilot's
inputs and does not use the autopilot; its hash is unchanged.
