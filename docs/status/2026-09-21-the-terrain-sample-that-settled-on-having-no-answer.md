### The terrain sample that settled on having no answer, 2026-09-21

**A place that has not answered is not a place that has settled.** The
visual-to-collision measurement asks the drawn surface for a height until two
answers running agree, because a sample reports success whether or not the
tiles beneath it had arrived. That comparison treated "no answer" and "no
answer" as agreeing, so two failures in a row ended the asking on the second
round - the least loading time it can give - and a cell whose terrain was
merely slow was reported as having no surface at all.

It showed up as `the_open_terrain_is_within_its_stated_distance_of_the_ground_flown_on_vulkan`
failing on Ubuntu, and only Ubuntu, while Rocky 9, the same compiler on the
same architecture, passed: "open did not answer with a height at KLAS-8L,
KLAS-26R". Las Vegas's two runway ends sit in one whole-degree cell, so one
slow cell took both with it. Every place must now have a height before two
rounds are compared at all, and a round the tileset never answered carries on
to the next instead of giving up on every place at once.

**A message that said the opposite of what it meant** went with it: a place
with no drawn surface printed "none flown" and then the flown height beside
it, which reads as the DEM having failed when the DEM was the half that
worked. It says "none-drawn" now.
