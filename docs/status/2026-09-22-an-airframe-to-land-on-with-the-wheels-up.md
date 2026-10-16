### An airframe to land on with the wheels up, 2026-09-22 — item done

**All five aeroplanes that had nothing to land on now have an airframe.** Four
were done here; the fifth, the F-35A, could not be, because no F-35A flight
model carrying airframe contacts exists anywhere and FGAddon has no F-35A to
measure a mesh from. It became the F-35B the same day — its own entry below —
and that closed this item.

**The problem.** JSBSim gives a retracted wheel no force. An aeroplane whose
only contacts are its undercarriage therefore has nothing at all between it
and the ground once the gear is up: landed wheels-up it passes through the
runway and keeps going. Five of the sixteen were in that state — the 737-300,
the 747-400, the B-2A, the F-22A and the F-35A.

**The source: the visual mesh, not a flight model.** The hunt for JSBSim
models carrying these points ran over FGAddon, FGMEMBERS, JSBSim's own
aircraft, the Naval Postgraduate School's mirror and arktools's, and turned up
exactly one usable belly — FlightGear's 737-300. This project already ships a
visual mesh for fourteen aircraft, and a mesh *is* the airframe. `tools/ground.py`
measures the points off it.

**Why it needs no alignment, which matters because the alignment is poor.**
`assets/models/alignment.txt` records where a mesh sits on its flight model,
but the height in that fit carries a residual of up to 2.48 m — the size of
the very number wanted. So height is not taken from it. Every mesh draws its
undercarriage extended, so the mesh's lowest point is a tyre on the ground,
and a point's height *above the tyres* is a difference within one mesh, in
which the alignment offset cancels exactly. That difference is added to the
flight model's own wheel contact. Only x and y come from the alignment, where
a foot of error slides a belly point along the belly and changes nothing.

**Telling the undercarriage from the belly.** The lowest thing at a gear
station is the wheels. The legs cannot be found by looking near the flight
model's own contacts — the B-2's mesh draws its nose leg 100 in forward of
where its flight model puts the nose wheel — so they are found from the shape:
the lowest point is taken at 48 stations along the keel, the typical belly
height is the median over the flat run of it, and any station lower than half
of that is over the undercarriage and is dropped.

**The B-2 read 100.9 in where its belly is 53, and it took two guards to see
why.** Measuring across the whole fuselage width rather than a narrow keel
picks up the point where a flying wing's body blends up into the wing; and the
B-2's mesh carries 6,875 vertices for a 52 m aeroplane, at which density a
station can hold two vertices whose lowest is wherever they happen to be, so
the stations that would have contradicted it were themselves nonsense. Either
guard alone brings the reading back to 56.4 in. Both are kept, deliberately
redundant, because they guard different things — and this was measured rather
than assumed: a deliberate widening of the band no longer reddens the check,
which is how the redundancy is known to be real.

**What it measures.** Belly height above the wheel contact:

| aircraft | belly | mesh length vs published | mesh span vs published |
|---|---|---|---|
| 737-300 | 50.3 in | 33.30 m vs 33.40 (+0.3%) | 28.89 m vs 28.88 (+0.0%) |
| 747-400 | 106.5 in | 70.94 m vs 70.66 (+0.4%) | 65.42 m vs 64.44 (+1.5%) |
| B-2A | 53.0 in | 21.21 m vs 21.03 (+0.8%) | 52.20 m vs 52.43 (−0.4%) |
| F-22A | 38.7 in | 18.92 m vs 18.92 (+0.0%) | 13.56 m vs 13.56 (+0.0%) |

**Checked against the one independent answer there is.** FlightGear's own
JSBSim 737-300 puts its belly 43.7 in above the wheel contact. Measured from
the mesh, with no reference to that model, it comes out 50.3 in: **the two
agree to 6.6 in on a 33 m aeroplane**, which is what makes the same method
trustworthy on the three that have no second opinion. `python3 tools/ground.py
--check` holds that agreement to 10 in, holds each mesh to within 1.5% of its
published length and span, and fails if any airframe contact sits at or below
the wheels, where it would drag on every ordinary landing.

**A second fault, found by the same test: the A320 rolled along on its wing
tips.** Its nine contacts that never retract — wing tips, nacelles, nose, tail
— are airframe, but carried JSBSim's rolling friction of 0.02, a tyre's,
because the model it was made from left it there. JSBSim tells a wheel from a
wing tip by nothing but what the model says. Landed wheels-up the A320 was
still doing 118 knots after three minutes and had run 10,962 m without
stopping. This project now states one number for an airframe scraping a
runway — **0.4, aluminium on dry concrete** — and the A320 stops in 1,332 m.

**How hard an airframe is.** A belly that sinks into concrete puts the centre
of gravity under the runway, which is what the 737 did at first: its contacts
were a tenth of the gear's spring and it settled 1.17 ft under. The spring is
now set so the whole aeroplane's weight on one point gives a quarter of a
foot, with damping critical for it so it settles rather than rings.

**Verified**, `every_aircraft_put_down_with_its_wheels_up_rests_on_its_airframe`:
fifteen of the sixteen flown onto a runway at 0.6 of their approach speed
with the gear up and the engines closed. The four that fell through now rest — the
747-400 at +4.90 ft, the B-2 at +6.73, the F-22 at +1.29, and the A320, which
never stopped, now stops. The five whose wheels do not retract are flown too
and held to a different thing, both named in the test: they have no wheels to
raise, so they land on them and free-roll, and are asked not to go through the
runway rather than to stop. The flying boat is the one aircraft left out, with
its reason. **The test was watched failing** on nine aircraft before any of
this, and again on the count when the Mosquito's gear lever turned out to move
an indicator while none of its contacts retract.

**Tails found on the way**, both in `COMPLETION_PLAN.md`: the visual
alignment's height is badly fitted for three aircraft, which is still open;
and the F-15C's airframe contacts carried the old 0.2 friction on a spring of
10,000 lb/ft and slid 2,734 m, which was fixed the same day - it now stops in
1,029 m where the stated friction gives 1,093, and rests 2.9 ft above the
runway instead of 0.35 ft below it.
