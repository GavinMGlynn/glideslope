### A start on the ground puts the wheels on it, 2026-09-19

Found starting the airliners. An aircraft started on the ground had its
centre of gravity set at the ground's height, which left its wheels as far
under the surface as they hang below it: four feet for the Cessna 172P,
which its struts threw back into the air, and nine for the A320, which they
threw hard enough to end its flight in NaNs on its first seconds. The ground
tests had worked around it for the Cessna alone, with a rest height of 4.4
ft set by hand. `Aircraft::initialize` now raises an aircraft started on the
ground by its deepest wheel's compression, so that wheel starts touching and
the aircraft settles onto its struts, whatever the aircraft and whatever the
ground; the ground tests set the Cessna down at the surface itself, on level
ground and on slopes. Every published figure flown from the ground lands
where it did - the brakes held each aircraft for eight seconds or more, long
enough for the bounce to die - but for the Mosquito's take-off over 50 ft,
2,516 ft to 2,520, and its swing, 2.59 lb/sq in to 2.58. The selftest, which
starts on the ground, has a new hash for it: on this machine,
`e999a51640cdff03` to `d36123c1eecc3e23`.
