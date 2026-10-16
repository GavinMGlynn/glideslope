### A short runway refused the J-3 Cub, Mosquito, F-35B and B-2A on their measured landings; the runway lines not yet, 2026-10-13 — item still open

**What is still missing, first**: **the runway lines a model is shown still
do not say their landing length, nor what the aircraft needs.** Any word
added to the copilot's or the planner's request changes it for every
recording: played back, a request matches its recording but for its numbers
(`copilot::Match::but_numbers`), so a new word, or a new number, is a new
request, and all 13 copilot recordings in `tests/data/copilot/` would have
to be recorded again against the live models. **It waits to be batched with
the fly-over prompt change** (Phase 13, the copilot marks fly-over
waypoints), the owner's decision of 2026-10-13, so that the 13 recordings
are recorded again once; the wording proposed is ", N m to land on" at the
end of each runway line (its length less any displaced threshold) and "It
needs N m of runway to land on, M m wet." in the aircraft's brief. The
check itself refuses a short runway, and its refusal tells the model the
runway's landing length and the aircraft's need, as before. **The F-22A has
no figure**: she publishes no stall speed, so has no approach speed, and
`glideslope_cli land f22` has none to fly; every landing is refused her
first for that ("the aircraft has no approach speed to land at"), on 10 km
of runway as on 10 m.

**Searched again, 2026-10-13, for a published figure** (pinned in
`docs/ASSETS.md` where newly read): the Cub's owner's manual and How to Fly
a Piper Cub give none, and the L-4's Pilot's Flight Operating Instructions
(AN 01-140DA-1) are sold, not published; the Mosquito's Pilot's Notes (A.P.
2019E) say of the flapless landing only that its run "does not become
excessive", and the A&AEE reports give none; the B-2A's fact sheet and AFI
11-2B-2 volume 3 give none (only 1,000 ft of overrun beyond the minimum
runway required); the F-22A's fact sheet, Selected Acquisition Report and
AFMAN 11-2F-22A volume 1 give none; and the F-35B's product cards and Fast
Facts give none, of a rolling vertical landing or any other. Figures on
aggregator sites (the Cub's 470 ft, the B-2's 3,450 ft) have no primary
source and are not used.

**So they are measured, not published** (`basis="measured"`, a third basis
`sim::read_published_figures` accepts): `glideslope_cli land` hands each to
the approach autopilot at a gate 2 nm out, at sea level in calm air, every
tank full, and the runway she used, threshold to stop, is written 2 per cent
over and rounded up to 100 ft for the drift between machines - 2 to 7 per
cent over in all. **The figure line is written by `tools/measured_landing.py`**
from a `land` run; the re-fly test below is the check. Each figure is the
AI's demonstrated landing at sea level, in calm air, with full tanks, plus
that allowance: **not a regulatory distance and not a minimum**, and nothing
in it allows for weight, heat, height or a tailwind:

| Aircraft | Touched | Stopped | Written | Needed |
| --- | --- | --- | --- | --- |
| J-3 Cub | 292 m in | 399 m | 1,400 ft | 427 m |
| Mosquito FB VI | 777 m in | 1,633 m | 5,500 ft | 1,676 m |
| F-35B | 302 m in | 2,720 m | 9,200 ft | 2,804 m |
| B-2A | 621 m in | 2,708 m | 9,100 ft | 2,774 m |

**Factor 1**, where a published distance gets its class's margin: the AI's
own landing is not a demonstrated minimum but the landing a plan flies. For
those that publish one it approximates the factored figure for some only:
the 172P stops 547 m in against her 558 m need, but the F-15C stops 2,252 m
in, 5 per cent over her factored 2,138 m. **The F-35B's need is her rolling landing's**: she can land
vertically, on a pad, but no plan flies her to a hover yet (the transition
law's item, above), so a planned landing is the approach autopilot's rolling
one; when a plan can convert her, a pad will do, and her figures file says
so.

**Verification** (linux-release):

- `the_five_with_no_published_landing_distance_are_refused_a_runway_shorter_than_a_measured_one`:
  each of the four briefed her measured figure, pinned in the test, by the
  brief and the plan request the frontends make, refused a runway a metre
  short by the copilot and the planner alike, saying so, and given one a
  metre over; the F-22A refused a landing on 10 km of runway for having no
  approach speed. Five of five. It also reads every aircraft's figures file
  and checks that those with a measured landing distance are exactly the
  four it pins - the set the re-fly tests below find by globbing the same
  files, so the two cannot name different aircraft.
- `the_<aircraft>_lands_within_her_measured_landing_distance`, one for each
  figures file with a measured landing distance, found by reading the files
  (configure fails unless there are four): `land` flown again must stop
  within the figure and no more than a tenth short of it.
- `every_aircraft_is_refused_a_runway_shorter_than_it_needs_to_land_on_and_given_a_longer_one`:
  the aircraft with no landing distance at all are now two, the F-22A and the
  flying boat.
- `every_landplane_landed_by_hand_on_a_wet_short_runway_is_stopped_on_it_by_the_ai`:
  **the F-35B is no longer left out** - her wet runway is now sized from her
  measured need, 3,225 m, and she stops 2,359 m in. 13 of 13 landplanes.
  **Whichever of #185 and #186 lands second** runs this test again on the
  merged tree and refreshes the wet runway lengths written in
  `a_wet_short_runway`'s comment (1,928, 3,190 and 3,225 m here), which are
  worked by hand from the figures.
- **Seen red**: with `read_published_figures` giving a measured basis no
  need, the five test and the every-aircraft test failed; with the B-2A's
  figure put at 8,000 ft, `the_b2_lands_within_her_measured_landing_distance`
  failed, "stopped 2708 m past the threshold, not from 2194 to 2438 m". Both
  reverted.
- The selftest hash does not move, `182dd6c996e0ee4c`.

**Tail - the F-35B's landing distance when a plan can land her vertically.**
Her 9,200 ft is her rolling landing's, because no plan flies her into a hover
(Phase 9, "No pilot or AI plan flies the F-35B between jet-borne and
wing-borne flight", which now says so). When one does, her need for a
vertical landing is a pad, and the check must know which landing the plan
flies; her figure is revisited then.
