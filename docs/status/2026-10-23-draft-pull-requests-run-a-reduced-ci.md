### Draft pull requests run a reduced CI, 2026-10-23

**What is still not true first.** Not yet seen on real runs: the draft run
and the ready run of this branch's pull request are recorded below when
they finish.

**Why.** Every push to a draft ran the full matrix: about 50 jobs and 980
runner-minutes (run 39846357752). Agents push many times while iterating,
and most of those runs are thrown away. The owner (GitHub Pro: 40
concurrent jobs, 5 macOS) approved the change for throughput.

**What changed** (`.github/workflows/ci.yml`):

- `pull_request` triggers on `ready_for_review` as well as the default
  `opened`, `synchronize`, `reopened`, so marking a draft ready starts the
  full run.
- On a draft (`github.event.pull_request.draft`, which every
  `pull_request` payload carries, `synchronize` included): every build job
  runs, and the flights are compared; the tests run for linux-debug on
  Ubuntu and windows-debug on Windows only. Rocky's, macOS's, Ubuntu
  linux-release's, Windows release's and clang-cl's tests, the server image
  and the packages are skipped.
- A job-level `if` cannot read the matrix, so the preset rows a draft skips
  moved to jobs of their own: `ubuntu-release-test` (linux-release) and
  `windows-release-test` (windows-release, windows-clang), their steps
  repeated as `macos-debug-test`'s are. `tests/cmake/ci_shards.cmake` reads
  them unchanged: 8 sharded jobs, 37 shards, every test in exactly one.
- **CI passed fails on a draft**, with "draft run: reduced matrix; mark
  ready for the full run". A job skipped by its `if` reports success and
  does not block a merge even when required ([GitHub: control jobs with
  conditions](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-jobs-with-conditions)),
  so a draft's skipped jobs must not count. On a ready pull request it
  aggregates every job as before, the two new jobs added.
- **CI draft passed** (job `draft-green`) aggregates the draft's subset:
  every build, linux-debug's and windows-debug's tests, the flights
  compared, and the change detection. It is not required by main's
  protection.
- **Concurrency**: unchanged. A draft run and a ready run share the group
  `ci-refs/pull/N/merge-pull_request`, and `cancel-in-progress` cancels the
  running or pending run of the group when a new one is queued ([GitHub:
  control workflow concurrency](https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/control-workflow-concurrency)):
  marking ready cancels the draft's run in progress, and nothing a draft
  does afterwards can cancel the ready run, since `converted_to_draft` is
  not a trigger ([GitHub: events that trigger workflows, pull_request](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#pull_request)).

CLAUDE.md, `tools/start_item.sh` and `package.yml`'s header say the new flow:
iterate on the draft until CI draft passed is green, mark it ready, wait for
the full run's CI passed, then merge. CLAUDE.md also says an agent pushes
once per fix round.

**Verification.** `ci_shards.cmake` run against the new workflow: "8 sharded
jobs and presets, 37 shards: every shard dealt, every test in exactly one".
The real runs: pending.
