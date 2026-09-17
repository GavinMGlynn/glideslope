# Assets

**What this file is.** Where every piece of third-party data this project uses
comes from, which version, and under what terms — aircraft models, terrain,
imagery, weather data, fonts and sound. The code licence (GPL-3.0-or-later) does
not cover any of it; each source's own terms do.

**Four things are used: a Cessna 172P flight model derived from JSBSim's,
the Cessna 172P handbook's published figures, the Copernicus DEM, and the
EGM2008 geoid grid** - the last two fetched by the tests and read by the
program, not committed. No imagery, visual model, font or sound is used or
fetched yet.

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

**What that asks of glideslope:** nothing is redistributed yet, so no notice is
shown yet. When the client draws terrain from the DEM, or the server serves
heights from it, notice (a) is shown with the terrain's attribution, and (c)'s
sentence goes in the documentation that ships with the program.

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

## Planned sources

These are named in `REQUIREMENTS.md`. Their entries are filled in when they are
first used.

| Source | For | Phase | Terms known now |
| --- | --- | --- | --- |
| Further JSBSim aircraft models | Flight dynamics | 5 | Recorded per model, as above |
| Open imagery | Default visual imagery | 2 | Source not yet chosen |
| OpenStreetMap | Buildings | Tail | ODbL; source of the building data not yet chosen |
| aviationweather.gov | METARs | 3 | To be recorded |
| Open-Meteo | Winds aloft | 3 | The free API is for non-commercial use only |
| FlightGear aircraft | Visual models | 5 | Mostly GPL; checked per model |
| Cesium ion | Optional visual terrain and imagery | 5b | The user's own account and terms |
| Google Photorealistic 3D Tiles | Optional visual scenery | 5b | The user's own key or Cesium ion token, and Google's terms |

Commercial providers are reached only with the user's own credentials. Nothing
here is cached beyond what a provider's terms allow, and whichever provider is
drawing is credited on screen.
