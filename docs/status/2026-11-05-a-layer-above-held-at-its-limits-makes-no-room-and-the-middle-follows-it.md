### A layer above held at its limits makes no room, and the squeezed aircraft's middle follows it, 2026-11-05

**What was missing.** When an aircraft is squeezed between two layers,
`sim::separate` holds it in the middle of the gap and asks the layers to
make room (2026-11-05, the layers make room). The middle follows any
aircraft near it that cannot be moved. But a layer *above* counted as
movable whenever it gave way. If that layer was itself held under someone
else, a floor 700 ft over the middle would cross its own ceiling. The room
was refused, so the layer stayed where it was. The middle stayed too, and
the squeezed aircraft's held height sat inside the minimum from that layer.
A layer below had the same gap for its own limits: only its ground was
checked, not a floor of its own.

**What changed** (`sim/separation.cpp`). The test the room-making uses,
whether a layer can take the room (not itself squeezed, its own limits not
crossed, not taken within 500 ft of its ground), is now one function,
`with_room`. When the middle works out who can be moved, it asks that same
function about each layer, above or below, against that layer's limits as
already worked out (every aircraft it gives way to comes before it in the
order, or gives way to nobody). A layer that cannot take the room is
treated like a person's aircraft: the middle is kept the minimum and margin
from it. The room-making itself is unchanged, now written as a call to
`with_room`.

**Verification.**
- `a_layer_above_held_at_its_limits_makes_no_room_and_the_squeezed_aircrafts_held_height_keeps_the_minimum_from_it`
  sets up this case. A person's aircraft is at 4,600 ft. The AI layer at
  3,800 gives way to it, so it is held under 3,900. Another AI layer is at
  3,000, and an AI aircraft is squeezed between them at 3,300. It asserts:
  - the layer above is not given the 4,100 ft floor over the old middle;
  - the squeezed aircraft is held at 3,100, which is 700 ft under the
    layer's held 3,800;
  - the lower layer makes room from there, down to 2,400.

  The same test then checks the case below (review of #340). A person's
  aircraft is at 2,300 ft. The AI layer at 3,000 gives way to it, so it is
  held over 3,000. A ceiling of 2,700 under the old middle would cross
  that, though its ground is far below. The squeezed aircraft is held at
  3,700, which is 700 ft over the layer, and the layer above makes room
  from there, up to 4,400. The old code checked only the ground for a
  layer below, so this case behaves differently now.

  **A layer that is itself squeezed** is also refused the room
  (`with_room`, as the room-making always did), and the middle now follows
  it as well. No test builds that case: it needs two aircraft squeezed at
  once, one of them a layer of the other.

  **Seen to fail**, each with its own half of the fix taken out:
  - below (the old ground-only check for a layer below): held at 3,400,
    400 ft from the layer below. Red on "with the layer below held at its
    limits, the squeezed aircraft's held height keeps the minimum and
    margin from it";
  - the whole old `movable` back: red on the first assertion, "the layer
    above ... only the 3,800 over the middle that followed it".

  Green again with the fix restored.

  Earlier, **seen to fail** with the fix taken out for a layer above (`movable`
  again true for any layer above that gives way): the held height stays at
  the 3,400 middle, 400 ft from the layer above. The test went red on "with
  the layer above held at its limits, the squeezed aircraft's held height
  keeps the minimum and margin from it", and green again with the fix
  restored.
- The other separation tests are unchanged and pass, linux-debug: the
  monitor's rules, the climb-through and head-on, the autopilot's ceiling
  and floor, and the between-layers hand-over (still 12.5 s lost from the
  lower layer, the lower down to 2,826 ft, 36,000 of 36,000 steps). The
  multi-process kept-apart runs were not run here; CI runs them.

The selftest hash does not move: the selftest flies the test pilot, and the
monitor does not touch it. The new test's entries in `tests/ci_costs/` are
estimates (1 s) until CI's own figures replace them.
