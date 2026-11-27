### A livery on the aeroplane, and its control surfaces moving, 2026-11-27

Phase 12c, "A livery on the aeroplane, and its control surfaces moving".

**What is still not true, first.**
- **Another aircraft on a server is drawn with its surfaces still.** A state
  update carries no surface positions (docs/TRANSPORT.md is unchanged), so
  only the client's own aeroplane, flown or predicted here, moves them. That
  is a new Phase 12c item.
- **The B-2 has no rudder drawn.** It has no fin: it yaws on split drag
  rudders at its wingtips, each two halves opening up and down at once about
  one hinge, which one part turned one way cannot draw. Its elevators are its
  inboard elevons. Named, with that reason, in the coverage test.
- **A livery is flat colours, not a texture.** Each is a colour scheme of
  boxes over the model's material colours: wingtips, fin and cheatline in a
  trim colour, or a colour all over. No airline's or air force's markings are
  copied.
- **The light is still baked**: a surface's part is lit as the wing it was
  cut from, not as it turns.

## How it is data

- `assets/models/<id>.livery`, one per aircraft: a name and coats, each a
  colour and a box in the model's body frame. A box cuts the triangles it
  crosses, so a coat's edge is straight however coarse the mesh.
- `assets/models/<id>.surfaces`, one per aircraft: each surface a hinge line
  (two points), how far forward of it the part reaches (nothing for a hinged
  surface; a quarter chord for the all-moving tailplanes of the PA-28, F-15,
  F-22 and F-35), how far aft and how thick, the JSBSim property it is
  deflected by, and which way a positive value moves its trailing edge.
  The F-22's tailplanes are drawn from `fcs/elevator-pos-rad`, not her
  differential `fcs/dht-left-pos-rad` and `dht-right-pos-rad`: those two sum
  the elevator and ailerons, with opposite signs to each other, but nothing
  in her aerodynamics reads them - her pitch and roll moments are tabled on
  the elevator and the ailerons - so they draw a deflection her flight model
  does not fly. Her tailplanes move with the stick's pitch alone.
  The C182's model writes no right aileron, so its right aileron reads the
  left's and moves the other way; the 787's and S.23's write the right
  aileron with the left's sign, so theirs are `up`.
- Both were measured off each mesh's own sections with a scratch script
  (not committed: the files are hand data, and each says how it was
  measured): hinges a quarter of the local chord forward of the trailing
  edge on the wing, three tenths on the tail and fin.
- `gfx::split_model` (src/gfx/parts.hpp) paints the coats and cuts each
  surface's box out into a part of its own; untouched triangles keep their
  shared vertices. The window client draws the body and each part, the part
  placed by `gfx::hinged` at the deflection `Flight::surface_angles_rad`
  reads from the flight model each frame.
- `--hold-stick A,E,R` holds the stick and pedals for a test.

## Verification

- `every_aircraft_wears_a_livery_and_has_its_ailerons_elevators_and_rudder_hinged`:
  all 16 of the catalogue's 16 (asserted against the catalogue's count).
  Every coat, alone, paints more than 0.05% of its model; every surface's
  part is between 0.05% and 8% of it; the body and the parts together cover
  the model's area to 1e-4. The B-2's rudder is the one named left out; the
  test asserts the B-2 is in the catalogue and that its surfaces file has no
  rudder.
- `the_stick_and_pedals_move_every_aircrafts_surfaces_the_way_they_should`:
  each aircraft trimmed in the air, the stick and pedals full one way then
  the other for a quarter of a second, and each surface turned by what its
  property reports. Each surface's swing is measured from a third run with
  the controls left where the trim had them, since a trimmed elevator already
  sits over: the F-22's about 1.5 degrees. Stick right: the left aileron's
  trailing edge down and the right's up. Stick back: the elevators' up. Right
  pedal: the rudder's right. Every swing must be a degree at least, but the
  F-22's rudders, held to half a degree: their command passes through her
  rudder limiter and yaw law, and they swing about 0.9 degrees in that
  quarter second. The movements are counted against the catalogue, not the
  files - two ailerons, two elevators and each fin's rudder, both ways, less
  the B-2's rudder - so a line taken out of a file fails it: 164 expected,
  164 seen and the way asked. The F-22's elevators sit at +2.07 degrees
  trimmed and swing -2.85 and +1.69; her rudders +0.88 and -0.97.
  - The count seen red: with the Cessna's rudder line taken out of the data
    the client reads, "162 surface movements seen, where the catalogue's
    aircraft have 164"; put back, green.
  - Seen red, by itself, on its first run: every rudder was written `left`,
    and the 787's and S.23's right ailerons `down`, so every rudder's
    movements and those two right ailerons' went the wrong way - the A380's
    rudder 1.23 m, the 787's right aileron 0.21 m. The data was fixed.
- `the_cessna_drawn_wears_her_livery_and_her_ailerons_follow_the_stick_on_vulkan`
  (tests/cmake/client_livery.cmake, tests/tools/surfaces_check.cpp): three
  shots of the Cessna from behind at tick 2, stick full left, full right, and
  not drawn. The aeroplane's outline is where the shots with and without her
  differ; at least a hundredth of it must be the cheatline's colour, read
  from c172p.livery; the pixels the stick moved must number ten or more, nine
  in ten of them beyond a third of her half-width from her middle, and a
  quarter on each side. Green on CI run 45958365262 on all three drivers
  (Linux vulkan, Windows direct3d12 and vulkan). Locally: an outline of 1,107
  pixels, 102 of them the trim's colour, and 143 moved by the stick, all out
  on the wings, 74 left of her middle and 69 right.
  - Seen red twice, each by a deliberate bug in the data the client drew,
    reverted after. Her livery's coats painted white: "no livery: 0 of the
    aeroplane's 1107 pixels are its trim, under a hundredth". Her ailerons
    put on the elevator's property, which a stick held only left or right
    does not move: "the ailerons did not follow the stick: 4 pixels moved".
    With the data put back, green again: the same 1,107, 102 and 143.
- Unit tests of the cutting and turning:
  `a_coat_or_a_surface_cuts_the_triangles_it_crosses_and_loses_nothing`,
  `a_surface_turns_about_its_hinge_its_trailing_edge_the_way_its_line_says`,
  `a_livery_or_surfaces_line_that_cannot_be_read_is_refused_with_its_line`.
