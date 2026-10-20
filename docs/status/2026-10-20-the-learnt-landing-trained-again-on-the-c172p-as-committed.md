### The learnt landing's policy trained again on the C172P as committed, 2026-10-20 — item done

**What is still not true first.**
- **The whole policy cannot be made again from the committed scripts.**
  Its last 3.375 million decisions (parts 8 and 9 in its header) were trained
  by tools/rl as committed (d85808cb; unchanged since) and can be. Parts 1
  to 7 were trained at earlier commits, and its 21-input parent partly on
  uncommitted work, as the header has said since 2026-10-02; those cannot.
- **A run again gives the same weights, not the same zip.** Part 8 run
  again to 29000004 and part 9 to 30375004 (the latter under its own command
  with `--steps 1375000`): the vecnorm files' SHA-256 are equal and the
  exported weights are equal (the committed file's, byte for byte below its
  header); the checkpoint zips' bytes differ (their
  archive metadata), so the header's checkpoint hash names this machine's
  file, not a remade one.
- **It still flies its approach fast** (its own Phase 10b item): all 160
  gate corners are over the stabilized gate's +10 kt between 500 and 50 ft,
  up to +42.4 kt, as before.

**Why.** The policy was trained on the C172P before her rudder was put in the
part of the slipstream that reaches her fin (this branch's other status
file). On her as committed it touched before the threshold at 8 of its
gate's 160 corners (all with the wind across from the right, 20 m low) and
at 3 of the 40 held-out starts at each load.

**How** (tools/rl/train.py, as committed; the header of
assets/rl/c172p-landing.txt is the record, its lineage parts 8 and 9):
- Part 8: from the committed checkpoint (step27000004.zip, its SHA-256 as the
  old header gave it), `--seed 160 --envs 8 --steps 4000000 --lr 3e-5
  --log-std -2.5257`, as part 7 was; 13 minutes.
- Part 9: from part 8's 29000004, `--seed 163 --envs 8 --steps 1500000
  --every 125000 --lr 1e-5 --log-std -2.5257`; 6 minutes.
- Two more runs from 29000004 were tried and not chosen from: seed 161 at
  1e-5 and seed 162 at 3e-5 on six environments, a checkpoint each 250,000.

**Chosen** on the forty held-out starts at 25, 50, 75 and 100 lb a tank
(evaluate.py --held-out) and the gate's 160 corners (corners.py): the first
checkpoint with all 160 starts and all 160 corners within the limits, part
9's at 30375004. Part 8's four checkpoints held 148 to 159 of 160 and 0 to
23 corners short; none of the two other runs' 24 met both; part 9's at
30.25M held 160 with 2 corners short. Worst held out 282 ft/min and 3.14 m
across. Training, evaluation and the runs again together took about 1 h 45
min of wall-clock.

**Exported** with export.py, its parity fixture from the chosen checkpoint
(tests/data/rl/c172p-landing-parity.txt).

**Verification.** linux-release: the 99 tests named learnt, parity,
landing, copilot or lesson green with this policy - every learnt-landing
test (27) among them, the parity pair on its own fixture, and the gate's
corners in every wind; before it was exported, the
same tests on part 8's 29000004 and part 9's 30375004 passed but for the
parity pair, and on the seed-161 run's 30.25M the gate's corners with the
wind from the left failed (not chosen).
