### Flap limit speeds are kept, and the default take-off height is 1,000 ft, 2026-11-06

Two Phase 10a tails found in the review of #348
(2026-11-06-a-low-no-plan-height-is-levelled-at-and-the-circuit-joined-there).

**What it does not do, first.**
- **The flap limit item stays open: three airliners' limits are from
  secondary sources only, and the A380's from a briefing.** The 737-300's,
  747-400's and 787-8's are from pilots' study cards, a virtual airline's
  compilation and a simulator community's list, each with its address in
  her aircraft file and docs/ASSETS.md. Their type certificate data sheets
  (FAA A16WE revision 64, FAA A20WE, EASA IM.A.120 issue 30, EASA
  IM.A.115 issue 30) give VMO/MMO and send the rest to the flight manual,
  which Boeing does not publish. The A380's are from Airbus's A380-800
  Flight Deck and Systems Briefing for Pilots, a third party's copy, not a
  limitations document; her data sheet, EASA.A.110 issue 17, gives none.
  **The A320's are primary**: EASA.A.064 issue 62, page 45.
- **Indicated limits are taken as calibrated.** The Cessnas' are their
  handbooks' calibrated figures; the rest are indicated (the PA-28's
  115 mph is her white arc's top), and the simulation flies calibrated
  airspeed.
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
  has until her flap is up past it. The 5 kt is chosen: no manual found gives a margin for a level-off,
  and it is borrowed from the one Boeing keeps under the landing flap
  placard on the approach (the same manual). The manual's low
  level-off: "use manual thrust control as needed to manage speed and
  prevent flap overspeeds" (4.2, Low Altitude Level Off).
- **Nor does a climb.** Where the next flap's manoeuvre speed is past the
  limit of the flap she has, the flap comes up at the limit less its
  margin. The 747-400's model has a lever stop at 15 degrees, which her
  type has not; her flaps-up manoeuvre speed there, about 257 kt, is past
  flaps 20's 230. She used to climb to it and fly 262 kt.

The limits, and where each is from, are in docs/ASSETS.md ("Each
landplane's flap limit speeds").

**Research.** Searches, the second round (from the review of #349):
- the FAA type certificate data sheets A16WE (737; revision 64 read),
  A20WE (747; boeing.com's copy would not download, revision 41 seen in a
  search: "see AFM"), T00021SE (787), A58NM (A380, not found to read);
- EASA's: EASA.A.064 (A320, issue 62, read: VFE by configuration),
  EASA.A.110 (A380, issue 17, read: "Refer to approved Airplane Flight
  Manual"), IM.A.120 (737, issue 30, read: "See Airplane Flight Manual"),
  IM.A.115 (787, issue 30, read: the same);
- the 747-400's, 787-8's and A380's flap placards again, for addresses;
  Airbus's A380 pilots' briefing, already in docs/ASSETS.md, for its
  slats and flaps table.

Searches, the first round:
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
- **A take-off to a plan** now keeps her flap under its limit too. Before
  and now, fastest past her flap's limit, planned to 1,500 ft and on: the
  747-400 31.8 kt past (262 kt with her flap at 10.5 degrees, held to
  flaps 20's 230: her model's lever stops at 15 degrees, and she was
  accelerated to her flaps-up manoeuvre speed before it came up) to 5.1
  kt under (225 kt). Every other planned take-off is unchanged: the
  737-300 48.6 kt under, 787-8 30.5, A320 31.7, A380 0.9, Learjet 35A
  24.1, C172P 33.3, C182 37.4, PA-28 23.2; the Mosquito flies none out.
- Taken off to 650 ft with no plan, before and now, against the limits:
  the 747-400 32.3 kt past to 4.6 under, the A380 5.9 past to 1.6 under.
- Nothing else calls the take-off's default height.
- The selftest hash does not move: it flies a pilot's input log.
- Run on linux-release, excluding the gust_sweep label: every test
  matching take, depart, circuit, lesson, autopilot, selftest, lift_off,
  level, hand, capture, gear, go_around, going_around, instructor, flap,
  catalogue or aircraft: 414 of 414 passed, 11 of them skipped for want of a key (the language-model tests asking a provider now). After the review of #349 (comments, sources, the flap lever line put back): every test matching take, depart, circuit, lesson, level, capture, gear, flap, catalogue or instructor, 202 of 202.

**Closed on the owner's blocked-item ruling** (2026-10-09: blocked items
close on measured figures). Primary sources for the 737-300's, 747-400's,
787-8's and A380's flap limit speeds were searched for and are not public:
- their type certificate data sheets (FAA A16WE rev 64 and A20WE rev 41,
  and EASA's for the 737, 787 and A380) send the limits to the flight
  manual;
- no flight manual was found.

Their published figures from secondary sources stand, each labelled
secondary with its URL. The item is ticked on them. It reopens if the owner
rules otherwise, and a primary source replaces each figure when one turns
up.
