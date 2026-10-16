### Ride along in an AI aircraft, then take it over: planned, 2026-09-25

**Asked for by the project owner.** They want to step into the cockpit of
any AI aircraft, watch it fly, and take it over. They also asked how a change
of control is seen now.

**The answer: barely.**
- Offline, the HUD's `AP` line appears while the AI flies and goes when you
  do.
- No control's position is shown, and the model's control surfaces do not
  move (a tail).
- Online, nothing is shown at all.

**Three items added to Phase 7**, and two features to `FEATURES.md`:
- who is flying, and the controls, on screen;
- riding along in any AI aircraft;
- taking one over.

**Two decisions, the defaults proposed to the owner:**
- the aircraft you had goes to the AI pilot rather than leaving the sky;
- the work belongs in Phase 7, as a matter of who flies an aircraft.

**What it will need.** State updates carry no control positions, and a full
one has room for none: 1,130 of 1,232 bytes. So the server will send them for
the one aircraft each client watches, as it sends a client its own motion.
