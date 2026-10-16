### The plan grows: weather hazards, sixteen aircraft, learning to fly, the whole Earth, 2026-09-18

At the project owner's asking. **Learning to fly** is Phase 5c: checklists for
every aircraft as part of its data, ticking themselves from the aircraft's
state, and lessons the AI pilot demonstrates and debriefs (`REQUIREMENTS.md`
section 4.3) - a debrief, never a score, which `FEATURES.md` rules out. **`FEATURES.md`** gains wind that shears and
gusts, hazardous air, weather you can see, and the same air for everyone, and
its choice of aircraft now names them. **`COMPLETION_PLAN.md`** gains Phase 3b
(seven items: the same air on every machine, gusts flown, the boundary layer,
reported shear, microbursts, thermals and mountain waves, visible weather),
and Phase 5 the roster of `REQUIREMENTS.md` section 4.2 - the J-3 Cub, PA-28,
Cessna 182, the Short S.23 flying boat, the Mosquito (first after the Cessna),
the Learjet 35A, the A320, A380, 737, 747 and 787, and the F-15, F-22, F-35A
and B-2 - with water where the DEM says it is and a HUD for fast aircraft. A
tail, found doing it: terrain over the whole Earth, which "anywhere on Earth"
needs and nothing in the plan provided, and which a jet makes pressing.

**Why Phase 3b's weather is computed here:** JSBSim's own gust and turbulence
models keep hidden state that a restored aircraft does not carry, and the
server's weather is authoritative, so every client's prediction must fly the
same air as the server. Each perturbation is to be a function of position, time
and shared parameters alone.

**What JSBSim does not ship:** flight models for the A380, Learjet, Mosquito,
F-35A and B-2, which are written here. Much of the F-35A's and B-2's performance
is not public; they will be held to what is, and no more claimed.
