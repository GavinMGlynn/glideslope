### CI's test jobs given 45 minutes, not 30, 2026-10-08 — fix

A Linux debug shard of #125's run was cancelled at 30 minutes: the circuit
lesson took 627 s, its cost in the tables an estimate from before #123 changed
the circuit. New tests arrive with estimated costs, and one wrong estimate
cancelled a whole run. The Ubuntu, macOS and Windows test jobs' limit is now
45 minutes; the shard counts still aim each shard at about 15 minutes, so a
wrong estimate slows a run instead of failing it. Measured costs from a green
run should replace the estimates (tools/ci_test_costs.py).
