# deploy_systemd.cmake - a server started by systemd accepts a client.
#
#   cmake -DSERVER=<glideslope_server> -DCLIENT=<glideslope_cli> -DDATA=<data>
#         -DUNIT=<deploy/glideslope-server.service> -DWORK=<scratch>
#         -DPORT=<a port> -P deploy_systemd.cmake
#
# **Half of the deployment item's verification**: "a server started from each
# accepts a client". This is the systemd half. The Dockerfile's half needs a
# Docker daemon, which is not on every machine this builds on, and is not
# tested here - `COMPLETION_PLAN.md` says so where the item stands.
#
# **The unit is the one that ships**, read from `deploy/`, with only the paths
# rewritten: the binary and the data are the build's, the store is a scratch
# file, and the hardening that needs a system user - `User=`, `StateDirectory=`
# and the two `Protect` settings that would hide the build tree - is dropped
# because this runs as whoever is building. Everything else about the unit is
# exercised as written, including `systemd-analyze verify`.
#
# Without systemd, or without a user bus to run a unit on, it reports itself
# skipped (exit 77), never passed.

cmake_minimum_required(VERSION 3.28)

find_program(SYSTEMD_RUN systemd-run)
find_program(SYSTEMCTL systemctl)
find_program(SYSTEMD_ANALYZE systemd-analyze)
if(NOT SYSTEMD_RUN OR NOT SYSTEMCTL OR NOT SYSTEMD_ANALYZE)
    message(STATUS "no systemd here, so a unit cannot be run")
    cmake_language(EXIT 77)
endif()

set(_unit "${WORK}/glideslope-check.service")
set(_store "${WORK}/deploy.sqlite")
set(_log "${WORK}/deploy.log")
file(REMOVE "${_store}" "${_log}" "${WORK}/deploy.err")

# The shipped unit, with its paths pointed at the build.
file(READ "${UNIT}" _text)
string(REPLACE "/opt/glideslope/glideslope_server" "${SERVER}" _text "${_text}")
string(REPLACE "/opt/glideslope/data" "${DATA}" _text "${_text}")
string(REPLACE "/var/lib/glideslope/server.sqlite" "${_store}" _text "${_text}")
string(REPLACE "WorkingDirectory=/opt/glideslope" "" _text "${_text}")
foreach(_drop "User=glideslope" "Group=glideslope" "StateDirectory=glideslope"
              "ProtectHome=true" "ProtectSystem=strict")
    string(REPLACE "${_drop}" "" _text "${_text}")
endforeach()
file(WRITE "${_unit}" "${_text}")

# **The unit itself is valid.** This is what catches a directive misspelt or a
# setting this systemd does not know.
execute_process(COMMAND "${SYSTEMD_ANALYZE}" verify "${_unit}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the unit does not verify:\n${_err}${_out}")
endif()
message(STATUS "the unit verifies")

execute_process(COMMAND "${SYSTEMCTL}" --user stop glideslope-check.service
                RESULT_VARIABLE _ignored ERROR_VARIABLE _ignored2
                OUTPUT_VARIABLE _ignored3)

execute_process(
    COMMAND "${SYSTEMD_RUN}" --user --unit=glideslope-check --collect
            "--property=StandardOutput=file:${_log}"
            "--property=StandardError=file:${WORK}/deploy.err"
            "${SERVER}" --headless --data "${DATA}" --store "${_store}"
            --port ${PORT} --ai 0
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(STATUS "systemd would not run a user unit here: ${_err}")
    cmake_language(EXIT 77)
endif()

# Give it a moment to bind and print its key.
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 4)

if(NOT EXISTS "${_log}")
    execute_process(COMMAND "${SYSTEMCTL}" --user stop glideslope-check.service)
    message(STATUS "systemd wrote no output file, so there is no key to read")
    cmake_language(EXIT 77)
endif()
file(READ "${_log}" _said)
if(NOT _said MATCHES "server key ([0-9a-f][0-9a-f]+)\n")
    execute_process(COMMAND "${SYSTEMCTL}" --user stop glideslope-check.service)
    message(FATAL_ERROR "the server under systemd printed no key:\n${_said}")
endif()
set(_key "${CMAKE_MATCH_1}")
if(NOT _said MATCHES "listening on port ${PORT}")
    execute_process(COMMAND "${SYSTEMCTL}" --user stop glideslope-check.service)
    message(FATAL_ERROR "it is not listening on ${PORT}:\n${_said}")
endif()

# **And it accepts a client**, which is the whole of the verification.
execute_process(
    COMMAND "${CLIENT}" connect "127.0.0.1:${PORT}" "${_key}" 3
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
execute_process(COMMAND "${SYSTEMCTL}" --user stop glideslope-check.service)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "the client could not connect:\n${_err}${_out}")
endif()
if(NOT _out MATCHES "session with ${_key}")
    message(FATAL_ERROR "the client got no session with the server's key:\n${_out}")
endif()
file(READ "${_log}" _said)
if(NOT _said MATCHES "admitted ([0-9a-f]+) to slot ([0-9]+)")
    message(FATAL_ERROR "the server never admitted it:\n${_said}")
endif()
message(STATUS "a server started by systemd admitted ${CMAKE_MATCH_1} to slot "
               "${CMAKE_MATCH_2}, and the client completed a session")
