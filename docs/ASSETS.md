# Assets

**What this file is.** Where every piece of third-party data this project uses
comes from, which version, and under what terms — aircraft models, terrain,
imagery, weather data, fonts and sound. The code licence (GPL-3.0-or-later) does
not cover any of it; each source's own terms do.

**One thing is used: the Cessna 172P flight model from JSBSim.** No dataset,
imagery, visual model, font or sound is used or fetched yet.

## The rule

An asset is recorded here **in the commit that first uses it**, with its source,
its pinned version (a commit, a dataset release, or a URL plus SHA-256), its
licence, and the attribution text it requires. Terms are quoted from the source
at that point, not paraphrased from memory.

## In use

### JSBSim's Cessna 172P

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`) |
| Files | `aircraft/c172p/` (the model, its reset files and its notes), `engine/eng_io320.xml`, `engine/prop_75in2f.xml` |
| Licence | LGPL-2.1, as the JSBSim repository; the text ships as `licenses/JSBSim.txt` |
| Where it goes | Copied at configure time into `data/jsbsim/` beside the programs, in the build tree and in every package |

The model file's own header says: "This model was created using publicly
available data, publicly available technical reports, textbooks, and guesses.
It contains no proprietary or restricted data. If this model has been validated
at all, it would be only to the extent that it seems to "fly right", and that it
possibly complies with published, publicly known, performance data (maximum
speed, endurance, etc.). Thus, this model is meant for educational and
entertainment purposes only."

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
