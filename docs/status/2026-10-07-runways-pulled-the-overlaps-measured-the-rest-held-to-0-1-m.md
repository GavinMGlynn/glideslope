### Runways pulled: the overlaps measured, the rest held to 0.1 m; protocol version 5; 16R seen red, 2026-10-07 — tail done

**The owner's decision** (REQUIREMENTS.md section 9, 2026-10-07): keep the
nearest runway winning, hold the ground off the overlaps to 0.1 m, and
measure and state the overlaps. An **overlap** is where another runway's
pavement, or the 5 m `runway_overlap_band_m` round it, covers the place:
there the ground is the mean of two lines that cannot agree.

**Measured** (`no_runway_in_the_world_is_pulled_off_its_line_beyond_a_bound`,
11,201 runways, 3,619 reached by another at 102,229 places, 34,683 of them on
an overlap): off the overlaps no runway is pulled more than 0.0005 m - none
over 0.1 m; on them 262 runways are pulled more than 0.1 m and 12 more than
0.3 m, the worst LKMB 04/22 at 0.612 m, then LKMB 16/34 0.585, LFQB 05/23
0.443, FMEE 14/32 0.433 m. Held: 0.1 m off the overlaps; on them, 0.62 m,
at most 12 over 0.3 m and **262** over 0.1 m (pinned to the measurement,
from the review). The item is ticked. **Seen to fail** with rules 1 put back
(every weight its blend alone): "off the overlaps, the worst pull is
0.676418 m", and the shoulder test's "A: pulled 0.027283 m". Without the 5 m
band counted as overlap, 111 runways were over 0.1 m off the pavements'
overlaps, at worst 0.43 m - all within 5 m of another's edge, where the mean
gives way to the nearest line.

**16R, seen red** (from the review): with `CollisionGround::height_above_geoid`
returning the raw DEM, the airliners' 16R test fails - "737-300 was wrecked:
struck the ground with its airframe", the A320 likewise - and the military
jets' - "f15c was wrecked: hit the ground sinking at 759 ft/min"; the light
aeroplanes' three groups pass, as they did before the flattening. Put back,
all five pass.

**Protocol version 5**, not 4: #107 moves the network's to 4 and lands
first. `protocol_version`, the client written from `TRANSPORT.md`
(`0x05`), the gearstick refusal (`474c4453050401`), `TRANSPORT.md` and the
version test say 5; the ground's hash ("ground rules 2") is unchanged.

**Run, green**: the runway-ground tests and all five 16R groups, the
`TRANSPORT.md` agreement tests, the protocol tests, the gearstick refusal,
the client written from the document and the collision-ground tests (25).
