### The visual terrain against the terrain flown, 2026-09-21 — item done

Phase 5b's "a measured visual-to-collision terrain mismatch". **The open
provider's surface is 0.18 m from the ground an aircraft meets at worst,
Cesium ion's is 10.2 m and Google's is 10.1 m**, at the twelve surveyed
runway ends of six airfields.

**What the ground is.** The ground an aircraft meets is always the open DEM -
the rule that lets a server and every client agree on where it is. A visual
provider may put its surface somewhere else, and how far is what this
measures. `--mismatch FILE` names places, samples the drawn surface at each
through Cesium Native's `sampleHeightMostDetailed`, asks the DEM for the same
places, and prints both. The places are the twelve surveyed runway ends in
`tests/data/dem/surveyed.txt`, already in the repository because the DEM
itself is held to them.

One tileset is opened for each whole-degree cell rather than one for them
all: the open provider builds its terrain over the region it is given, and a
region from Barrow to Boston is most of a continent.

| Airfield | open | Cesium ion | Google |
| --- | --- | --- | --- |
| KDEN, Denver | 0.08 m | 3.38 m | 2.94 m |
| KLAS, Las Vegas | 0.02 m | 1.67 m | 0.77 m |
| KBOS, Boston | 0.01 m | 2.44 m | 1.84 m |
| PAJN, Juneau | 0.02 m | 2.79 m | 2.66 m |
| PANC, Anchorage | **0.18 m** | 1.66 m | 0.23 m |
| PABR, Barrow | 0.05 m | **10.22 m** | **10.12 m** |

The open provider's figure is what it should be: the mesh drawn is built from
the same DEM the aircraft meets, so what is left is its interpolation between
the points it is built on. Cesium ion's is a different survey of the same
ground, a metre or three out over most of them and ten at Barrow, where the
Arctic coast is thinly surveyed by anyone. Google's follows the same shape
for the same reason, and agrees with ion's worst figure to a tenth of a
metre - two independent surveys finding the same ten metres at Barrow is
better evidence that the Arctic coast is the hard part than either alone.

**Asking once is not enough, and the answer does not say so.** A sample
reports success whether or not the tiles beneath it had arrived. Boston and
Anchorage answered, repeatably, with a surface 36 km and 12 km *below* the
ellipsoid - no land is there - while Denver and Las Vegas answered properly.
What told them apart was which airfields straddle a whole-degree boundary:
Boston's and Anchorage's two runway ends fall in different cells, so each was
asked about alone, and the same call had half as long to load. The sample is
now made again until two answers running agree to a centimetre, which is the
only thing that says the tiles it needed were there. With that, all twelve
answer with a height and the whole measurement repeats byte for byte.

A height outside -500 m to 9,000 m is still set aside rather than folded into
a bound, and a provider that gives one fails: a bound with 36 km in it would
mean nothing.

**Verification run.** `the_open_terrain_is_within_its_stated_distance_of_the_ground_flown_on_<driver>`
holds the open provider to 0.25 m and Cesium ion and Google to 12 m each,
the figures above with a little room; each takes about 45 seconds, and the
two streamed ones report themselves skipped where there is no key. Watched to
fail with the bound tightened to 50 mm: "open's terrain is 179 mm from the
ground flown at PANC-7R, beyond the 50 mm stated in
docs/PROJECT_STATUS.md", and again for Google with its bound at 5 m:
"google's terrain is 10121 mm from the ground flown at PABR-8, beyond the
5000 mm stated in docs/PROJECT_STATUS.md". Google's takes 40 seconds.
