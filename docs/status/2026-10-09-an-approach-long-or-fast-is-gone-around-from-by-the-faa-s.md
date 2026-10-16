### An approach long or fast is gone around from, by the FAA's stabilized-approach criteria, 2026-10-09 — first half of the item; the item stays open

**What is still missing, first.** **Two aeroplanes going around together
are not sequenced**: nothing but the separation monitor's height limits keeps
two circuits apart, and no test flies two go-arounds at once. **The learnt
landing is not judged by the gate**: from every one of its gate's 160
corners (five winds, 32 corners each) the 172P's policy passes under 500 ft
outside +10/-5 kt of its reference speed - up to 35 kt fast, and a few
corners 5 kt slow - and lands within its limits all the same; held to the
gate, all 160 would have gone around (each of the five corner tests, 32 of
32). Its own gate box is what admits it; teaching the policy to hold its
speed is a tail.

**The rule** (`sim::StabilizedApproach`, `sim::unstabilized`,
`src/sim/lander.hpp`): at and below a 500 ft gate (the FAA Safety Team's
"It's All in Your Approach"; FSF ALAR Briefing Note 7.1's VMC gate), the
speed within +10/-5 kt of the aeroplane's own reference speed (Private
Pilot ACS, "Normal Approach and Landing") down to 50 ft, below which the
flare takes it off; and to the touch, where her path down the glidepath from
where she is meets the runway inside the touchdown zone - the first third of
the runway (Airplane Flying Handbook, FAA-H-8083-3C, ch. 9) or its first
3,000 ft (the Pilot/Controller Glossary's touchdown zone), whichever is
less. Past the zone's end and still in the air is the same rule. Nothing is
per aircraft: the speed is each aircraft's `vref_kts`, from its figures. The
approach lander goes around, from the approach or the flare, and says why
(`Lander::why_gone_around`); the go-around circuit then flies her round
to land again, as from a balloon.

**Adapted, not adopted.** The 500 ft gate is airline practice (FSF ALAR
Briefing Note 7.1's VMC gate) and the +10/-5 kt is the Private Pilot ACS's
approach standard; neither is a rule the AI is bound by, and both are
applied here as the go-around's trigger.

**Sustained, not momentary** (from the review of #146). Judged step by step,
a gust's spike sent 12 of 14 aeroplanes round from a well-flown approach in
the gusts test's severity-3 turbulence. FSF ALAR Briefing Note 7.1:
"momentary overshoots made necessary by atmospheric conditions are
acceptable", frequent or sustained ones are not. The gate now goes around
only after two seconds running unstabilized (`sustained_s`, a stated
choice). The target already carries half the gust factor where the approach
is flown so (the AFH's practice, the gusts test's +5 kt).

**A jet's engines flown spooled** (CI's macOS release run 37882278266 sent
the A380 round in the gusts test, "6 kt slow at 446 ft"; on Linux she
landed, and the 787-8 had been named out for 8 kt slow). The cause, from the
787-8's trace: on these approaches a jet is flown at idle, her throttle
working between 0 and 0.4, and a turbofan from idle is slow to give thrust -
hers went from 2,800 to 14,500 lb over five seconds of full throttle - so a
gust that took speed off left her slow for seconds. Real jets keep an
approach idle above flight idle for this. 14 CFR 25.119 (eCFR, checked)
sizes the landing climb on "the power or thrust that is available 8 seconds
after initiation of movement of the power or thrust controls from the
minimum flight idle to the go-around power or thrust setting"; 14 CFR
33.73(b) asks an engine for "95 percent rated takeoff power or thrust in not
over 5 seconds" from the minimum flight idle lever position. Engines meet
them with an approach idle above flight idle, scheduled with the gear and
the landing flap. The lander's
quarter of the throttle's travel is our stand-in for that schedule - a real
one is an N1 floor, not a lever position. The lander now holds a jet's
throttle at no less than a quarter of its travel, and works her speed error
at 0.1 where a propeller keeps 0.05, both down to twice the flare height and
not below it (still spooled into the flare, the 787-8 hand-landing test's
787 was left with her throttle too far open at the take-back). The worst
deviation each held for the gate's whole two seconds in the gusts test,
linux-release, against 5 kt slow and 10 kt fast: 737-300 2.1 / 1.1, 787-8
2.9 / 0.9, A320 2.4 / 1.6, A380 3.0 / 3.0, B-2A 1.4 / 4.0, 172P 2.0 / 3.7,
182 1.5 / 3.3, F-15C 0.5 / 0.0, F-35B 0.5 / -0.1, Cub 2.6 / 2.7, Learjet
0.9 / 0.1, Mosquito 0.2 / 1.2, Cherokee 1.5 / 2.3, S.23 2.4 / 4.2 - at
least 2.0 kt inside the slow limit and 5.8 inside the fast. The 787-8 is no
longer named: all fourteen are judged. Seen to fail on the law before
(f6b4e9bd), the 787-8 not named: sent round, "8 kt slow at 387 ft". The
propeller aeroplanes' figures are unchanged.

**Bounded: two go-arounds for the gate, then the third is landed** (from the
review). `Controller` counts the go-arounds the gate made since the approach
was given; from `most_go_arounds` (2) the next approach is flown with the
gate waived - a balloon or a runway not clear still sends her round. Airline
practice commonly allows two approaches and then a diversion; this AI has
no diversion, and a circuit flown for ever is no answer, so landing is the
stated choice over holding.

**No floor on the touchdown zone's go-around.** A float past the zone's end
at a few feet goes around (two seconds on). The AFH (FAA-H-8083-3C, ch. 9,
"Go-Arounds (Rejected Landings)",
https://www.faa.gov/sites/faa.gov/files/regulations_policies/handbooks_manuals/aviation/airplane_handbook/10_afh_ch9.pdf):
"the need to discontinue a landing may arise at any point in the landing
process", and "the most critical go-around is one started when very close to
the ground" - critical, not forbidden. It also gives an unstable approach as
a reason to go around, with 500 ft typical in VMC. Touchdown margins to the zone's end, the least in each test
(linux-release): fast 190 m, too high 189 m, as flown 192 m (each the
Mosquito, touching ~724 m along a 914 m zone), gusts 227 m (the Mosquito),
the short runway 90 m (the Cherokee, 177 of 267 m).

**The go-around climbs to 300 ft/min, not only out of a sink.** Gone around
two seconds after the gate (now sustained), a Cherokee 17 kt fast at 470 ft
held the path's incidence and flew level at full power under the 500 ft
that ends a go-around, for half an hour. The incidence now rises a degree
for each 200 ft/min she climbs slower than 300.

**Verification of the review's fixes**, each seen red first:
- `an_approach_flown_well_in_gusts_is_not_sent_round_by_the_stabilized_gate`
  (all 14): judged a step at a time, 12 sent round ("5 kt slow at 478 ft"
  and the like); now none, with the margins above.
- `an_approach_unstabilized_every_time_goes_around_twice_and_then_lands`: the
  172P with air rising 4 m/s on every short final goes around twice ("12 kt
  fast at 293 ft"), flies the circuit each time, and lands the third,
  stopped 945 m along. Unbounded: five go-arounds and no landing in the half
  hour.
- `the_learnt_landing_at_a_fast_corner_of_its_gate_is_not_sent_round_by_the_stabilized_gate`
  (test_learnt.cpp): at the corner 2.4 miles out, 60 m right, 20 m high, 5
  degrees off and 8 kt fast, the gate would find her unstabilized for 46.6 s
  running; handed the learnt landing she never goes around and is stopped.
  With the gate applied to the learnt landing: sent round at 500 ft ("33 kt
  fast"), not stopped.
- 181 tests matching the landing, lesson, circuit, go-around, gusts, stall,
  instructor, taken-back and learnt-landing names pass, with the jets'
  spooled approach; the least touchdown margin to the zone's end is 189 m;
  the selftest hash is unmoved (`182dd6c996e0ee4c`).

**The aim is inside the touchdown zone on any runway.** On a runway under
900 m the first third is shorter than the 300 m the approach aimed at, and
every approach there went around for the zone, round the circuit for ever.
The lander now aims at 300 m or half the touchdown zone, whichever is less
- the Airplane Flying Handbook (ch. 9) has the aiming point short of the
touchdown, the float carrying her past it, and moved towards the threshold
on a short field - the other half for the float. A runway of 1,800 m or
more is aimed at as before.

**The go-around climbs from a sink.** The go-around flew the incidence
learnt on the path; begun two seconds into an approach 20 kt fast, that had
been learnt at next to no lift, and the A320, 787-8, A380 and Learjet,
gone around at full power, dived into the ground at 1,800 to 5,100 ft/min.
Now a degree more incidence for each 200 ft/min she still sinks, up to the
flare's own limit short of the stall.

**Verification** (linux-release), every landplane taught the approach, the
flying boat named (no runway to fly round to), 13 each:
- `an_approach_twenty_knots_fast_at_the_gate_goes_around_and_lands`: at
  495 ft, 20 kt over her reference speed (trimmed level; the A320 cannot be
  trimmed fast down the glidepath, the Mosquito not even level, and is
  flown from as put): every one goes around "20 kt fast", flies the circuit
  and is stopped on the runway. Started above the gate, 520 or 505 ft, the
  throttle had taken up to 9 kt off by 500 ft, so the test builds it under.
- `an_approach_too_high_to_touch_down_in_the_zone_goes_around_and_lands`: a
  kilometre out at 450 ft, 227 ft over the glidepath: every one goes around
  for the touchdown zone, flies the circuit and is stopped on the runway.
- `a_stabilized_approach_does_not_go_around`: from two miles out on the
  glidepath at her reference speed, every one touches down with no
  go-around.
- `a_light_aeroplane_lands_on_a_runway_under_900_m_without_going_around`:
  an 800 m runway (twice the 172P handbook's 1,335 ft landing distance over
  50 ft), every light landplane taught the approach (4, counted): none goes
  around; the 172P touches 167 m along, the 182 163, the Cub 139, the
  Cherokee 177, against a zone ending at 267 m, and each stops on the
  runway (291 to 462 m along). **Seen to fail** aimed at 300 m: all four
  went around, "would touch down 303 to 328 m past the threshold; the
  touchdown zone ends at 267 m".
- **Seen to fail**: the trigger switched off, the fast and high tests red
  for all 13 (eight jets and the Mosquito, too high, flown into the ground
  at 630 to 940 ft/min); with the slow limit at half a knot, the stabilized
  test red (the 737-300 and the 172P "1 kt slow"); both reverted.
- The landing, lesson, circuit, go-around, gusts, vacating and learnt-landing
  tests pass (76 with the short runway's, including the light-and-heavy landings without a
  go-around and all 160 learnt corners, which do not see the gate).
  The server's multi-process landing tests were not run here.

**The selftest hash does not move** (`182dd6c996e0ee4c`, linux-release,
before and after): the selftest flies no approach.
