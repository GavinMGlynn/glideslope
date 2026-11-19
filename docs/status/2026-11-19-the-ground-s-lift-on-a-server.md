# 2026-11-19: on a server the air has the ground's lift

Phase 12a, "On a server the air has no ground's lift": done.

**What is missing first:** the lift's ground reaches 32 km up and down the
wind, and a DEM tile it reaches that is not yet in the cache is fetched in
the middle of a step, on the server and every client alike - 20 to 30 MB,
synchronously, as the collision ground already does under an aircraft but
now up to 32 km ahead of her. A tile that cannot be fetched lifts nothing
over the lattice's places that reach it, so the two ends differ only where
one end could fetch it and the other could not. A tail in the plan.

## What now works

The server, the window client and the headless client (`glideslope_cli
connect --predict`) fly the weather over the collision ground: its thermals
stand on it, and the terrain's lift (ridge lift and mountain waves,
`world/lift.hpp`) is made from it.

**The lift is cheap enough to fly again** (`world::GroundLift`). The exact
lift is 256 DEM heights along the wind and a 255-wavenumber transform every
step: 188 us a step in the sanitized debug build here, per station - a
sanitized build fell 4.7 s behind on 2026-10-06. Now the terrain's transform
is made once at each place of a lattice fixed on the Earth (1/480 degree,
232 m north-south), kept, and the four about the aircraft blended linearly
by nearness, over the ground beneath her: one sum over the wavenumbers a
step. Pure in place: a place's transform is the same whenever and in
whatever order it was made, so a client flying a step again meets the
server's lift there. **Figures**, sanitized debug build: 6.3 us a step flown,
against 188 us exact (30 times less); a minute at 60 m/s, each step flown
three times as a predicting client does, made 52 transforms over 25 cells.
At most 2,048 transforms (4 kB each) are kept; past that all are let go and
made again as wanted.

The headless client reads the collision ground before joining when it
predicts (as it did for `--stovl-land`), and flies the server's air over
it; `--no-ground-lift` flies the same air over no ground, for the test
below.

## Verification

- `the_ground_lifts_lattice_is_its_exact_lift_at_each_of_its_places`: at
  512 places and heights of the lattice, the exact lift to 1e-9 m/s.
- `between_its_places_the_ground_lifts_lattice_is_within_3_percent_of_the_strongest_lift`:
  over 50,400 places and heights across synthetic hills in a 40 kt wind,
  0.038 m/s at worst against a strongest 3.07 m/s (1.2%). **Seen red**: with
  the nearest place's transform alone, not the four blended, 0.52 m/s off.
- `the_ground_lifts_lattice_is_the_same_whatever_was_made_before`: flown
  backwards, and after every transform was let go and made again, bit for
  bit the same.
- `a_minutes_flight_over_hills_makes_two_of_the_ground_lifts_transforms_a_cell`:
  52 transforms for 25 cells, and prints the cost a step against the exact.
- `a_client_predicting_over_hills_in_a_strong_wind_flies_the_servers_ground_lift_within_the_seas_bound`
  (`tests/cmake/server_ground_lift.cmake`): a Cessna at 1,200 ft over the
  hills north of the Hawkesbury in a 50 kt southerly, two predicting clients
  through 250 ms relays. Flying the lift, the prediction's median error was
  0.035 to 0.039 m, inside server_weather's 0.25 m over the sea and airport;
  the client flying the same air over no ground, 0.093 to 0.097 m. The test
  holds the first within 0.25 m and under 0.7 of the second's in the same
  run. **Seen red** with both clients over no ground - the lift on the
  server's end only: 0.114 against 0.093 m.

## Searched

"ridge lift model flight simulator orographic updraft terrain slope wind"
and "FlightGear ridge lift implementation": FlightGear's ridge lift
samples the terrain's slope at a few points about the aircraft; Forster-Lewis's
FSX method (xp-soaring.github.io) samples ground elevation at a few points
and compares local with main slope. Both are cheap because they are local;
ours keeps its linear theory along 64 km of wind and makes it cheap by
keeping the transform rather than by seeing less.
