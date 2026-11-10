### Back on her reserve behind a person who stays, every landplane lands, 2026-11-11

**What it does not do, first.**
- **Nothing keeps her apart from a person who stays**: all 15 lose the
  minimum (the F-15C too now, 218 ft within 1.5 nm, where she was 658 ft
  under the old count) - the person's conflict, closed on its figures with
  the previous item (the owner's ruling of 2026-10-09).
- **Her reserve is still ten minutes**, not ICAO's 30 minutes at holding
  speed (Annex 6; EASA CAT.OP.MPA.181's AMC) nor the FAA's 45 at cruise
  (14 CFR 91.167, 121.639): `minimum_fuel_s` is shared with the short-of-
  fuel rule and the diversion's reach, which every diversion test is
  measured with. What changed is what the ten minutes are counted at.
- **Short of fuel (`short_of_fuel`) is still counted at what she burns
  now**; only the wait with nowhere to divert counts at her circuit's burn.

**The cause, measured** (the old rule, the A320, 787-8, A380 and B-2A
behind a person's C172P who stays, linux-release). The wait's rule - go when
what she has lasts less than ten minutes and her way back - was counted at
what her engines burnt at that moment, and she was circling down to wait
at idle: the A320 at 0.46 lb/s (553 lb lasting 1,209 s), the 787-8 0.55,
the A380 1.7, the B-2A 0.84. Her way back - out to the six-mile point level
at her circuit's height, flap out, and down the final - burnt the A320 2.3
lb/s, the 787-8 2.2, the A380 8.8, the B-2A 2.7: her ten minutes were
two or three. She was never sent round; there were no go-arounds. The A320
and the A380 ran dry 1.2 and 0.8 nm out and struck short (2,133 and 2,030
ft/min); the 787-8 and the B-2A ran dry on the landing roll and stopped on
the runway, 1.08 and 0.83 nm past the threshold, unable to taxi clear -
"not down" in the old test was "not landed and clear".

**What it does.**
- **Her reserve and her way back are counted at the more of what she
  burns now and what she burnt round her circuit**
  (`Controller::going_burn_pps`, `circuit_burn_pps`, `fuel_to_go_lbs`):
  the circuit's burn - configured, at her approach's speeds, with its
  go-around climbs - is measured on her as she flies it, step by step, and
  is what her way back burns. She waits until what is in her tanks falls
  below ten minutes and her way back at that burn (ICAO's MAYDAY FUEL: her
  fuel on landing below her reserve), then comes back with priority.
- Landed so, she kept her reserve: the A320 touched down with 1,788 lb (594
  s at her circuit's 3.01 lb/s), the A380 8,687 (662 s at 13.1), the 787-8
  2,018 (619 s at 3.26), the B-2A 2,643 (638 s at 4.14).

**Researched first** (owner's standing rule). Searched: ICAO Annex 6 final
reserve fuel (30 minutes at holding speed at 1,500 ft for turbine
aeroplanes, 45 for piston; at the estimated mass on arrival); EASA
CAT.OP.MPA.181 and its AMC (the same 30 minutes); 14 CFR 121.639 and
91.167 (45 minutes at normal cruising consumption) and an FAA
interpretation that using part of it needs no emergency; ICAO's 2012
MINIMUM FUEL and MAYDAY FUEL (IFALPA's bulletin, SKYbrary, the UK CAA's
notice: MAYDAY FUEL when the usable fuel predicted on landing is below
the final reserve); A320 and A380 fuel flows (PPRuNe: cruise 1,000 to
1,300 kg/h an engine for the A320, about 12 t/h for the A380; holding
lower); and the cost of gear and flap (A320 FCOM multipliers quoted at 1.6
to 2 times clean; a jet's 1,100 pph clean against 2,400 gear and half
flap). Used: the reserve is a quantity of fuel at the burn she will fly
it at, not a time at whatever she burns at the moment; configured flight
burns two or more times clean, and an idle descent far less.

**Verification** (tests/unit/test_lesson.cpp).
- `every_landplane_back_on_her_reserve_behind_a_person_who_stays_in_the_circuit_lands_measured_for_separation`
  (gust_sweep; renamed from `..._is_measured_for_separation_and_landing`):
  a person's C172P staying in the circuit, told to leave and staying; the
  one behind's hold out of reach and nowhere to divert. Her tanks are set
  as she first asks to hold 300 s past what she must go on, at the burn it
  is counted at. All 15 asked, waited (64 to 556 s) and came back on her
  fuel, and landed on her own runway and taxied clear - the set of the 15
  asserted landed, and the set losing the minimum (all 15) asserted
  exactly.
- The other tests of the wait and the hold out of reach unchanged and
  green: every landplane kept apart from a person who leaves when told,
  every landplane diverting (the F-22A's fuel still says go after 64 s),
  and their slowest-and-fastest.

**Seen to fail.** `going_burn_pps` set back to what she burns now alone -
the old rule, the tanks set by it as before: "11 of 15 landed"; the A320
and the A380 struck the ground (2,133 and 2,030 ft/min), the 787-8 and the
B-2A not landed and clear - the four the old test named.

**Times.** linux-release: the every-landplane staying test about 150 s.
