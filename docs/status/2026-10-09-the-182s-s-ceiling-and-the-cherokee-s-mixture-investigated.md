### The 182S's ceiling and the Cherokee's mixture, investigated, 2026-10-09 — no item closed, no model changed

**What is still missing, first.** All three items stay open. **The 182S
still climbs to 12,886 ft at figure 5-7's speeds against 18,100**: no
sourced change to its propeller or engine reaches it, because its handbook's
chart asks more of the propeller than a propeller gives. **The Cherokee is
still leaned at every height on JSBSim's mixture curve**: on the FAA's, full
rich or leaned, its stall at the warning is never called recovered, because
at the recovery's speed with 40 degrees of flap it cannot hold its height;
that waits on the stall recovery's law (PR #136), not on the engine.

**The 182S, measured** (JSBSim 1.3.1 on the committed model, climbing at
figure 5-7's speeds, 3,100 lb, leaned to 13.1 to 1 above 3,000 ft):

| | power | propeller | advance ratio | blade |
|---|---|---|---|---|
| sea level, 82 KCAS | 230 hp full rich | 0.74 | 0.53 | 23.9 deg |
| 14,000 ft, 74.9 KCAS | 137 hp | 0.77 | 0.57 | 22.6 deg |
| 18,000 ft, 72.8 KCAS | 119 hp | 0.78 | 0.59 | 22.2 deg |

- **The engine's lapse is right**: 137 hp at 14,000 ft is 59.6% of 230, where
  Gagg and Ferrar's lapse, 1.132 sigma - 0.132, gives 58%, and figure 5-9's
  16 in. and 2,400 rpm there is 51% at peak EGT, a few percent under best
  power. Not the cause.
- **The propeller already gains efficiency with height**, 0.74 to 0.77; the
  lighter loading up high is worth about 1.5% more by momentum theory
  (McCormick's ideal efficiency, 2 / (1 + sqrt(1 + T / (q A))): 0.868 at sea
  level, 0.881 at 14,000 ft).
- **What figure 5-7 asks.** It climbs 924 ft/min at sea level and 285 at
  14,000 ft, a loss of 60 hp of excess power. With the model's 93 hp of
  lapse that needs the propeller to go from about 0.65 at sea level to about
  0.80 at 14,000 ft (on a textbook polar, zero-lift drag 0.028 and Oswald
  efficiency 0.75), or, with the model's propeller, the drag at 14,000 ft's
  climb to be about 27% less than the model's 369 lb. A propeller's efficiency does not rise
  fifteen points over 0.04 of advance ratio, so the propeller is not where
  the figure is to be found.
- **The drag, tried and kept out.** The model's drag rises from about 314 lb
  at sea level's climb to 369 at 14,000 ft's (its drag-due-to-alpha table
  is an Oswald efficiency of about 0.5, times 1.15 for the windmilling
  propeller of the glide). Taken down (glideslope_cli figures, linux-release):
  at x1.0, the pinned table, the ceiling is 14,890 ft and the climb 1,028
  ft/min (out), top speed 148.2 kt (out), glide 9.84 (out); at x0.85, 17,110
  ft, but the climb 1,094, cruise 143, top speed 149 and glide 11.0, all
  out. Reaching the chart by drag needs a lower-drag polar and the glide's
  drag put where it belongs, in a windmilling propeller (its tables end
  where it would begin to windmill), with every figure flown again - and
  the chart may simply be what a new aeroplane flown by a test pilot does.
  A decision for the owner, not a tuning to guess.

**The Cherokee, traced.** With `make_pa28.py` given the FAA's curve and a
float carburettor and the catalogue full rich below 5,000 ft (as #130
tried), `every_aeroplane_recovered_at_the_first_sign_of_a_stall_...` traced a
half second at a time: the engine is not the fault. Idle at 700 rpm and
AFR 10.5 through the entry, it opens up within three seconds of the
recovery (2,089 rpm and 109 hp after two, 133 hp after six), full rich
throughout, the mixture never moving. The aeroplane then settles at the
recovery's 79 KCAS with 40 degrees of flap, full throttle, 139 hp, sinking
190 ft/min for as long as it is flown - outside the 100 ft/min that counts
as level, so it is never called recovered, and the height it loses is all
the flight's. Leaned (catalogue 0) on the FAA's curve it makes 147 hp at
13.0 to 1 and sinks 137 ft/min, also never recovered. On JSBSim's curve,
leaned, the engine made about 155 hp - the 6.6% that no engine has - and
sank 45. 139 hp full rich at 4,700 ft and 2,470 rpm is what the O-360
should make there (Gagg and Ferrar's 85% at 2,700 rpm, times the rpm). Its
sink falls with the speed: at 1,947 lb with 40 degrees of flap and full
throttle full rich at 4,800 ft it sinks 73 ft/min at 79 KCAS and climbs 61
at 72, 118 at 68 and 160 at 64. So a recovery that flies the least sink,
as PR #136's does, should recover it; on main's law it cannot be closed.
Not tried with #136, which is not merged.

`glideslope_cli figures`, the committed models, and the stall test were
run on linux-release; nothing is committed but this entry and the plan's
three items. The selftest hash does not move: no model changed.
