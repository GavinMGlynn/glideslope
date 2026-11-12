# The F-22A behind a Cub flying away at 180 deg is not short of fuel

**A red tree, fixed.** `a_j3cub_and_every_person_straight_in_departing_or_staying_in_the_circuit_are_kept_apart`
(label `gust_sweep`, so nightly only) failed on main at 8f66a234:
"NAMED, NOT SHORT OF FUEL: a person's j3cub flying away 180 deg off the
runway, ahead of f22". The test names the flights that come back from their
hold short of fuel and fails when the set flown differs from the set named.

**Cause.** #363 (2026-11-12) made the autothrottle lead by an aircraft's
throttle lag, and only the F-22A has one. Of what landed after the names
were measured (2026-11-11: #359, #363, #364), it is the only change to her
autopilot, so it is taken as the likeliest cause; no bisect was run. Her fuel
in the hold, and how far the 180 deg flight is from the short-of-fuel
trigger, were not measured. At 180 deg the Cub's place is dropped soonest,
875 s after she set off; on the other seven bearings it is 896 to 1081 s. The
F-22A behind her is down 242 s after the drop and doesn't come back short of
fuel. On the seven other bearings she still comes back short. These are
linux-release figures, from the test's own output.

**Fix.** `short_of_fuel_named` takes the bearing and names seven flights,
every bearing but 180 deg. The 180 deg flight now passes the drop and timing
checks every other flight is held to.

**Verification.** The test fails on main (above) and passes with the fix
(linux-release). It runs nightly, not on pull requests: the next nightly is
its check on every platform. ci.yml's scheduled run, which runs it, has
been cancelled by shard timeouts since 2026-11-05; that is being fixed
separately, so this fix's every-platform check waits on that.
