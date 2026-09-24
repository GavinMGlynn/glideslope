# githooks_in_a_hook.cmake - the hooks' test, run as a git hook runs it,
# leaves the repository being pushed alone.
#
#   cmake -DTEST=<githooks.cmake> -DHOOKS=<.githooks> -DWORK=<scratch>
#         -P githooks_in_a_hook.cmake
#
# **Built, not hoped for.** The pre-push hook runs the quick tests, and git
# gives a hook GIT_DIR and GIT_WORK_TREE naming the repository being pushed.
# On 2026-09-24 githooks.cmake inherited them and made its commits there: two
# pull requests' branches were pushed with the tree deleted, and the
# repository was left bare, under the test's name. Here a decoy repository
# with one commit stands for the one being pushed; githooks.cmake is run with
# git's variables naming it, as a hook would have them; and afterwards the
# decoy must hold the one commit it had, its tree, and none of the test's
# configuration.

cmake_minimum_required(VERSION 3.28)

set(_decoy "${WORK}/decoy")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${_decoy}")
function(decoy)
    execute_process(COMMAND git ${ARGN} WORKING_DIRECTORY "${_decoy}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(DECOY_OUT "${_out}" PARENT_SCOPE)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "git ${ARGN} in the decoy failed:\n${_err}")
    endif()
endfunction()
decoy(init -q -b main)
decoy(config user.email decoy@example.com)
decoy(config user.name decoy)
file(WRITE "${_decoy}/kept.txt" "the decoy's own file\n")
decoy(add -A)
decoy(commit -q -m "the decoy's only commit")
decoy(rev-parse HEAD)
set(_head "${DECOY_OUT}")

# As a hook is run: git's variables name the repository being pushed.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "GIT_DIR=${_decoy}/.git" "GIT_WORK_TREE=${_decoy}"
            "GIT_INDEX_FILE=${_decoy}/.git/index"
            ${CMAKE_COMMAND} -DHOOKS=${HOOKS} -DWORK=${WORK}/scratch -P ${TEST}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the hooks' test failed when run as a hook runs it:\n${_out}${_err}")
endif()

decoy(rev-parse HEAD)
if(NOT DECOY_OUT STREQUAL _head)
    decoy(log --oneline -6)
    message(FATAL_ERROR "the hooks' test moved the decoy's branch:\n${DECOY_OUT}")
endif()
decoy(config --get core.bare)
if(NOT DECOY_OUT STREQUAL "false")
    message(FATAL_ERROR "the hooks' test set the decoy's core.bare to ${DECOY_OUT}")
endif()
decoy(config --get user.name)
if(NOT DECOY_OUT STREQUAL "decoy")
    message(FATAL_ERROR "the hooks' test set the decoy's user.name to ${DECOY_OUT}")
endif()
if(NOT EXISTS "${_decoy}/kept.txt" OR EXISTS "${_decoy}/a.txt")
    message(FATAL_ERROR "the hooks' test changed the decoy's files")
endif()
message(STATUS "run as a hook runs it, the hooks' test left the repository being pushed alone")
