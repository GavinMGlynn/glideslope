### The server's AI aircraft, 2026-09-22 — item done

**What is missing first: they all fly one plan.** There is one flight plan in
the data, a tour of Sydney Harbour, so every AI aeroplane flies it. They are
stacked 500 ft apart rather than put in one piece of sky, which says what is
happening instead of hiding it. More plans, and aircraft that fly different
ones, are not built.

**An AI aeroplane is an aeroplane with its controller set to the AI.** It goes
through `sim::Controller::to_ai(plan)`, the same path a player's aircraft
takes when it is handed over, and its controls each step are what
`Controller::fly()` returns - the autopilot steered by the navigator. Nothing
on the server bypasses that, which is what keeps "the LLM plans, the
controllers fly" true at this end.

**Verified**,
`the_server_runs_the_number_of_ai_aircraft_it_is_given_and_four_when_given_none`:
asked for two it runs two, asked for one it runs one, and asked for nothing it
runs four. The count is read back out of the server's own report and cross-
checked against the number of aeroplanes it says it flew, so a server that
printed a number and made a different number would fail. **Watched failing**:
one that ignores `--ai` and always makes four is told "asked for 2 AI aircraft
and it ran 4".

**Seen flying**: three AI aircraft at 2,933, 3,429 and 3,929 ft over ground at
72 ft off Bondi, each tracking north up the plan's first leg.

**A CMake list cannot hold a list.** The test's cases were written
`"2;2" "1;1" ";4"` and flattened into six elements, so every case read one
number and the first comparison failed. They are `"asked:want"` now.
