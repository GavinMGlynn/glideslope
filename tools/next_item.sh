#!/bin/sh
# The next item to work on: the first unticked item in the lowest phase that
# has one. There is no judgement in this, which is the point - the phase order
# in COMPLETION_PLAN.md is not arbitrary, and choosing an item from a later
# phase because it happens to be near the code in hand is how that order gets
# quietly abandoned.
#
#   tools/next_item.sh          the next item
#   tools/next_item.sh --all    every open item, in plan order
plan="$(dirname "$0")/../docs/COMPLETION_PLAN.md"
if [ "$1" = "--all" ]; then
    awk '/^## Phase /{p=$0} /^## Tails/{exit} /^- \[ \]/{if (p != "") print p" | "$0}' "$plan"
    exit 0
fi
awk '/^## Phase /{p=$0} /^## Tails/{exit} /^- \[ \]/{if (p != "") {print p; print; exit}}' "$plan"
