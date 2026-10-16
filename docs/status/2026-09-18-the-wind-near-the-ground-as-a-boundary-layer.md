### The wind near the ground as a boundary layer, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** where the METAR and the forecast disagree, the
shear between 10 and 80 m is their disagreement: at Sydney on 18 September the
observation was 6 kt from the south and the forecast's 80 m wind 8 kt from the
north-north-east, so the wind swings round in 70 m. The ground under the
profile is the station's, flat: the terrain's own height and roughness do not
shape it, and 3 cm - short grass - is the roughness everywhere.

**The profile** (`world::conditions_at`): the METAR's wind 10 m above the
station, where it is measured; below, falling logarithmically to nothing at a
roughness length of 3 cm - 72% of it 2 m up, about where a light aircraft sits
on its wheels; above, through Open-Meteo's winds 80, 120 and 180 m above the
ground, logarithmically in height between each; then linearly to its lowest
pressure level, and its levels above, as before. The forecast is now asked for
those winds near the ground (`world::open_meteo_url`), and a response without
them gives the old profile. `glideslope_cli weather` prints them.

**The test:** a recorded Open-Meteo response with the winds near the ground,
read as the response gives them; with Sydney's METAR, the wind at 80, 120 and
180 m is the forecast's, at 10 m the METAR's, at 30 m logarithmically between,
at 2 m the METAR's times ln(2/0.03)/ln(10/0.03), and still below 3 cm; and a
3-degree approach from 300 m to 15 m, flown by the test pilot, is given the
profile's wind at its height before every step, to 0.01 ft/s. **Watched to
fail:** linear rather than logarithmic between the heights (at 30 m); the
forecast's winds near the ground ignored (at 80 m).
