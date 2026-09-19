# Assets

**What this file is.** Where every piece of third-party data this project uses
comes from, which version, and under what terms — aircraft models, terrain,
imagery, weather data, fonts and sound. The code licence (GPL-3.0-or-later) does
not cover any of it; each source's own terms do.

**Fourteen things are used: Cessna 172P and 182S, Piper PA-28-180 and Piper
J-3 Cub flight models derived from JSBSim's, the four aircraft's handbooks'
published figures, the Mosquito FB Mk VI's trials
and Pilot's Notes (for a flight model written here), the Copernicus DEM, the
EGM2008 geoid grid, EOX's Sentinel-2 cloudless imagery, METARs from
aviationweather.gov and winds aloft from Open-Meteo** - the DEM, the geoid and the imagery fetched
as they are needed, the weather when it is asked for, with one recorded
response of each weather service committed for the tests. No visual model,
font or sound is used or fetched yet.

## The rule

An asset is recorded here **in the commit that first uses it**, with its source,
its pinned version (a commit, a dataset release, or a URL plus SHA-256), its
licence, and the attribution text it requires. Terms are quoted from the source
at that point, not paraphrased from memory.

## In use

### The Cessna 172P flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/c172p/c172p.xml`, `engine/prop_75in2f.xml`, `engine/eng_io320.xml` |
| Changes | Made by `tools/make_c172p.py`, whose docstring lists each change and the published figure it answers: the propeller's power and thrust coefficients, zero-lift drag and drag with angle of attack, the lift curve up to the stall, and flap lift. The engine file is unchanged. |
| In the repository | `assets/jsbsim/`, as the script makes it; a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |
| Where it goes | Copied at configure time into `data/jsbsim/` beside the programs, in the build tree and in every package |

The model file's own header says: "This model was created using publicly
available data, publicly available technical reports, textbooks, and guesses.
It contains no proprietary or restricted data. If this model has been validated
at all, it would be only to the extent that it seems to "fly right", and that it
possibly complies with published, publicly known, performance data (maximum
speed, endurance, etc.). Thus, this model is meant for educational and
entertainment purposes only."

### The Cessna 172P's published figures

| | |
| --- | --- |
| Source | Cessna Model 172P Pilot's Operating Handbook, 12 May 1981 (the Island Enterprises reprint): section 1 specifications, section 2 powerplant limitations, figures 3-1, 5-1, 5-3, 5-5, 5-6 and 5-8 |
| In the repository | `assets/figures/c172p.xml`: individual numbers, each with its section or figure, not the handbook's text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Cessna 182S flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/c182/c182.xml`, `engine/prop_81in2v.xml`, `engine/engIO540AB1A5.xml` |
| Changes | Made by `tools/make_c182.py`, whose docstring lists each change and why: the handbook's empty weight, seats and tanks; flaps to 38 degrees; main wheels that no longer castor; the lift curve for the stalls; the elevator's and incidence drag; a stopped engine's friction; the propeller's thrust at low advance ratio. The engine file is unchanged. |
| In the repository | `assets/jsbsim/`, as the script makes it; a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Cessna 182S's published figures

| | |
| --- | --- |
| Source | Cessna Model 182S Skylane Information Manual, P/N 182SIM, 1997 (the Pilot's Operating Handbook of 3 February 1997 with revision 4 of 1 November 2001), page ii and figures 3-1, 5-1, 5-4, 5-6, 5-7 and 5-9, as copied at <http://tssflyingclub.org/documents/C182S_POH.pdf>; FAA type certificate data sheet 3A13, revision 66, section XIII, for the static rpm and the flaps' travel |
| In the repository | `assets/figures/c182.xml`: individual numbers, each with its page or figure, not the handbook's text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Piper PA-28-180 Cherokee flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/pa28/pa28.xml`, `engine/engIO360C.xml`, and `engine/prop_75in2f.xml` for its propeller |
| Changes | Made by `tools/make_pa28.py`, whose docstring lists each change and why: the handbook's empty weight, seats and tanks; flaps at 10, 25 and 40 degrees; main wheels that no longer castor; a fixed-pitch 76 in propeller in place of the model's constant-speed one, with its power and thrust at low advance ratio; the 172's gear springs and dampers; the stabilator's moment and lift; the lift curve and flap lift for the stalls; induced drag with ground effect; zero-lift and gear drag. The engine file is unchanged. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/pa28/pa28.xml`, `engine/engIO360C.xml`, `engine/prop_pa28_76in.xml`); a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Piper PA-28-180 Cherokee's published figures

| | |
| --- | --- |
| Source | Piper Cherokee 180 "E" Owner's Handbook, P/N 753 806, issued October 1969, revised January 1974, sections I, III and V, as copied at <https://www.coyoteflight.com/resources/Aircraft_Manuals/Piper_PA-28-180E.pdf>; the Airplane Flight Manual, Model PA-28-180, FAA approved 3 August 1962, revision 4, for the calibrated stalling speeds, as copied at <https://www.nehemiahaviation.com/files/pa28flightmanual.pdf>; FAA type certificate data sheet 2A13, revision 64, section III, for the static rpm |
| In the repository | `assets/figures/pa28.xml`: individual numbers, each with its section, not the handbook's text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Piper J-3 Cub flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/J3Cub/J3Cub.xml`, with its `Engines/Continental A-65-8.xml`, `Engines/CM7445 MCCauley.xml` and `Systems/Conventional Controls.xml` |
| Changes | Made by `tools/make_j3cub.py`, whose docstring lists each change and why: the files renamed, without spaces; the booklet's empty weight and the type certificate's seats, baggage and tank; the type certificate's elevator travel; the airframe's drag at zero lift and a light aircraft's induced drag; the engine's rpm limit; and the propeller's diameter and tables. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/j3cub/j3cub.xml`, `aircraft/j3cub/Systems/conventional-controls.xml`, `engine/engA65-8.xml`, `engine/prop_j3cub_74in.xml`); a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Piper J-3 Cub's published figures

| | |
| --- | --- |
| Source | Piper's Owner's Manual for the J3C-65 (undated; a Wag-Aero reproduction), pages 11, 12, 41 and 47, as copied at <https://stpeteair.org/wp-content/uploads/cub_owners_manual.pdf>; Piper's booklet "How to Fly a Piper Cub" (1945), page 12, as copied at <https://home.adelphi.edu/~allendon/fly_a_cub.pdf>; FAA Aircraft Specification A-691, revision 34, section IV and propeller item 2, for the static rpm, the stations and the elevator's travel |
| In the repository | `assets/figures/j3cub.xml`: individual numbers, each with its page, not the manual's text |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Mosquito FB Mk VI: its trials, Pilot's Notes and engine curves

The flight model - `assets/jsbsim/aircraft/mosquito-fb6/`,
`assets/jsbsim/engine/merlin25.xml` and
`assets/jsbsim/engine/prop_dh_hydromatic.xml` - is written for this project
and is under its licence, GPL-3.0-or-later; no other simulator's Mosquito is
used. Its numbers come from these documents, and the figures it is held to
(`assets/figures/mosquito-fb6.xml`) quote them by paragraph or figure.

| Document | Copy consulted | What is taken from it |
| --- | --- | --- |
| A&AEE Boscombe Down report 767,e, 12th part, 1 February 1944: Mosquito FB Mk VI HX809, level speed performance at normal and increased boost rating | Scan at <http://www.wwiiaircraftperformance.org/mosquito/hx809.pdf> and `hx809-level.jpg` beside it, "courtesy Neil Stirling" | Level speeds and full-throttle heights at +18 lb/sq in; the loading; the propellers' type and diameter |
| A&AEE report 767,e, 7th part, 19 September 1943: Mosquito FB Mk VI HJ679, brief performance trials | Scan at <http://www.wwiiaircraftperformance.org/mosquito/hj679.pdf> and `hj679-climb.jpg` | Climb rates, time to 20,000 ft, the boost obtained, the loadings |
| Rolls-Royce, Merlin 24, 25, 26, 27, 224, 225 altitude performance (AVIA 6/5817, fig. 1) | Scan at <http://www.wwiiaircraftperformance.org/mosquito/merlin25-powercurve.jpg> | The engine's power with height at +18 and 3,000 rpm; the reduction gear, 0.42:1 |
| A.P. 2019E, Pilot's Notes for the Mosquito FB 6, 1950 edition | A transcription distributed by Zeno's Warbird Video Drive-In, <http://www.zenoswarbirdvideos.com> | Engine limits, the supercharger's gear change heights, fuel tanks, stalling speeds, safety speeds, single-engine performance, take-off handling |
| A.P. 2019E, Pilot's Notes, 1944 edition | Page scans at <https://www.fs2000.org/2004/02/15/pilots-notes-for-mosquito/> | The swing on take-off; stalling speeds at full load |
| Aircraft Data Sheet, Mosquito B Mk IV, card 3(b), 1 May 1944 | Scan at <http://www.wwiiaircraftperformance.org/mosquito/Mosquito_MkIV_ads.jpg> | Span, wing area, length; the take-off distance over 50 ft |
| Air Fighting Development Unit report 74, tactical trials, Mosquito VI | Scan at <http://www.wwiiaircraftperformance.org/mosquito/Mosquito-VI-tactical.pdf> | Loadings, for the empty weight's estimate |

All are Crown copyright documents of 1943 to 1950. **Only facts are taken** -
numbers, each cited where it is used, and a few words quoted with each figure
to say where its number is - not their text, charts or scans, none of which is
in the repository.

### The Copernicus DEM, GLO-30 Public

| | |
| --- | --- |
| Source | The Copernicus DEM GLO-30 Public, as Cloud Optimized GeoTIFFs in the AWS Open Data bucket `copernicus-dem-30m` (<https://copernicus-dem-30m.s3.amazonaws.com/readme.html>) |
| Version | The bucket names no release. Its objects are dated 2022-05-09; each tile's metadata gives its creation as 2019-10-19 and its heights as "WGS 84 Geoid EGM08". What is used is pinned file by file, by SHA-256 |
| Pinned | `tests/data/downloads/files.txt`: `Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif` (33-34 S, 151-152 E), 20,882,213 bytes, SHA-256 `6e20871096986cd00fc3903ea95670a0d83860236a0e4f1360ac9ac67def485d` |
| In the repository | No tile: the tests fetch the tile into the build tree, or the directory `GLIDESLOPE_DOWNLOADS` names. `assets/dem/coverage.txt` says which 1-degree cells have a tile at 30 m, only at 90 m, or none; `tools/make_dem_coverage.py` makes it from both buckets' `tileList.txt` (30 m: 1,110,900 bytes, SHA-256 `10604e3052c98a09e9216f1a8f0a555a04148419757575f783d4937fd44316dc`; 90 m: 1,111,950 bytes, SHA-256 `e5a5efe088e70506bc1007d22006bdcb09b0ec03177b62f9652363c13f49ed97`), and a test checks it still matches them |
| Licence | "Licence for Copernicus DEM instance COP-DEM-GLO-30-F Global 30m Full, Free & Open", published beside each tile as `INFO/eula_F.pdf` (SHA-256 `32049914c37f14e7d53b48d13d74a49e77c030236e2acc7a0df426f9344feba2`). It grants, free of charge, worldwide and without limit in time, "(a) reproduction; (b) distribution; (c) communication to the General Public; (d) adaptation, modification and combination with other data and information." |

Its Article 6, quoted:

> (a) When communicating to the General Public or distributing the Copernicus
> WorldDEM-30, the User shall inform the General Public of the source by using
> the following notice: © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH
> 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights
> reserved.
>
> (b) Where the Copernicus WorldDEM-30 data have been adapted or modified, the
> User shall provide the following notice: "produced using Copernicus
> WorldDEM-30 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH
> 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights
> reserved".
>
> (c) Users exercising the right of distribution or communication to the General
> Public of the Copernicus WorldDEM-30, modified or not, must ensure that the
> Subsequent Users understand that neither the Licensor nor any other legal
> entities in charge of the Copernicus programme or the delivery of Copernicus
> data and information under the Copernicus programme may be held liable with
> regard to any aspect of the Copernicus WorldDEM-30. The following sentence or
> its translation in any language shall be added by such Users in a licence or
> any legal warning or notice covering their distribution or communication to
> the General Public of the Copernicus WorldDEM-30: "The organisations in charge
> of the Copernicus programme by law or by delegation do not incur any liability
> for any use of the Copernicus WorldDEM-30".
>
> (d) User shall make sure not to convey the impression to the General Public
> that the user's activities are officially endorsed by the Provider, the
> Licensor or any other legal entities in charge of the Copernicus programme or
> the delivery of Copernicus data and information under the Copernicus
> programme.

**Its stated accuracy**, from the Copernicus DEM Product Handbook (version 5.0,
29 November 2022, <https://dataspace.copernicus.eu/sites/default/files/media/files/2024-06/geo1988-copernicusdem-spe-002_producthandbook_i5.0.pdf>,
SHA-256 `b5a0b027bddef7122e39de368dba9cf10adce8631a2e3026d246b4be98c3f0c4`),
for the EEA-10, GLO-30 and GLO-90 instances, quoted:

> Absolute Vertical Accuracy 1) 2) 3) < 4m (90% linear error)
>
> 3) Due to the global coverage of the TanDEM-X DEM / WorldDEM / Copernicus DEM,
> all accuracy statistics and values stated in this document are calculated as
> an arithmetic mean. Local deviations can occur.

**What that asks of glideslope, and where it is done.** The client draws
terrain made from the DEM - adapted, so notice (b) - and `glideslope_cli height`
prints heights from it. Both show notice (b) wherever the data are shown: the
client along the bottom of every frame that has terrain in it, as its other
credits are, and the CLI after the heights (`world::copernicus_dem_notice`).
The client's font has capitals, digits and a little punctuation and no
copyright sign, so on screen the notice is in capitals with "(C)" for "©"; a
test reads it back out of the frame. (c)'s sentence is in `README.md`, which
every package carries, with the notice. Nothing says or suggests the Copernicus
programme endorses this project, as (d) asks.

### The EGM2008 geoid, GeographicLib's 5-minute grid

| | |
| --- | --- |
| Source | GeographicLib's geoid distribution, `egm2008-5.zip` from <https://sourceforge.net/projects/geographiclib/files/geoids-distrib/>, listed on <https://geographiclib.sourceforge.io/C++/doc/geoid.html> |
| What it is | NGA's Earth Gravitational Model 2008 evaluated on a 5-arc-minute grid, 4320 by 2161 points, quantised to 3 mm; its header says "WGS84 EGM2008, 5-minute grid", dated 2009-08-29, with a maximum bilinear interpolation error of 0.478 m (RMS 0.012 m) |
| Pinned | `tests/data/downloads/files.txt`: `egm2008-5.zip`, 16,773,259 bytes, SHA-256 `408f05e0c04a9f2e17b9ea2d27123f936e9dea60128bb3411a272f8ddbe318dd` |
| In the repository | Nothing: fetched, as the DEM is |
| Use | Converting the DEM's heights above the geoid to heights above the WGS84 ellipsoid (`world/geoid.hpp`) |
| Licence | **Public domain**, as the model's. GeographicLib's geoid page and the zip state no terms of their own for the grids (of the library, GeographicLib's documentation says: "It is licensed under the MIT License; see LICENSE.txt."). The grid is NGA's EGM2008 evaluated by GeographicLib, and PROJ's data package, which redistributes NGA's EGM2008 as a grid GeographicLib produced, records it so; see below |

PROJ-data's `us_nga/us_nga_README.txt`
(<https://github.com/OSGeo/PROJ-data/blob/master/us_nga/us_nga_README.txt>),
quoted:

> ### Vertical grid: EGM2008 geoid model
>
> *Source*: [NGA](http://earth-info.nga.mil/GandG/wgs84/gravitymod/egm2008/egm08_wgs84.html)
> *Format*: GeoTIFF converted from GTX
> *License*: Public Domain
>
> 2.5 minute worldwide geoid undulation grid that transforms physical heights
> to WGS84 ellipsoidal heights.
>
> This file has been produced by [GeographicLib](https://geographiclib.sourceforge.io/html/gravity.html)
> using the EGM2008 gravity model

NGA's own page could not be read when this was recorded; the statement above
is PROJ's, about the same model.

### Surveyed heights the DEM is tested against

| | |
| --- | --- |
| In the repository | `tests/data/dem/surveyed.txt`: twelve runway ends, five coastal waters and five summits, each with its position, height and source |
| Runway ends | The FAA's airport data - surveyed runway-end positions and NAVD 88 elevations - as published by AirNav (`https://www.airnav.com/airport/<ID>`), read 2026-09-18, for KDEN, KLAS, KBOS, PAJN, PANC and PABR. Individual numbers, not AirNav's pages |
| Summits | The US National Geodetic Survey's datasheets for triangulation stations GT1811, KL0637, FQ0624, GM0779 and CD0994 (`https://geodesy.noaa.gov/api/nde/pid?pid=<PID>`), read 2026-09-18: adjusted NAD 83 positions and NAVD 88 heights. Works of the United States government |
| Coastal waters | Positions chosen off coasts, whose height is sea level by definition |

### Imagery: EOX's Sentinel-2 cloudless, 2016

| | |
| --- | --- |
| Source | EOX IT Services GmbH's Web Map Tile Service, `https://tiles.maps.eox.at/wmts/1.0.0/`, layer `s2cloudless` ("Sentinel-2 cloudless layer for 2016 by EOX - 4326"), tile matrix set `WGS84` - latitude and longitude, two tiles across at level 0 - JPEG tiles of 256 pixels, read to level 13, about 10 m a pixel (`gfx::open_imagery()`) |
| What it is | A cloud-free mosaic of the whole Earth made from the Copernicus Sentinel-2 satellites' images of 2016 |
| Version | The 2016 layer, which EOX no longer changes: its tiles are dated 2 August 2017 (`Last-Modified`) |
| In the repository | Nothing. The client fetches tiles as the view needs them and keeps them in `cesium-cache.sqlite` in the cache directory for as long as the service's `Cache-Control: max-age=604800`, a week, allows; the terrain test's reference keeps the tiles it reads in the downloads directory |
| Licence | **CC BY 4.0.** The service's own capabilities document (`WMTSCapabilities.xml`, read 2026-09-18) says of the layer: "EOxCloudless https://cloudless.eox.at by EOX IT Services GmbH (Contains modified Copernicus Sentinel data 2016) released under Creative Commons Attribution 4.0 International License." EOX's announcement (<https://eox.at/2017/08/sentinel-2-global-cloudless-mosaic/>) says the same, and that the service endpoints may be used directly in an application. The later years' layers are CC BY-NC-SA 4.0 and are not used |
| Credit | **"Sentinel-2 cloudless - https://s2maps.eu by EOX IT Services GmbH (Contains modified Copernicus Sentinel data 2016)"**, the text EOX asks for, on every frame with imagery in it, in capitals as the client's font has them. Changes made: the tiles are resampled onto the terrain and lit by its slope |

EOX's announcement, quoted:

> Sentinel-2 cloudless by EOX IT Services GmbH is now licensed under a
> Creative Commons Attribution 4.0 International License.

The Copernicus Sentinel data the mosaic is made from are named in the credit,
as EOX's text has them.

### METARs from aviationweather.gov

| | |
| --- | --- |
| Source | The Aviation Weather Center's Data API, of the US National Weather Service (NOAA): `https://aviationweather.gov/api/data/metar?ids=<ICAO>&format=json`, documented at <https://aviationweather.gov/data/api/> |
| Version | None: a METAR is the latest observation. What the program reads is each report's raw text (`rawOb`), with the station's position and elevation |
| In the repository | `tests/data/weather/aviationweather-metars-2026-09-17T1600Z.json`: one response, fetched 2026-09-17 at about 16:08 UTC, for CYYZ, EGLL, KBOS, KDEN, NZCH, PABR, SCCI, UUEE, YSSY and ZBAA, unmodified. `tests/data/weather/aviationweather-gusts-2026-09-18T0800Z.json`: the three reports with gusts - EGPH, KABQ and KMWN - of one response for 69 stations fetched 2026-09-18 at about 08:25 UTC, each report as the response gave it. `tests/data/weather/aviationweather-shear-2026-09-18.json`: five reports found searching 2026-09-18's responses for wind shear and its remarks - LPPT's `WS R02` and RJAA's `WS R34R`, from a week's history of 49 stations, and KJAC's `WSHFT`, KCQC's and KABQ's `PK WND`, from 1,357 reports over eight regions - each as the response gave it |
| Use | `world/weather.hpp`: the surface wind, temperature and QNH a flight's weather starts from; `glideslope_cli weather STATION`; `glideslope --weather STATION` |
| Licence | **Public domain**, as National Weather Service information. The API page's "Disclaimer" link is <https://www.weather.gov/disclaimer>; see below |
| Credit | Not required. `glideslope_cli weather` names the source; nothing presents the data as official NWS material |

The National Weather Service's disclaimer, read 2026-09-17, quoted:

> The information on National Weather Service (NWS) Web pages are in the public
> domain, unless specifically noted otherwise, and may be used without charge
> for any lawful purpose so long as you do not: 1) claim it is your own (e.g.,
> by claiming copyright for NWS information -- see below), 2) use it in a
> manner that implies an endorsement or affiliation with NOAA/NWS, or 3) modify
> its content and then present it as official government material.

The Data API page's own guidelines, quoted, which the program keeps to - it
sends its own User-Agent, and asks for one station when a flight starts and
every fifteen minutes after:

> Set a custom user agent to prevent automated filtering inadvertently blocking
> valid traffic. Consider product update frequency. For example most METARs
> update once per hour and TCF is issued every other hour. Wait between
> consecutive requests — maximum 100 requests per minute. Exceeding request
> limits will result in access being blocked.

### Winds aloft from Open-Meteo

| | |
| --- | --- |
| Source | Open-Meteo's free forecast API, `https://api.open-meteo.com/v1/forecast`, asking for wind speed, wind direction, temperature and geopotential height on 19 pressure levels from 1000 to 30 hPa, and wind speed and direction 10, 80, 120 and 180 m above the ground, hourly, for one day (`world::open_meteo_url`) |
| Version | None: the forecast for the hour asked for. Open-Meteo combines national weather services' models, chosen per place |
| In the repository | `tests/data/weather/open-meteo-sydney-2026-09-17.json`: one response for -33.9461, 151.1772 (the grid point -33.919155, 151.1596), 2026-09-17 00:00 to 23:00 UTC, fetched 2026-09-17, unmodified. `tests/data/weather/open-meteo-sydney-2026-09-18.json`: the same request with the winds 10, 80, 120 and 180 m above the ground added, 2026-09-18 00:00 to 23:00 UTC, fetched 2026-09-18, unmodified |
| Use | `world/winds_aloft.hpp`: the wind and temperature above the surface; `glideslope_cli weather STATION`; `glideslope --weather STATION` |
| Licence | **CC BY 4.0**, with Open-Meteo's terms for the free API: non-commercial use only, and under 10,000 calls a day. This project is free software with no subscriptions or advertising; a server run for profit would need Open-Meteo's commercial API |
| Credit | **"Weather data by Open-Meteo.com"**, with a link to <https://open-meteo.com/>. `glideslope_cli weather` prints it with the link and the licence; the client's HUD shows it along the bottom whenever the flight is in reported weather (`world::open_meteo_credit`); this entry credits the recorded response. Changes made: the levels' geopotential heights are converted to geometric heights, and winds from speed and direction to north and east components |

Open-Meteo's terms (<https://open-meteo.com/en/terms>), read 2026-09-17, quoted:

> By using the Free API for non-commercial use you agree to following terms:
> Less than 10'000 API calls per day, 5'000 per hour and 600 per minute. You
> may only use the free API services for non-commercial purposes. You accept to
> the CC-BY 4.0 licence, as specified in the licence conditions.

Its licence page (<https://open-meteo.com/en/licence>), quoted:

> API data are offered under Attribution 4.0 International (CC BY 4.0) [...]
> Attribution: You must give appropriate credit, provide a link to the licence,
> and indicate if changes were made. You may do so in any reasonable manner,
> but not in any way that suggests the licensor endorses you or your use. You
> must include a link next to any location Open-Meteo data are displayed, for
> example: `<a href="https://open-meteo.com/">Weather data by Open-Meteo.com</a>`

A HUD cannot hold a link; the client shows the credit's text on screen and
prints it with the link when the flight's weather is fetched, as
`glideslope_cli weather` does.

## Planned sources

These are named in `REQUIREMENTS.md`. Their entries are filled in when they are
first used.

| Source | For | Phase | Terms known now |
| --- | --- | --- | --- |
| Further aircraft: flight models, from JSBSim's or written here from published data | Flight dynamics | 5 | Recorded per model, as above |
| OpenStreetMap | Buildings | Tail | ODbL; source of the building data not yet chosen |
| FlightGear aircraft | Visual models | 5 | Mostly GPL; checked per model |
| Cesium ion | Optional visual terrain and imagery | 5b | The user's own account and terms |
| Google Photorealistic 3D Tiles | Optional visual scenery | 5b | The user's own key or Cesium ion token, and Google's terms |

Commercial providers are reached only with the user's own credentials. Nothing
here is cached beyond what a provider's terms allow, and whichever provider is
drawing is credited on screen.
