### Ubuntu had quietly stopped being checked, 2026-09-21

**A job that runs out of time is reported as cancelled, not failed.** It
therefore does not look like a red build, and the Ubuntu job had been ending
that way for hours without anyone treating it as a problem - including in
this session, where it was noted three times as "cancelled, not failed" and
left alone.

Measured rather than guessed: the job ran **60 min 17 s** and **60 min 15 s**
against a `timeout-minutes: 60`, twice. A third cancellation really was a
push cancelling it at 33 minutes, which is what made the other two look like
the same thing.

**Why that job and not the others.** Ubuntu builds both presets and runs the
whole suite twice, the sanitized one included; Rocky 9 builds
`linux-release` alone, which is why it finishes in 14 to 17 minutes doing
what looks like the same work. The suite has grown from 282 tests to 319 in
this session, and Ubuntu crossed the hour on the way.

Ubuntu is now given 150 minutes. The other three were at 44 to 47 minutes of
their 60, which is the same cliff a little further off, and are given 90.
