### The S.23's hull tried as a planing hull: her gust landings' pass was an artefact of her buoyancy tail, and the hull waits on Phase 9, 2026-10-13 — item still open

**What is missing, first**: the S.23's hull is not a planing hull, and she
skips and porpoises on the water in gusts and in calm air. Her flying-boat
landing item in Phase 10b waits on a new Phase 9 item, "The S.23's hull is
not a planing hull". Nothing from this round is merged. The work is kept on
branch `s23-planing-hull-wip` at 9c67bec1, three WIP commits on #184's
d736416b: the hull, the buoyancy fade with a porpoise test, and a
step-attitude hold in the lander.

**What her pinned model's hull does** (JSBSim's Short_S23, its
hydrodynamics unchanged until now):
- Its planing lift is a flat clip. Above 34 kt it is 20,000 lb (1,000 x
  a tweak factor of 20) wherever the hull's reference point is within
  6.5 ft of the water. It rises over 0.6 ft of height, with nothing
  growing with immersion, and is cut to zero beyond about 4 degrees of bank.
- Its buoyancy table, from its author's Gerris runs, gives about 1,000 lb
  with her keel nearly 3 ft clear of the water, and nothing only from 9 ft.
- Nothing models the afterbody planing.

**The buoyancy tail hid skipping.** A landing's touch, and every rise the
tests measure, is taken from the first step the water's forces act. With
the tail, that was with her keel about 3 ft up, so a skip below that height
was never seen. The gust seeds that passed on the Savitsky hull before the
tail was cut (risen at most 0.49 ft) passed on that artefact.

**Tried on the wip branch** (`tools/make_short_s23.py`, nothing
hand-edited in assets/jsbsim):
- **Savitsky's planing lift** (1964, "Hydrodynamic Design of Planing
  Hulls"): C_L0 = 0.012 lambda^0.5 tau^1.1 on 1/2 rho V^2 b^2, with beam
  b = 10 ft. lambda is the main step's immersion over b sin(tau), from the
  keel skids' geometry, capped at 4. tau is the hull's trim plus the angle
  her sink adds. The hydrostatic term is left to the buoyancy table, and
  there is no deadrise correction (unsourced). The roll cut-off is removed:
  kept, it dropped the lift mid-run and she porpoised on take-off.
- **Heave damping** of the water's added mass, rho (pi/8) b^2 V (strip
  theory, as in Zarnick's DTNSRDC-78/032), about 9,200 lb per ft/s at
  70 kt, wetted in over the step's first 0.5 ft (an estimate). The same
  term at the step's lever, 4.53 ft squared, damps pitch.
- **The buoyancy faded out as the keel clears the water**: force, pitch and
  roll moments faded to nothing over the keel's first 0.25 ft above the
  surface, the immersed table its author's. Buoyancy is the water
  displaced, so none acts with the hull out of it.
- A calm-air test, `a_flying_boat_alighting_in_calm_air_holds_the_step_
  without_porpoising_down_to_taxi_speed`. From her hull's touch to 20 kt,
  no rise over 0.5 ft above the lowest she has been since.
- In the lander, a hull on the water is held at her touchdown attitude
  until off the step (FAA-H-8083-23, ch. 6, "Normal Landing"). It replaces
  a wheel's rollout, which eases the stick back as she slows and took her
  trim from 5 to 12.5 degrees on the step. Its gain (0.2 a degree) was
  fitted.

**Measured** (linux-release):

| | pinned hull (#184) | Savitsky hull | and buoyancy fade | range |
|---|---|---|---|---|
| take-off time, 45,000 lb | 32.86 s | 35.5 s | 30.99 s | 25.93-35.08 |
| take-off run, 45,000 lb | 823 yd | 862 yd | 682 yd | 675.75-914.25 |
| take-off time, standard | 25.20 s | 27.25 s | 24.13 s | 21-27 |
| draught | 3.68 ft | 3.69 ft | 3.69 ft | 3.5-4.5 |
| calm hop on the water | 4.10 ft | - | 0.86 ft (0.50 with the hold) | 0.5 |

The take-offs are back in range with the buoyancy faded; the run only 6
yd inside. The porpoise test is red on #184's hull (4.10 ft, seen) and on
the new one (0.86 ft). With the hold it reaches 0.50 against 0.5, which is
not a robust pass.

The gust test's eight seeds on the faded hull with the hold. The rise
limit is 1.0 ft, less 0.25 margin:

| seed | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| touched, ft/min | 213 | 130 | 119 | 175 | 189 | 284 | 224 | 166 |
| rose, ft | 1.67 | 0.25 | 0.23 | 0.35 | 1.06 | 1.20 | 1.05 | 2.18 |

Without the hold, seed 1 rose 1.52 and seed 8 2.20: the hold is not what
fails them.

**Open questions**:
- **Afterbody planing.** Above about 9 degrees of trim her lowest keel
  point is the afterbody, and she rides it with no planing lift there.
  NACA's porpoising stability limits are the source for that term.
- **Whether a step-attitude hold belongs in the lander.** It is the
  handbook's technique, but its gain was fitted to the test, and even with
  the elevator near full nose-down her trim drifts to 9 degrees on the
  step.
- **The hop limit on the step.** 0.5 ft is the calm landing's limit on a
  rise, taken over for a hull planing on the water.

**Verification**: none landed; docs only. The figures above are from
linux-release runs of the take-off figures (`glideslope_cli figures
short_s23`), the new test, and the eight gust seeds on the wip branch's
states.
