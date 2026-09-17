# Releases

**There are no releases yet.** Nothing has been tagged or published, and there
is nothing in a download that flies: the only program is `glideslope_cli`, which
reports its version.

## What a download is

The `package` workflow (`.github/workflows/package.yml`) builds one package per
platform and keeps it as a workflow artifact:

| Platform | File | Built on |
| --- | --- | --- |
| Linux x86_64 | `glideslope-<version>-linux-x86_64.tar.gz` | Rocky Linux 9, GCC 14 (gcc-toolset-14) |
| macOS arm64 | `glideslope-<version>-macos-arm64.tar.gz` | macOS 15, AppleClang |
| Windows x64 | `glideslope-<version>-windows-x64.zip` | Windows, MSVC |

Each has a `.sha256` file beside it, and unpacks to one folder of the same name
holding `glideslope_cli`, `LICENSE` and `README.md`. Nothing is installed; the
program runs from wherever the folder is.

Before an artifact is kept, the workflow unpacks it somewhere other than where it
was built and runs it: the Linux tarball in stock Rocky Linux 9 and Ubuntu 24.04
containers with no toolchain, and the macOS and Windows packages in a separate
directory on their own runner.

## What each operating system will say

- **Linux:** nothing. The tarball needs a C library at least as new as Rocky
  Linux 9's and the system's C++ runtime, both of which a desktop install has.
- **Windows:** the program is not signed, so a copy downloaded through a browser
  may draw a SmartScreen warning. It needs no Visual C++ redistributable: the
  runtime is linked in, and the workflow checks the program does not ask for it.
- **macOS:** the program is not signed or notarised, so macOS refuses to run a
  copy downloaded through a browser until it is allowed in System Settings,
  under Privacy & Security, or its quarantine attribute is removed with
  `xattr -d com.apple.quarantine glideslope_cli`. Signing and notarisation are a
  tail in `COMPLETION_PLAN.md`.
