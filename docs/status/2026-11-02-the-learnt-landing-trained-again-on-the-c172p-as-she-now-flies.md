### The learnt landing trained again on the C172P as she now flies, 2026-11-02 — item done

**What is still not true first.**
- **The whole policy still cannot be made again from the committed scripts.**
  Parts 8 to 12 in its header were trained by tools/rl as committed
  (d85808cb; unchanged since) and can be; parts 1 to 7 were trained at
  earlier commits, and its 21-input parent partly on uncommitted work, as
  the header has said since 2026-10-02.
- **It was not trained from nothing**: warm started from the committed
  policy's checkpoint (step31750004, the header's part 10), as parts 7 to 10
  were from theirs - the method the policy was made by.
- **Six environments, not eight**, the machine being shared (at most six
  training processes, torch on one thread each). A run again needs
  `--envs 6`.
- **The verification's starts and the gate's corners were flown in choosing
  it**, as before; the tests fly the same starts again, so their figures are
  not an independent sample (test_learnt.cpp's comment on the starts now
  says so: it still said they were never flown in choosing).
- **It still flies its approach fast** (its own Phase 10b item, left open).
- **The worst landing is close to its margin**: from the gate's corners
  with the wind behind, 281 ft/min against the chosen margin of 285 (limit
  300).
- Three limits are not measured for margin, as before: rising after the
  touch, bank after it and nose-down after it.

**Why.** The policy was trained on the C172P of 2026-10-20. Her flaps',
elevator's and pitch-rate lift then moved onto the dynamic pressure without
its sideways part (2026-10-23), and side force and pitching moments left
out the crosswind after; on her as committed (d56a7ca6, c172p.xml sha256
9806731d...) it touched 39 to 69 m shorter from the verification's starts,
and - in the choosing rule's held-out starts, which no test flies - before
the threshold from 1 of 40 at 75 lb a tank and 4 of 40 at 100 lb, all in
headwinds of 9.6 to 13.1 kt (training draws up to 8): 32 to 65 m short.
**No test flies those five, and none is added**: they are outside the
learnt landing's gate, which admits no more than 8 kt ahead
(`LearntGate::most_headwind_kts`; she bounced at 10 and 15 kt), so the
simulation never hands her to the policy there. A test of them would hold
the policy where it is not offered; the plan's new Phase 10b item, "The
learnt landing in headwinds over its gate's 8 kt", is the gate widened or
the held-out starts drawn inside it.

**How** (tools/rl/train.py as committed; the header of
assets/rl/c172p-landing.txt is the record, its lineage parts 11 and 12):
- Part 11: from step31750004, `--seed 167 --envs 6 --steps 1500000 --every
  125000 --lr 1e-5 --log-std -2.5257`; its checkpoint at 32500004.
- Part 12: from part 11's 32500004, `--seed 169 --envs 6 --steps 750000
  --every 62500 --lr 3e-6 --log-std -2.5257`; its first checkpoint,
  32562500 (62,496 decisions on), is committed. 3e-6 is a smaller step than parts 9 and 10's 1e-5:
  every run at 1e-5 or more went on, past about 750,000 decisions, to touch
  short of the threshold.
- Tried and not chosen from: seed 165 at 3e-5 from step31750004 (as part 8;
  3 million, twelve checkpoints, none within the margin and every one from
  32250002 on touching short at the corners); seed 166 at 1e-5 from that
  run's last (not flown for choosing, its parent having gone short); and
  seed 168 at 1e-5 from part 11's 32500004 (twelve checkpoints, none within
  the margin).
- **Run again**, parts 11 and 12 each to the chosen checkpoint: the vecnorm
  files' sha256 are equal, and the exported weights and normalisation are
  the same to the last bit; the zips' bytes differ (their archive metadata).
- Training, choosing and the runs again took about 75 minutes.

**Chosen with margin**, by the rule of 2026-10-20: the first checkpoint
whose every landing - the forty held-out starts at 25, 50, 75 and 100 lb a
tank, the verification's 27 at each, and the gate's 160 corners
(evaluate.py and corners.py, the bindings) - is at least 5 per cent inside
each limit: under 285 ft/min, within 4.75 m of the centreline, at least 15
m past the threshold. None of part 11's twelve met it (32500004, the
nearest, sank just under 300 ft/min at a corner with the wind behind);
32562500 is the first of part 12's.

**Before and after** - the committed policy (part 10) and this one (part 12),
both on her as committed. Sink and across from the C++ tests' printed
figures in linux-release; the least distance past the threshold from the
bindings, which agree with the simulation to 4e-15:

| test | before: sink, across, least past threshold | after |
|---|---|---|
| verification, full tanks, calm and 10 kt across | 117 ft/min, 1.08 m, 105 m | 128, 0.64, 232 |
| verification, 25 lb a tank | 161, 1.91, 259 | 181, 0.84, 330 |
| verification, 50 lb | 134, 1.86, 201 | 166, 0.72, 300 |
| verification, 75 lb | 105, 1.54, 156 | 148, 0.65, 268 |
| gate's corners, calm | 77, 0.98, 103 | 102, 0.61, 232 |
| gate's corners, across from the left | 138, 2.83, 86 | 136, 0.77, 241 |
| gate's corners, across from the right | 215, 1.30, 147 | 225, 1.18, 207 |
| gate's corners, ahead | 178, 3.14, 20 | 159, 0.62, 162 |
| gate's corners, behind | 157, 2.62, 105 | 281, 2.32 (2.31 in the bindings, as the policy's header gives it), 208 |
| (choosing only) held-out, worst of four loads | 221, 4.68, -65 (5 short) | 236, 2.34, 54 |

**From the verification's 27 starts at full tanks** (the parity fixture's
flights): it now touches 232 to 280 m past the threshold, 82 to 139 m
further than the committed policy did on her as she is (105 to 152 m), and
25 to 88 m further than it did on her of 2026-10-20 (168 to 209 m) - nearer
its aim of 300 m; it sinks 83 to 128 ft/min and touches within 0.64 m of the
centreline. The longest touchdown at the corners is 321 m along the 3,000 m
runway. The tests' limits are unchanged.

**Exported** with export.py from the chosen checkpoint, its parity fixture
(tests/data/rl/c172p-landing-parity.txt) from the same run: 106 cases,
agreeing to 3.66e-15 in an observation and 1.44e-15 in an action in
linux-release.

**Verification.** With part 12's policy: the 28 tests named learnt and the
two rudder tests green in linux-release (the figures above). In an
`-march=x86-64-v2` release build (no fused multiply-adds, as CI builds; no
`vfmadd` in its test program: `cmake --preset linux-release -B
build/linux-release-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2
-DCMAKE_C_FLAGS=-march=x86-64-v2`), with only the test program and the CLI
built, the 27 of them those fly are green with the same printed figures,
and the parity test agrees to 0; the three that need the server or the
window client were not built there. The full linux-release suite (`-LE
nightly`, `ctest -j4`, no display), with the policy as committed here:
1423 of 1424 passed (24 of them skipped: no key, device or service), 1
failed - `the_google_terrain_draws_with_its_attribution_or_says_why_not_on_vulkan`
("google via ion-token drew no terrain at all", again when run alone), the
Google tile service's 429 refusal that #294 handles, not this change.
