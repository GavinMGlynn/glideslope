### The F-35A becomes the F-35B, 2026-09-22 — item done

**What is missing: it cannot hover, land vertically or take off short.** The
F-35B is a STOVL aeroplane and its LiftFan, swivel nozzle and roll posts are
what make it one. None of that is modelled. It flies its wing, weights and
engine in conventional flight and uses a runway like any other fighter, and
the STOVL work is a tail in `COMPLETION_PLAN.md`.

**Why the variant changed.** The F-35A was the only aircraft with no visual
model and the only one still falling through the runway with its wheels up,
and both had one cause: FGAddon has no F-35A. It has an F-35B, under a
verbatim GPL-3.0, whose model draws its undercarriage — which is what
`tools/ground.py` needs to measure an airframe. The project owner chose the
variant.

**The figures are the B's own, read from the documents rather than a
summary.** Lockheed Martin's F-35B product card (©2023, 23-08442_002, PIRA
AER2023060207) and the April 2020 Fast Facts (FG19-24749_004) were fetched and
their text extracted here; `ASSETS.md` records both with their SHA-256.

| | F-35A | F-35B |
|---|---|---|
| Length | 51.4 ft | 51.2 ft |
| Empty weight | 29,300 lb | **32,300 lb** |
| Internal fuel | 18,250 lb | **13,100 lb** |
| Engine | F135-PW-100 | **F135-PW-600** |
| Thrust | 43,000 / 28,000 lb | **38,000 / 26,000 lb** |
| Range | >1,200 nm | **>900 nm** |
| Max g | 9.0 | **7.0** |

**Two conflicts between primary sources, resolved deliberately.** Internal
fuel is 13,100 lb on the product card and 13,500 lb in Fast Facts; the product
card is the later Lockheed publication and the B's own, so 13,100 is used.
Thrust is 38,000/26,000 lb on the product card and 40,000/25,000 lb in Fast
Facts — but Fast Facts prints that same pair for the A, the B and the C,
labelled "uninstalled thrust ratings", so it is a family figure and not the
B's; the product card's is used.

**No ceiling is claimed, and the ceiling test is gone.** Lockheed publishes no
service ceiling for any F-35. The "above 50,000 feet" the F-35A was held to is
the Air Force's fact sheet, for the A alone, so the F-35B is held to its
maximum Mach and its range and to nothing else. The figures file and the test
file each say so where the figure used to be.

**What had to be retuned, and how it was measured.** On 38,000 lb of thrust
and 3,000 lb more empty weight, the A's drag numbers gave Mach 1.35 against a
published 1.6. The wave drag is the number this project sets to fly the
figure, as the script's docstring says. Measured: 0.066 → Mach 1.35, 0.060 →
1.48, 0.056 → 1.56, 0.052 → 1.64, and **0.054 → 1.609 against the published
1.6**. The range figure did not move with it, because that flight is level at
Mach 0.8 where there is no wave drag — so the two were set independently.

**The visual model, and one thing left out of it.** FGAddon's F-35B at the
same revision 21588 every other model is pinned at: `F-35B.ac`, `Engine.ac`
and `Gear.ac`, 20,649 vertices. Converted whole it came out **10% longer than
the published aeroplane** — an `antennas` object holding a nose air-data boom
reaching 1.5 m past the radome. With it left out the mesh is 15.64 m against a
published 51.2 ft, **+0.2%**. Its span is 3.6% over, which is its wingtip
navigation lights; the wing alone is 1.6% over, so the lights stay on the
model and the check states the reason.

**Two counts the switch moved, and both were assertions that caught it.** The
figure count fell from 94 to 93 when the ceiling went, and the visual-model
size test had no published dimensions for an aeroplane it had never had a
model for. Neither failed silently: each named exactly what had changed, which
is what those counts are for.

**The emptied list was watched failing.** `awaiting_a_model` is the list of
aeroplanes allowed to go through the runway, and it is now empty. Putting
`f35b` back into it turns the test red - "f35b no longer does, so the list
above is stale" - so the guard catches a stale entry as well as a real
regression, and both directions have been seen.

**Verified locally**: it flies **Mach 1.609** against its published 1.6 and
ranges **1,372 nm** against more than 900; its model sits **0.31 m** from the
contacts it stands on, better than most of the fleet; and it comes to rest on
its airframe **1.4 ft above the runway** with its wheels up, which emptied the
list of aircraft that do not. A full `ctest` run stands behind it; the
platform-wide figures wait on the next CI run.
