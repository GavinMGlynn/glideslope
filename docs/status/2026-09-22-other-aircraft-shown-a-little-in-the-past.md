### Other aircraft, shown a little in the past, 2026-09-22 — item done

**What is missing first: no snapshot has ever arrived over a network.** There
is no handshake and no state update on the wire, so every snapshot these
tests use was put there by the test. What is built is the rule for showing an
aircraft from snapshots that arrive late, out of order, or not at all.

**Three behaviours, each with a stated limit.** An aircraft is drawn between
the two snapshots straddling the moment 100 ms ago. When none has arrived it
is carried on from its last velocity - marked as a guess - for at most half a
second, because half a second at 200 m/s is 100 m and that is as far as this
project will invent. When a snapshot finally arrives the difference is taken
up over a quarter of a second, because the jump is the thing a player sees.

**The bound was reasoned before it was measured, and the measurement agreed.**
Two metres: carrying on straight through half a second of a rate-one turn at
200 m/s leaves the arc by about 1.3 m. Measured over **every pattern of loss
across twelve snapshots - all 4,096** - the worst is **1.309 m**, at the
pattern that loses every snapshot but the first. Under jitter of up to 80 ms,
which puts snapshots out of order, it is 9 mm; with nothing lost, 2.9 mm.

**Two bugs in the blend, and the test found both.** The first measured the
correction from `held_.back()` - which by then is the snapshot that *ended*
the guess, not the one the guess was carried on from - and reported 20 m. The
second, after that was fixed, compared one frame's answer with the previous
frame's: those are answers for different moments, so it measured 16.7 ms of
the aircraft's own motion, 3.3 m, and called it a jump. The correction is now
worked out at one instant - what the guess would have said for this moment,
against what is now known for the same moment - and the error fell to 24 mm
in that case.

**And one wrong measurement of mine.** The first run reported 10 m and I was
about to raise the bound; the 10 m was the aircraft being held at the oldest
snapshot for moments *before* any snapshot covered them, which is a startup
artefact and not interpolation error. The test now measures from the first
moment its snapshots cover, and the startup behaviour is held by its own
test.
