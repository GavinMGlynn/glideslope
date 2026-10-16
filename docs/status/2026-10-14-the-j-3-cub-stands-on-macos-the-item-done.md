### The J-3 Cub stands on macOS: the item done, 2026-10-14 — docs only

`every_aircraft_standing_on_her_brakes_at_idle_stays_finite_and_on_the_ground`
passed on every platform, macOS release included, in CI run 38454086267 on the
stack that merged #188, #187, #191, #190 and #189 - the run that first showed
her flight going NaN on macOS (run 38447161811, `propulsion/engine/AFR` first,
her flight already blown up after going over on her nose under full brakes
with the stick central) now finds her standing. The fix was in how the tests
stand a tail-wheel aeroplane, stick back and pedals half on (#188); her model
is unchanged.
