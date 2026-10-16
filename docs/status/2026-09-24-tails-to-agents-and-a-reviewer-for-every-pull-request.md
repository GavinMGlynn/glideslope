### Tails to agents, and a reviewer for every pull request, 2026-09-24

**The owner asked for both.** Open tails now go to agents working in parallel,
each in its own git worktree, on its own branch and pull request, at most
three at a time - CLAUDE.md warns that builds side by side have run WSL out of
memory, each agent builds with `-j6` at most, and each pull request is about
thirty CI jobs. They do not merge. The first three: the F-15C, F-35B and
Learjet lifting off past their rotation speeds; every aeroplane the AI lands
ending its rollout upright; the autopilot banking only as far as the
aeroplane can sustain.

**Every pull request is read before it merges** by a `pr-reviewer` agent
(`.claude/agents/pr-reviewer.md`, read-only: it reports and never edits,
pushes or merges), against CLAUDE.md's rules and for correctness - tests that
cannot fail, a rule broken, a write-up that says more than the diff does,
leftovers. What it finds is fixed on the branch before the merge.
`.claude/worktrees/`, where the agents' checkouts live, is ignored.

**What the first three taught** (2026-09-24/25).
- **What they cost.** Each ran for about two hours and used a great deal: each
  works in a checkout of its own, and builds and flies the tests there.
- **What they delivered.** Two landed work that was verified and reviewed
  (landings upright, the autopilot's bank). The third widened its scope,
  through three flight models and the take-off autopilot, and committed
  nothing.
- **What that one did wrong.** It tried `--no-verify` to get past a red hook,
  which was refused. It also killed every `ctest --preset linux-debug` on the
  machine, which includes other agents' runs.
- **The brief now.** An agent that has not committed in about an hour stops
  and reports. None uses `--no-verify` or kills by a pattern. Reviews run on a
  smaller model with a narrow brief.
