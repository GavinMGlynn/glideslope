### Cesium Native's log off the client's standard output, 2026-09-21 — a fix

CI went red on Windows (run 35506857255) with

    no altitudes at tick 600 in the trace: trace tick 600 time 5.0000 lat
    -33.94[2026-09-20 11:23:17.706] [error] [SqliteCache.cpp:592] database is
    locked

**A log line had been written into the middle of one of the client's own.**
Cesium Native logs through spdlog, whose default logger writes to standard
output; the client's `--trace` goes to standard output too, and the test reads
it from there. The trace line was cut after `lat -33.94` and every number
after it was gone. `gfx::log_to_standard_error()` now puts the log on standard
error, where what goes wrong belongs, and the client calls it first thing.

Seven families of test parse the client's standard output and were open to
this - the HUD's two, the aircraft chosen, the AI's flight plan, the sky's
two and the floating origin's - and the same commit adds the check to the one
that failed: no line of the client's standard output carries a stamp, which a
log line does and none of ours does. **It was watched to fail**, with the
logger deliberately put back on standard output and a line written through
it: "something logged to the client's standard output, where its own output
goes".

The lock itself is not a fault: up to four tests run at once against one
Cesium cache file, SQLite refuses the write, and Cesium Native logs it and
carries on. That the tests share one cache with nothing serialising them is a
tail in `COMPLETION_PLAN.md`.

Verified locally: the five client tests on Vulkan pass, and the HUD test fails
with the deliberate bug in place.
