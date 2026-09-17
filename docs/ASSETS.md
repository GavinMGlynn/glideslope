# Assets

**What this file is.** Where every piece of third-party data this project uses
comes from, which version, and under what terms — aircraft models, terrain,
imagery, weather data, fonts and sound. The code licence (GPL-3.0-or-later) does
not cover any of it; each source's own terms do.

**Nothing is used yet.** No aircraft, dataset, imagery, font or sound is in the
repository or fetched by any code.

## The rule

An asset is recorded here **in the commit that first uses it**, with its source,
its pinned version (a commit, a dataset release, or a URL plus SHA-256), its
licence, and the attribution text it requires. Terms are quoted from the source
at that point, not paraphrased from memory.

## Planned sources

These are named in `REQUIREMENTS.md`. Their entries are filled in when they are
first used.

| Source | For | Phase | Terms known now |
| --- | --- | --- | --- |
| JSBSim aircraft models | Flight dynamics | 1 | To be recorded per model |
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
