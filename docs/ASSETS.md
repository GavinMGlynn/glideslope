# Assets

**What this file is.** Where every piece of third-party data this project uses
comes from, which version, and under what terms — aircraft models, terrain,
imagery, weather data, fonts and sound. The code licence (GPL-3.0-or-later) does
not cover any of it; each source's own terms do.

**Two things are used: a Cessna 172P flight model derived from JSBSim's, and
the Cessna 172P handbook's published figures.** No dataset, imagery, visual
model, font or sound is used or fetched yet.

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

## Planned sources

These are named in `REQUIREMENTS.md`. Their entries are filled in when they are
first used.

| Source | For | Phase | Terms known now |
| --- | --- | --- | --- |
| Further JSBSim aircraft models | Flight dynamics | 5 | Recorded per model, as above |
| Copernicus DEM | Collision terrain and default visual terrain | 2 | To be recorded |
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
