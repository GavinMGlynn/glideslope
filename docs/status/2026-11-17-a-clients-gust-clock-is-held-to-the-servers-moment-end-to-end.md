# A client's gust clock is held to the server's moment, end to end; the worst errors are the clocks' difference moving

**Still open, first.** A step a client flies before its clocks' difference
moves is flown at the moment the old difference said: out by as many steps as
it moved (1 to 3 here), in up to one compared update in eight. Nothing on the client can know
that sooner. The fix is the server flying each input from the step its
client names (branch `12a-inputs-name-their-step`), not this item's. The
4 m worst of 2026-10-08 was not reproduced (worst here 1.20 m, in steady air). The item
stays `[ ]`.

## What now works

**The headless client keeps, for every step it flies forward, the moment
it asked the air at, and the clocks' difference it flew with.** When the
server's word for that step comes, it holds the moment against the word's
own step (`simulation_time_s`) and sorts every update compared in air into
one of three kinds, printed as `gust clock:`:

- flown at the server's moment;
- flown before the clocks' difference moved, out by exactly as many steps
  as it moved since;
- out otherwise: a wrong gust clock.

`a_client_predicting_in_gusts_meets_each_at_the_servers_moment_within_the_steady_bound`
(`tests/cmake/server_weather_gusts.cmake`): 30G45 from the west, then
35G50 from the south-west from 12 s over 5 s, one predicting client
through a 250 ms relay. It holds that the three kinds add up to every
update compared in air, that at least 300 were at the server's moment,
that none was out otherwise, and that the median is within the steady
air's 0.25 m.

**Seen red, then reverted**, with the client's air clock (cli `air_`)
wrong:

| clock          | at the moment | out otherwise   | median  | worst   |
|----------------|--------------:|----------------:|--------:|--------:|
| right          | 556 of 556    | 0               | 0.032 m | 0.059 m |
| a step out     | 13 of 555     | 542             | 0.034 m | 0.299 m |
| five s out     | 0 of 530      | 530             | 0.155 m | 0.475 m |

(A step out, the 13 at the moment were flown just before the clocks'
difference moved back by the same step.)

A step out leaves the median as it was (0.034 m), and five seconds out
leaves the median under the steady bound. So the error alone does not tell
a wrong gust clock; the moment does.

## The large worst errors: the clocks' difference moving, not the gusts

The large worst errors come from the clocks' difference moving, not from
the gusts. In each run, every update with a large error was flown before
the clocks' difference moved by a step:

| air     | run | moved / compared | worst at the moment | worst, moved |
|---------|----:|-----------------:|--------------------:|-------------:|
| gusting | 1   | 0 / 533          | 0.059 m             | -            |
| gusting | 2   | 26 / 552         | 0.059 m             | 0.450 m      |
| gusting | 3   | 0 / 536          | 0.059 m             | -            |
| gusting | 4   | 13 / 511         | 0.064 m             | 0.422 m      |
| gusting | 5   | 22 / 530         | 0.163 m             | 0.636 m      |
| steady  | 1   | 26 / 483 (1 step)| 0.056 m             | 0.435 m      |
| steady  | 2   | 0 / 543          | 0.058 m             | -            |
| steady  | 3   | 0 / 538          | 0.058 m             | -            |
| steady  | 4   | 68 / 538 (3 steps) | 0.249 m           | 1.201 m      |
| steady  | 5   | 0 / 551          | 0.064 m             | -            |

Steady air (the same METARs without gusts) shows the same: 0.435 m when
the difference moved one step, and 1.201 m when it moved three. That is
about 0.4 m for each step, one step's travel at about 50 m/s: a step
misplaced, not other air.
The 2026-10-08 worst of 3.98 m was taken while that run's difference was
15 steps from the one held, and at 0.4 m a step that is 6 m: consistent with the
same cause, larger - not reproduced here, so not proven.

Searched: "deterministic turbulence seed shared clock client server
prediction tick offset". Unity Netcode and Colyseus both derive per-tick
effects from an agreed tick, and adjust the estimated prediction tick in
small steps. Neither makes a moving estimate exact without the input
naming its tick.

The selftest's hash does not move: it flies no server.
