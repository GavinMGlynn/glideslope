### The autopilot holds the bank it is asked for, and the navigator's waypoint turns are done: every aircraft within 0.066 of her turn radius, 2026-10-13 — item done

**What is still missing, first.** Nothing of the item. Every one of the 32
ninety-degree fly-by turns is now within a tenth of the turn radius, and none
is named. No platform code changed, so this was not built on Windows.

**The bank hold** (src/sim/autopilot.cpp). The aileron's integral, which finds
the aileron a bank needs held, used to work only at the bank limit. Below the
limit every bank asked was held short. Asked for 15 degrees, the Cherokee at
74 kt was 4.15 short, the 182S 3.04, the Mosquito 3.05 and the S.23 2.83. A
loiter-law arc, flown by a bank asked, was therefore flown wide. Now:
- The integral works at any bank of 5 degrees or more, once the bank
  commanded has caught up with the bank wanted.
- It is not wound further while the aileron is at its stop the way the error
  asks (anti-windup).
- It does not run while the bank asked is falling towards level faster than
  0.5 degrees a second (`steady_bank_degps`). Without that gate the Mosquito
  turned 4.29 degrees past a heading against the 4 allowed. With the gate
  she turns 3.78, against 3.45 under the old law.
- The gate is one-sided. Gated on any change of bank, either way, the arc's
  correcting bank lost its integral and the Cherokee strayed 0.092 r, not
  0.066.
- Wings level the integral is still off, as before, because there it chased
  turbulence and a stall's wing drop.
- There is no tuning for any one aeroplane.
- `Autopilot::bank_limit_deg()` exposes the bank limit for the test.

**New test**
`every_aircraft_asked_for_a_steady_bank_of_15_or_25_degrees_holds_it_within_a_degree`
covers every aircraft at her climb and cruise speeds, at 15 and 25 degrees
each way, level at 5,000 ft. The bank is held within a degree from 40 s to
60 s, or the autopilot's lesser limit where a full-throttle turn has spent
what it may; no case reached a lesser limit. That is 128 cases, asserted, and
the run takes 8 s.
- **Seen to fail on the old law**: 16 cases, all at 15 degrees, from the
  747-400's 1.02 to the Cherokee's 4.15.
- Now every case is held within 0.11 degrees.

**The fly-by turns, now** (stray / turn radius at 25 degrees; climb, then
cruise): 737-300 0.001, 0.002; 747-400 0.007, 0.004; 787-8 0.003, 0.004;
A320 0.001, 0.002; A380 0.002, 0.003; Learjet 0.001, 0.001; B-2A 0.003,
0.002; F-15C 0.005, 0.002; F-22A 0.004, 0.005; F-35B 0.010, 0.003; Mosquito
0.016, 0.013; S.23 0.021, 0.007; 172P 0.016, 0.011; 182S 0.022, 0.013; Cub
0.031, 0.008; Cherokee 0.066, 0.024. The Cherokee is no longer named in the
test, and every case is held to a tenth.

**The OpenAI coast recording was made again**, with one live call
(`GLIDESLOPE_LIVE_MODEL=1`, the owner's key read at run time). Its prompt
carries the flight's state, which the fly-by turns and the bank hold change.
It was recorded once, after the code was final. The new
`tests/data/copilot/coast-openai.jsonl` was scanned for `sk-`, `bearer`,
`authorization` and `api-key`, and for each key file's contents: none found.
`the_copilot_follows_the_coast_as_told_by_openai_as_recorded` plays it back
and passes.

**The selftest hash does not move**, `182dd6c996e0ee4c`: the selftest asks
for no bank.

**Verified**, linux-release, display unset:
- 488 tests by
  `ctest -j4 -R "orbit|loiter|turn|navigator|plan|approach|crosswind|turbulen|gust|lesson|bank|take_off|takes_off|takeoff|first_leg|stall|land|selftest|heading|copilot" -E "server|client|vulkan|window|shot|_now|coast_as_told_by_openai"`:
  all passed.
- Every `as_recorded` test, the server ones among them, one at a time
  (`ctest -j1 -R as_recorded`): 16 of 16 passed, the coast recording among them.
