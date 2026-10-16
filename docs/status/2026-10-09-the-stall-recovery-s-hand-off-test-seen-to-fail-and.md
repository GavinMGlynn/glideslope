### The stall recovery's hand-off test seen to fail, and registered with ctest, 2026-10-09

With the owner's approval (2026-10-09), the deliberate bug the agent could not
plant was planted by hand: the autopilot's elevator left unpaced
(`src/sim/autopilot.cpp`, the `toward(..., a_hands_pace)` on the elevator taken
out). `engaging_the_stall_recovery_and_letting_it_go_moves_no_control_faster_than_a_hand`
went red - "18 flights stepped a control or were not let go; the first: 737-300
at its warning moved a control 0.050404 in a step, against 0.008333" - and green
again with the line restored. The test was compiled into `glideslope_tests` but
missing from `GLIDESLOPE_UNIT_TESTS`, so ctest never ran it; it is registered
now, and `every_compiled_unit_test_is_registered_with_ctest` passes.
