# glideslope

A multiplayer flight simulator in C++: real flight physics and live wind, flown
over real-world terrain streamed from the internet. Fly yourself, or hand any
aircraft to an AI pilot and take it back.

> **Status: Phases 0, 1, 2, 3 and 3b are complete, and Phase 4, the autopilot
> and navigation, is under way.** A Cessna 172P flies to its handbook, stands on
> the real ground anywhere on Earth, and can be flown from the keyboard or a
> joystick with a HUD, in the weather reported at an airfield now - its gusts,
> turbulence, boundary layer, wind shear, thermals and waves, the same air on
> every machine, under the cloud and haze the report gives. The terrain is
> drawn, with satellite imagery on it, but only around where the flight starts.
> [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) is the single source of
> truth for what works, with the gaps named first.

## The idea

- **Real flight dynamics** from JSBSim, in real wind from live weather reports.
- **The real Earth**, anywhere on it: open terrain data by default with no
  account, and Cesium ion or Google Photorealistic 3D Tiles with your own key.
- **Up to four players** on a server that owns every aircraft, with controls
  that still answer on the frame you move them.
- **AI pilots** that take over any aircraft and hand it back — an autopilot
  first, flight plans next, and later a copilot you talk to.

It is not a scored or competitive game, and what it deliberately does not do is
in [`docs/FEATURES.md`](docs/FEATURES.md).

## Building

```sh
git clone --recurse-submodules https://github.com/GavinMGlynn/glideslope.git
cmake --preset linux-release      # or linux-debug, macos-*, windows-*
                                  # the first builds Cesium Native's dependencies
                                  # through vcpkg: most of an hour, once
cmake --build --preset linux-release
ctest --preset linux-release
```

What the build makes, today:

```sh
glideslope_cli figures c172p            # fly the Cessna's published figures
glideslope_cli selftest                 # a five-minute flight, and its hash
glideslope_cli height -33.9461 151.1772 # the ground's height, from the DEM
glideslope_cli weather YSSY             # the weather at Sydney airport now
glideslope --weather YSSY               # fly from over Sydney, in its weather
glideslope --screen terrain --at -39.55,174.27,500 --toward -39.50,174.10,1500 \
    --metar "METAR NZHA 180800Z 27012KT 6000 -RA BKN015 14/10 Q1016"
                                        # Taranaki under broken cloud, in rain
glideslope --screen origin              # a test scene, in a window
```

`cpack --preset linux-release` (or `macos-release`, `windows-release`) makes a
package that runs from wherever it is unpacked. Terrain data and weather are
downloaded when first needed, terrain into the user's cache directory (or
`GLIDESLOPE_CACHE`); on Linux that needs the system's libcurl, which almost
every distribution has.

## Data

The terrain is the Copernicus DEM, drawn and read as the program runs:
produced using Copernicus WorldDEM-30 © DLR e.V. 2010-2014 and © Airbus
Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European
Union and ESA; all rights reserved. The organisations in charge of the
Copernicus programme by law or by delegation do not incur any liability for any
use of the Copernicus WorldDEM-30.

The imagery is Sentinel-2 cloudless - <https://s2maps.eu> by EOX IT Services
GmbH (Contains modified Copernicus Sentinel data 2016), under CC BY 4.0.

Weather data by Open-Meteo.com (<https://open-meteo.com/>), under CC BY 4.0;
METARs from aviationweather.gov, NOAA's Aviation Weather Center. Every source's
terms are in [`docs/ASSETS.md`](docs/ASSETS.md), and every linked library's
licence is in `licenses/` in a package.

## The documents

- [`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md) — the design, and every
  decision made about it.
- [`docs/FEATURES.md`](docs/FEATURES.md) — what the simulator should be.
- [`docs/COMPLETION_PLAN.md`](docs/COMPLETION_PLAN.md) — the road to done, in
  phases, each item with its verification.
- [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) — what works today.
- [`docs/ASSETS.md`](docs/ASSETS.md) — where third-party data comes from, and
  its terms.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE). Third-party data — terrain, imagery,
weather and aircraft models — is under its own terms, recorded in
[`docs/ASSETS.md`](docs/ASSETS.md).
