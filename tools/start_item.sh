#!/usr/bin/env bash
# start_item.sh - begin an item: its branch, pushed, with its pull request open.
#
#   tools/start_item.sh BRANCH "Title of the pull request"
#
# **Why**: CI runs on pull requests, not on pushes to a branch, so a branch
# pushed without its pull request is tested by nothing. This makes the branch
# from an up-to-date main, pushes it, and opens its pull request as a draft at
# once - every push after this one is tested. When CI passed is green, mark it
# ready and merge it:
#
#   gh pr ready && gh pr merge --rebase --delete-branch
#
# CLAUDE.md: each item on its own branch, into main by pull request when CI is
# green everywhere.

set -euo pipefail

if (($# != 2)); then
    echo 'usage: tools/start_item.sh BRANCH "Title of the pull request"' >&2
    exit 2
fi
branch="$1"
title="$2"

if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    echo "start_item: commit or put away what is changed first" >&2
    exit 2
fi
git fetch --quiet origin main
git switch --quiet -c "$branch" origin/main
# A pull request needs a commit that main has not got; an empty one says
# what the branch is for until the work arrives.
git commit --quiet --allow-empty -m "Begin: $title"
git push --quiet -u origin "$branch"
gh pr create --draft --base main --head "$branch" --title "$title" \
    --body "Begun with tools/start_item.sh; CI runs on every push from here."
echo "start_item: $branch is pushed, and its draft pull request is open"
