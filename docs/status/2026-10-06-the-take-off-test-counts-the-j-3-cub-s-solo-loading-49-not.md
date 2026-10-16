### The take-off test counts the J-3 Cub's solo loading: 49, not 48, 2026-10-06 — fix

The engines work (#100) gave the Cub a "solo" loading, and
`every_landplane_takes_off_at_every_loading_within_ten_knots_of_its_speed_for_its_weight_and_unhurt`
still asserted 48 loadings, so main went red: #100 was merged after its rebase
without a fresh CI run. The count is now 49; the test passes in linux-debug.
Found by the Learjet batch (#99), which carried the same change.
