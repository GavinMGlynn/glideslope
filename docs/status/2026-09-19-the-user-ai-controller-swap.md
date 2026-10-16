### The user/AI controller swap, 2026-09-19 — item done (CI run 35372183417)

**What is missing first:** the swap across the network is Phase 7's; here
it is one client's.

**The controller** (`sim::Controller`) is who flies an aircraft: its pilot,
through the controls their hands and devices set, or the AI pilot - the
autopilot, with a navigator when it has a plan. Handed to the AI, the
autopilot engages from the controls the aircraft has. Handed back, the pilot's
hands are seldom where the AI had the controls, so the controls move from the
AI's towards the pilot's at a hand's pace - full travel in a second - until
they meet, and follow the pilot's directly from then.

**The test:** the test pilot flies the Cessna through six phases - the takeoff
roll, the climb, the cruise, a 30-degree turn, the descent and a flapped
approach - hands it to the AI for ten seconds and takes it back. To the AI, no
control moves more than 0.0022 of its travel in a step. Back, the controls
meet the pilot's in 0.09 to 0.69 s, moving 1/120 of their travel a step at
most, and are the pilot's exactly after; the load factor changes by 0.033 g in
a step at most over the three seconds after either hand-over. **Watched to
fail:** handing back by jumping to the pilot's controls - the controls moved
up to 1.1 of their travel in a step, and the load factor 0.36 g in the climb,
0.24 in the descent, 0.13 on the approach and 0.08 in the turn; on the ground
it cannot move, which is why the controls are checked too.
