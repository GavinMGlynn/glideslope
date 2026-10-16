### Packaging, 2026-09-17

**Proved on every platform.** The `package` workflow's first run on `main`
(35199775868, commit `4b94984`) passed all five jobs — build the Linux tarball,
run it on Rocky 9, run it on Ubuntu 24.04, build and run the macOS tarball,
build and run the Windows zip. Each unpacked
`glideslope_cli --version` printed `glideslope_cli 0.1.0`, matching its file
name, and each checksum verified:

- the Linux tarball, built on Rocky 9, in stock `rockylinux/rockylinux:9` and
  `ubuntu:24.04` containers;
- the macOS tarball, whose program depends on `/usr/lib/libc++.1.dylib` and
  `/usr/lib/libSystem.B.dylib` only;
- the Windows zip, whose program depends on `KERNEL32.dll` only.

CI on the same commit (35199775840) was green in all four jobs.

**Watched to fail.** A throwaway branch removed the static runtime line and the
workflow was run on it by hand (35199809443): the Windows job alone went red,
with `dumpbin` listing `VCRUNTIME140.dll` and the step saying the packaged
program depends on the Visual C++ redistributable. The branch was deleted.

`cpack --preset linux-release` (and `macos-release`, `windows-release`) makes
`glideslope-0.1.0-<platform>.tar.gz` — `.zip` on Windows — with a SHA-256 file
beside it. It holds one folder of the same name with `glideslope_cli`,
`LICENSE` and `README.md` side by side. On Windows the C++ runtime is linked
into the program (`CMAKE_MSVC_RUNTIME_LIBRARY`), so a zip unpacked on a machine
without the Visual C++ redistributable still runs.

`.github/workflows/package.yml` runs on demand and when the build files change:

- **Linux:** built on Rocky 9 with gcc-toolset-14, then run in a stock
  `rockylinux/rockylinux:9` container and a stock `ubuntu:24.04` container,
  neither with a toolchain.
- **macOS:** built, unpacked into a different directory, and run.
- **Windows:** built, unpacked into a different directory, run, and read with
  `dumpbin /dependents`, which must not list `vcruntime` or `msvcp`.

Every run checks the checksum file, and requires `glideslope_cli --version` to
print exactly the version in the package's file name.

Before the workflow first ran, the same was checked by hand on Rocky Linux 10:
the tarball is 28 KB, unpacks to the one folder with its three files, and the
unpacked `glideslope_cli --version` prints `glideslope_cli 0.1.0` and exits 0.
