#!/bin/sh
# The next item to work on: the first unticked item in the lowest phase that
# has one. There is no judgement in this, which is the point - the phase order
# in COMPLETION_PLAN.md is not arbitrary, and choosing an item from a later
# phase because it happens to be near the code in hand is how that order gets
# quietly abandoned.
#
#   tools/next_item.sh          the next item
#   tools/next_item.sh --all    every open item, in plan order
#
# **An item before the first `## Phase` is not phase work and is skipped.**
# Three tails once landed in the preamble - the script that inserted them
# looked for the "## Tails" heading with a plain string search and found the
# one inside the preamble own `sed` example instead - and this named them as
# the next thing to do.
plan="$(dirname "$0")/../docs/COMPLETION_PLAN.md"
if [ "$1" = "--all" ]; then
    awk '/^## Phase /{p=$0} /^## Tails/{exit} /^- \[ \]/{if (p != "") print p" | "$0}' "$plan"
    exit 0
fi
awk '/^## Phase /{p=$0} /^## Tails/{exit} /^- \[ \]/{if (p != "") {print p; print; exit}}' "$plan"
