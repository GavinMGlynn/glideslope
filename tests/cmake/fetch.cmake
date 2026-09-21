# fetch.cmake - downloads the files the tests need, each pinned by SHA-256.
#
#   cmake -DLIST=<files.txt> -DDIR=<directory> -P fetch.cmake
#
# Each line of LIST is: name, size, SHA-256, URL. A file already in DIR with the
# right hash is not fetched again. A download that fails, or arrives with any
# other hash, is deleted, and one with the wrong hash always fails.
#
# Without the network the tests that need these files cannot run. Then this
# exits 77, which ctest reports as skipped - unless GLIDESLOPE_REQUIRE_NETWORK
# is set in the environment, as CI sets it, where it is a failure.

cmake_minimum_required(VERSION 3.28)

file(MAKE_DIRECTORY "${DIR}")
file(STRINGS "${LIST}" _lines REGEX "^[^#]")
set(_failed "")
set(_wrong "")
foreach(_line IN LISTS _lines)
    string(REGEX REPLACE " +" ";" _fields "${_line}")
    list(GET _fields 0 _name)
    list(GET _fields 1 _size)
    list(GET _fields 2 _sha256)
    list(GET _fields 3 _url)
    set(_path "${DIR}/${_name}")
    if(EXISTS "${_path}")
        file(SHA256 "${_path}" _have)
        if(_have STREQUAL _sha256)
            message(STATUS "${_name}: present")
            continue()
        endif()
        file(REMOVE "${_path}")
    endif()
    message(STATUS "${_name}: fetching ${_size} bytes from ${_url}")
    # **Tried more than once.** A third party being briefly unwell is not a
    # reason to red the tree: SourceForge was down for ten seconds on
    # 2026-09-21 and Rocky 9 failed on a commit that had nothing to do with
    # it. A file that never arrives still fails, after ${_tries} attempts.
    set(_tries 3)
    set(_code 1)
    set(_why "not attempted")
    foreach(_try RANGE 1 ${_tries})
        file(DOWNLOAD "${_url}" "${_path}.part" STATUS _status TLS_VERIFY ON
             INACTIVITY_TIMEOUT 60)
        list(GET _status 0 _code)
        list(GET _status 1 _why)
        if(_code EQUAL 0)
            break()
        endif()
        file(REMOVE "${_path}.part")
        message(STATUS "${_name}: attempt ${_try} of ${_tries} failed: ${_why}")
        if(_try LESS _tries)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 3)
        endif()
    endforeach()
    if(NOT _code EQUAL 0)
        file(REMOVE "${_path}.part")
        string(APPEND _failed "  ${_name}: ${_why} (after ${_tries} attempts)\n")
        continue()
    endif()
    # Checked here rather than by EXPECTED_HASH, which stops the script before
    # the bad file can be deleted.
    file(SHA256 "${_path}.part" _got)
    if(NOT _got STREQUAL _sha256)
        file(REMOVE "${_path}.part")
        string(APPEND _wrong "  ${_name}: arrived with SHA-256 ${_got}, not ${_sha256}\n")
        continue()
    endif()
    file(RENAME "${_path}.part" "${_path}")
endforeach()

# A file that arrived different is never skipped: the source changed, or
# something between it and here did.
if(NOT _wrong STREQUAL "")
    message(FATAL_ERROR "fetched, but not what was pinned:\n${_wrong}")
endif()
if(NOT _failed STREQUAL "")
    if("$ENV{GLIDESLOPE_REQUIRE_NETWORK}" STREQUAL "")
        message(STATUS "could not fetch, so the tests needing these are skipped:\n${_failed}")
        cmake_language(EXIT 77)
    endif()
    message(FATAL_ERROR "could not fetch:\n${_failed}")
endif()
