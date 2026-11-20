# this_machines_tool.cmake - the tool a build named, or the same tool on this
# machine's PATH where the build's path is not here.
#
#   include(this_machines_tool.cmake)
#   this_machines_tool(CXX)     # CXX: a variable holding a tool's full path
#
# **CI builds on one runner and tests on another**, and while the runners'
# images change, the MSVC a build named may not be on its tester (train #387:
# built with 14.51 where the tester had only 14.52), so every test that
# compiles with it failed to start: "is not a full path to an existing
# compiler tool". Then the tool of the same name on the PATH, which the job's
# developer environment put there, said so - not skipped. A name that is no
# full path, or a path that is here, is left as it is; a tool found in neither
# place fails the test, naming both.

function(this_machines_tool _var)
    set(_tool "${${_var}}")
    if(_tool STREQUAL "" OR NOT IS_ABSOLUTE "${_tool}" OR EXISTS "${_tool}")
        return()
    endif()
    get_filename_component(_name "${_tool}" NAME)
    find_program(_here NAMES "${_name}" NO_CACHE)
    if(NOT _here)
        message(FATAL_ERROR "${_var} ${_tool} is not on this machine, nor ${_name} on its PATH")
    endif()
    message(STATUS "${_var} ${_tool}, the build's, is not on this machine: ${_here}, on its "
                   "PATH, instead")
    set(${_var} "${_here}" PARENT_SCOPE)
endfunction()
