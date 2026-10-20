### The learnt landing's policy trained again on the C172P as committed, 2026-10-20 — item done

**What is still not true first.**
- **The whole policy cannot be made again from the committed scripts.**
  Its last 4.75 million decisions (parts 8 to 10 in its header) were trained
  by tools/rl as committed (d85808cb; unchanged since) and can be. Parts 1
  to 7 were trained at earlier commits, and its 21-input parent partly on
  uncommitted work, as the header has said since 2026-10-02; those cannot.
- **A run again gives the same weights, not the same zip.** Each of parts 8,
  9 and 10 was run again to the checkpoint taken from it: the vecnorm files'
  SHA-256 are equal and the exported weights are byte for byte the same; the
  checkpoint zips' bytes differ (their archive metadata), so the header's
  checkpoint hash names this machine's file, not a remade one.
- **The verification's starts and the gate's corners were flown in choosing
  it** (below), as the corners were for the policy before; the tests fly the
  same starts again, so their figures are not an independent sample.
- **It still flies its approach fast** (its own Phase 10b item): all 160
  gate corners are over the stabilized gate's +10 kt between 500 and 50 ft.
- Three of the landing's limits are not measured for margin: rising after
  the touch (under 3 ft; 0.0 ft at every verification start), bank after it
  (under 15 degrees) and nose-down after it (above -10 degrees).

**Why.** The policy was trained on the C172P before her rudder was put in the
part of the slipstream that reaches her fin (this branch's other status
file). On her as committed it touched before the threshold at 8 of its
gate's 160 corners (all with the wind across from the right, 20 m low) and
at 3 of the 40 held-out starts at each load.

**How** (tools/rl/train.py, as committed; the header of
assets/rl/c172p-landing.txt is the record, its lineage parts 8 to 10):
- Part 8: from the committed checkpoint (step27000004.zip), `--seed 160
  --envs 8 --steps 4000000 --lr 3e-5 --log-std -2.5257`, as part 7 was; 13
  minutes.
- Part 9: from part 8's 29000004, `--seed 163 --envs 8 --steps 1500000
  --every 125000 --lr 1e-5 --log-std -2.5257`; 6 minutes.
- Part 10: from part 9's 30375004, `--seed 164`, otherwise as part 9; 6
  minutes. Its checkpoint at 31750004 is committed.
- Two more runs from 29000004 were tried and not chosen from: seed 161 at
  1e-5 and seed 162 at 3e-5 on six environments, a checkpoint each 250,000.

**Chosen with margin** (from the review of #224). Part 9's 30375004 was
committed first, as the first checkpoint with every held-out start and gate
corner within the limits; with the wind across from the left it sank 298
ft/min against the limit of 300, a test that sometimes tests its rule, and a
held-out start at 100 lb a tank touched 4 m past the threshold. The rule is
now: the first checkpoint whose every landing - the forty held-out starts at
25, 50, 75 and 100 lb a tank, the verification's 27 at each, and the gate's
160 corners (evaluate.py and corners.py, the bindings; the simulation agrees
to 3e-15) - is at least 5 per cent inside each limit: under 285 ft/min,
within 4.75 m of the centreline, at least 15 m past the threshold (5 per
cent of the 300 m aim). Of the 37 checkpoints of parts 8 to 10 and the two
other runs, 31750004 is the first (part 10's 31250004 touched at 15 m, on
the edge; none before it met the held-out starts with margin). Training,
choosing and the runs again took about 3 hours of wall-clock.

**Each test's worst against its limit** (the C++ tests' printed figures, the
same in linux-release and an `-march=x86-64-v2` release build - no fused
multiply-adds, as CI builds: `cmake --preset linux-release -B
build/linux-release-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2
-DCMAKE_C_FLAGS=-march=x86-64-v2`, no `vfmadd` in its test program; the
least distance past the threshold from the bindings, which the tests do not
print):

| test | sink (< 300 ft/min) | across (<= 5 m) | least past threshold |
|---|---|---|---|
| verification, full tanks, calm and 10 kt across | 92 | 0.89 | 168 m |
| verification, 25 lb a tank | 161 | 0.99 | 303 m |
| verification, 50 lb | 140 | 1.13 | 263 m |
| verification, 75 lb | 113 | 1.09 | 216 m |
| gate's corners, calm | 93 | 0.42 | 171 m |
| gate's corners, across from the left | 145 | 1.20 | 155 m |
| gate's corners, across from the right | 194 | 0.87 | 216 m |
| gate's corners, ahead | 157 | 2.86 | 102 m |
| gate's corners, behind | 135 | 2.70 | 159 m |
| (choosing only) held-out, worst of four loads | 213 | 3.84 | 20 m |

The longest touchdown at the corners is 244 m along the 3,000 m runway. The parity test
agrees to 3.11e-15 in an observation and 1.4e-15 in an action in
linux-release, and to 0 in `-march=x86-64-v2`.

**Exported** with export.py, its parity fixture from the chosen checkpoint
(tests/data/rl/c172p-landing-parity.txt).

**Verification.** With part 10's policy: the 28 tests named learnt and
the rudder test green in both linux-release and `-march=x86-64-v2`, and in
linux-release the tests named landing, copilot, lesson, policy, committed or
selftest, 127, green.
