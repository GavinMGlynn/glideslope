### The living documents, honest, 2026-09-17 — Phase 0 complete

Every living document was re-read in full against the repository at the end of
Phase 0. What that pass changed:

- this file: a summary of the finished phase; the compiler gate's CI evidence
  moved from the warnings entry, where it had landed, to the gates entry; a
  hand check in the packaging entry relabelled as what came before the workflow;
- `RELEASES.md`: what a package is and what each operating system will say. It
  had said there was nothing to download; the workflow now keeps a package per
  platform as an artifact. What macOS and Windows say about a *downloaded*
  copy is stated as expected, not as seen, because no downloaded copy has been
  opened yet;
- `CLAUDE.md` and `README.md`: how to make a package.

`TRANSPORT.md`, `THREATS.md` and `GUIDE.md` still say their subject does not
exist, which is still true. `ASSETS.md` still says nothing third-party is used,
and `ext/README.md` that nothing is pinned; both true. `FEATURES.md` marks
nothing `DONE`, because nothing on it is a thing a player could use yet.

**The verification**, taken literally: a fresh agent with no other context was
given this file alone, told not to open anything else, and asked what works
today, what does not, how the working parts were proved, and what comes next.

**Its answer was right on every point** when checked against the repository and
the CI runs: the CLI's three behaviours and the version-only library; no flight
model, state restore, terrain, renderer or server; each item's proof by platform
and compiler, including that most deliberate breakages were made locally and
that the sanitizer claim rests on one small GCC program; no downloaded package
opened yet; and Phase 1 next.

It also named what it could not tell from the file alone, and this pass fixed
each: the summary listed six things beside "7 of 7"; the CI table had five rows
under "four jobs"; the package workflow's five jobs were not named; "gearstick",
"the brief", "presentation" and "tails" were not explained; and it was not
always clear which proofs were local and which were CI. The corrected file has
not been re-read by a fresh agent.
