# copilots_nearest_station_run.cmake - the inner half of
# copilots_nearest_station.cmake, started beside the stub: it waits for the
# stub's port, runs the client and the server against it, writes what they
# said to RESULT_FILE, and stops the stub.
#
#   cmake -DSERVER=... -DCLIENT=... -DDATA=... -DCACHE=... -DRECORDINGS=...
#         -DSTUB_PORT_FILE=... -DRESULT_FILE=... -DHEARD=... -DSTORE=...
#         -DKEY=... -DPORT=... -P copilots_nearest_station_run.cmake

cmake_minimum_required(VERSION 3.28)

# **Its port held machine-wide** (port_lock.cmake): already held by the
# script that runs this one, and so not waited for again.
include(${CMAKE_CURRENT_LIST_DIR}/port_lock.cmake)

while(NOT EXISTS "${STUB_PORT_FILE}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
endwhile()
file(STRINGS "${STUB_PORT_FILE}" _stub LIMIT_COUNT 1)
set(ENV{GLIDESLOPE_WEATHER_SERVICE} "http://127.0.0.1:${_stub}")
set(ENV{GLIDESLOPE_CACHE} "${CACHE}")
# The client first and the server last, so that what the server prints
# is what comes out of the pipeline; the client's own words in HEARD.
execute_process(
    COMMAND "${CLIENT}" --data "${DATA}" connect "127.0.0.1:${PORT}" "${KEY}" 20 --after 1
            --fly --heard "${HEARD}"
            --copilot c172p "fly to Manly at 3,000 ft, then orbit over Manly beach"
            --copilot-provider anthropic --copilot-model claude-haiku-4-5-20251001
            --copilot-playback "${RECORDINGS}/manly-server-anthropic.jsonl"
            --copilot-at 5 --copilot-stay 5
    COMMAND "${SERVER}" --port ${PORT} --seconds 400 --until-empty --ai 1 --headless
            --data "${DATA}" --timeout 30 --store "${STORE}" --weather around
            --on-leave ai
            --hand-over-planner anthropic:claude-haiku-4-5-20251001
            --hand-over-playback "${RECORDINGS}/leave-server-anthropic.jsonl"
    RESULTS_VARIABLE _rcs OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
file(WRITE "${RESULT_FILE}" "=== exits ${_rcs}\n=== out\n${_out}\n=== err\n${_err}\n")
file(DOWNLOAD "http://127.0.0.1:${_stub}/stop" "${RESULT_FILE}.stop")
port_listened("${_out}" "${_err}")
