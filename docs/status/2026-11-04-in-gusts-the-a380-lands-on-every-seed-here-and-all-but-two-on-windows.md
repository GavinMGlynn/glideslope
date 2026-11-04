### In gusts the A380 lands on every seed here, and on all but two on Windows, 2026-11-04 - item still open

**What is still missing, first**: the item. The A380 is no longer named
whole in `not_yet_landed_in_gusts()` (tests/unit/test_lesson.cpp), but on
Windows release she fails two of forty seeds, now named on those alone in
`named_on_a_seed()`: seed 3, where she touches at 374 ft/min, and seed 7,
where she floats at idle in ground effect into a go-around from a touch
1,010 m along. macOS is not yet seen with this change: the pull request's
full run flies seeds 1 to 8 there. The F-22A is still named whole, and the
other names are unchanged.

**What now works**: the A380 is judged in gusts. On all forty seeds on
linux-release and on an `-march=x86-64-v2` build (no fused multiply-adds,
as CI builds; the same figures) she lands within every limit with every
margin: worst touch 331 ft/min (seed 23), longest touch 769 m along (seed
6). On Windows release she passes thirty-eight with every margin (worst
327 ft/min, seed 13). Twelve of sixteen are judged. Every other aeroplane
flies exactly as before: each change is per-aircraft data in her figures
file, which only the A380 has, and the printed gust lines of the other
fifteen are identical, all 600 of them, before and after on linux-release
and on the x86-64-v2 build.

**What the AI was doing** (linux-release, traced, as it was):

- **Seed 7**, 907 m along: her flare held half her power to the touch, as
  the lander lands every aeroplane in turbulence, and she floated within
  fourteen feet of the runway for 800 m at 0.41 to 0.6 of her throttle.
- **Seed 18**, 350 ft/min: a lull at 157 ft at full throttle, then level
  and 36 ft over the path at 110 ft. The path law asked 1,200 ft/min and put
  her nose from 3.7 degrees down to -5.4; her quarter of a million
  kilograms sank on past what was asked, to 1,500 ft/min into her flare at
  75 ft.
- **Seed 34**, airframe struck: on short final a gust's steady bank held
  her two to four degrees right, her aileron trim wound to its stop against
  it, and in the flare it rolled her 7.4 degrees left with her aileron
  still at -0.2 to -0.3. Her wing met the runway first.

**The change** (per-aircraft data in assets/figures/a380.xml, read by
src/sim/figures.cpp and flown by src/sim/lander.cpp):

1. **Retarded at 20 ft** (`gusty_retard_ft="20"`): below 20 ft over her
   wheels in a gusty flare neither the half throttle nor the speed's floor
   holds her power on; it comes off at the flare's own half a lever a second,
   and a sink is still cushioned. **Sourced**: Airbus, Safety First, "A
   Focus on the Landing Flare" (September 2020), p. 7, for the
   A320/A330/A340/A350/A380: the 20 ft "RETARD" auto callout, and
   "Delaying the retard of the thrust levers may increase the landing
   distance because the autothrust will target Vapp or the selected speed".
   The A380 FCTM itself was not found to quote.
2. **Wings held harder** (`gusty_aileron_per_deg="0.07"`, the B-2A's):
   twice calm air's 0.035. **Tuned, not sourced.**
3. **Her nose kept up near the ground** (`gusty_most_nose_down_deg="3"`):
   below 200 ft in gusts (`gusty_nose_held_below_ft`), her nose is not put
   for the path more than 3 degrees under the attitude she flew it at as
   she came down through 200 ft (`path_pitch_`, about 1 degree for her),
   that least raised by one second (`gusty_nose_floor_lead_s`) of her
   nose-down rate: held at the least alone, her nose came down through it
   at 2.7 degrees a second to -4.7 on Windows (seed 31, 451 ft/min). The
   figure, the height and the lead are **tuned, not sourced**.

Both new attributes are refused out of bounds: a retard height outside 0
to 50 ft, a let-down outside 0 to 10 degrees.

**Tried and measured** (the A380's forty seeds, every margin):

| Variant | Linux | Windows release |
|---|---|---|
| As it was | 37 (7, 18, 34) | - |
| Aileron 0.07 | 38 (6 float, 18) | - |
| Retard at 20 ft | 35 | - |
| Aileron 0.07, retard 20 | 39 (18: 428) | - |
| ... and the path's sink capped at 1,000 below 300 ft | 39 (18: 413) | - |
| ... and capped at 900 below 300 ft | 40 | - |
| ... and the nose floor, 2 to 4 deg below 200 ft, or 3 to 4 below 300 | 40 | 35 (at 3 below 200) |
| ... and no nose let down in the flare (Airbus: no forward stick) | 13 to 39 | - |
| ... floor 3, lead 1 s (taken) | **40 (331)** | **38** |
| ... floor 2, lead 1 s | 38 | 37 |
| ... floor 2 or 3, lead 2 s | 40 or 38 | 38 |

Letting the nose down in the flare only as the Airbus article has it ("The
PF must avoid forward stick inputs once flare is initiated") failed her on
up to twenty-seven seeds, most of them floats past the zone, and was not
taken.

**Forty seeds, before and after** (pass with every margin; worst touch):

| Build | Before | After |
|---|---|---|
| linux-release | 37 (named whole) | 40 (331) |
| x86-64-v2 | 37 (named whole) | 40 (331) |
| Windows release | not measured | 38: seed 3 at 374 ft/min, seed 7 a go-around from 1,010 m |

**Verification**: `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_40` pass on linux-release, on an `-march=x86-64-v2` build and on
Windows release (tools/windows_build.sh, the test binary run seed by seed),
with the A380 judged on every seed but Windows's 3 and 7.
`a_gusty_retard_height_and_nose_let_down_are_read_and_refused_outside_their_bounds`
passes on Linux and Windows. **Seen red** (linux-release, forty seeds):
without `gusty_retard_ft` seed 6 failed ("went around: would touch down
1023 m past the threshold"); without `gusty_aileron_per_deg` seed 34
("put something other than its wheels on the runway"); without
`gusty_most_nose_down_deg` seed 18 ("touched at 428.337112 ft/min, not 25
inside 360"); and on Windows, without the lead, seed 31 ("touched at
451.200611 ft/min"). Each passed again once restored. On linux-release
the 207 tests matching figures, registration, gusts, landings, flares and
approaches pass (one skipped without a key); the full suite is left to CI.
