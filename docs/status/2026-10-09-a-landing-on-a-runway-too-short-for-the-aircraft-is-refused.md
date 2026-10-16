### A landing on a runway too short for the aircraft is refused, for ten of the sixteen, 2026-10-09 — item open

**Windows build, fixed after CI.** MSVC kept `world::landing_length_m` (an
inline function of `world/runways.hpp`, a header the copilot may include) as
a symbol of the copilot's archive where GCC and Clang inlined it, so the
copilot's symbol check refused the Windows build (CI run 37882278266). It is
added to `GLIDESLOPE_COPILOT_NAMES` with the header's other names.

**What it is not, first.** **Six aircraft are refused nothing for
length**: the J-3 Cub, Mosquito FB.VI, S.23, F-22A, F-35B and B-2A have no
landing distance from a primary source, searched for on 2026-10-09, and
each file says so (`<no_landing_distance>`, with what was searched and
found) rather than inventing one. The Cub's 470 ft and the B-2A's 3,450 ft
appear only on aggregator sites; the F-22A's 3,000 ft is the ATF's design
requirement retold; the Mosquito's Pilot's Notes give none; the F-35B's
need depends on whether she lands vertically or rolls, and neither is
published; and the S.23 lands on water, where a runway check does not
apply. **The model is not told** a runway's landing
length or what the aircraft needs: the runway lines listed are unchanged, so
no recording had to be made again; a model learns of a short runway only
from its refusal. The landing autopilot still lands at the runway's end as
the file places it, not at a displaced threshold - only the check counts
the displacement. So the item stays open.

**What changed.**
- **Each figures file says what runway its aircraft needs to land, or why
  nothing does**: exactly one of `<landing_distance ft basis factor>` and
  `<no_landing_distance>` (`sim::read_published_figures`, which refuses a
  file with neither, both, or no reason; `sim::landing_need_m`). The need is
  `ft` x 0.3048 x `factor`:
  - **Light aeroplanes** (`basis="over_50_ft_obstacle"`, factor 1.43): the
    handbook's total landing distance over a 50 ft obstacle - C172P 1280 ft
    (1985 POH section 1, figure 5-11; 558 m needed), C182S 1350 ft (1997
    Information Manual section 1; 589 m), Cherokee 180 E 1150 ft (Owner's
    Handbook section I; 502 m) - times 1.43, the landing distance within 70
    per cent of that available: EASA Air OPS CAT.POL.A.330(a), performance
    class B, recommended to every pilot by the UK CAA's Safety Sense leaflet 7.
  - **Transports** (`basis="field_length"`, factor 1): the airport planning
    document's landing field length, which is already the demonstrated
    distance over 0.6 (14 CFR 121.195(b); Airbus's chart says so on its
    face), dry, sea level, at the maximum landing weight the file loads -
    737-300 4,600 ft (D6-58325-6 rev D fig. 3.4.10), 747-400 6,200 ft
    (D6-58326-1 rev F fig. 3.4.2, at 574,000 lb), 787-8 5,400 ft (D6-58333 rev
    O fig. 3.4.1), A320 4,400 ft (AC Jul 2025 fig. 3-4-1-991-005-A01, CFM56,
    64.5 t), A380 6,350 ft (AC Nov 2024 fig. 3-4-1-991-001-A01, 394 t), each
    read from the chart by eye.
  - **Jets from a flight manual** (`basis="over_50_ft_obstacle"`, factor
    1.67, the 60 per cent of 14 CFR 121.195(b) and 135.385(b)): the Learjet
    35A's AFM figure 5-52, actual landing distance from 50 ft at 15,300 lb,
    sea level, ISA, about 3,050 ft (its own figure 5-53 is the same /0.6;
    1553 m needed); the F-15C's T.O. 1F-15A-1 figure A8-3, total distance
    over a 50 ft obstacle with maximum anti-skid braking at 36,946 lb, sea
    level, standard day, dry, about 4,200 ft read through four panels, to
    perhaps 300 ft (2138 m needed) - no regulation factors a military
    landing, so the stricter margin is used.
  - Every document relied on is pinned by SHA-256 in `docs/ASSETS.md`
    ("Source, the landing distance" rows; the A380's and Learjet's in their
    document tables).
- **A runway end's landing length** (`world::landing_length_m`): its
  length less its displaced threshold, now read from OurAirports'
  `le_`/`he_displaced_threshold_ft` (`RunwayEnd::displaced_m`; 0 where empty,
  or not inside the runway). Bankstown's 29C, 1415 m long, displaced 196 ft,
  has 1355 m to land on.
- **The shared landing check holds it** (`copilot::landing_refusal`, now
  given the need): after the runway is found among those the model was told
  of, its landing length from the runway data - not from the line as the
  model wrote it - must be at least the need, or "the runway YSBK 29C has
  N m to land on, and the aircraft needs M m: choose a longer one", told back
  to the model as every refusal is. The copilot's `Brief` and the planner's
  `PlanRequest` carry `landing_need_m`, filled from the figures file by
  `frontend::brief_for`.

**Verification.**
`every_aircraft_is_refused_a_runway_shorter_than_it_needs_to_land_on_and_given_a_longer_one`
(unit, synthetic runways, the catalogue's 16 aircraft asserted): each of
the ten with a landing distance is refused a runway a metre short of its
need, and one a metre long with a 2 m displaced threshold, and given one a
metre long; the six without are named in the test and refused nothing
even on a 10 m runway; the planner's and the copilot's needs are the same.
**Seen to fail** with the length check taken out of `landing_refusal`:
"737-300 needs 1402.080000 m: just short "", displaced "", just long """
(and the planner's eighth refusal: "it was taken"); and with the displaced
threshold ignored, the runway's whole length counted: "displaced """.
Both reverted.
`a_plan_from_the_ground_may_end_in_a_landing_on_a_runway_it_was_told_of_and_on_no_other`
has an eighth refusal, a runway 1 m short once its displaced threshold is
taken off, told back to the model. `a_runways_file_gives_each_open_runway_end_with_a_place_and_a_heading`
reads 16R's 279 ft displaced threshold and its landing length, and a
runway displaced at both ends (Bankstown's 11C 508 ft, 29C 196 ft), each
way's landing length less its own end's, the two different; **seen to
fail** with both ends given the `le_` end's displacement; reverted. (The
light margin's rule is CAT.POL.A.330(a), landing on dry runways for
performance class B; first cited as A.230, class A's, and corrected on
review.)
Run again, all passing (linux-debug, 37 tests, after the Learjet and F-15C
gained figures): the figures-reading tests, every copilot and planner
unit test, and every recording played back - the CBD orbit by OpenAI and by
Anthropic, for the 747-400 and the F-22A, the coast, the glide to a runway,
Sydney to Bankstown landed by the AI, and the server's copilot and hand-over
recordings - none made again, since no prompt changed.
