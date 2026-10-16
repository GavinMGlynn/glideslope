### Windows builds take vcpkg's packages from GitHub Packages, 2026-09-30 — tail still open

*Since made public: every Windows job restores its 39 packages from the
feed ("A slow Windows configure was a new runner image", above).*

**What is still missing, first**: the packages are private until each is made
public, and until then they are no use: a personal account's private packages
have 500 MB of storage and 1 GB of transfer a month on the free plan, then are
blocked. Main's first build after this lands uploads them (26-28 minutes a
Windows configure, once); then each is made public - by the packages API if
its undocumented visibility change works, otherwise by the owner, once per
package. The restore after that is not yet seen. Linux and macOS keep the Actions cache keyed on the toolchain: vcpkg runs
nuget.exe under Mono there, which ubuntu-24.04 and macOS no longer carry
(microsoft/vcpkg#37540), and their packages are small (about 20 MB).

**Why.** The owner chose (2026-09-30) Microsoft's recommended vcpkg cache for
GitHub Actions, a NuGet feed in GitHub Packages, over the Actions cache: it
holds one package per ABI hash, so a new runner image's compiler rebuilds and
uploads once and every later job on that image restores, while jobs still on
the old image find theirs; it is free for a public repository and outside the
Actions cache's 10 GB, and free once public. The owner made a classic token (read:packages,
write:packages) and stored it as the repository secret `VCPKG_PAT`;
`GITHUB_TOKEN` cannot read or write vcpkg's packages there.

**What changed.** A composite action, `.github/actions/vcpkg-github-packages`,
fetches nuget.exe 6.14.0 (pinned by SHA-256), writes a NuGet config of its own
in the runner's temporary directory with the feed, the token and
`defaultPushSource`, and sets `VCPKG_BINARY_SOURCES` to
`clear;nugetconfig,<that file>,readwrite;nugettimeout,600`. Without the secret
it warns and vcpkg builds from source. CI's three Windows builds and the
Windows package use it; their vcpkg Actions-cache steps are gone. Only main
writes (`readwrite`); a pull request reads, as with every other cache. vcpkg
names each package's repository from `GITHUB_REPOSITORY`, which links it to
this repository - its access, not its visibility (review found this; GitHub's
access-control documentation says so). ci.yml passes its secrets to
package.yml (`secrets: inherit`), and a change under `.github/actions/` now
counts as one packaging must check.
