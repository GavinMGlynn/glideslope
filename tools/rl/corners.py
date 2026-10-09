"""Flies the learnt landing's gate's 160 corners - the 32 corners of
sim::LearntGate in its five edge winds, as tests/unit/test_learnt.cpp flies
them - in JSBSim's Python bindings, and reports each one the stabilized
gate would send round (outside +10/-5 kt of the approach speed between 500
and 50 ft, or bound past the touchdown zone, two seconds running, as
sim::unstabilized judges) and each landing outside evaluate.py's limits.
For choosing a checkpoint on the gate; the C++ tests are the verification.

    ~/.venvs/glideslope-rl/bin/python tools/rl/corners.py CHECKPOINT.zip|POLICY.txt [--jobs N] [-v]

The corners' numbers are LearntGate's, copied: a change there is a change
here.
"""
import math
import os
import sys
from concurrent.futures import ProcessPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import landing as L  # noqa: E402
import policy_file  # noqa: E402
from env import Flier  # noqa: E402
from evaluate import within_limits  # noqa: E402

VREF = 59.8
NEAREST, FURTHEST = 1.6 * 1852.0, 2.4 * 1852.0
WINDS = {"calm": (0.0, 0.0), "left": (15.0, 0.0), "right": (-15.0, 0.0),
         "ahead": (0.0, 8.0), "behind": (0.0, -5.0)}


def corners():
    out = []
    for wname, (cross, head) in WINDS.items():
        for c in range(32):
            e = lambda bit, lo, hi: hi if c & (1 << bit) else lo  # noqa: E731
            s = L.Start(out_m=e(0, NEAREST + 5, FURTHEST - 5), across_m=e(1, -59.5, 59.5),
                        high_m=e(2, -19.5, 19.5), heading_offset_deg=e(3, -4.9, 4.9),
                        airspeed_kts=e(4, VREF - 3 + 0.2, VREF + 8 - 0.2),
                        crosswind_kts=cross, headwind_kts=head)
            out.append((f"{wname} c{c:02d}", s))
    return out


def load(path):
    if path.endswith(".zip"):
        import export
        return export.from_checkpoint(path, [])
    return policy_file.read(path)


POLICY = None


def one(arg):
    global POLICY
    path, name, s = arg
    if POLICY is None:
        POLICY = load(path)
    p = POLICY
    flier = Flier(L.Runway(), p.approach)
    obs = flier.begin(s)
    run = most = 0
    worst_fast = worst_slow = -99.0
    at500 = None
    zone_bad = False
    while True:
        obs, _, done, _ = flier.decide(p.act(obs))
        r = flier.readings
        w = L.where(r, flier.rw)
        ft = w.above_m * L.FEET_PER_METRE
        if not flier.flight.touched and ft <= 500.0:
            if at500 is None:
                at500 = r[9] - VREF
            bad = False
            if ft >= 50.0:
                over = r[9] - VREF
                bad = over > 10.0 or over < -5.0
            touches = -w.along_m + max(0.0, w.above_m) / math.tan(3.0 / L.DEGREES)
            if touches > 914.4:
                bad = True
                zone_bad = True
            run = run + 1 if bad else 0
            most = max(most, run)
        if not flier.flight.touched and 50.0 <= ft <= 500.0:
            worst_fast = max(worst_fast, r[9] - VREF)
            worst_slow = max(worst_slow, VREF - r[9])
        if done:
            break
    f = flier.flight
    wrong = within_limits(f, flier.rw)
    return name, most / 10.0, worst_fast, worst_slow, at500, zone_bad, f.touch.sink_fpm, \
        f.touch.across_m, f.touch.along_m, wrong


def main():
    path = sys.argv[1]
    jobs = int(sys.argv[sys.argv.index("--jobs") + 1]) if "--jobs" in sys.argv else 4
    args = [(path, n, s) for n, s in corners()]
    with ProcessPoolExecutor(jobs) as ex:
        res = list(ex.map(one, args))
    unstab = short = 0
    wf = ws = sink = acr = 0.0
    for name, run_s, fast, slow, at500, zb, sk, ac, al, wrong in res:
        u = run_s >= 2.0
        unstab += u
        short += bool(wrong)
        wf, ws, sink, acr = max(wf, fast), max(ws, slow), max(sink, sk), max(acr, abs(ac))
        if "-v" in sys.argv or u or wrong:
            print(f"{name}: at 500 ft {at500 if at500 is not None else float('nan'):+5.1f} kt, "
                  f"fast {fast:+5.1f} slow {slow:+5.1f}, unstab {run_s:4.1f} s{' ZONE' if zb else ''}; "
                  f"{sk:4.0f} fpm {ac:+5.2f} m {al:4.0f} m {'; '.join(wrong)}")
    print(f"{len(res)} corners: {unstab} unstabilized (2 s), {short} short; most fast {wf:+.1f} kt, "
          f"most slow {ws:+.1f} kt (50-500 ft); worst sink {sink:.0f} fpm, across {acr:.2f} m")


if __name__ == "__main__":
    main()
