### The status log is one file per item, 2026-10-16 — item done

**What it is.** The log that was the "Log, newest first" section of
`PROJECT_STATUS.md` (409 entries, some 30,000 lines) is now `docs/status/`, one
file per entry, named `YYYY-MM-DD-first-words-of-the-title.md`. `ls -r
docs/status` lists it newest first. `PROJECT_STATUS.md` keeps the header, the
honest summary, the gaps and the known risks; the section that held the
completion plan's detail on 2026-09-23 is
`docs/status/2026-09-23-detail-from-completion-plan.md`. There is no index: a
line added at the top of one file would conflict on every merge.

**Why.** An agent reads the entries it needs instead of a 33,000-line file, and
two items no longer edit the top of the same file.

**How it was split.** By a script that cut the section at each `### ` heading
and wrote each entry verbatim. The date is the last one in the heading; one
entry has none (the F-15C's lift from NASA's 3/8-scale drop model) and took
2026-10-09 from the next older entry. The script asserts that the files, joined
newest first, reproduce the section line for line apart from blank lines
between entries (a few entries were separated by none or by two).

**What changed with it.** CLAUDE.md's living-docs rule says an item adds its own
`docs/status/` file, plus the summary when what works changes. The pre-commit
hook accepts a plan tick with a `docs/status/` file as well as with
`PROJECT_STATUS.md`. The pr-reviewer agent reads the new place.
`COMPLETION_PLAN.md` and the code comments that cite "(PROJECT_STATUS,
2026-10-14)" are left: the date still finds the file.

**Verification.** The hook's test gained two cases, a second tick refused with
neither file and the same tick accepted with a `docs/status/` file; it was run
against the old rule (the status case refused, red) and against the new
(10 of 10 as they should go).
