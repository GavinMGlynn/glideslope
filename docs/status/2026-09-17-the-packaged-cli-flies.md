### The packaged CLI flies, 2026-09-17

**Proved on every platform.** The `package` workflow's run 35223462263 (commit
`29ed2f3`) passed all five jobs, each unpacked copy flying the selftest to the
same hash as the CI build of its platform: `3bf3106980a5ad7b` from the Linux
tarball in both containers, `6d19773416ce7b3f` on macOS, `d9a3d51b8c2cdd52` on
Windows.

Every package's check now runs `glideslope_cli selftest` out of the unpacked
copy — the Linux tarball in the stock Rocky 9 and Ubuntu 24.04 containers, the
macOS tarball and the Windows zip from a separate directory — after
`--version` and `aircraft c172p`. The selftest needs the model, the tuned
propeller and the selftest log to have travelled with the program, and flies
five minutes with them. The workflow now also runs when anything under
`assets/` changes, since that is what a package carries.
