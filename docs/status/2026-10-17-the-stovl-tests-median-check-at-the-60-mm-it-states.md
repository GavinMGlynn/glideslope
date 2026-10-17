### The STOVL test's median check at the 60 mm it states, 2026-10-17 — a fix

**What is still not true first.** The median bound catches no planted bug,
as #214 recorded; the pitch check is what catches the pitch-integral bug.

#214 (eddfe993) set the heavy F-35B STOVL test's median bound to 60 mm in
its header and its failure message, but the check itself still compared
with 15: a median from 16 to 60 mm, which CI has shown on unbroken code
(28 mm on windows-clang, run 38842431267), would have failed it with a
message saying "over 60 mm". Found by #210's rebase, which copied the
sibling's bounds. The check is now 60 (tests/cmake/server_stovl_landing.cmake).
Run here in linux-release: passed, 124 s.
