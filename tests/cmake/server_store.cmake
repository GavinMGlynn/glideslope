# server_store.cmake - a server given a store keeps its key across restarts.
#
#   cmake -DSERVER=<glideslope_server> -DWORK=<a scratch directory>
#         -P server_store.cmake
#
# **The rule this pins** is REQUIREMENTS.md 6.6: `--key HEX (server secret,
# minted once and stored if not given)`. A client is given the server's public
# key out of band - written down, pasted into a command line - so a server
# that minted a fresh key at every start would lock out every client it had at
# each restart. "Minted once" is only true if the key that comes back after a
# restart is the one that went in.
#
# **Every way the server can come by a key is walked here**, and they are
# four: given on the command line, read from a store, minted into a store, and
# minted with no store. The counts below fail if any is missed.
#
# It needs nothing from the network and no data, so it never skips.

cmake_minimum_required(VERSION 3.28)

file(REMOVE "${WORK}/kept.sqlite" "${WORK}/other.sqlite")

# Starts the server for a moment and gives back the key it printed and where
# it said the key came from.
function(start_server _key _whence)
    execute_process(
        COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "the server would not start with ${ARGN}:\n${_err}")
    endif()
    if(NOT _out MATCHES "server key is ([a-z]+)\n")
        message(FATAL_ERROR "the server did not say where its key came from:\n${_out}")
    endif()
    set(${_whence} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    if(NOT _out MATCHES "server key ([0-9a-f]+)\n")
        message(FATAL_ERROR "the server did not print its public key:\n${_out}")
    endif()
    string(LENGTH "${CMAKE_MATCH_1}" _digits)
    if(NOT _digits EQUAL 64)
        message(FATAL_ERROR "a key is 64 hexadecimal digits, not ${_digits}")
    endif()
    set(${_key} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

set(_ways 0)

# One: a fresh store. The key is minted, and the store is made.
start_server(_first _whence --store "${WORK}/kept.sqlite")
if(NOT _whence STREQUAL "minted")
    message(FATAL_ERROR "with an empty store the key should be minted, not ${_whence}")
endif()
if(NOT EXISTS "${WORK}/kept.sqlite")
    message(FATAL_ERROR "the server did not make the store it was given")
endif()
math(EXPR _ways "${_ways} + 1")

# Two: the same store, twice more. The key is read back both times, so this
# is not a one-restart accident.
start_server(_second _whence --store "${WORK}/kept.sqlite")
if(NOT _whence STREQUAL "kept")
    message(FATAL_ERROR "the second start should read the key, not ${_whence} it")
endif()
if(NOT _second STREQUAL _first)
    message(FATAL_ERROR "the key changed across a restart:\n"
                        "  first  ${_first}\n  second ${_second}")
endif()
start_server(_third _whence --store "${WORK}/kept.sqlite")
if(NOT _third STREQUAL _first)
    message(FATAL_ERROR "the key changed on the third start:\n"
                        "  first ${_first}\n  third ${_third}")
endif()
math(EXPR _ways "${_ways} + 1")

# Three: no store. Every start mints, and two starts differ - which is the
# thing the store is there to prevent, shown to be real.
start_server(_loose_a _whence)
if(NOT _whence STREQUAL "minted")
    message(FATAL_ERROR "with no store the key should be minted, not ${_whence}")
endif()
start_server(_loose_b _whence)
if(_loose_a STREQUAL _loose_b)
    message(FATAL_ERROR "two servers with no store minted the same key, which "
                        "means the key is not being minted at all")
endif()
math(EXPR _ways "${_ways} + 1")

# A different store is a different key, so the key is the store's and not the
# machine's.
start_server(_other _whence --store "${WORK}/other.sqlite")
if(_other STREQUAL _first)
    message(FATAL_ERROR "two different stores hold the same key")
endif()

# Four: given on the command line. It wins over the store, and is not written
# into it - the store still holds the key it held before.
set(_given "1111111111111111111111111111111111111111111111111111111111111111")
start_server(_from_flag _whence --store "${WORK}/kept.sqlite" --key "${_given}")
if(NOT _whence STREQUAL "given")
    message(FATAL_ERROR "--key should be used as given, not ${_whence}")
endif()
if(_from_flag STREQUAL _first)
    message(FATAL_ERROR "--key was ignored in favour of the store")
endif()
start_server(_after _whence --store "${WORK}/kept.sqlite")
if(NOT _after STREQUAL _first)
    message(FATAL_ERROR "--key overwrote the key kept in the store:\n"
                        "  was  ${_first}\n  now  ${_after}")
endif()
math(EXPR _ways "${_ways} + 1")

# A store that cannot be opened stops the server rather than letting it run
# with nowhere to keep the key.
execute_process(
    COMMAND "${SERVER}" --port 0 --seconds 0.05 --ai 0 --store "${WORK}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(_rc EQUAL 0)
    message(FATAL_ERROR "the server started with a directory for a store")
endif()

if(NOT _ways EQUAL 4)
    message(FATAL_ERROR "only ${_ways} of the 4 ways a server comes by a key were walked")
endif()
message(STATUS "all 4 ways a server comes by a key: given, kept, minted into a "
               "store, minted loose. Across three restarts the stored key held at "
               "${_first}")
