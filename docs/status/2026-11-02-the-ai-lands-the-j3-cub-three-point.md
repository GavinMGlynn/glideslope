### The AI lands the J-3 Cub three-point, 2026-11-02

Phase 10b's "The AI lands the J-3 Cub three-point" is done in calm air and
in every crosswind to her 12 kt, dry and wet. She is held off a foot as the
FAA's Airplane Flying Handbook (FAA-H-8083-3C, AFH below) has it. She
touches at 11.82 to 11.90 degrees, where she stands at 12.1, and her tail
wheel meets the runway 0.033 to 0.067 s after her first main wheel.

**What is still not true first.**
- **In gusts she is still flown on to her main wheels**, as the AFH prefers
  in gusty wind (ch. 9, "Turbulent Air Approach and Landing", and ch. 14).
- **She touches stalled, from under a foot.** Her model's lift peaks at 12.5
  degrees of incidence (1.62 at 1.7 million). In every landing her wing
  passes that peak in her last foot, reaching 14.0 to 14.3 degrees as she
  touches. Above her last foot the lander holds it to 12, under the peak
  (asserted in the hard-flare lesson, where it reaches 10.6 there). The AFH has an aeroplane touch "at or just above the approximate
  stalling speed" (ch. 9, "Touchdown", page 9-8), and a touch from "no more
  than a foot off the surface" undramatic (ch. 14, page 14-4). It does not
  call a three-point landing a full-stall landing in so many words.
- She touches at 114 to 125 ft/min, where she touched at 68. The sink limit
  is 300, and the server's gear judgement passes every landing.
- **Two of the test bounds are judgements**, not sourced figures:
  - the trim's rate in the hold-off;
  - the tenth of a second the tail wheel test allows.
- Whether her manual's 38 mph stall is indicated is still open (the
  wing-section file).

**The source.** The AFH, ch. 14, "Three-Point Landing", page 14-4: "Round out
(level-off) should be made with the main wheels about one foot off the
surface. From that point forward, the technique is essentially the same
that is used in nose-wheels: a gentle increase in AOA to maintain flight
while slowing. In a tailwheel aircraft, however, the goal is to attain a
much steeper fuselage angle than that commonly used in nose-wheel models;
one that touches the tailwheels at the same time as the main wheels."

On the same page, under "Touchdown": "the airplane is held off the surface
of the runway until the attitude needed to remain aloft matches the
geometry of the landing gear".

Under "Crosswinds", pages 14-5 to 14-6: "only one main wheel makes initial
contact, either in concert with the tailwheel in three-point landings".

**Why she touched at 10.2 before** (the investigation, on her model as on
main, traced every tenth of a second):
- **Her elevator was not out of authority.** At the touch it stood at 64%
  of its travel, with 6 to 8 degrees of incidence in hand. Driving it to
  full travel with the flare's gain raised to 9, 12 and 18 took her only to
  10.5. Her travel (A-691's 34 up), CG (13.94 in, mid-range), slipstream at
  idle (1.11 times the free stream) and the tail's downwash in ground
  effect (not modelled; it would add nose-down) were checked, and none is
  the cause.
- **The lander asked for 12.1 and got 10.2.** Its flare law is
  proportional, and it settled 1.9 degrees short with her nose rate at
  nought, held off only half a foot.

**What changed** (src/sim/lander.cpp). Each change is behind her figures'
`lands="three_point"` and not in gusts, and only the Cub's figures say it:
- **Held off a foot**, the AFH's "about one foot"
  (`Lander::three_point_hold_ft`), where she was held at half a foot.
- **In her last foot only, her incidence is not limited short of the
  stall.** Her nose is bounded there by her standing attitude. Above that
  foot the lander's limit stands, 12 degrees for her. With a limit kept all
  the way down she touched at 11.2 with it at 12, 11.5 at 12.5, 11.7 at 13,
  and 11.8 from 13.5 up.
- **The back pressure goes on increasing until her nose is there.** The
  trim takes up the attitude's error at 3 a degree a second while she is
  held off, not 0.02. Swept at 0.1, 0.3, 1 and 3, the touch was at 10.6,
  11.6, 11.7 and 11.8 degrees. The rate is chosen, not sourced.

**The tests.**
- **The margin, 0.43 degrees, derived from her gear.** Unloaded, her three
  wheels meet a level runway at atan(43.31 / 201.97) = 12.10 degrees, her
  standing attitude. Her main tyres' standing deflection, about 1.5 in over
  her 202 in wheelbase, is 0.43 degrees. Within that the tail wheel reaches
  the runway as her mains take her weight. It was 2.5 degrees, fitted.
- **Her tail wheel within 0.1 s of her first main**, asserted in all five
  Cub landing tests (tests/unit/test_lander.cpp). This is a judgement: her
  main tyres take 0.065 to 0.073 s to squash by their standing deflection
  (1.5 to 1.7 in) at her touch's sink. In a crosswind the first main is the
  upwind one, as the AFH's "Crosswinds" has it.
- **The hard-flare lesson is renamed**
  `a_flare_the_pilot_pulls_hard_and_hands_back_in_the_air_is_landed_with_no_stall_above_her_last_foot`
  (it was `..._is_landed_without_a_stall`). For an aeroplane landed
  three-point, her wing is held to every aeroplane's bound above her last
  foot, and her nose to a degree over her standing attitude. Handed back
  from a hard flare, the Cub's wing reaches 10.6 degrees above her last foot
  and 13.8 within it, past her peak at 12.5. Her nose reaches 11.8. Every
  other aeroplane is held to the bound as before.

**Figures** (linux-release, after rebasing on main with #294 and the
F-22A's lander changes):
- **Landing tests**, from a mile out, in calm air and every crosswind from 1
  to 12 kt, both sides, dry and wet (50 landings): she touches at 11.82 to
  11.90 degrees and 114 to 125 ft/min, 360 to 363 m in. Her tail wheel
  comes 0.033 to 0.067 s after her first main, and she stops 434 to 439 m
  in.
- **`land j3cub`**, from 2 nm: 363 m in, 11.8 degrees, 116 ft/min, stopped
  440 m in. Her measured landing distance, 1,500 ft, is unchanged.

**Verification.** Each of these was seen red, then restored:
- **The margin and the tail wheel:** with the lander as on main, all five
  Cub landing tests fail. The first two failures are "the Cub touched with
  her nose at 10.225225 degrees, where she stands at 12.103087 in calm air,
  dry" and "the Cub's tail wheel met the runway 0.166667 s after her first
  main wheel, not within 0.100000 s in calm air, dry".
- **The hard-flare lesson's last foot:** with the bound counted down to the
  touch, it fails with "j3cub handed back from a hard flare had her wing
  raised to 13.830836 degrees above her last foot, past 13.000000". With
  the nose bound set a degree under her standing attitude, it fails too.
- **The full linux-release suite** (-LE nightly), after the rebase: 1,430 of
  1,430 pass. The copilot tests that need a key skip.
