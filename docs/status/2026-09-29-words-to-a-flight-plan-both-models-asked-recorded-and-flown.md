### Words to a flight plan: both models asked, recorded and flown, 2026-09-29 — item done

Claude and ChatGPT each planned "take off, climb to 3,000 ft and orbit the
CBD", and CI flies both plans from their recorded answers with no key. The
detail - models, calls, cost, the navigator's orbit join, and what was seen
to fail - is in "Words to a flight plan, 2026-09-25", below.

**A live model call is made only when asked for.** The two tests that ask a
model now, `take_off_climb_to_3000_ft_and_orbit_the_cbd_is_planned_by_{openai,anthropic}_now_and_flown`,
cost money, and a key on the machine is not consent to spend it. They
report themselves skipped unless `GLIDESLOPE_LIVE_MODEL=1` is set, checked
before anything else. They are labelled `live`, and the pre-push hook runs
its quick tests with `-LE live` and without the variable: a skipped test
measures as quick, so both were on its list. To ask the models and
record them again:

```sh
GLIDESLOPE_LIVE_MODEL=1 ctest --preset linux-debug -L live
```

Each writes its recording under `build/linux-debug/tests/plans_flown/`, to be
read by eye and copied to `tests/data/copilot/`. No other test can call a
model: the copilot's unit tests ask a stand-in for each service.
Seen to skip: the script run with a command and no playback, and without
the variable, exits 77 before it reads anything else.
