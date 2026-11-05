### Flap limit speeds are kept, and the default take-off height is 1,000 ft, 2026-11-06

Two Phase 10a tails found in the review of #348
(2026-11-06-a-low-no-plan-height-is-levelled-at-and-the-circuit-joined-there).

**What it does not do, first.**
- **The five airliners' flap limit speeds are from secondary sources.**
  No Boeing or Airbus limitations document giving the 737-300's, 747-400's,
  787-8's, A320's or A380's was found public. Theirs are from pilots' study
  cards, a virtual airline's compilation and FlyByWire's documentation,
  each named in her aircraft file and in docs/ASSETS.md. This is a new
  Phase 10a tail.
- **Indicated limits are taken as calibrated.** The Cessnas' and the
  Piper's are their handbooks' calibrated figures; the rest are indicated,
  and the simulation flies calibrated airspeed.
- **Five landplanes have no limit to keep**, named in the test: the B-2A
  and F-22A (no flight manual public), the F-35B (none public, and no flaps
  in her model), the F-15C (her manual's 250 kt, but no flaps in her
  model), the J-3 Cub (no flaps). The Mosquito has hers, but takes off
  with no flap, and the test asserts she flies none out.
- **A no-plan height below 400 ft** is still levelled at with the flap
  out, at take-off thrust up to the flap's limit; nothing here asks one.
- **Measured on linux-release only.** Nothing here is platform-specific
  code, so there was no Windows build.

**1. An AI take-off with no height given ends at 1,000 ft, not 500**
(`Controller::ai_take_off_to_ft`). Three choices were weighed for a low
no-plan level-off: hold the climb to acceleration height, level at a
reduced thrust, or raise the default.
- **Reduced thrust was built and dropped.** Held at her climb speed by
  the throttle until level, and accelerated only then, the A380 fell to
  143 kt in the circuit lesson's climb out, under its band, as her throttle
  came back. Flown to 500 ft, the 737-300 still began to accelerate a few
  feet short of the take-off lesson's end, and its debrief said "Hold the
  take-off safety speed".
- **Holding the climb to 400 ft** was tried in #348 and sent the F-22A
  291 ft past 650.
- **Raising the default is sourced**: "During training flights, 1,000
  feet AFE is normally used as the acceleration height to initiate thrust
  reduction and flap retraction" (Boeing, 737 Classic Flight Crew Training
  Manual, FCT 737 CL (TM) revision 20, 30 June 2021, 3.28, Flap Retraction
  Schedule). A demonstration is a training flight, and 1,000 ft is clear of
  the take-off lessons' end at 500.

**2. Each landplane's flap limit speeds (VFE) are in her aircraft file**
(`flap-limit DEG KCAS`, `sim::flap_limit_kts`). A flap between two
settings is held to the limit of the one with more flap.
- **A level-off never passes her flap's limit.** At the take-off's
  thrust, the throttle holds her 5 kt under the limit of the flap she
  has until her flap is up past it. The 5 kt is Boeing's margin under the
  landing flap placard on the approach (the same manual). The manual's low
  level-off: "use manual thrust control as needed to manage speed and
  prevent flap overspeeds" (4.2, Low Altitude Level Off).
- **Nor does a climb.** Where the next flap's manoeuvre speed is past the
  limit of the flap she has, the flap comes up at the limit less its
  margin. The 747-400's model has a lever stop at 15 degrees, which her
  type has not; her flaps-up manoeuvre speed there, about 257 kt, is past
  flaps 20's 230. She used to climb to it and fly 262 kt.

The limits, and where each is from, are in docs/ASSETS.md ("Each
landplane's flap limit speeds").

**Research.** Searches:
- VFE placards and limitations: 737-300 (FCOM, flap placard, the bespin
  limitations PDF, which would not download), 747-400 (three wordings),
  787-8 (two), A320 VFE CONF, A380 VFE CONF and flap angles, Airbus flight
  deck briefings on smartcockpit and Scribd;
- the C172P, C182S and PA-28-180 handbooks' airspeed limitations, the
  Learjet 35A AFM's section I, the Mosquito's Pilot's Notes, the F-15's
  T.O. 1F-15A-1 section V (the archive.org OCR text), each from the copy
  docs/ASSETS.md already records;
- F-22A, F-35B and B-2A flap limits; the J-3's type certificate; the Short
  S.23's flap limit;
- Boeing's low-altitude level-off: "low altitude level off" FCTM, flap
  placard, command speed, takeoff thrust. Found and read: the 737 Classic
  FCTM revision 20 (aviationtricks.com copy, SHA-256
  `dd4de63d66690aaf016b7febfa2fa68cb092f14089be04d4fcc94bd059b200fe`),
  4.2: "If ALT ACQ occurs before N1 is selected, automatic thrust reduction
  occurs and the autothrottle speed mode engages".

**Verification.**
- `no_flap_is_ever_flown_past_its_limit_speed_in_an_ai_take_off`
  (test_lesson.cpp, new). Every step from brakes off, each of the ten
  landplanes with limits is taken off with no plan to 500, 650, 1,000 and
  3,000 ft, and to a plan's 1,500 ft and on. No wreck, the take-off over,
  flap out (none for the Mosquito), and never past its limit. 50 of 50
  flown; the five without limits named, and a name with limits fails.
  Nearest: A320 to 500 ft 1.2 kt under, A380 1.4 under, 747-400 4.6
  under. Red on the old `departure.cpp` and `.hpp`: the 747-400 to 500 ft
  flew 32.3 kt past, 262 kt with her flap at 10.5 degrees.
- `a_flap_limit_speed_is_read_for_each_setting_and_a_flap_between_two_is_held_to_the_lower`
  (test_catalogue.cpp, new): settings sorted, between two the lower limit,
  past the last its limit, flap in none; a repeated setting, a line without
  its speed and a limit for flap in refused. Red with the half-degree
  tolerance taken out of `flap_limit_kts`.
- `an_instructor_demonstrates_a_take_off_and_hands_it_over` now flies the
  default height (it asked 1,000 ft by hand). Every taught aeroplane's
  debrief is empty. Red with the default at 500: the 737-300's said "Hold
  the take-off safety speed".

**What moved.** Measured on linux-release.
- Taken off to 650 ft with no plan, the fastest with flap out, before and
  now: 747-400 269 to 232 kt, A380 228 to 220; the 737-300 193 to 194,
  787-8 199, A320 206 to 208, Learjet 35A 179 to 180, F-22A 265 (no
  limit). Past 650 at most: 747-400 10.5 to 8.8 ft, A380 10.4 to 10.6,
  A320 3.0 to 3.3, 787-8 4.7 to 4.8; the rest unchanged.
- The circuit lesson's climb out, now: 737-300 164 to 189 kt, A320 160 to
  192, A380 153 to 174, Learjet 35A 163 to 192, inside its band.
- Nothing else calls the take-off's default height.
- The selftest hash does not move: it flies a pilot's input log.
- Run on linux-release, excluding the gust_sweep label: every test
  matching take, depart, circuit, lesson, autopilot, selftest, lift_off,
  level, hand, capture, gear, go_around, going_around, instructor, flap,
  catalogue or aircraft: 414 of 414 passed, 11 of them skipped for want of a key (the language-model tests asking a provider now).
