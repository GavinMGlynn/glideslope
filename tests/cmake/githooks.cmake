# githooks.cmake - the pre-commit hook refuses what it says it refuses, and
# lets the rest through.
#
#   cmake -DHOOKS=<.githooks> -DWORK=<scratch> -P githooks.cmake
#
# A scratch repository with the hook turned on, and one commit tried for each
# rule it has: a debugging marker, a change left unstaged, the same split let
# through on purpose, a key, and a tick in the plan with and without the status
# document beside it - and a plain commit, which must go through. Every case is
# counted, and the count is held to the number of cases.

cmake_minimum_required(VERSION 3.28)
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/docs")

function(git)
    execute_process(COMMAND git ${ARGN} WORKING_DIRECTORY "${WORK}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(GIT_RC ${_rc} PARENT_SCOPE)
    set(GIT_ERR "${_err}" PARENT_SCOPE)
endfunction()

git(init -q -b main)
git(config user.email test@example.com)
git(config user.name "hook test")
git(config core.hooksPath "${HOOKS}")
file(WRITE "${WORK}/a.txt" "one\n")
file(WRITE "${WORK}/docs/COMPLETION_PLAN.md" "- [ ] **An item.**\n")
file(WRITE "${WORK}/docs/PROJECT_STATUS.md" "status\n")
git(add -A)
git(commit -q -m "a plain first commit")
if(NOT GIT_RC EQUAL 0)
    message(FATAL_ERROR "a plain commit was refused:\n${GIT_ERR}")
endif()

set(_cases 0)
# expect(<refused|accepted> <what>) - the commit just tried went that way.
# (`${how}`, not `how`: in a macro an argument is text substituted, not a
# variable, and `if(how ...)` compared a variable nobody set - so the first
# draft of this test checked nothing, and was seen to pass with the hook
# broken.)
macro(expect how what)
    math(EXPR _cases "${_cases} + 1")
    if("${how}" STREQUAL "refused" AND GIT_RC EQUAL 0)
        message(FATAL_ERROR "the hook let through ${what}")
    elseif("${how}" STREQUAL "accepted" AND NOT GIT_RC EQUAL 0)
        message(FATAL_ERROR "the hook refused ${what}:\n${GIT_ERR}")
    endif()
    git(reset -q --hard HEAD)
endmacro()

file(APPEND "${WORK}/a.txt" "std::printf(\"TEMPDBG %d\", x);\n")
git(add a.txt)
git(commit -q -m "a marker")
expect(refused "a TEMPDBG marker")

file(APPEND "${WORK}/a.txt" "x = 1; // DELIBERATE BUG\n")
git(add a.txt)
git(commit -q -m "a marker")
expect(refused "a DELIBERATE BUG marker")

file(APPEND "${WORK}/a.txt" "two\n")
file(APPEND "${WORK}/docs/PROJECT_STATUS.md" "more\n")
git(add a.txt)
git(commit -q -m "half of it")
expect(refused "a commit with a tracked file changed and not staged")

file(APPEND "${WORK}/a.txt" "two\n")
file(APPEND "${WORK}/docs/PROJECT_STATUS.md" "more\n")
git(add a.txt)
set(ENV{GLIDESLOPE_PARTIAL} 1)
git(commit -q -m "half of it, meant")
unset(ENV{GLIDESLOPE_PARTIAL})
expect(accepted "a split commit with GLIDESLOPE_PARTIAL=1")

file(APPEND "${WORK}/a.txt" "key = AIzaSyA1234567890abcdefghijklmnopqrstuv\n")
git(add a.txt)
git(commit -q -m "a key")
expect(refused "a Google API key")

file(WRITE "${WORK}/docs/COMPLETION_PLAN.md" "- [x] **An item.** Done.\n")
git(add docs/COMPLETION_PLAN.md)
git(commit -q -m "ticked alone")
expect(refused "a tick in the plan without PROJECT_STATUS.md")

file(WRITE "${WORK}/docs/COMPLETION_PLAN.md" "- [x] **An item.** Done.\n")
file(APPEND "${WORK}/docs/PROJECT_STATUS.md" "the item, done\n")
git(add -A)
git(commit -q -m "ticked with its status")
expect(accepted "a tick with PROJECT_STATUS.md beside it")

file(APPEND "${WORK}/a.txt" "three\n")
git(add a.txt)
git(commit -q -m "plain")
expect(accepted "a plain commit")

if(NOT _cases EQUAL 8)
    message(FATAL_ERROR "${_cases} cases were tried, and there are 8")
endif()
message(STATUS "the pre-commit hook: all 8 cases went the way they should")
