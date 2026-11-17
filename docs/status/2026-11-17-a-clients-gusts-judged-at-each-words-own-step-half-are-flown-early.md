# A client's gusts are judged against the server's, each word at its own step: about half are flown one to three steps early

**Still open, first.** A client's air is a step or more off the server's
for up to half its steps. Judged against what each word itself shows, 44 to
302 of 457 to 551 updates in air were flown one step early, at most three. The
air is asked at the moment the clocks' difference estimate says. That
estimate is the least difference over 50 words, and a word whose input
reached the server later than the fastest one was flown later there. No
client can know that before the word comes. The cure is the server flying
each input from the step its client names (branch
`12a-inputs-name-their-step`), not this item's. The 4 m worst of 2026-10-08
was not reproduced (worst here 1.20 m, in steady air). The item stays `[ ]`.

## What the test holds

**The headless client keeps, for every step it flies forward, the moment it
asked the air at and the estimate it flew with.** Each word from the server
shows its own clocks' difference: the server's step its input began on,
less the step this client began it on (`Prediction::began_at`). That needs
neither the estimate nor the input's send stamp. The step that difference
gives is the word's own step. Its moment is held against the word's
(`simulation_time_s`), and every update judged in air is one of three, said
as `gust clock:`:

- flown at the server's moment;
- flown on an estimate other than the word's own difference, out by exactly
  that much;
- out otherwise: a wrong gust clock.

`a_client_predicting_in_gusts_meets_each_at_the_servers_moment_within_the_steady_bound`
(`tests/cmake/server_weather_gusts.cmake`): 30G45 from the west, then
35G50 from the south-west from 12 s over 5 s, one predicting client through
a 250 ms relay. It holds:

- the three kinds add up to every update judged;
- at least 300 were judged;
- none was out otherwise;
- none was flown later than the server flew it, since the estimate is the
  least of the words' differences;
- the median error is within the steady air's 0.25 m.

**Not caught**: an estimate wrong early by a step. Steps a step early are
expected, in about half of updates.

| run   | at the moment | early (most) | later | out otherwise | median  |
|-------|--------------:|-------------:|------:|--------------:|--------:|
| 1     | 243 of 537    | 294 (3)      | 0     | 0             | 0.030 m |
| 2     | 242 of 542    | 300 (1)      | 0     | 0             | 0.032 m |
| 3     | 245 of 547    | 302 (1)      | 0     | 0             | 0.032 m |
| 4     | 306 of 551    | 245 (1)      | 0     | 0             | 0.032 m |
| 5     | 413 of 457    | 44 (1)       | 0     | 0             | 0.031 m |

**Seen red, then reverted**, each once:

- Prediction's offset biased a step late (`offsets_` given each
  difference plus one). This moves the placement and the air clock
  together, so the estimate alone cannot see it. The test went red: 360 of
  518 were flown later than the server.
- The client's air clock (cli `air_`) a step out. The test went red: 249 of
  428 were out otherwise.

The error alone does not tell a wrong gust clock. Run before this test's
final form, with the client's clock a step out the median was 0.034 m, as
right. Five seconds out it was 0.155 m, under the steady bound.

## The large worst errors: misplacement when the estimate moves, not the gusts

The large worst errors come from misplacement when the clocks' difference
estimate moves, not from the gusts. These figures are from the test's
earlier form, which sorted steps by whether the estimate had moved since
they were flown:

| air     | moved / compared   | worst otherwise | worst, moved |
|---------|-------------------:|----------------:|-------------:|
| gusting | 26 / 552 (1 step)  | 0.059 m         | 0.450 m      |
| gusting | 13 / 511 (1 step)  | 0.064 m         | 0.422 m      |
| gusting | 22 / 530 (1 step)  | 0.163 m         | 0.636 m      |
| steady  | 26 / 483 (1 step)  | 0.056 m         | 0.435 m      |
| steady  | 68 / 538 (3 steps) | 0.249 m         | 1.201 m      |

Six other runs had no move, and their worst was 0.058 to 0.064 m. Steady
air, the same METARs without gusts, shows the same: about 0.4 m for each
step moved, one step's travel at about 50 m/s. That is a word placed a step
off, not other air. The 2026-10-08 worst of 3.98 m was taken while that
run's difference was 15 steps from the one held. That fits the same cause,
larger, but it was not reproduced here, so it is not proven.

Searched: "deterministic turbulence seed shared clock client server
prediction tick offset". Unity Netcode and Colyseus both derive per-tick
effects from an agreed tick, and adjust the estimated prediction tick in
small steps. Neither makes the estimate exact without the input naming its
tick.

The selftest's hash does not move: it flies no server.
