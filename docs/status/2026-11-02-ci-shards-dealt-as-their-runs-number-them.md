### CI's test shards dealt as their runs number them, 2026-11-02

**What is still not true first.**
- **The Windows shards' times after this change are predicted, not yet
  measured.** The figures below are the dealing's own, from the cost tables
  over the Windows test lists of run 41415219089; the first full run of this
  branch measures them.
- **windows-debug's shards were not re-predicted.** They were dealt wrong in
  the same way (their runs leave out timing and nightly) and are fixed by the
  same change, at their 8 shards; their new figures were not worked out.
- **The 429 quit test's new wait has not yet run on CI** (below); it was
  seen red and green only on this machine.

**1. The dealing was undone by ctest's numbering.** `ctest -I` numbers its
tests among those `-LE` leaves, not over the build's whole list; the dealing
(`tools/ci_shard.cmake`) numbered them over the whole list. So from the first
test a run left out (the five nightly tests on a pull request, and timing as
well on a debug preset), every shard ran the test after the one it was dealt,
then two after, up to five: the last few hundred tests of every preset fell
where their numbers did, whatever they cost. Seen in run 41415219089: Windows
clang-cl's shard 3 was dealt 34.6 minutes of work and ran tests the table puts
at 46 - among them four F-35B take-over tests that hold the runner alone, 18
minutes - and was cut off at 45 with 482 of 490 done; Rocky 9's shard 4 ran
the same shifted list. Reproduced here: `ctest -N -I shard.txt -LE nightly`
over a linux-release build lists, for shard 4 of 4, tests 1172, 1178, 1181 ...
where the dealing gave 1171, 1176, 1180 ..., exactly the tests Rocky 9's
shard 4 started; without `-LE` it lists what was dealt.

The dealing now takes `EXCLUDE`, the run's `-LE` regex, and lists the tests
with it, so its numbers are the run's. Every test job of `ci.yml` passes it,
the same expression its ctest line has. A test it leaves out is no longer
weighed at nought by its label; what is listed is run, and costs its table.

**2. Windows release and clang-cl go from 3 shards to 4.** Even dealt right,
3 shards held 36.1 and 34.6 minutes of measured work each, past the 20 the
workflow aims for, with about 3 minutes of each job before and after ctest.
The full run's jobs peaked at 37 at once (run 41415219089, from its jobs'
start and end times); two more make 39, inside the plan's 40.

Predicted per shard, minutes of work at 4 at a time (the dealing's figures;
"as run" is the table's figure for what each shard of run 41415219089
actually started, serial tests at their whole time):

| preset | before, as dealt | before, as run | after (4 shards) |
|---|---|---|---|
| windows-release | 36.1, 36.1, 36.1 | 26.6, 31.6, 49.6 | 27.1 each |
| windows-clang | 34.6, 34.6, 34.6 | 26.5, 27.4, 46.2 | 26.0 each |

Measured, those shards took 25.4, 31.7 and 40.7 minutes (release) and 25.4,
19.6 and 45+ (clang-cl) of ctest. At 27 minutes of work, with 3 of job around
it, a shard has 15 minutes - a third - in hand of its 45.

**Verification.** `every_ci_test_shard_is_dealt_and_between_them_they_run_every_test_once`
(`tests/cmake/ci_shards.cmake`) now deals every sharded preset as a pull
request's run leaves tests out, and the first debug and release preset as
every other event's runs do as well (timing, and nothing): four ways, each
asserted done. For every shard it reads the `-I` file back with
`ctest -N -I ... -LE ... -FA ".*"` and fails unless the tests ctest lists are
the ones dealt (`shard_N.names`, new beside each file); and it asserts each
job's call of the dealing passes the EXCLUDE that matches its ctest line's
gate. Seen failing: with the dealing's `-LE` taken out, it fails at once on
linux-debug's shard 1 ("ctest runs other tests than were dealt it"), and
passes with it back (82.6 s, 41 shards, 8 dealings plus 2 other-event ones).

**3. The 429 quit test on Rocky 9 (job 125180276633) now waits on the
event.** "answered 0 requests 429, and 4 with FILE": the stub, which counts
every connection it accepts, was never reached by the refresh before the
client quit - the refresh had begun 37 ms before the client ended, five
frames of two ticks. Not #300's doing: #300 line-buffers standard error, and
the lines this test times from go to standard output, flushed as said; the
stub's count is of connections. Of the failed test jobs in the last 172 runs
of ci.yml and nightly.yml it is this test's only failure, and 20 loops here
passed (the refresh begun 34 to 44 ms before the quit ended). But a test that
only sometimes builds its situation is worse than none, so the situation is
now built by an event: `glideslope_http_stub --answered FILE` writes FILE once
it has answered its first request with its status (or begun its stalled
transfer), and the client's `--quit-once FILE` holds the quit at `--quit-at`,
flying on and drawing no shot, until FILE exists (two minutes at most). The
quit's flight waits on it; the control does not. The 2 s bound is unchanged,
timed from the quit as before.

**Verification.** Both quit tests on vulkan (429 and stalled), linux-release,
DISPLAY and WAYLAND_DISPLAY unset: 20 of 20 passed each. Seen red: with the
weather refresh delayed 300 ms before its first request (a sleep put into
`WeatherFetch`, removed after) and the quit's `--quit-once` taken out, the
429 test failed 20 of 20 on "the refresh was never turned away"; with the
delay and the wait, it passed 5 of 5.
