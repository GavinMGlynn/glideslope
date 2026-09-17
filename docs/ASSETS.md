# Assets

**What this file is.** Where every piece of third-party data this project uses
comes from, which version, and under what terms — aircraft models, terrain,
imagery, weather data, fonts and sound. The code licence (GPL-3.0-or-later) does
not cover any of it; each source's own terms do.

**Three things are used: a Cessna 172P flight model derived from JSBSim's,
the Cessna 172P handbook's published figures, and the Copernicus DEM**, which
the tests fetch one tile of and the program reads. No imagery, visual model,
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

### The Copernicus DEM, GLO-30 Public

| | |
| --- | --- |
| Source | The Copernicus DEM GLO-30 Public, as Cloud Optimized GeoTIFFs in the AWS Open Data bucket `copernicus-dem-30m` (<https://copernicus-dem-30m.s3.amazonaws.com/readme.html>) |
| Version | The bucket names no release. Its objects are dated 2022-05-09; each tile's metadata gives its creation as 2019-10-19 and its heights as "WGS 84 Geoid EGM08". What is used is pinned file by file, by SHA-256 |
| Pinned | `tests/data/downloads/files.txt`: `Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif` (33-34 S, 151-152 E), 20,882,213 bytes, SHA-256 `6e20871096986cd00fc3903ea95670a0d83860236a0e4f1360ac9ac67def485d` |
| In the repository | Nothing: the tests fetch the tile into the build tree, or the directory `GLIDESLOPE_DOWNLOADS` names |
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

**What that asks of glideslope:** nothing is redistributed yet, so no notice is
shown yet. When the client draws terrain from the DEM, or the server serves
heights from it, notice (a) is shown with the terrain's attribution, and (c)'s
sentence goes in the documentation that ships with the program.

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
