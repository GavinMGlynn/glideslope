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
# It runs the chosen tests with every process pinned to one CPU and at the
# lowest priority, while that CPU is kept busy by a second process doing
# nothing useful - roughly a quarter of a shared, slower runner. PRESET is
# linux-debug unless the first argument does not begin with '-'.

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
taskset -c "$cpu" nice -n 19 ctest --preset "$preset" --output-on-failure "$@"
