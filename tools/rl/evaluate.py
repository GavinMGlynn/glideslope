"""Flies a policy file from the verification's starts, in JSBSim's Python
bindings, and prints each landing - the same 27 starts, and the same limits,
as tests/unit/test_learnt.cpp flies in the simulation.

    ~/.venvs/glideslope-rl/bin/python tools/rl/evaluate.py assets/rl/c172p-landing.txt
"""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import landing as L
import policy_file
from env import Flier


def fly(policy: policy_file.Policy, start: L.Start, flier: Flier | None = None,
        record=None) -> L.Flight:
    flier = flier or Flier(L.Runway(), policy.approach)
    obs = flier.begin(start)
    while True:
        if record is not None:
            record(flier, obs)
        obs, _, done, _ = flier.decide(policy.act(obs))
        if done:
            return flier.flight


def within_limits(f: L.Flight, runway: L.Runway) -> list[str]:
    """What is wrong with a landing, by the autopilot's limits: nothing, for
    one that touched on the runway within 5 m of the centreline sinking under
    300 ft/min, and stayed down and upright for five seconds after."""
    wrong = []
    if not f.touched:
        wrong.append("never touched: " + f.ended)
        return wrong
    if not f.touch.sink_fpm < 300.0:
        wrong.append(f"sank {f.touch.sink_fpm:.0f} ft/min")
    if not abs(f.touch.across_m) <= 5.0:
        wrong.append(f"{f.touch.across_m:.2f} m off the centreline")
    if not 0.0 <= f.touch.along_m <= runway.length_m:
        wrong.append(f"touched {f.touch.along_m:.0f} m along")
    if not f.highest_after_touch_ft < 3.0:
        wrong.append(f"rose {f.highest_after_touch_ft:.1f} ft after the touch")
    if not f.worst_roll_after_touch_deg < 15.0:
        wrong.append(f"banked {f.worst_roll_after_touch_deg:.1f} deg after the touch")
    if not f.least_pitch_after_touch_deg > -10.0:
        wrong.append(f"nose {f.least_pitch_after_touch_deg:.1f} deg down after the touch")
    return wrong


def main() -> int:
    policy = policy_file.read(sys.argv[1])
    rw = L.Runway()
    flier = Flier(rw, policy.approach)
    bad = 0
    worst_sink = worst_across = 0.0
    for name, start in L.verification_starts():
        f = fly(policy, start, flier)
        wrong = within_limits(f, rw)
        bad += bool(wrong)
        worst_sink = max(worst_sink, f.touch.sink_fpm)
        worst_across = max(worst_across, abs(f.touch.across_m))
        print(
            f"{name}: {f.touch.sink_fpm:5.0f} ft/min, {f.touch.across_m:+6.2f} m across, "
            f"{f.touch.along_m:5.0f} m along, pitch {f.touch.pitch_deg:4.1f}, "
            f"rose {f.highest_after_touch_ft:.1f} ft"
            + ("" if not wrong else "   <-- " + "; ".join(wrong))
        )
    n = len(L.verification_starts())
    print(f"{n - bad} of {n} within limits; worst sink {worst_sink:.0f} ft/min, "
          f"worst {worst_across:.2f} m across")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
