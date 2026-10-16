### The plan's open tails triaged, 2026-10-06

**What this is.** A documentation change only, approved by the owner on
2026-10-06: no code moved, no ticked item changed, and no open item was lost.
`COMPLETION_PLAN.md` had 74 open items; it now has 51 open in the current goal
(50 kept and one new) and 12 in a new final section, "Later - not part of the
current goal". The plan's "finished when" command still reads only the phases,
and `tools/next_item.sh` stops at Tails, so neither sees the new section.

**Counts.** 74 open before = 12 moved to Later + 10 folded into one new item
+ 2 merged into another + 50 kept (48 as they were, 2 with text added).
After: 51 open in the current goal (the 50 and the new item), 12 in Later.
A script compared the item titles of `main` and of this branch: the 165 ticked
items are identical, each moved item's text is unchanged, each folded or
merged one is named below, and every other open item is still there.

**Moved to Later** (kept `[ ]`, wording unchanged): terrain over the whole
Earth; weather seen as it is; thermals and lee waves; the F-35B hover,
vertical landing and short take-off; signed and notarised macOS builds; one
Linux download; a hosted public server; free buildings; a livery and moving
control surfaces; the aeroplane lit by a baked light; the Learjet 35A drawn as
nothing; a flight flying one station's weather wherever it goes.

**Folded** into "A month of clean nightly runs" (each needed only runs watched
or counted): the four-player test once counting five players (a hundred
Windows debug runs owed); the command-line forger test once failing after
300 s; taking over at 100 ms not refused once; the window client stepping over
2.5 m at a take-over (a pass on windows-release owed); Windows debug test
programs crashing on exit on the development machine; the window client
crashing on exit on Windows; the Cesium cache locking (nine of ten suite runs
owed); the slow weather request on the Windows development machine; two
window clients on a cold Cesium cache stalling; CI failing more often than it
passes (the month counted). Its verification is 30 consecutive nightly runs
with none of them failing, or each cause found and fixed under its own item.

**Merged.** "The window client is sometimes put right 20 to 31 m after a long
frame" into "A server that falls behind real time puts its clients' prediction
off by metres" (since PR #89 what remains came once the clocks were known, the
server-behind-real-time cause; the unexplained 9 to 13 m loaded runs are now
that item's, and the month of CI runs with the 20 m bound is counted in the
nightly item). "The AI leans for best power, but climbs only the Cherokee to
its ceiling" into "The light aeroplanes' engines make most power far too
rich" (its remaining work is the per-aeroplane engine items, each already with
the ceiling to reach).

**Left where they were, though they wait on runs.** CI's cache and timing
measurements (vcpkg rebuilds, 90-120 minute runs, the Windows compiler cache,
the cost tables, the 30-minute Windows configure) need a measurement or a
table edit, not only a count of nightly runs; and "A client whose every frame
is slow..." and "The client with the window does not blend corrections..."
name tests still to write.
