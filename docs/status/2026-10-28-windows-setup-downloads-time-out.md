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

**What now works.** Each download is given 120 s to connect and be answered
(`-TimeoutSec`, which in PowerShell 7.4 and later is `-ConnectionTimeoutSeconds`
and nothing more) and may go 60 s without a byte of its body
(`-OperationTimeoutSeconds`, which alone bounds a download that stalls
part-way; without it there is no limit). It is tried up to four times, 10, 20
and 30 s apart; the fourth failure fails the step with the URL and its reason.
The files are tens of megabytes and take seconds. Which of the two phases hung
in the three runs is not known: each log ends at the step's start.

**Verification.** The retry function, its limit cut to 5 s and two tries,
run on the Windows development machine in Windows PowerShell 5.1 against
`http://10.255.255.1/x` (an address that never answers): each try timed out
("The operation has timed out"), it was tried again, and it gave up with its
reason after 11.2 s; the same function fetched `https://github.com/`. That
exercises the connect limit only: a body stalling part-way was not built, and
Windows PowerShell 5.1 has no `-OperationTimeoutSeconds` - the runners'
PowerShell 7.4+ has (the pr-reviewer's reading of Microsoft's 7.4 documentation
and PowerShell PR #19558). This PR's own full CI run installs the driver
through the new code on every Windows job.
