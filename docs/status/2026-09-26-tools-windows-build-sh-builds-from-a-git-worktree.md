### `tools/windows_build.sh` builds from a git worktree, 2026-09-26 — tail done

**Cause.** The script fetched the commit into the Windows working copy from
this repository's working tree, reached as `\\wsl.localhost\...`. In a git
worktree, where agents work, `.git` is not a directory but a file saying
`gitdir: /home/gavin/Development/glideslope/.git/worktrees/<name>` - a Linux
path, which Git on Windows cannot follow: `fatal: not a git repository:
/home/gavin/Development/glideslope/.git/worktrees/...`, then `Could not read
from remote repository`, before anything was built.

**Now** it fetches from the repository's common git directory
(`git rev-parse --path-format=absolute --git-common-dir`), reached the same
way. Every worktree keeps its objects there, so the commit is found whichever
working tree made it; in the main working copy the common directory is its own
`.git`, so nothing changes there.

**Verified** from an agent's worktree
(`.claude/worktrees/agent-...`), with the script as committed: before the
change it stopped at the fetch with the error above; after it,
`WINDOWS_TEST="^a_program_that_has_ended_can_still_allocate_until_its_last_thread_is_gone$"
tools/windows_build.sh` fetched the branch into `C:\Development\glideslope`,
built windows-debug with MSVC, ran the test (1 of 1 passed) and printed
`windows_build: built`. cmd.exe's "UNC paths are not supported" notice, from
starting in a `\\wsl.localhost` directory, is harmless: the batch file changes
to the Windows working copy first.
