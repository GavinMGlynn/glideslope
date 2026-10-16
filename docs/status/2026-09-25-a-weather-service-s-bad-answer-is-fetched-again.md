### A weather service's bad answer is fetched again, 2026-09-25 — tail done

**Found by CI.** Three tests failed at once on a pull request that had not
touched the weather. The weather service had answered a 200 whose body was not
JSON (`JSON at byte 0: not a value`), and the client stopped there. A download
that failed was already tried again; an answer that arrived and did not parse
was not.

**Now** both fetches, the METAR from aviationweather.gov and the forecast from
Open-Meteo, fetch an answer that does not parse again: three tries, a second
apart and then two. After the last it is reported as a download that failed
(`could not download ...: its answer was not JSON`), which is what it is, and
what a test without the network recognises. A well-formed answer that says
there is no report is not retried.

**Verified** by `an_answer_that_is_not_json_is_fetched_again_and_then_taken_for_a_failed_download`.
A stand-in for the network answers HTML twice and then a recorded report, for
each service, and each is read on the third try. Answering nothing but HTML,
each ends as a download that failed after three fetches. Four cases, counted.
With one try, it goes red.

**Also:** the network check's floor on updates compared came down from 150 to
24, two a second over the time compared. A Windows clang runner compared 147,
because the hand-over and take-back leave some twelve seconds of the twenty to
compare, and a slow server sends fewer updates a second.
