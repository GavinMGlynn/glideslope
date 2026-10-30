### The flying boat's gust touch traced: she touches flat because she is fast, and raised to her trim she balloons, 2026-10-30 — item still open

**What is still missing, first**: all of it. The S.23 still touches flat in
gusts and skips, and stays named in the gust tests. This round traced why and
tried three changes to the gust flare for a hull; none passes, and nothing in
the code changed. Docs only.

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
4.5 degrees without her wing flying her off it, and floated off as in calm
air it takes about 20 s, a kilometre - past the touchdown zone. Raised early
to bleed it, she balloons. The open choices, for the owner: (a) a hull's
approach in gusts flown with less of the gust additive (the seaplane
handbook lands at the slowest possible speed; the additive is the
landplane's AFH ch. 9 rule); (b) a hull's touchdown zone judged on open
water differently from a runway's; or (c) the flare's power used to hold
her off at a constant height while she slows, as the rough-water landing
is flown (FAA-H-8083-23, ch. 6), with the flare-climb limit read against
that hold.

**Verification**: none landed. The figures above are from the gust tests'
own output (`in_gusts_every_aeroplane_not_named_lands_within_its_limits_seed_1`
to `_8`, linux-release) on main and with each change built in, and a
per-step trace of her height, attitude, speed and sink added to the test
for the run and taken out again.
