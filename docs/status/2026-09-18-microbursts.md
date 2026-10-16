### Microbursts, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** nothing places a microburst but whoever sets the
weather - a test, or `glideslope --weather STATION --microburst LAT,LON` - so
a thunderstorm in a METAR makes none. The model's column is steady: a real
burst's ring vortex, its tilt and its moving with the storm are not in it. It
fades above 1 km, where the model's column would fall forever, and there it is
no longer mass-conserving.

**The model** (`world::microburst_wind`) is Oseguera and Bowles' downburst
(NASA TM-100632, 1988): a column falling within about a radius of its centre,
spreading along the ground, fastest some 70 m up about 1.1 radii out; with this
project's constants - an outflow depth of 200 m and a ground layer of 30 m - a
10 m/s downdraught of 1 km radius spreads at up to 11 m/s each way, a 44 kt
change from headwind to tailwind across it. It is a function of place and time,
growing over two minutes and fading over two, so it is the same air on every
machine; a `WeatherReport` carries any number, and the client's refreshed
reports keep them.

**The test:** at a thousand places the field is the model's formulas, written
out again in the test; its divergence is nothing to 1e-7 per second at 500
places - what falls spreads; it is nothing before it starts and after it ends,
and half grown at a minute; and down a 3-degree approach to Sydney through a
burst 2.5 km before the runway, the wind at every 10 m is the model's to 1e-9,
the headwind strongest before the centre, the downdraught at it, the tailwind
after. **Watched to fail:** the downdraught turned to an updraught.
