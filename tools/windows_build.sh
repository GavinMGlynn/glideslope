#!/usr/bin/env bash
# windows_build.sh - build this branch on Windows, from WSL, before pushing it.
#
#   tools/windows_build.sh [PRESET] [TARGET...]
#   WINDOWS_TEST=REGEX tools/windows_build.sh ...   and then run those tests
#
# PRESET is a Windows configure/build preset, windows-debug unless given; the
# TARGETs, if any, are built instead of everything. What it does:
#
#   1. refuses unless this working copy has no uncommitted changes;
#   2. refuses unless the Windows working copy has no changes to tracked files,
#      then fetches this commit into it straight from this repository's git
#      directory - the common one, so a git worktree works too - through
#      WSL's network path - before it is pushed, which is the point: it is
#      how code that is not Linux's alone is compiled before CI sees it;
#   3. builds it with MSVC, through vcvarsall, as CI's Windows jobs do, and
#      runs the tests WINDOWS_TEST names, if it names any.
#
# **Why it exists**: the Windows code paths - sockets, windows, anything under
# #ifdef _WIN32 - were first compiled by CI, after they had been pushed to
# main, and broke it. Code only CI compiles is code nobody has compiled.
#
# The first configure of a Windows build directory builds Cesium Native's
# dependencies through vcpkg, which takes most of an hour; after that they come
# from vcpkg's binary cache.
#
# WINDOWS_CLONE overrides where the Windows working copy is
# (default /mnt/c/Development/glideslope).

set -euo pipefail

preset="${1:-windows-debug}"
shift || true
targets=("$@")
clone="${WINDOWS_CLONE:-/mnt/c/Development/glideslope}"
here="$(git rev-parse --show-toplevel)"

branch="$(git -C "$here" branch --show-current)"
commit="$(git -C "$here" rev-parse HEAD)"
if [[ -n "$(git -C "$here" status --porcelain --untracked-files=no)" ]]; then
    echo "windows_build: commit first - there are uncommitted changes here" >&2
    exit 2
fi

if [[ ! -d "$clone/.git" ]]; then
    echo "windows_build: no Windows working copy at $clone" >&2
    exit 2
fi
if [[ -n "$(git.exe -C "$(wslpath -w "$clone")" status --porcelain --untracked-files=no)" ]]; then
    echo "windows_build: the Windows working copy has changes to tracked files; not touching it" >&2
    exit 2
fi
win_clone="$(wslpath -w "$clone")"
# This repository, as Windows reaches it: \\wsl.localhost\<distro>\... It is
# fetched from the repository's git directory, not its working tree: in a git
# worktree, where agents work, .git is a file naming the real directory by its
# Linux path, which Git on Windows cannot follow. Every worktree keeps its
# objects in the common git directory, so the commit is there whichever
# working tree made it. Git on Windows refuses the directory as owned by
# somebody else; the upload-pack that reads it is a process of its own, so it
# is told it is safe itself, for this fetch.
repo_unc="$(wslpath -w "$(git -C "$here" rev-parse --path-format=absolute --git-common-dir)")"
git.exe -C "$win_clone" fetch --quiet \
    --upload-pack='git -c safe.directory=* upload-pack' "$repo_unc" "$commit"
git.exe -C "$win_clone" checkout --quiet -B "$branch" "$commit"
git.exe -C "$win_clone" submodule update --quiet --init --recursive

vswhere="/mnt/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
vs="$("$vswhere" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
      -property installationPath | tr -d '\r')"
if [[ -z "$vs" ]]; then
    echo "windows_build: no Visual Studio with the C++ tools" >&2
    exit 2
fi

build_args=""
if ((${#targets[@]})); then
    build_args="--target ${targets[*]}"
fi
batch="$(mktemp --suffix=.bat -p "$(wslpath "$(cmd.exe /c 'echo %TEMP%' 2>/dev/null | tr -d '\r')")")"
cat > "$batch" <<EOF
@echo off
call "$vs\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64 >nul || exit /b 1
cd /d "$win_clone" || exit /b 1
cmake --preset $preset || exit /b 1
cmake --build --preset $preset $build_args || exit /b 1
EOF
if [[ -n "${WINDOWS_TEST:-}" ]]; then
    echo "ctest --preset $preset --output-on-failure -R \"$WINDOWS_TEST\" || exit /b 1" >> "$batch"
fi
echo "windows_build: $branch at ${commit:0:7}, preset $preset, in $win_clone"
status=0
cmd.exe /c "$(wslpath -w "$batch")" || status=$?
rm -f "$batch"
if ((status != 0)); then
    echo "windows_build: FAILED ($status)" >&2
    exit "$status"
fi
echo "windows_build: built"
