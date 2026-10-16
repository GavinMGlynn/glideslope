### From the review of #106: the flare light, heavy and in gusts; the go-around's climb and speed; the taxi stop pinned; one source for how she lands, 2026-10-06 — fixes, and a tail found

**What is wrong first.** **The AI cannot land in gusts, and two loadings
balloon.** Flown in moderate turbulence (below), ten of the fourteen
aeroplanes taught the approach balloon, bounce or come down hard - the
737-300 wrecked at 743 ft/min, the C172P, C182 and PA-28 striking their
airframes, the Cub rising 9.4 ft - and the Short S.23 is lifted over her
flare's height climbing and goes around. At her light loading the 737-300
climbs at 181 ft/min in her flare (her nose overshoots the flare's attitude
by two and a half degrees after the sink is arrested), and the Mosquito at
her heaviest at 20. The flare before the look-ahead failed all twelve of
these cases too, and five more (the 737-300 and A380 heavy, the Mosquito
light, the F-15C gusty and the B-2A heavy); holding the flare's attitude
within one to three degrees of where the nose is did not help. A tail.

**The sweep, in two tests.**
`every_aeroplane_lands_light_and_heavy_without_a_balloon_a_bounce_or_a_go_around`:
every aeroplane taught the approach (14) flown down it in calm air at her
lightest and heaviest landing loadings (the lightest her figures name; the
one named "landing" where there is one, else the heaviest; for the C172P,
C182 and PA-28, which name none, the figures' loading and the same with a
quarter of its fuel), her reference speed worked for the weight in
proportion to its root. Each must touch in the touchdown zone, unwrecked,
upright, climb nowhere in its flare, rise no more than half a foot, stop,
and never go around. 28 cases: 26 judged and pass, the 737-300 light and
the Mosquito heavy named in the test with their reasons, flown and shown.
Seen red: both, before they were named.

`every_aeroplane_flown_down_in_gusts_reaches_the_runway_or_goes_around`:
the same fourteen at the lesson's loading in gusty air - fifteen knots down
the runway with JSBSim's MIL-F-8785C turbulence at severity 3, flown five
knots fast (half a ten-knot gust factor, as the FAA's Airplane Flying
Handbook, FAA-H-8083-3C, chapter 9, adds) - asserting only that each
reaches the runway or goes around; how each lands is shown, not judged.
**Not the same case on every platform**: first run as one test with the
gusty cases judged and ten of them named, CI (run 37446481961) failed on
five of seven test jobs, all on the F-15C in gusts, which landed cleanly on
Linux, ballooned at 53 ft/min on Windows (clang, release and debug alike)
and was wrecked at 780 ft/min on macOS. Turbulence makes floating point's
differences into different gusts at the flare, so a gusty case judged
strictly is a test that sometimes tests its rule; the calm cases agreed on
every platform.

**The go-around** is now asserted to climb to its 500 ft and never to fly
slower than 1.05 times her landing stall: the Mosquito climbs to 501 ft and
is never slower than 108.6 knots against a 94.5-knot stall. That a gust on
an approach flown well makes no go-around is not asserted: in gusts the
Short S.23 is lifted over her flare's height climbing and goes around (on
Linux), and the gusty cases are not judged (above). Seen red with the
go-around's power held to three tenths: the Mosquito came down and struck
her airframe. The speed margin itself was not seen red: flown at nine
degrees more incidence the go-around was no slower, the attitude bound
holding the nose.

**The taxi stop is pinned** -
`a_pilot_taxiing_at_no_more_than_half_throttle_who_hands_over_is_stopped_and_above_it_is_not`:
every landplane taught the approach (13; the flying boat named) run up on
the runway to ten knots and handed over with the throttle at a half is
given the roll's lander and stopped; at six tenths it is given the plain
autopilot. Seen red with the bound made "under a half": 26 things wrong.
The existing hand-over and take-over tests (44 with the landing tests)
pass.

**One source for how she lands.** `sim::landing_speeds(data, model)` - her
approach speeds, or none without a stall speed, refusing unreadable figures
- is what the server's `controller_for` and the client's `Flight` both
call, and what the landed-by-hand test tells its controller from.
`how_she_lands_is_what_her_approach_is_flown_at_or_none_without_a_stall_speed`
walks the catalogue (16): fourteen told their approach's reference speed,
the 747-400 and F-22A none, and a model with no figures refused. The
server now says when it cannot read a model's figures, rather than
swallowing it; the client lets the error stop it, as its other reads of the
figures do. The client's `Flight` itself is not under test (no test builds
one). The server's comment on `learn_speeds`, left above `controller_for`,
is back where it belongs.
