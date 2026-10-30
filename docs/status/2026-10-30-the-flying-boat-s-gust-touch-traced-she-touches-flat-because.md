### The flying boat's gust touch traced: she touches flat because she is fast; held off as the seaplane handbook holds her, gusts climb her past the flare's limit, 2026-10-30 — item still open

**What is still missing, first**: all of it. The S.23 still touches flat in
gusts and skips, and stays named in the gust tests. This round traced why,
tried three changes to the gust flare for a hull, then the seaplane
handbook's hold-off in six variants; none passes, and nothing in the code
changed. Docs only.

**Reproduced on main** (253cb8fa, linux-release, the gust test's eight
seeds, her figures as the test prints them; rise limit 1.0 ft less 0.25):

| seed | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| touched, ft/min | 217 | 226 | 74 | 167 | 195 | 262 | 50 | 162 |
| rose, ft | 8.93 | 4.13 | 0.95 | 10.04 | 4.80 | 2.54 | 0.89 | 1.80 |

Seeds 1 and 4 are wrecked coming back down (768 and 644 ft/min). In calm air
she still holds the step (0.35 ft against 0.5).

**Why she touches flat.** In calm air her flare closes the power and floats
her from 80 to 72 kt over about eight seconds, the nose coming up to 4.2
degrees, and she touches at 72 kt, sinking 70 ft/min. In gusts she flies her
approach at 92 kt (her reference speed plus half the gust factor), the flare
holds half her power to the touch and aims at 150 ft/min, and its gust law
lets the nose down whenever the sink's trend shows a climb coming - down as
far as the attitude the flare began at, about -1.4 degrees. Traced on seed 1
she met the water at 86 to 90 kt and 0.6 degrees. **The flat touch is the
speed**: at 88 kt her wing carries her at that attitude, and raised to her
calm trim at that speed she climbs.

**Tried, each on all eight seeds** (`sim::Lander`'s flare, for a hull in
gusts only; touch attitude and speed as she met the water):
- **No power held and her calm touchdown sink** (the landplane's gust
  power-on law taken off a hull): she touches at 1.1 to 4.3 degrees and 77
  to 90 kt, still too fast; rises 0.49 to 8.48 ft, seed 1 wrecked.
- **And a floor on her attitude rising to 4.5 degrees as her keel nears the
  water** (TN 923's lower trim limit; over the flare's last half): she
  touches at 3.1 to 5.2 degrees and 70 to 85 kt; rises 0.00 to 3.51 ft,
  five of eight over the limit; she now climbs 91 to 173 ft/min in her
  flare on seven (the limit 100, less 25). On seed 1 the rise was a skim in
  the hold-off - her keel met the water at 83 kt and 3.4 degrees and she
  rose 1.3 ft over it before her touch.
- **The floor over the whole flare**, held rather than raised while a gust
  climbs her: she touches at 3.7 to 5.5 degrees and 68 to 80 kt; rises 0.00
  to 1.88 ft, three over (seed 1 at 0.81, and seeds 2 and 4, touched at 224
  and 225 ft/min); but she climbs 94 to 211 ft/min in her flare, all eight
  over. The nose raised to her trim at 88 to 92 kt is a balloon.

**What that says.** Her touch trim and her touch speed go together: the
gust additive (12 kt here) has to be gone before she can be on the water at
4.5 degrees without her wing flying her off it. Raised early to bleed it, she
balloons.

**The book's technique, tried: a hold-off above the water.** FAA-H-8083-23
(2004), ch. 6, "Landing" (p. 6-2): "the major objectives are to touch down
at the lowest speed possible, in the correct pitch attitude"; the touchdown
attitude "typically is very close to the attitude for taxiing on the step";
"Normal Landing" (p. 6-3): "As the seaplane approaches the water's surface,
smoothly raise the nose to the appropriate pitch attitude for touchdown";
"Rough Water Landing" (p. 6-7): "it may be better, however, to level off
just above the water surface and increase the power sufficiently to
maintain a rather flat attitude until conditions appear more acceptable,
and then reduce the power to touch down", and "make the touchdown at a
somewhat flatter pitch attitude than usual. This prevents the seaplane from
being tossed back into the air". (Its part 3, chapters 4 to 6, from faa.gov,
`faa-h-8083-23-3.pdf`, SHA-256 `91208b29...44c4`; not yet in docs/ASSETS.md.)

Built for a hull in gusts only (`sim::Lander`'s flare): the flare's sink law
flown to a hold height over the water instead of to it, the power closed
and none held, until her airspeed - smoothed over 2 s, so that a lull does
not end it - is down to her reference speed, the gust additive gone; then
the calm flare's law to the water from there. Measured on the eight seeds
(rise limit 0.75 ft with its margin, flare climb 75 ft/min with its):

| hold-off variant | touch attitude, deg | touch speed, kt | rose, ft | flare climb, ft/min |
|---|---|---|---|---|
| keel held 3 ft up, the speed unsmoothed, the gust balloon law off in the hold (as in the next two) | 3.1-5.4 | 70-88 | 0.00-3.51, 4 over | 47-213, 7 over |
| keel held 6 ft up, the speed unsmoothed | 3.0-5.4 | 70-81 | 0.09-3.17, 3 over | 39-229, 7 over |
| 6 ft, speed smoothed | 3.5-5.8 | 63-80 | 0.00-1.56, 4 over | 83-258, all 8 over |
| 6 ft, the balloon law kept above the hold height | 3.1-5.2 | 70-86 | 0.00-4.75, 4 over | 97-205, all 8 over |
| 6 ft, the balloon law in the hold with the nose kept within 1 deg of the highest it held | 2.9-5.0 | 72-86 | 0.00-4.07, 3 over | 82-205, all 8 over |
| the same, within 2 deg, twice the balloon law's gain | 2.1-4.9 | 72-87 | 0.00-3.89, 5 over | 29-201, 4 over |

**The hold-off's conflict, measured**: her nose comes up to her trim as she
slows, and she touches near it on most seeds - but held level in moderate
turbulence, unpowered, a vertical gust climbs her 80 to 260 ft/min, past the
flare's 75; let her nose down to stop it (the balloon law) and she meets the
water fast and flat again. The rough-water passage above is the book's own
answer to that conflict - power held, flatter at the touch - and it is the
opposite of the calm trim. Not solved; what would decide it: whether the
flare-climb limit reads a gust's climb in a level hold-off as a balloon,
and whether a hull in gusts touches at the book's flatter rough-water
attitude rather than her calm trim. And the first round's choices stay
open: (a) a hull's approach in gusts flown with less of the gust additive
(the book lands at the lowest speed possible; the additive is the
landplane's AFH ch. 9 rule); (b) a hull's touchdown zone judged on open
water differently from a runway's - the hold-offs above touched 444 to 732 m
along, inside it.

**Verification**: none landed. The figures above are from the gust tests'
own output (`in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_8`, linux-release) on main and with each change built in, and a
per-step trace of her height, attitude, speed and sink added to the test
for the run and taken out again.
