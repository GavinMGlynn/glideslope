### Thermals, ridge lift and mountain waves, 2026-09-18 — item done (CI run 35336574855)

**What is missing first:** the thermals do not know the ground. They rise as
strongly over the sea, a lake or a north slope as over a sunlit field, from
the station's report alone, and nothing marks them - no cumulus, since nothing
draws cloud yet. The terrain's lift sees the ground only along the wind
through the aircraft, 32 km each way: a ridge parallel to the wind lifts
nothing, and the air across the wind is not in it. It is linear theory, so it
has no rotor, no hydraulic jump and no flow blocked by a mountain too high for
the wind to carry over it (N h / U well above 1), and with one wind and one
stability for the whole column it makes waves that rise away rather than lee
waves trapped under a stable layer.

**Thermals** (`world::thermal_updraught`) are Allen's updraft model
(NASA/TM-2006-213477, 2006), transcribed from the MATLAB in his appendix B:
his mean updraft and radius with height, his bell fitted to Konovalov's
measured updrafts, the ring of sinking air round each in the layer's upper
half, and the sink between them that his mass balance sets. Where he places
updrafts at random, one stands in each cell of a grid - his count per area,
taken at 0.4 of the layer's depth, sets its size, 470 m for a layer 1.4 km
deep - drawn from the weather's seed, 0.7 to 1.3 times his strength, living
twenty minutes and growing and fading over three at each end; the pattern
drifts with the wind halfway up the layer. The layer comes from the report: a
parcel 1 C warmer than the METAR's air, rising dry-adiabatically through the
forecast's temperatures until it is no warmer, up to 4 km; its convective
velocity 2 m/s for 1,500 m, with the cube root of the depth; none under 300 m
deep or in a surface wind over 25 kt, as in Allen's.

**Ridge lift and mountain waves** (`world::terrain_updraught`) are linear
theory: the terrain along the wind, 256 samples 250 m apart from the DEM,
transformed; each wavenumber a wave rising with height where it is longer than
2 pi U / N, and dying away where it is shorter; the vertical wind U times the
displaced air's slope. At the ground that is the wind up the slope - lift on
the windward side, sink in the lee - and above, waves leaning into the wind.
The wind is the report's 1,000 m above the station, the stability its lapse
between 1 and 4 km. The client gives the weather its DEM; the CLI's `air`
command compares a synthetic ridge's lift and a hot afternoon's thermals
across the platforms with the gusts and turbulence.

**The tests:** Allen's updrafts are the size his worked example gives and the
speeds his figure 10 shows, within 0.05 m/s; in its prime, each thermal of the
pattern is his model to 1e-12 m/s, and beyond its reach the air sinks at his
rate; over the pattern the air sinks as much as it rises within 15% (it is 3
to 8%); each thermal grows and fades smoothly, one after another; the
convective layer is the depth a parcel rises to, to 1e-6 m. The terrain's
lift is within 2.4% and 1.0% of the strongest wind over two ridges in stable
air - linear theory integrated in the test by quadrature, with nothing in
common with the code's transform - and within 0.1% of potential flow with no
stability. A Cessna gliding round a thermal at 65 KCAS in a 45-degree bank
climbs as it does in still air at each height, sinking faster higher up where
the air is thinner, and faster by the model's updraught along its path: 0.006
m/s off 2.5 m/s of lift, where the test allows 2%. Without a forecast's
temperatures aloft there are no thermals: the standard atmosphere's lapse
alone made a 303 m layer, and thermals at night. **Watched to fail:** the waves
leaning downwind, the transform unpadded, the thermals sinking, and no sink
between thermals - each caught by the test meant for it.

**Found on the way:** Queney's hydrostatic formula, the first reference, was
18% off the code over a ridge 5 km wide; the code is right. Its waves are
non-hydrostatic, and a ridge's slopes hold wavelengths short enough for that
to matter several kilometres up; linear theory integrated exactly agrees
within 2.4%. And the transform pads the terrain with as much level ground
again: a transform takes what it is given to repeat, and with the copies 64 km
away rather than 128, the narrow ridge's waves were 4.2% off rather than 1.0%.
