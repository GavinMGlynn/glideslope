### Landings that stay landed, 2026-09-24

**Every light aeroplane the AI landed was bouncing, and no test said so.**
The landing tests asked for a gentle first touch and a stop on the runway,
and never which way up she stopped. The rollout's stick went fully back the
moment the wheels touched, at nearly flying speed: the C172P went 53 ft back
into the air in calm air and came down 16 degrees nose down; the J3 Cub
stalled out of its bounce on to its back and still passed. Both light
aircraft landing tests now also require her to stay within 15 degrees of
bank, above 10 degrees nose down and under 3 ft from the touch to the stop,
and went red on the old lander before anything was changed. What
`sim::Lander` does now, each from a source:

- **The rollout holds the attitude she touched at, less up to two degrees
  while she is fast**, and brings the stick back as she slows, fully back by
  half the reference speed - the FAA's Airplane Flying Handbook
  (FAA-H-8083-3C) chapter 14 lands a tailwheel aeroplane with a little
  forward pressure to keep the main wheels down.
- **A bounce is flown with the flare's law**, not the rollout's: more than a
  foot above where the wheels met the runway, with no weight on them.
- **The round out asks for as much nose as the path needs**: one and a half
  times the change of flight path the sink error means at her speed, which is
  the old 0.02 degrees per ft/min for the Cub and a third of it for a
  Mosquito at 120 knots, which was asked for thirteen degrees in two and a
  half seconds and ballooned to 73 ft. The handbook (chapter 9) has the round
  out's rate proportional to the rate of closure. Climbing in the flare, the
  nose is held, never pushed.
- **Speed is flown mostly in proportion, with its trend fed back**, as an
  autothrottle flies it: 0.05 of the throttle a knot, an integral of 0.02 a
  knot a second, and 0.1 against each knot a second the speed is changing.
  The integral was 0.24 and set the Mosquito's throttles swinging from shut
  to full every six seconds down final; every aeroplane in the approach
  lesson now crosses the threshold within two knots of its reference speed.
  Below twice the flare height the throttle opens no faster than half its
  travel a second, and in the flare it closes over two seconds rather than
  in one step - the handbook reduces power gradually through the round out,
  and shutting both the Mosquito's throttles at once rolled her three
  degrees on the torque.
- **The aileron has a trim term** for what holds a steady bank against it,
  as the rudder already had.
- **The rollout steers gently while fast** - five degrees off the runway
  heading at most at the reference speed, twenty by half of it - **and with
  the brakes when the rudder runs out**: past half its travel the rudder
  brings in the brake on the side the nose is wanted and lets the other off.
  The Mosquito's castoring tailwheel steers nothing and her rudder could not
  hold a swing growing to 18 degrees a second; her Pilot's Notes give
  differential brakes from the rudder bar.
- **The approach flies the centreline by L1 guidance** (Park, Deyst and How,
  AIAA 2004-4900), with its look-ahead never inside the turn she can fly at
  25 degrees of bank. Steering by heading, 0.6 degrees a metre, a 787 handed
  the approach from a circuit S-turned 370 m either side of the line.

**The circuit harness slows on base**, to 1.4 times the landing stall as the
handbook's chapter 9 has it, rather than carrying the downwind leg's twenty
knots over the reference on to final. Verification: the light aircraft land
upright, on the centreline, in calm air and a 10-knot crosswind; the five
four light aircraft's and the Mosquito's circuits by the book leave empty
debriefs, the Mosquito touching 5.6 m from the centreline (it was 25.7) and
the others within 3.5 m.
