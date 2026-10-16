### Gusts and turbulence: the same air on every machine, 2026-09-18 — items done (CI run 35331164089)

**What is missing first:** gusts and turbulence are the wind's: nothing yet
makes wind shear near the ground (the boundary layer is the next item), a
microburst, a thermal or a wave. The turbulence is translation only: Dryden's
model also turns an aircraft - rates in roll, pitch and yaw from the air's
gradients - and that is not given to JSBSim yet. Its spectrum's shape along a
line is not exactly Dryden's (below). And the weather's patterns are drawn on
a flat Earth about the reporting station, so they are the weather of a few
hundred kilometres around it.

**The air is a pattern the wind carries** (`world/air_motion.hpp`): gusts and
turbulence are functions of place, time and a seed the report carries, with no
state - so every machine with the report flies the same air, and an aircraft
restored to a state meets the air it left. The seed is made from the station
and the observation's time (`world::air_seed_of`).

**Gusts:** the wind along its direction rises from a METAR's mean speed
towards its gust and falls back, as smooth noise over 4 to 16 seconds, in full
up to 10 m above the station and fading to none 600 m above that. **Turbulence**
is MIL-F-8785C's Dryden model - its lengths and intensities by height, with a
table of intensities by severity above 2,000 ft - made as a sum of cosine
waves; a report's severity is judged from its gusts' spread over its mean wind
(5 kt light, 15 moderate, 30 severe), unless it gives one. For reported weather
JSBSim's own turbulence is now off; `sim::Weather` can still ask for it, and
the Phase 3 test of it stands.

**The tests.**
- The same air: two weathers from each of three recorded gusty reports -
  Edinburgh, Albuquerque in a thunderstorm, Mount Washington - give exactly the
  same wind at a thousand places and times, asked in either order and asked
  again; another seed is other air.
- An aircraft restored in a gust: its first step, from the same place at the
  same time, meets the original's wind within a millionth of a foot a second,
  and every difference after that is the weather's between where the two are,
  to 1e-9 - restoring is not exact, and Dryden's air differs by a foot a second
  over a metre.
- Gusts flown: ten minutes at each station's anemometer height stay between the
  mean and the gust and reach both within half a knot; 700 m up, none; a report
  without gusts gives a steady wind; a minute's flight through Albuquerque's
  gusts, twice, ends in the same place.
- Dryden: MIL-F-8785C's lengths and intensities at 100 and 5,000 ft; each
  component's RMS within 12% of its intensity along 200 km at four heights;
  and, over four seeds, three directions and two heights, the distance at which
  its correlation falls to 1/e within 15% of Dryden's length.
- Across platforms: `glideslope_cli air` prints the air at a thousand places and
  times in whole 1e-11 m/s, and CI's cross-platform job holds every platform
  within 1e-9 m/s of the first (`tests/cmake/cross_platform_flights.cmake`,
  whose test now also refuses one sample 1e-8 m/s off).

**Watched to fail:** hidden state - the air nudged by how many times it had
been asked (three tests); gusts overshooting the gust by 30%; the severity
table read a row off; the waves left full length (correlated to 2.53 of
Dryden's length). The rescaling of the waves to Dryden's intensity was also
removed, and nothing failed: the waves already make nearly Dryden's variance,
and the rescaling corrects only what the 2 m to 50 km band leaves out.

**Why the waves are shortened.** Waves in every direction cross a line at an
angle, so along it they are longer than they are: the first version's
turbulence stayed correlated 2.5 times Dryden's length along the wind and 1.4
times across and vertically. Each wave is shortened by those measured factors,
2.47 and 1.40, which brings the average back to Dryden's length; any one line
still varies from 0.8 to 3 times, as 48 waves sample a spectrum.

**Also:** the plan's first verification for this asked for bit-identical air on
every platform, which floating point across compilers cannot promise, and was
amended to 1e-9 m/s before this was built.
