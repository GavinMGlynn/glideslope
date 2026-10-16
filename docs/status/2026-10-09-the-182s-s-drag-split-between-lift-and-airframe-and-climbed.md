### The 182S's drag split between lift and airframe, and climbed to its ceiling, 2026-10-09 — item done

**What is still not right, first.** The handbook's sea-level climb and its
ceiling are both inside their 10%, but not close: 997 ft/min against 924
(+8%) and 17,039 ft against 18,100 (-6%). No drag polar meets both closely.
Figure 5-7's 924 ft/min at sea level and 285 at 14,000 ft, with the model's
engine lapse and propeller, ask the drag to fall from 331 lb at 82 KCAS to
277 at 74.9 KCAS - in the ratio of the dynamic pressures, 0.834, which is
drag that is all parasite and none due to lift (fitted, 3 lb of drag due to
lift at 82 KCAS, where even an elliptic wing, efficiency 1.0, has 105). The
chart is a new aeroplane flown by a test pilot, or a smoothed curve; the
model sits between its two ends. The windmilling propeller is a drag of its
own sized to the glide figure, not a propeller that windmills.

**The finding.** The drag high up was too much because the drag due to lift
was: JSBSim's `Drag_due_to_alpha` table is k CL^2 with k = 0.083 through the
climb's lift coefficients (0.5 to 1.0), an Oswald efficiency of 0.52 on
this wing's aspect ratio of 7.37, and 0.45 once `make_c182.py` put the
glide's windmilling propeller into it as x1.15. A strut-braced wing of 7.4
has 0.7 to 0.83 (Raymer's straight-wing estimate 0.83; Virginia Tech AOE
3104's 182, 0.80 with a zero-lift drag of 0.025; a drag build-up of the
172S's same wing, Harada, Ohio State 2026, 0.70 and 0.028). The climb's
lift coefficient rises with height (0.78 at sea level, 0.94 at 14,000 ft at
figure 5-7's speeds), so drag due to lift is what grows up high.

**The changes** (`tools/make_c182.py`, each in its docstring with its
source):
- **Drag due to lift x0.75** (in place of x1.15): an efficiency of 0.69,
  the lowest of the sources.
- **Only through attached flow**: the rows past the lift's peak (0.28 rad)
  are JSBSim's as pinned, since past the stall the drag is separation's, not
  lift's. Scaled with the rest, CI run 37858975068 was red on every platform
  once #136's stall recovery was under it: left thirty seconds in the stall,
  the 182S was handed over at 70.2 kt sinking 1,511 ft/min (main: 62.6 kt,
  1,156) and its recovery peaked at 1.82 g, over the 2 g limit less its 10%.
  With the stalled rows kept: 63.1 kt, 1,176 ft/min, 1.53 g (main 1.52);
  the figures below are with them.
  `every_published_figure_has_a_flight_and_every_flight_a_figure`'s count,
  120, had not been raised for the new ceiling figure; it is 121.
- **Zero-lift drag 0.027 -> 0.030**, so that cruise and top speed stay in
  range: drag moved from lift to the airframe.
- **The windmilling propeller's drag, 0.021, charged only while the engine
  is stopped** (`aero/coefficient/CDwindmill`, by
  `propulsion/engine[0]/set-running`): 72 lb at the glide's 77.5 KCAS, a
  drag coefficient of 0.1 on the propeller's disc, about what the model's
  own engine friction absorbs at 1,300 rpm at that speed. It was x1.15 on
  the drag due to lift in every flight, the climb included.
- **The take-off's extra propeller thrust ends at an advance ratio of 0.5,
  not 0.8.** It answers the take-off (lift-off at 0.36); fading to 0.8 gave
  the climb 6% more thrust than the propeller's tables. The propeller in the
  sea-level climb is then about 0.70.
- **The service ceiling is a figure** (`assets/figures/c182.xml`: 18,100
  ft, 10%, at figure 5-7's speeds, 82 KCAS at sea level falling 0.51 kt a
  thousand feet, flown from 6,000 ft, leaning).

**Measured** (`glideslope_cli figures c182`, linux-release), before -> after:

| Figure | Handbook | Range | Before | After |
|---|---|---|---|---|
| Static rpm | 2,300-2,400 | 2,300-2,405 | 2,400 | 2,400 |
| Rated power | 230 hp | ±3% | 233.6 | 233.7 |
| Take-off ground roll | 795 ft | ±10% | 789 | 789 |
| Climb, sea level | 924 ft/min | ±10% | 963 | 997 |
| Service ceiling | 18,100 ft | ±10% | 12,886 | 17,039 |
| Cruise, 6,000 ft | 140 KTAS | ±3 kt | 139.6 | 138.7 |
| Top speed | 145 KTAS | ±3 kt | 147.3 | 144.1 |
| Glide | 8.9:1 | ±10% | 8.93 | 8.99 |
| Stalls, up / 20 / full | 54-56, 50-52, 49-50 KCAS | ±2 kt | 55.3, 51.4, 49.8 | 55.1, 51.3, 49.7 |

`plan-speeds c182` and `glide-speeds c182` measure what the figures file
holds (64 and 129 kt; 64): unchanged. Level at full throttle at 3,000 ft
she flies 139.8 kt, not 144; the file says so.

**Tried and kept out** (efficiency is 0.52 / the drag-due-to-lift scale):

| Drag due to lift | Zero-lift | Climb | Ceiling | Cruise | Top | Out |
|---|---|---|---|---|---|---|
| x0.65 (0.80), take-off thrust to 0.8 | 0.029 | 1,159 | 19,951 | 141.3 | 146.4 | climb, ceiling (over), glide |
| x0.75, take-off thrust to 0.8 | 0.031 | 1,093 | 18,026 | 137.0 | 142.4 | climb (glide not sized) |
| x0.85, take-off thrust to 0.8 | 0.031 | 1,048 | 16,453 | 135.9 | 141.8 | climb, cruise, top (glide not sized) |
| x0.65, take-off thrust to 0.5 | 0.031 | 1,030 | 18,544 | 138.1 | 142.9 | climb |
| x0.72, take-off thrust to 0.5 | 0.030 | 1,011 | 17,522 | 139.0 | 144.3 | none, climb at +9.4% |
| x0.78, take-off thrust to 0.5 | 0.029 | 994 | 16,715 | 140.0 | 145.6 | none, ceiling at -7.6% |

Top speed holds the zero-lift drag to 0.031 or less; with the take-off's
thrust left in the climb, no efficiency puts the sea-level climb under
1,016 with the ceiling over 16,290. Cowl flaps open in the climb would take
the sea-level climb down too, but the model has no cowl flaps.

**Verified**, each seen red:
- `the_ai_climbs_a_cessna_182s_at_its_handbooks_speeds_to_its_published_service_ceiling`:
  17,039 ft. On the committed model before this change, 12,886 - red.
- `every_light_aeroplane_is_climbed_to_its_published_ceiling_or_named_with_its_reason`:
  four climbed, none named.
- `the_committed_cessna_182s_is_what_its_tuning_script_makes`, the 182S's
  ten other figure tests, and the 203 tests matching c182, the light aeroplanes, landings, glides, recoveries, departures and the leaner: all pass (six skipped, wanting a key).

**The selftest hash does not move**: `be036519d2c19ea0` in linux-release;
the selftest flies the 172P.
