### The flying boat in gusts, held on the water within the band between her touch and TN 923's floor: named on no seed, but over forty-eight seeds still failing six, 2026-11-03 - item still open

**What is still missing, first**: the item. Over 48 gust seeds on Linux she
still fails six landings: she rises 1.80, 3.34 and 1.21 ft off the water
after touching (seeds 13, 25, 32), touches at 347 and 345 ft/min (seeds 11,
40; judged 335 with the margin), and goes round for a balloon above the
flare (seed 30). The cause is that she touches at speeds where her wing
still carries her (below). macOS release's seed 3, on which she was named,
has not been flown on this change; CI's full run is its check.

**What now works**: she is named on no seed on any platform
(`named_on_a_seed` is empty). On Windows release seed 5 she rises 0.52 ft,
where she rose 0.87 before. On Linux all eight seeds now rise less than
before (figures below).

**Why she rose**: she touches where her wing still carries her. Held off
with her nose capped at 5 degrees, she is let down at 64 to 92 kt, about 1.0
to 1.4 times her measured 66 kt stall. At 4 to 5 degrees of incidence there,
her lift is her weight. Traced on seed 13, the water's first touch makes
246,000 lb ft nose-up from the hull moment of her model's author
(`hydro/hull-pitch-moment-lbsft`, Dawson's S-40 tank data). Savitsky's
centre of pressure makes 131,000 to 161,000 nose-down, so the net is about
100,000 nose-up. Her stick held her attitude with a third of its travel
against that, and her nose rose at 3 degrees a second. Her wing took her
weight back and she climbed at 170 ft/min. With the S-40 moment taken out of
the built data, the rises on 22, 32 and 45 went away, but she could no
longer take off (four take-off tests failed) and porpoised in calm air. It
stays in.

**The change** (`sim::Lander`, a hull in gusts on the water alone): her
attitude on the water is held within the band the seaplane handbook's
flatter touch leaves her. That band runs from her touch down to NACA TN
923's floor (3 degrees, "abnormally low trims (possibly 3 degrees or less)
should be avoided ... on landing": `rough_water_least_trim_deg`), so an
error the size of that band asks for the whole stick. It still asks for no
more than 3.5 degrees, and is damped at 0.7 of critical as before. Before
this, the band was her whole margin to the afterbody's 10.8 degrees: six
degrees from a touch at 4.5, so a degree of error asked for a sixth of her
stick. Calm water and every landplane are unchanged, because the band is a
hull's and is used in gusts alone.

**What the books say** (FAA-H-8083-23, 2004 ed., read from faa.gov's
faa-h-8083-23-3.pdf):
- ch. 6, "Normal Landing": "the touchdown is made at the lowest possible
  speed for the conditions".
- ch. 6, "Rough Water Landing": "make the touchdown at a somewhat flatter
  pitch attitude than usual. This prevents the seaplane from being tossed
  back into the air".
- ch. 4, "Skipping": "landing at excessive speed with the nose at too high a
  pitch angle".

Her own flaps cannot dump her lift: Cassidy (Flying Empires, p. 30, quoted
in her model) gives a minute and a half to retract them. They are already
brought up from the touch. No S.23 alighting speed was found: the Pilot's
Notes and Cassidy's Empire Flying Boat Manual were not reached.

**Forty-eight seeds, Linux** (x86-64-v2, which gives linux-release's figures
exactly; no FMA in the binary):

| | rises over 0.75 ft | hard | went round |
|---|---|---|---|
| before | 13 (3.32), 22 (0.80), 25 (6.36), 32 (2.95), 45 (0.91) | 11, 40 | 30 |
| now | 13 (1.80), 25 (3.34), 32 (1.21) | 11, 40 | 30 |

Tried as well, and not taken: a flatter step (3.25 or 3.0 degrees), which
raised seed 25's nose from her 1.6-degree touch and threw her up 10 ft; and
a higher nose cap in the hold-off (5.5 or 6.0 degrees), where a lull drops
her on at 382 to 401 ft/min (seed 3).

**The eight committed seeds**, rose ft (touched ft/min):

| seed | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| Linux before | 0.00 | 0.61 | 0.31 | 0.22 | 0.18 | 0.25 | 0.18 | 0.14 |
| Linux now | 0.00 (108) | 0.34 (244) | 0.19 (294) | 0.05 (187) | 0.07 (313) | 0.00 (177) | 0.18 (25) | 0.01 (153) |
| Windows release now | 0.27 (237) | 0.00 (271) | 0.13 (94) | 0.00 (241) | 0.52 (200) | 0.01 (214) | 0.20 (141) | 0.00 (233) |

**Why the 48 seeds are not the test**: a platform's floating point gives
each seed other gusts at the flare, so the failing seeds differ by platform,
and naming Linux's would not name Windows's or macOS's. They are named here
and in the item, with their figures, until the hold-off itself is changed.

**Verification**:
- `in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1` to
  `_8`: the S.23 is judged on every seed and platform. All eight pass on
  linux x86-64-v2 and on Windows release (`tools/windows_build.sh
  windows-release`). **Seen red**: before this change, Windows seed 5 rose
  0.87 ft (CI runs 41100266299 and 41303118662).
- The gust and hull tests on Linux (`ctest -R
  'in_gusts|hold_off|hull|s23|flying_boat|seaplane|water|alight'`): 47 of
  48 passed. The one failure was the server's climb-out test, which was not
  built in this tests-only build directory.
