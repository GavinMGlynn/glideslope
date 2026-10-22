### The take-over back to the F-35B left waits on events, 2026-10-23

**What is still not true first.** `--take-over-after` and
`--next-aircraft-after` still name a time; it is now only the earliest the
client asks, and each waits on the answer before it. A W pressed while a
take-over is unanswered is held, as A is, and said only on standard output.

**The failure.**
`the_client_with_the_window_at_a_loading_taking_over_an_f35b_loaded_otherwise_flies_her_again_at_that_loading`
on Ubuntu linux-debug, run 39847179061 (PR #236), job 120074638549. The client
joined in aircraft 0, took over the AI's aircraft 4, then at 9 s rode along in
"aircraft 0" and at 12 s asked to take it over: refused.

**The times were the client's own fixed steps** (`ticks`, 120 a second, from
the frame loop's start): flight time on this machine, which a slow pass
outruns - building the F-35B taken over took a 3,079 ms pass there, and the
next pass flew past 9 s at once.

**Why it was refused.** A take-over gives the aircraft left an AI's number
(`free_number`: 0 became 5, "a number below the players' is never an
AI's"). The server had no aircraft 0. The client chose 0 because
`Online::heard` returned early on the update that handed it aircraft 4,
before taking that update in for everybody else: aircraft 5 was not added,
aircraft 0 not dropped, until the next update - and W (the ride along) was
pressed in between, from the sky as it was before the take-over. A second
race sat behind it: W pressed before the answer at all, the next aircraft
past 4 is none, so the client went back to its own, and the second take-over
(which needs something ridden along in) was never asked.

**The fixes.**
- `src/frontend/client/online.cpp`: every update heard is taken in for
  everybody (`take_in_everybody`), the one that gives a take-over or a join
  again included.
- `src/frontend/client/main.cpp`: W while a take-over is unanswered is held,
  as A already was, and pressed once the answer (taken, refused or given up)
  is in and the sky it gives has been read. `--take-over-after` given again
  waits for the answer to the one before.
- `tests/cmake/client_loading.cmake`: the ride along is asked in the very
  pass of the first take-over (`--next-aircraft-after 6`), so it is always
  held, and the second take-over (`--take-over-after 6` again) follows the
  answer and the ride. It requires W held and then ridden in a number neither
  the one given up nor its own, and that number taken over.

**Seen red, locally (linux-debug, load average 37 to 46 from other work):**
- the old client with the new test: W pressed unanswered went "back in your
  own aircraft", one take-over only - flights `37530:vertical_landing;45400:as
  her model is`.
- the W hold without the `Online` fix: "W, held, is pressed now, aircraft 4
  this client's own / riding along in aircraft 0 / the server refused to take
  over aircraft 0" - CI's failure, made every time.
- both: passed, "the one she left, aircraft 5 now, ridden along in once the
  take-over was answered, taken over, and made again at 37530 lb".

The ride-along, take-over, A-held and hand-over window tests pass with it.
