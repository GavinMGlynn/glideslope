#!/usr/bin/env bash
# slow_ctest.sh - run tests as a slow CI runner would, before pushing them.
#
#   tools/slow_ctest.sh [PRESET] -R REGEX [ctest arguments...]
#
# **Why**: tests that run programs against each other - a server and its
# clients - were written and passed on a fast machine, and then failed on CI's
# debug runners, where a server falls seconds behind real time: a roll that
# depended on how far it had flown, a window's last frame before the button it
# was waiting for, a count of updates per second of wall clock. A test that
# takes time should take simulated time; this is how to find the ones that do
# not before CI does.
#
# It runs the chosen tests with every process pinned to one CPU, at the lowest
# priority, while that CPU is kept busy by a process doing nothing useful.
#
# **That is harsher than a CI runner, and on purpose.** It is the only setting
# found to reproduce what CI did: a four-player test that stopped its server
# before the last player had flown failed here as it did on CI, and passed at a
# CPU and a half and at one CPU alone - CI's runners are slow in more ways than
# the CPU (a cold disk, a server building its terrain), which pinning cannot
# imitate. So a test written to wait on events passes here, only slowly - each
# ctest timeout is 30 minutes - and one written against the clock fails. It is
# a check to run on a multi-process test before pushing it, not on every push;
# the nightly workflow repeats them on real runners. PRESET is linux-debug
# unless the first argument does not begin with '-'.

set -euo pipefail

preset="linux-debug"
if (($#)) && [[ "$1" != -* ]]; then
    preset="$1"
    shift
fi
if ! (($#)); then
    echo "usage: tools/slow_ctest.sh [PRESET] -R REGEX [ctest arguments...]" >&2
    exit 2
fi

cpu="$(( $(nproc) - 1 ))"
# The load that shares the CPU, stopped however this ends.
taskset -c "$cpu" sh -c 'while :; do :; done' &
load=$!
trap 'kill "$load" 2>/dev/null || true' EXIT

echo "slow_ctest: on CPU $cpu alone, shared with a busy loop, at the lowest priority"
taskset -c "$cpu" nice -n 19 ctest --preset "$preset" --output-on-failure --timeout 1800 "$@"
