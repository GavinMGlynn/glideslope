### The fixed test ports' block widened to 24700-24999, 2026-10-08 — fix

The weather tests of #124 took ports 24910-24920, past the block's end at 24899,
and `every_fixed_test_port_lies_outside_the_ephemeral_ranges_and_no_two_tests_share_one`
went red once #124 was rebased onto #122. The block is widened to 24999 - still
below every platform's dynamic range (32768 on Linux, 49152 on macOS and
Windows). The check passes in linux-debug.
