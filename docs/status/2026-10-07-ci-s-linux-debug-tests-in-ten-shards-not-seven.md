### CI's Linux debug tests in ten shards, not seven, 2026-10-07 — fix

Shards of seven kept running past the 30-minute job limit (shard 7 of #108's run;
shard 3 of #107's, #108's and #109's before the slow orbit tests were split in
#111), because the costs of the newest tests are estimates and the debug runner is
about 4.2 times slower than the development machine. Ten shards bring each back
under the two-thirds target. Measured costs from a green run should replace the
estimates (the open CI-costs item).
