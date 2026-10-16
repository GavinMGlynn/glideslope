### Standing on the DEM, 2026-09-18 — item done

**What is missing first:** only the tests connect the simulation to the DEM;
no program flies over it yet, and the selftest and the published-figure checks
still stand on JSBSim's own level ground. Nothing has flown into terrain: this
is ground contact for wheels, and whatever JSBSim does with the rest of the
airframe.

**`sim::Terrain`** (`sim/terrain.hpp`) is the ground the simulation stands on:
heights above the WGS84 ellipsoid at a latitude and longitude, from anything
- the frontends will give it the DEM. `Aircraft::set_terrain` puts a JSBSim
ground callback in place of JSBSim's level ground: the contact point straight
below, at the terrain's height, and the surface normal from the slope by
central differences 15 m to each side. `AircraftState` now reports height
above the ground and the terrain's elevation.

**A real bug, found by it.** `InitialConditions` latitude was given to JSBSim's
geocentric setter, while every latitude here - and the state JSBSim reports -
is geodetic. At 45 degrees that started an aircraft 0.19 degrees, 21 km, north
of where it was asked; at Sydney, where the figures and the selftest start,
about 20 km. It is now geodetic. The nine figures stay in range; the selftest
ends somewhere slightly different, and its hash is different.

**The tests.**
- Level terrain through the callback, at sea level and at 1,656 m, holds the
  Cessna exactly as JSBSim's own ground does: the same height, attitude and
  strut compression to within a millionth.
- On a 10% slope, facing up it and down it, the Cessna rests on all three
  wheels, its centre of gravity 4.39 ft above the slope as 4.36 ft above level
  ground, pitched by the slope's angle plus what its struts add - worked out
  from their measured compressions over the wheelbase - to within 0.1 degree.
- Engine stopped and brakes off, it rolls down that slope at over 5 kt in ten
  seconds, and stays still on level ground.
- On the real DEM, through the downloaded tiles and the geoid: at Boston's
  runway 33L (4.8 m), Denver's 34L (1,624 m), and halfway up Courchevel's
  runway, the Cessna comes to rest on all three wheels; the ground JSBSim
  stands it on is the DEM's height where it stopped, to 0.01 ft; its centre of
  gravity is 4 to 5 ft above it; and at Courchevel it is tilted by the DEM's
  own slope under its wheels, within 0.5 degree.

**Setting down.** JSBSim's initial altitude places the centre of gravity:
set down at the ground's height, the wheels start four feet under it and the
struts throw the aircraft up. On level ground it settles; facing down a slope
it went end over end for half a minute. The tests set it down 4.4 ft up.

**Watched to fail:** the normal set straight up (the rolling test: 0.000 kt
down the slope - the slope and rest tests passed, which is why the rolling test
exists).

**Also:** CI said MSVC refuses `getenv` in the tests (now one helper in the
harness), and CMake on macOS needs Objective-C++ enabled at the top of the
project before any target; CI's Windows runner now finds lavapipe
(`vulkaninfo`: llvmpipe, Vulkan 1.4.354) through the registry.
