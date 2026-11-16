#!/usr/bin/env bash
# package_weather.sh - the package smoke's two weather checks, which skip, saying
# why, when a weather service does not answer, and nothing else.
#
#   tools/package_weather.sh cli PATH_TO_GLIDESLOPE_CLI
#   tools/package_weather.sh frame PROGRAM ARGS...
#
# **Why**: package.yml runs the unpacked program against the live weather
# services, which are somebody else's. Open-Meteo's SSL connect timeouts failed
# every package job for hours on 2026-11-14, and what was being judged -
# whether the package unpacks and runs - had nothing to do with it. A service
# that does not answer (a timeout, a refused connection, a 5xx or a 429) is
# told by the program's own words: world::ServiceUnavailable, which the weather
# fetch gives the prefix "the weather could not be had" - the same line every
# weather-needing ctest test skips on (tests/cmake/client.cmake). Any other
# failure, such as a 400 or a 404 from a request that is wrong, a METAR that
# does not parse or a missing level, is a fault and still fails. So no new exit
# code is needed: the words are the contract, and a unit test pins them
# (only_a_weather_service_that_does_not_answer_is_weather_not_to_be_had).
#
#   cli    runs `glideslope_cli weather YSSY` and checks its 19 levels and
#          Open-Meteo's credit. Service not answering: a ::warning:: naming
#          the check as skipped, and exit 0.
#   frame  runs PROGRAM ARGS (the client, headless, with `--weather YSSY` in
#          ARGS, perhaps under gdb). If it fails with the service not
#          answering, a ::warning:: and the frame is drawn again without
#          `--weather YSSY`, so the package still has to draw a frame; a
#          failure of that run, or of the first for any other reason, is the
#          failure.
#
# Exit: 0 passed or skipped (the warning says which), 1 failed, 2 misused.
# Standard error is shown as it comes and kept to be read.

set -uo pipefail

# The program's own fatal word on it (the client's main.cpp and the cli's catch
# of a ServiceUnavailable): "glideslope: " or "glideslope_cli: ", the weather's
# prefix, and what went wrong. The client's non-fatal refresh path says the
# same words and carries on, so the words alone are not enough: see
# service_not_answering.
fatal_line='^glideslope(_cli)?: the weather could not be had: '

warn() {
    echo "::warning title=weather service not answering::$1"
}

stderr_file=$(mktemp)
out=$(mktemp)
trap 'rm -f "$stderr_file" "$out"' EXIT

# Runs "$@", showing both streams, and keeps what stderr held in $stderr_file.
run_keeping_stderr() {
    "$@" 2> >(tee "$stderr_file" >&2)
    local rc=$?
    wait # the tee's last bytes
    return "$rc"
}

# **An outage is the program ending with exit code 1 whose last word is that
# line**: a program that said it and then went on to another error, or ended
# any other way (another code, a signal), did not end for the service. Under
# gdb its own closing lines ("No stack.", "[Inferior 1 ... exited") are not
# the program's words and are passed over. $1 is the exit code.
service_not_answering() {
    [[ "$1" -eq 1 ]] || return 1
    local last
    last=$(grep -v -E '^(No stack\.|\[Inferior [0-9]+ .*exited)' "$stderr_file" \
               | grep -v '^[[:space:]]*$' | tail -n 1)
    [[ "$last" =~ $fatal_line ]]
}

case "${1:-}" in
cli)
    [[ $# -eq 2 ]] || { echo "usage: $0 cli PATH_TO_GLIDESLOPE_CLI" >&2; exit 2; }
    run_keeping_stderr "$2" weather YSSY > "$out"
    status=$?
    cat "$out"
    if [[ $status -ne 0 ]]; then
        if service_not_answering "$status"; then
            warn "SKIPPED the weather check: a weather service did not answer ($(tr '\n' ' ' < "$stderr_file")). The rest of the smoke ran and judged."
            exit 0
        fi
        echo "::error::glideslope_cli weather exited $status, and not for a service that did not answer"
        exit 1
    fi
    [[ "$(grep -c ' hPa .* m  *from ' "$out")" = 19 ]] \
        || { echo "::error::the weather does not have 19 levels"; exit 1; }
    grep -q "Weather data by Open-Meteo.com" "$out" \
        || { echo "::error::the weather does not credit Open-Meteo"; exit 1; }
    ;;
frame)
    shift
    [[ $# -ge 1 ]] || { echo "usage: $0 frame PROGRAM ARGS..." >&2; exit 2; }
    run_keeping_stderr "$@"
    status=$?
    if [[ $status -eq 0 ]]; then
        exit 0
    fi
    if ! service_not_answering "$status"; then
        exit "$status"
    fi
    warn "SKIPPED the weather in the frame: a weather service did not answer ($(tr '\n' ' ' < "$stderr_file")). The frame is drawn again without it."
    again=()
    skip_next=0
    for a in "$@"; do
        if [[ $skip_next -eq 1 ]]; then skip_next=0; continue; fi
        if [[ "$a" == "--weather" ]]; then skip_next=1; continue; fi
        again+=("$a")
    done
    [[ ${#again[@]} -lt $# ]] || { echo "::error::the frame failed, and had no --weather to leave out"; exit "$status"; }
    # Not exec: the trap that removes the temporary files must run.
    "${again[@]}"
    exit $?
    ;;
*)
    echo "usage: $0 cli PATH | frame PROGRAM ARGS..." >&2
    exit 2
    ;;
esac
