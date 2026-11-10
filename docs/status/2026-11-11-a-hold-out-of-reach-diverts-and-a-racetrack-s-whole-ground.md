### A hold out of reach diverts, and a racetrack's whole ground, 2026-11-11

**What it does not do, first.**
- **The F-22A does not divert.** Put on final at her landing weight, she
  has about 2,800 lb when her hold is given up: ten minutes at what she
  burns level at 6,000 ft, under the way to the alternate and the ten
  minutes' reserve, so she is rightly not diverted. She comes back short of
  fuel to the runway she held for, where the person still flies the
  circuit, and strikes the ground there (seen: "struck the ground with its
  airframe", 1,470 s, 2,804 lb). The every-landplane test names her and
  leaves her out; a new plan item.
- **With no suitable runway known** - nothing told, or every one too short,
  too far for her fuel, or past ground she cannot clear - she holds her
  best where she is, as before, until her fuel (Controller::short_of_fuel)
  brings her back to the runway she held for. Without an alternate the
  choice is the departure runway, then, and only on fuel.
- **What the server knows is the runways of its collision ground**: the
  airfields whose ground is loaded around the aircraft it flies. One past
  that is not known, and not chosen.
- **The final two miles of the alternate's approach are not judged**: the
  approach she must be able to fly is judged from where she joins its
  centreline to the six-mile point, 4 nm either side; inside that it is the
  lander's.
- **The racetrack's area has no secondary area**: TERPS's 1,000 ft is over
  the primary area; the secondary area (2 nm, tapering 500 ft to none, as
  its users quote it) is not added. The primary area here is the flown
  racetrack and two turn radii beyond it, chosen; the published templates'
  dimensions were not obtained (FAA's pages refused, 403).

**What it does.**
- **A hold out of her reach diverts her** (`Controller::diverts_with`,
  `sim::choose_diversion`). Giving her hold up, she asks once for the
  runways the caller knows (the server: both ends of every runway of its
  collision ground) and leaves for the nearest, by the straight line to its
  threshold, that is: another airfield (10 nm or more from the one she held
  for - past every circuit there); long enough (`sim::landing_need_m`);
  reachable - the way there at a height she reached, 1,000 ft over the
  highest ground within 4 nm of it and never under the height she left her
  circuit at (700 ft over the person); short enough for her fuel less ten
  minutes; and with an approach she can fly, 1,000 ft over the ground
  within 4 nm from where she joins down to six miles. She drops her place
  in the sequence she held for, flies there cleaned up at her hold's speed,
  joins the extended centreline where a three-degree descent brings her to
  the six-mile point, and lands as from a hold: the legs to final and the
  approach, a straight-in to the alternate's traffic.
- **A racetrack is flown over the ground of its whole area**
  (`GoAroundCircuit::clear_the_racetrack_ground`). While she follows,
  her circuit's ground is sampled again out to where her racetrack turns
  away and two turn radii beyond, and across as her circuit's is, each
  radius at 25 degrees and her downwind speed made true at the circuit's
  height (ISA): a higher, faster racetrack is a bigger one. She flies
  1,000 ft over its highest ground; raised for it, the bigger racetrack is
  sampled again until the two agree.

**Researched first** (owner's standing rule). Searched: 14 CFR 91.169
alternate selection and 91.167's fuel; 121.639 and the most-distant
alternate; AIM 5-5-15 minimum fuel, the Pilot/Controller Glossary, ICAO's
MAYDAY FUEL and "the nearest suitable aerodrome" (Skybrary, IFALPA's
bulletin, the FAA's InFO 08004); FAA Order 7130.3A holding pattern
criteria and its templates by altitude and speed (the FAA's PDF refused,
403; the FAA's history notes on template sizes; PPRuNe's thread on
holding obstacle clearance; Boldmethod and CFI Notebook on holding);
FMS/GPS hold size from TAS and leg time (the FAA's GPS holding leg-length
note; holding-pattern calculators).
- 91.169: an IFR alternate is one whose weather and approach she can use;
  91.167: fuel to it and 45 minutes. Used: "suitable" is long enough,
  reachable at a height she can fly and on her fuel.
- AIM 5-5-15 and ICAO: minimum fuel is advisory; an emergency is declared
  where the fuel on landing at "the nearest suitable aerodrome where a safe
  landing can be made" would fall below the final reserve. Used: nearest
  suitable; the reserve is the ten minutes the controller already keeps.
- Holding: 1,000 ft of obstacle clearance throughout the primary area of a
  level hold, the secondary area tapering from 500 ft to none (as
  Boldmethod and TERPS's users quote 7130.3/8260.3); template size grows
  with altitude and indicated speed (200 kt to 6,000 ft, 230 to 14,000,
  265 above); a hold's protected area grows with true airspeed. Used: the
  1,000 ft, and a racetrack sized by her true speed at her height.
- 14 CFR 91.177(a)(2)(ii): 1,000 ft over the highest obstacle within 4 nm
  of the course - the way to an alternate.

**Verification** (tests/unit/test_lesson.cpp, the two-going-around
harness: `Extra` gives a run its ground, alternates, ceiling and length).
- `a_diversion_is_to_the_nearest_runway_she_is_told_of_that_is_another_airfield_long_enough_reachable_and_clear_to_approach`:
  nine cases, each built and the count asserted - none known; only her own
  runway's ends; too short; beyond a 20,000 ft wall; a hill under its
  approach; too far for her fuel; two airfields and her own, the nearer
  other chosen; over a plain higher than she reached; over a plain she
  reached, flown 1,000 ft over it (3,000 ft, joined 9.3 nm out).
- `an_ai_aircraft_whose_hold_is_out_of_her_reach_diverts_to_another_runway_kept_apart_the_slowest_and_the_fastest`
  (every run) and `every_landplane_whose_hold_is_out_of_her_reach_diverts_...`
  (gust_sweep): a person's C172P staying in the circuit, the one behind
  held under 6,000 ft (as a layer above holds her) with her hold's fix over
  ground 20,000 ft high on the runway's left from 15 nm; told of her own
  runway's two ends and an airfield's 25 nm to its right. All 14 flown left
  the circuit, diverted to the alternate, landed on it and taxied clear,
  never short of fuel; least 3,245 m within 500 ft and 657 ft within
  1.5 nm. 15 landplanes: 14 flown, the F-22A named (above).
- `a_racetrack_beside_high_ground_past_its_circuits_ground_is_flown_1000_ft_over_it`
  (every run: the F-15C and the Cub behind an F-15C) and
  `every_landplane_following_another_round_a_circuit_beside_high_ground_beyond_8_nm_stays_500_ft_over_it`
  (gust_sweep, all 15 behind an F-15C): ground 1,800 ft over the runway
  everywhere more than 8.25 nm before its threshold. Every one flew out
  over it (9.3 to 10.6 nm), inside the area sampled (11.5 to 11.7 nm),
  least 956 ft over it round her circuit and 884 ft at all (her final
  approach crossing its edge); kept apart, both down in order and clear.
- Re-run with the racetrack's sampling in: the climb before a hold on
  raised ground (four pairs, 1,241 to 1,772 ft climbed, never within the
  minimum) and the go-around beside rising ground, unchanged and green.

**Seen to fail.**
- The racetrack's sampling taken out: the F-15C behind an F-15C flew 397 ft
  over the high ground (8.7 nm out, nothing sampled past 8 nm), and the Cub
  never flew out over it at all - the coverage check failed it.
- The diversion taken out: neither the Cub nor the F-15C diverted; the
  F-15C held under the ceiling until short of fuel, came back with 834 lb
  and struck the ground.
- The high ground from 13 nm, not 15: the F-15C, giving her hold up 10 nm
  out, had no way she could clear within 4 nm, and was not diverted.

**Times.** linux-debug, beside another test: the nine cases under a
second; the slowest and fastest diverting 204 s; the racetrack's two
100 s - timeouts 1,800 s. linux-release: every landplane diverting 185 s,
every landplane's racetrack 79 s (gust_sweep; about 2,600 and 1,100 s on
debug at fourteen times) - timeouts 14,400 s.
