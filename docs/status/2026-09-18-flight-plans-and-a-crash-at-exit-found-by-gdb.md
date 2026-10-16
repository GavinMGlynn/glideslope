### Flight plans, and a crash at exit found by gdb, 2026-09-18 — item done (CI run 35363959900)

**What is missing first:** a plan cannot yet be given to the client - that is
`--autopilot`, the last item of Phase 4 - so plans fly only in the tests. The
navigator flies straight legs and passes a waypoint when it is abeam, turning
only then, so it overshoots each turn and comes back to the leg; it knows no
holds, procedure turns, speed or altitude constraints, or fly-by turns.

**A plan is data** (`assets/plans/*.plan`, read by `sim::parse_flight_plan`):
the aircraft it is for, where it starts in the air, and its waypoints with
their altitudes and airspeeds, one to a line; anything it cannot read is
refused by its line. **The navigator** (`sim::Navigator`) flies each leg along
its great circle, steering for the leg's track where the aircraft is abeam
of it, back towards the leg by 30 degrees a kilometre off it, and into the
wind by the drift it measures; it gives the autopilot the waypoint's altitude
and airspeed.

**The test:** the Cessna flies the Sydney Harbour plan - off Bondi, the Heads,
the Harbour Bridge, Olympic Park, the airport: 45 km, turns of 90 and 135
degrees, climbs and descents of 500 ft. In calm air it passes the waypoints
0.4, 23, 0.3 and 4.8 m off, each at its altitude to the foot; in a 20 kt
wind from the south, 0.4, 30, 0.4 and 4.7 m. **Watched to fail:** the drift
not corrected, 372 m off the bridge in the wind; the intercept turned away
from the leg; and no waypoint ever passed.

**Found on the way: the packaged client's crash was a race at exit.** It
segfaulted once in the ubuntu:24.04 container, and not when run again. Flown
under gdb since, it crashed again (package run 35347082677) and left every
thread's stack: one of the terrain's worker threads was in OpenSSL, inside a
download, while the main thread was in `exit()` tearing the libraries down
under it. Cesium Native's pending work holds its task processor - the
terrain's pool of worker threads - for as long as that work lives, so the pool
outlived the terrain, and its threads, never joined, ran on into the process's
exit. The terrain now stops and joins its workers when it is destroyed -
once they are idle and the main thread has run everything their work left
for it, which may give them more: stopped sooner, what they had finished lay
unrun in Cesium Native's queue, and the sanitized build caught 15 MB of
decoded imagery leaked that the running threads had been keeping in reach.
Even then, CI's Ubuntu run (35351306516) leaked the same 54 imagery tiles at
exit: work waiting on the cache's own SQLite thread, which the worker pool does
not see, was abandoned when the workers stopped, and held Cesium Native's
imagery cache in a cycle. Every request is now counted from its asking to its
answer, the count going down on a worker so that what follows is queued before
the pool can look idle, and the terrain waits for none to be in flight too.
