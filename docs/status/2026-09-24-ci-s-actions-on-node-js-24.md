### CI's actions on Node.js 24, 2026-09-24

Every run warned that `actions/cache`, `actions/upload-artifact`,
`actions/download-artifact` (all `@v4`) and `ilammy/msvc-dev-cmd@v1` target
Node.js 20, which GitHub deprecated on 2025-09-19 and now forces onto Node 24.
Each `action.yml` was read at its newest major: `actions/cache@v6`,
`upload-artifact@v7` and `download-artifact@v8` declare `node24`, and so does
`actions/checkout@v5`, which stays. What changed between the majors and
touches these workflows: `download-artifact@v5` changed where a single artifact
downloaded *by id* lands - every download here is by name or pattern, so
nothing moves - and `@v8` fails on a digest mismatch rather than warning, which
is wanted. `upload-artifact@v7` adds unzipped single-file uploads, off unless
asked for. The cache majors changed their runtime and packaging, not keys.

`ilammy/msvc-dev-cmd` has no Node 24 release - its last is v1.13.0, of
2024-01-01, and four pull requests to move it sit unmerged - so it is replaced,
not bumped: `.github/actions/msvc-dev-env` finds Visual Studio with vswhere,
runs `vcvarsall.bat x64` and exports what it changed, as the action did, in
PowerShell the runner already has.

Verified on the branch's runs 35940490213 (30 jobs) and 35942681309 (32
jobs), and the package workflow's 35940494208: no annotation names Node.js 20,
and every Windows job builds in the environment the new action sets up.
