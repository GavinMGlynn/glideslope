### The Mosquito's airframe and engine are made by a script, 2026-10-14 — item still open

**What is still not true first.** **Her alpha-rate term is not changed** by
this commit: it still counts the crosswind, and the item "The Mosquito's
alpha-rate term counts the crosswind" stays open until it is. **Her text is
carried, not derived**: no pinned external model of her exists, so the
script holds her files as they were written on 2026-09-19 and applies its
changes to that.

**The cause.** CLAUDE.md says `assets/jsbsim/` is made, not written, but the
Mosquito's airframe (`aircraft/mosquito-fb6/mosquito-fb6.xml`) and engine
(`engine/merlin25.xml`) were written by hand in commit 22cf2f63 and never
changed since; only her propeller had a script
(`tools/make_mosquito_propeller.py`). So #198's alpha-rate change, made by
the scripts, did not reach her.

**The source.** None outside the repository: the history shows she was
written for glideslope from A&AEE's trials of HX809 and HJ679, the Pilot's
Notes (1944 and 1950), Rolls-Royce's Merlin 25 altitude curve and the B Mk
IV's data sheet (`docs/ASSETS.md` records the copies), with no other
simulator's model used. Each number's source is in the comment where it is
set, which the files keep.

**The fix.** `tools/make_mosquito_fb6.py` carries her two files as written,
the airframe in sections (header, metrics, mass and balance, ground
reactions, propulsion, flight controls, the aerodynamics' shared functions
and each axis) and the engine whole; its docstring names each document and
what it sets, what is estimated, and the changes made to the text as
written - none yet. A new test,
`the_committed_mosquito_airframe_and_engine_are_what_their_script_writes`,
runs it with `--check`.

**Verification.** The script writes both files byte for byte as committed:
`git status` shows nothing changed after running it, so no flight figure and
no selftest hash can move. The new test passes; seen red with a deliberate
edit to the committed airframe (one tank's capacity, 360 to 361 lb), which it
named, and green again with the edit reverted.
