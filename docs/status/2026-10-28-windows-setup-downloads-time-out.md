### A Windows job's driver download that hangs is tried again, 2026-10-28 - item done

**What is still not true first.** Only the setup's two driver downloads
(Mesa's lavapipe and LunarG's Vulkan runtime) are limited and retried; the
other steps of the Windows setup (`msvc-dev-env`, `setup-python`) are not,
and none has been seen to hang.

**What was wrong.** Three Windows jobs on 2026-10-28 were cancelled by their
45-minute limit with no test run: run 40640191712 (windows-clang tests 2/3),
40649601697 (windows-release build) and 40664057362 (windows-release tests 1/3
and windows-debug tests 5/7). In each, the job's log ends at the start of
`.github/actions/windows-setup`'s "Install a software Vulkan driver" step,
some 44 minutes before the cancel: an `Invoke-WebRequest` with no time limit
that never returned.

**What now works.** Each download is limited to 120 s and tried up to four
times, 10, 20 and 30 s apart; the fourth failure fails the step with the
URL and its reason. The files are tens of megabytes and take seconds.

**Verification.** The retry function, its limit cut to 5 s and two tries,
run on the Windows development machine in Windows PowerShell 5.1 against
`http://10.255.255.1/x` (an address that never answers): each try timed out
("The operation has timed out"), it was tried again, and it gave up with its
reason after 11.2 s; the same function fetched `https://github.com/`. The
runners' PowerShell 7 has the same `-TimeoutSec`. This PR's own full CI run
installs the driver on every Windows job.
