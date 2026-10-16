### Weather you can see, 2026-09-18 — item done (CI run 35363959900)

**What is missing first:** the cloud is flat. Each deck is two sheets - its
base, grey, and its top, white - over a disc 60 km across the station,
curved with the Earth, cloudy where its pattern says; there are no cloud
volumes, towers or shading, and a cumulonimbus is a deck 6 km deep. The cloud
does not drift with the wind, and a new report makes the sky again rather
than blending into it. Rain and snow are streaks and flakes in a box 40 m
across about the eye.

**What is read** (`world::parse_metar`): the visibility, in metres or statute
miles - 9999, CAVOK, 10SM, 1 1/2SM, M1/4SM, P6SM; the weather present, with
intensity, vicinity, descriptor and phenomena; and the cloud - FEW, SCT, BKN
and OVC with their heights and CB or TCU, VV, and SKC, CLR, NSC and NCD. Held
to aviationweather.gov's own decoding of all eighteen recorded reports.

**What is drawn** (`world/sky`, `gfx::Sky`): a deck for each layer at the
report's height above the station, 300 m deep (2 km for towering cumulus, 6
for cumulonimbus), cloudy over the middle of its eighths - FEW 1.5, SCT 3.5,
BKN 6, OVC 8 - in a pattern of the weather's seed, the same on every machine;
inside cloud, a whiteout. The haze is Koschmieder's law in the shader: what is
seen through air of visibility V keeps exp(-ln 20 d / V) of its contrast with
the haze, V the report's in a layer from the ground to 1,000 m above the
station or the lowest cloud, 40 km above it, integrated along each fragment's
path through the layer's top. Rain falls at 7 m/s, drizzle at 3, snow at 1,
below the lowest cloud. The flight draws its report's sky; the terrain screen
takes `--metar REPORT` and `--station LAT,LON`; `glideslope_cli sky` says what
a report shows over a station, and where its cloud is thick and where it has
gaps.

**The tests:** the METARs' new groups, against aviationweather.gov; a report's
decks, visibility and precipitation; each deck's pattern covers its eighths
within 0.03 of the sky; and frames on every driver - under broken cloud at
1,500 ft over Hawera, cloud overhead 30 m below the base (100% of the frame)
and the ground below (54%), a whiteout 30 m above it (100%), clear sky in a
gap (100%): the base within 30 m of where the test, from the DEM's ground and
1,500 ft, puts it. Through 3,000 m of mist from Taranaki's slope the frame
matches the DEM ray-cast faded by Koschmieder's law to 0.20 of 255 on average,
ground beyond 3 km within 6 of the haze's colour and ground within 1 km 55
from it. Heavy rain changes 0.7% of a frame, every changed pixel lighter.
**Watched to fail:** a deck's base read in metres rather than feet, the
whiteout gone, the visibility doubled, the rain gone, and a deck's cover
inverted - each caught by the test meant for it.

**Found on the way:** from 30 m below a broken deck, looking up sees only 150
m of it, so whether the frame shows cloud depends on where the eye is under
the pattern. The frame tests take their places from `glideslope_cli sky`,
which finds them from the same pattern; the base's height they compute for
themselves. And the whiteout was first shot looking straight up, where the
deck's top sheet alone fills the frame as evenly as a whiteout: with the
whiteout taken out, that still passed. It is shot looking level now, where
between the sheets the base shows below and the top above; the rain was
first broken so it would not compile, which tested nothing, and was broken
again so it did. Sydney's calm weather this afternoon left the HUD test's
Cessna zooming at 32 degrees of pitch at its shot, its horizon line drawn
across the credits, which then could not be read back: the credits are now
drawn last, over a strip that darkens what is behind them by half, so they
can be read over anything - the Copernicus notice must be.
