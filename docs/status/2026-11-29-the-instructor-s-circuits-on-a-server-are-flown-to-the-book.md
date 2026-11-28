### The instructor's circuits on a server are flown to the book, 2026-11-29

**What now works**: every lesson the server's instructor demonstrates is
debriefed "flown to the book", the seven circuits among them. Until now every
circuit but the light aeroplane's was debriefed "Hold the circuit height
downwind" on the server (YSSY 16R, terrain from the DEM), while the unit
tests' level ground flew them clean.

**The cause, measured**: the circuit lessons' downwind band watched
`position/h-agl-ft`, height over the ground below, while the instructor (the
go-around's circuit, `sim/circuit.cpp`) holds an altitude - runway elevation
plus circuit height, raised over the highest ground of the circuit - as a
pilot does on the altimeter. Flown through `sim::Instructor` over the DEM at
YSSY 16R (the server's runway: elevation 85.5 ft above the ellipsoid; still
air, as the server's log says), sampled every 5 s down the downwind stage,
against where the stage began:

| circuit | aeroplane | altitude | height over ground | ground under her | band | debrief |
|---|---|---|---|---|---|---|
| light aircraft | C172P | 0 to +23 ft | -77 to +44 ft | 73 to 195 ft | 120 | clean |
| airliner | 737-300 | -8 to +26 ft | -188 to +125 ft | 73 to 386 ft | 150 | faulted |
| fighter | F-15C | 0 to +21 ft | -178 to +138 ft | 73 to 382 ft | 150 | faulted |
| bomber | B-2A | 0 to +16 ft | -259 to +17 ft | 72 to 340 ft | 150 | faulted |
| business jet | Learjet 35A | 0 to +20 ft | -226 to +67 ft | 73 to 366 ft | 150 | faulted |
| Second World War | Mosquito FB.VI | 0 to +16 ft | -147 to +162 ft | 72 to 381 ft | 120 | faulted |
| flying boat | S.23 | 0 to +30 ft | -63 to +100 ft | 73 to 243 ft | 120 | clean here* |

\*Taught at Rose Bay heading 0 for the measurement; the server's test taught
it wherever the player was on the water, and faulted it there.

The faster aeroplanes fly bigger, longer circuits, further over the hills
north of Botany Bay; the C172P's is small enough to stay over lower ground.
Altitude is held within 30 ft everywhere: it is not the datum, the band, the
circuit height or wind (still air) - it is the ground.

**The fix, in the lessons**: the seven circuits' downwind band watches
`position/h-sl-ft`, the altitude, at the same 120 and 150 ft bands (the
judge and the bands unchanged). `light-aircraft-circuit.lesson` says why with
the figures above; the others point to it. The downwind-sink unit test names
the property the fault is judged on (`position/h-sl-ft`); on its level ground
nothing else changes.

**The same fault in the lessons flown in the air**, found by the new
assertion: in the full suite `seaplane-turns` in the S.23 was debriefed
"Hold your height through the turn" on the server (taught from the water at
Rose Bay, 3,000 ft over ground that rises under the turn), where it had been
flown to the book in #434's run. Every height judged from where a stage
began in the turns, stalls and climb-and-descent lessons - the turn's and
the stall entry's bands, the recovery's least height, and the climb's and
descent's ends - is now `position/h-sl-ft` too, 35 lines in 21 files; the
unit tests that name the property a fault is judged on (the turn that loses
height, the stall recovered badly, a recovery's allowance) name it. Heights
that are about the ground stay on `position/h-agl-ft`: the take-off's and
climb-out's ends from the runway, final's end at 40 ft, and the height a
lesson is begun at.

**Verification**

- `every_lesson_from_N_by_sevens_is_chosen_in_the_window_client_demonstrated_handed_over_and_debriefed`
  (tests/cmake/client_every_lesson.cmake) now asserts every lesson's debrief
  is "flown to the book", not only that a debrief is drawn. **Seen red**: with
  the airliner circuit's band put back on `position/h-agl-ft`, piece 2 failed
  at `airliner-circuit in the 737-300: the instructor's demonstration is
  debriefed "Hold the circuit height downwind", not flown to the book`;
  restored, green (airliner-circuit handed over at 670.2 s, flown to the book).
- The full linux-debug suite less `timing|nightly|gust_sweep`: with the
  circuits alone fixed, every test green but piece 1 of the every-lesson test,
  red at `seaplane-turns` as above; with the in-the-air lessons fixed, the
  lesson unit tests and all seven every-lesson pieces green again.
- The selftest hash does not move (the selftest tests green): no lesson is
  flown by it.

