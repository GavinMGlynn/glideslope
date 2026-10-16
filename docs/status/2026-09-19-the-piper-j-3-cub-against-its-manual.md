### The Piper J-3 Cub against its manual, 2026-09-19 — item done (CI run 35417893114)

The last of the three light aircraft. With it, Phase 5's "the light aircraft
fly to their figures" is done: the Cessna 182S, the Piper PA-28-180 and the
Piper J-3 Cub each land inside their tolerance on every figure recorded from
their handbooks, as the Cessna 172P does, forty-four figures in all across
the five aircraft with them.

**What is missing first:** the Cub's figures are few and loose. Its manual
and Piper's 1945 booklet give five that can be flown, in mph, none with an
altitude or an airspeed calibration, and only the climb with a weight; no
take-off distance, top speed or ceiling a flight here measures. Like the
Cessna 182S and the PA-28, it flies only from the catalogue - the client
still flies the Cessna 172P - and it has no visual model.

**JSBSim's J3Cub did not fly to the Cub's figures.** Its engine made its 65
hp at 2,800 rpm, 500 past the A-65's limit; its propeller was JSBSim's
generic fixed-pitch one, the Cessna 172P's numbers, so coarse that on 65 hp
it turned 1,590 rpm static, against the type certificate's 1,950 to 2,250;
its drag at zero lift was its wing section's and its gear's alone, and it
glided at 13.5 to 1, against 10; and its elevator moved 8 degrees each way,
not the type certificate's 34 up and 29 down, too little to raise the nose to
the wing's stall. `tools/make_j3cub.py` makes glideslope's from the pinned
files with each change listed and justified, and a test fails if the
committed files differ from what it makes.

| Figure | Manual | Range | glideslope |
| --- | --- | --- | --- |
| Static RPM, full throttle | 1950 to 2250 (A-691) | the same | 2060 |
| Climb, full load, 55 mph | 450 ft/min | ±10% | 437 |
| Cruise, 2,150 rpm | 73 mph, 63.4 KTAS | ±3 kt | 64.6 |
| Glide, 55 mph | 10:1 | ±10% | 9.63 |
| Stall | 38 mph, 33.0 KCAS | ±2 kt | 34.3 |

The stall is flown at 1,092 lb - the Trainer's instructor and pupil and full
fuel - not the 1,220 lb gross weight the others are: the manual names no
weight for its 38 mph, the booklet gives it as the Trainer's landing speed,
and at 1,220 lb it would take a lift coefficient of 1.85, more than the
Cub's flapless wing gives (the model stalls at 36 knots there).

**What the changes are, in short:** the booklet's empty weight and the type
certificate's seats, baggage and tank; its elevator travel; drag for the
fuselage, tail, struts and wires, and a light aircraft's induced drag; the
engine's 2,300 rpm limit; and the propeller's 74 in, its curves drawn in on
the advance ratio for a 45 in pitch and scaled for the static rpm, climb and
cruise.

**The figure flights learned two things.** A figures file can say an
aircraft has no flaps, and a figure asking for them is refused. And the
cruise flight stops leaning once the rpm has fallen 3% below its best: the
Cub's engine is at its best full rich, and leaned on down to half it
stopped. The Cessna 172P's cruise moved from 119.7 to 119.4 knots with it,
the PA-28's from 125.1 to 124.5.
