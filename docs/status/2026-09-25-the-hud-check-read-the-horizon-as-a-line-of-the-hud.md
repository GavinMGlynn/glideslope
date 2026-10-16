### The HUD check read the horizon as a line of the HUD, 2026-09-25 — tail done

**Found by CI, on every platform and every branch at once.** The HUD test
flies in the live weather, so where its horizon lies at the shot's tick
changes with the day's wind. Since the controls panel came in, the HUD has 11
lines, and its last rows reach down to where the horizon can cross them.
That day the flight was banked 16 degrees and the horizon crossed row 12. The
check read it as `" ?"` and failed: "line 12 reads \" ?\", which the HUD
should not show".

**Now** the check judges only what begins at the HUD's margin, in the first
column, as every HUD line does. The scene behind the text begins further in.
The horizon line runs across the middle third of the frame, from a third of
its width to two thirds, and never reaches the margin, whatever the attitude.
It can still cross the HUD's own rows, at about 30 degrees of bank; that is a
tail, with building the crossing on purpose.

**Verified.** The failing test passes, on the same day's weather, with the
other five HUD tests. **Seen to fail:** with the HUD drawing a GEAR line for
the Cessna's fixed gear, the check still refuses it.
