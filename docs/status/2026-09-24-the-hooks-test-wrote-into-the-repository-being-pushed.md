### The hooks' test wrote into the repository being pushed, 2026-09-24 — tail done

**What happened.** The pre-push hook runs the quick tests, and one of them,
`the_pre_commit_hook_refuses_an_unfinished_commit_and_nothing_else`, makes a
scratch repository and commits its cases into it. Git gives a hook `GIT_DIR`
and its like, naming the repository being pushed. The test inherited them, and
every git command it ran acted on that repository instead. Pushed from an
agent's worktree, where `GIT_DIR` is always set, it:

- put its five commits on top of the branch being pushed - the tree deleted,
  and `a.txt` in its place - which then went up with the push. PRs #4 and #5
  were both pushed like that;
- set the shared repository `core.bare = true`, with the test's name and
  address as its identity and the hooks path pointing into one agent's
  worktree.

Nothing reached `main`, which is protected, and no commit of ours carries the
test's identity. PR #5's reviewer found it: the pull request would have
emptied `main`.

**What was done.**

- The configuration was put back by hand.
- `tests/cmake/githooks.cmake` clears every variable
  `git rev-parse --local-env-vars` names before it starts. It then checks that
  git is working in its scratch repository before writing anything, and at the
  end that the four commits it let through are there.
- `.githooks/pre-push` clears the same variables before it builds and runs the
  tests.
- PRs #4 and #5 were put back to their real commits by force-push.

**Verified** by
`the_hooks_test_run_from_a_hook_changes_its_scratch_repository_and_not_the_one_pushed`
(`tests/cmake/githooks_in_a_hook.cmake`). It runs the hooks' test with git's
variables naming a decoy repository, as a hook would have them. Afterwards the
decoy must still have its one commit, its file, and no bare flag or foreign
name. With the clearing removed it goes red: git was working in the decoy.
The pre-push hook's own clearing is not tested separately.
