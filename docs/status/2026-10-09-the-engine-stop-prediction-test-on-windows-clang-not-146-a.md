### The engine-stop prediction test on Windows clang: not #146, a failure that was there before it, 2026-10-09 — found, not fixed

**What is still wrong, first.** `a_client_predicting_its_aircraft_stops_its_engine_when_the_server_says_and_is_put_right_no_more`
fails on CI's Windows clang runners when its server is held up: the
client, pacing itself well past its own clock to catch up, is put right by
metres, and the median speed error after the engine stops misses its 5 cm/s.
Not fixed; a plan item of its own.

**Evidence that #146 does not cause it.**
- It failed twice on #146's rebased head (7b2096e9, run 37891045143,
  windows-clang tests 1/3, attempts 1 and 2): 1,387 and 874 tenths of a
  mm/s, the client "paced: flown at 1.643 of this machine's clock at the
  end; the clocks' difference 43 steps from the one held then, 63 at
  worst", worst prediction error 38 m - a server held up, not physics.
- The same failure, before #146 existed: run 37798907886
  (readme-refresh-2026-10-09b, 23ccd442, 2026-10-08, which contains none of
  #146's commits), windows-clang tests 1/3: 962 tenths of a mm/s, "paced:
  flown at 1.690 of this machine's clock". The test also failed on Windows
  clang in runs 37732259070, 37691152903 and on main (37686044346, 2026-10-07)
  under its earlier bounds.
- The same #146 code passed it on Windows clang in the stack above it
  (separation-between-layers, fdfcbf31, run 37891046392, tests 3/3, 37.3 s),
  and on the Windows development machine (windows-release; this machine has
  no clang-cl for the windows-clang preset) at 7b2096e9: passed, 37.6 s.
- Nothing #146 changes is flown in it: the player's C172P is flown by the
  pilot's inputs and the AI's by the plain autopilot; the gate, the jets'
  approach idle and the go-around's incidence are the approach lander's.

Every failure is in a shard where the server fell behind; which shard the
test lands in moves as tests are added. The cause - why a held-up server
leaves the prediction off by metres when the take-over test's fix (inputs
flown from the step due when they came) was meant to stop it - is the plan
item's.
