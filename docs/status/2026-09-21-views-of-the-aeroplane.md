### Views of the aeroplane, 2026-09-21 — item done

Phase 5's last item. **The aeroplane is drawn, and `--view` says where it is
seen from**: `cockpit`, `ahead`, `behind`, `left`, `right`, `above` or
`orbit`, and V steps round them while flying. A view there is none of is
refused by name, with the ones there are - as `--aircraft` does.

**What it took.** `gfx/aircraft.hpp` holds the views and turns a model into a
mesh; `Flight` works out where the model goes and what the camera sees. The
cockpit looks out along the nose from the flight model's own eyepoint, and
rolls with the aeroplane. The six outside views stand off three times the
model's radius in the body frame and look back at it, held upright, so they
swing with the aeroplane rather than staying level; the orbit goes round once
a minute of the flight's own time, which makes the same command draw the same
frame every run.

**The cockpit draws no aeroplane.** The renderer culls nothing by its facing
and the models have no interior, so from inside, the skin would be drawn over
the windscreen. Nothing of it is drawn there, and the test holds the shot
with the aeroplane and the shot without it to being the same file.

**The light is baked in.** The mesh shader takes a position, a colour and a
texture coordinate, and no normal, so an aeroplane is shaded on the way in:
each vertex's colour times how much light its normal catches, under the sun
the terrain is lit by - north-west, 45 degrees up - so an aeroplane is lit as
the ground beneath it is. The sun is worked out once in the body frame rather
than rotating every normal, and the mesh is made again when the aeroplane has
banked five degrees. `gfx/terrain_colour.hpp` now hands out that sun and the
light on a surface, which is what the terrain was already doing inside
itself; the terrain's own figures are unchanged.

**The drawing that draws nothing.** `glideslope_model` grew from the model
reader alone to the views, the scene's geometry and the terrain's colours -
all of it arithmetic with no SDL and no Cesium Native behind it - so the unit
tests hold the cockpit's eye and where each view stands without linking a
renderer. `glideslope_gfx` is now the part that needs a GPU.

**How it is held.** The frame test shoots each of the seven views at tick 120
and, for the six outside ones, shoots the same frame again with
`--draw-aircraft off`. Every pixel that differs between the two is one the
aeroplane covered and nothing else did, so the pair gives its outline exactly,
with no guessing at which pixels are aeroplane and which are sky or ground.
`glideslope_view_check` then projects every vertex of the same model file from
the camera the client printed - the projection written out by hand, sharing
nothing with the renderer but the numbers - and holds the two outlines' edges
within two pixels.

| View | left | right | top | bottom |
| --- | --- | --- | --- | --- |
| ahead | 1 | 1 | 1 | 2 |
| behind | 1 | 2 | 1 | 1 |
| left | 1 | 1 | 1 | 1 |
| right | 0 | 2 | 1 | 1 |
| above | 1 | 1 | 1 | 2 |
| orbit | 1 | 2 | 0 | 2 |

The centre of what is drawn and the centre of the vertices projected are not
the same number and are not meant to be - a model's vertices crowd where it
has detail, and only the side facing the camera is drawn, which on a side
view of the Cessna is 13 px apart on a 320 px frame - so that is held to a
tenth of the frame, and the edges are what pin it.

`--draw-aircraft on|off` is a test flag, as `--shot` and `--trace` are, and
says so in the usage.

**Changing the view steps nothing in the flight**, held by every view having
traced the same number of steps and left the flight in the same state when
the frame was shot. The first attempt at that check compared the line
numbered with the flight's own tick, which is the same in every view whatever
the view did to the flight - it was watched not to fail, and replaced.

**What is left, and named as tails.** No livery and nothing moving: a model
carries no texture and no animation, so a surface is the flat colour of its
material and the control surfaces, propellers and undercarriage are welded
where the model has them, gear down. And the light is baked rather than
worked out on the GPU, which is why the mesh is made again as the aeroplane
banks.

**Verification run.** Locally, 276 of 276 tests pass in 489 s at `-j4`; the
views frame test is 339 s of that, seven views shot twice at 20 s a shot. The
four new checks are `the_cockpit_view_puts_the_eye_where_the_flight_model_says_the_pilots_is`,
`every_view_stands_where_its_name_says_and_looks_at_the_aeroplane`,
`the_client_draws_the_aeroplane_where_each_view_puts_it_on_<driver>` and
`the_client_refuses_a_view_there_is_none_of`. Each was watched to fail: the
outline check given the `behind` shot and the `left` view's camera reports it
43 px out and exits 1; the cockpit exclusion removed, its two shots stop
matching; and a deliberate extra step on the `above` view leaves the flight
at tick 240 where every other view is at 120.
