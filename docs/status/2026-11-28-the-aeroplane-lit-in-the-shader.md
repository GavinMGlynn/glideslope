### The aeroplane lit in the shader, 2026-11-28

Phase 12c, "The aeroplane is lit by a light baked into its mesh".

**What is still not true, first.** The light is the terrain's fixed sun,
north-west and 45 degrees up, everywhere and at every hour: no time of day,
and nothing casts a shadow. It is lit per vertex, as it was baked, not per
pixel.

## What changed

- A mesh to be lit carries a normal a vertex (`gfx::Mesh::normals`), and a
  draw may carry the sun as a unit vector in ECEF (`gfx::Draw::sun`). The
  renderer turns that sun into the mesh's own frame and passes it in the
  vertex shader's uniform block (176 bytes, from 160); `mesh.vert`
  multiplies the vertex colour by `gfx::light_on`'s
  0.35 + 0.65 max(0, n.s) / sqrt(0.5), clamped to 1. A draw without a sun -
  terrain, sky, the HUD - draws its colours as they are, so nothing else is
  drawn differently.
- **The normals are in a vertex buffer of their own**, not in `gfx::Vertex`:
  terrain's vertices would otherwise each have carried 12 unused bytes, a
  third more than their 36. A lit draw is drawn by a pipeline reading them
  a vertex each; an unlit one by a pipeline reading one normal of nothing
  for every vertex, stepped by instance. Terrain's memory and uploads are
  as they were.
- `gfx::mesh_from_model` copies colours and normals and lights nothing.
- The client makes an aeroplane's meshes once and gives every draw of it,
  body and each control surface, the sun over it. A surface's placement is
  turned about its hinge, so its normals turn with it: before, a surface
  was lit as the wing it was cut from. Before, the meshes were made again
  every time the sun moved five degrees in the body frame - and another
  aircraft's on a server likewise; now neither is.
- At a shot, the client says the bank, the sun in the body frame and how
  many times the aeroplane's meshes were made.

## Verification

- `the_cessnas_light_follows_her_bank_with_no_mesh_made_again_on_<driver>`
  (tests/cmake/client_light.cmake, tests/tools/light_check.cpp): the Cessna
  over the Nullarbor heading east, shot from above in the body frame - her
  tops - level at tick 2, and at tick 90 with the stick held full right,
  each with her drawn and not. Her tops (her pixels less their rim) are
  held to the brightness ratio the sun says for a face straight up out of
  her top, within 8%, and the sun's ratio must be 0.85 or less so a light
  that did not move would be seen. The banked run must say her meshes were
  made once. Locally on vulkan: banked 36.2 degrees, the sun's body z
  -0.707 level and -0.322 banked, a ratio of 0.646 asked and 0.674 drawn
  (2,883 and 2,877 pixels, means 492.7 and 332.0); meshes made once.
  - Seen red twice, each by a deliberate bug, reverted after. The old rule
    put back - meshes made again when the sun moves five degrees in the
    body: "the aeroplane's meshes were made 7 times as she banked". The
    light baked once - the sun held in the body frame where it was at the
    first frame: "banked she is drawn 0.998 as bright as level, where the
    sun says 0.646". Put right, green: 0.674 against 0.646 again.
  - At tick 150 she had banked 61.6 degrees, where her tops are in shadow
    and the curve of her wing and fuselage puts some faces back in the sun
    (0.407 drawn, 0.350 asked); tick 90 keeps her tops lit.
- **Its cost and time limit**: until CI measures it, its costs in
  tests/ci_costs are estimates, from the livery test's (three launches of
  the client) scaled by four to three for its four - Windows debug 133 s on
  Direct3D 12 and 127 on Vulkan, release 56 and 64, clang 67 and 75, macOS
  release 43 - and, where the livery test has no figure yet (Linux and
  macOS debug), from the behind view's test (two launches) doubled: 218,
  56 and 278. It ran 330 to 344 s on the Windows debug machine, where the
  livery test took 197, so its time limit is 1,500 s.
- **Every platform's figures**: it adds its ratios to figures.txt, which
  CI's "The figures said" step prints, passed or not. FIGURES_CI
- `every_aircrafts_drawn_meshes_carry_a_unit_normal_at_every_vertex_and_unlit_colours`:
  all 16 of the catalogue's 16 (asserted against its count), body and every
  surface's part as the client makes them: each vertex's normal within 0.02
  of unit length, and its colour and normal the model's own - nothing lit
  into it. 98 meshes, 1,657,663 vertices; the worst normal 0.0065 off unit
  length (the A320's).
  - Seen red: with `mesh_from_model` copying no normals, every one of the sixteen
    "normals are off unit length by 1"; put back, green.
