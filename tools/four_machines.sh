#!/usr/bin/env bash
# four_machines.sh - four clients on two systems and an AI Cessna in one sky,
# and a fifth refused: the verification of "Four machines in one sky".
#
#   tools/four_machines.sh
#
# Run from WSL, with the Windows working copy built by tools/windows_build.sh
# at the same commit. The server runs on Windows, with room for four players
# and one AI Cessna. Two clients join it from Windows (127.0.0.1) and two
# from Linux across WSL's network to the Windows host: on each system the
# client with the window (Direct3D 12 on Windows, Vulkan in WSL) and a
# predicting glideslope_cli. A fifth asks from Linux once the four are in.
#
# It fails unless:
# - the server admits four players and flies them and the AI;
# - each client with the window is given a Cessna, and draws the other four;
# - each client's worst correction is under 20 m (too large to hide) and none
#   is snapped, and each glideslope_cli's prediction error is under 10 m;
# - the fifth is refused as full (TRANSPORT.md, 05).
#
# **Why not CI**: it is two operating systems on one network, which a CI job
# is not. The same parts run in CI on one machine each - the network checks,
# and the client with the window on a server.
#
# WINDOWS_CLONE overrides where the Windows working copy is
# (default /mnt/c/Development/glideslope).

set -uo pipefail

win="${WINDOWS_CLONE:-/mnt/c/Development/glideslope}/build/windows-debug"
lin="$(cd "$(dirname "$0")/.." && pwd)/build/linux-debug"
host="$(ip route show default | awk '{ print $3; exit }')"
port=47950
work="$(mktemp -d)"
ready="four-ready-$$.txt"

for program in "$win/glideslope_server.exe" "$win/glideslope.exe" "$win/glideslope_cli.exe" \
               "$lin/glideslope" "$lin/glideslope_cli"; do
    if [[ ! -x "$program" ]]; then
        echo "four_machines: no $program - build both working copies first" >&2
        exit 2
    fi
done

# A client adds to the file it says what it heard in; a run starts with none.
rm -f "$win/four-win-cli.txt" "$win/four-windows.bmp"

key="$(cd "$win" && ./glideslope_server.exe --port 0 --seconds 0.05 --ai 0 \
       --store four.sqlite | tr -d '\r' | sed -n 's/^server key \([0-9a-f]*\)$/\1/p')"
if [[ -z "$key" ]]; then
    echo "four_machines: the Windows server printed no key" >&2
    exit 1
fi

(cd "$win" && timeout 900 ./glideslope_server.exe --port $port --players 4 --ai 1 \
    --until-empty --headless --timeout 5 --data data --store four.sqlite \
    --ready-file "$ready" > "$work/server.txt" 2>&1) &
# Joined once it is flying, not after a number of seconds.
for _ in $(seq 1 300); do
    [[ -f "$win/$ready" ]] && break
    sleep 1
done
rm -f "$win/$ready"

(cd "$win" && timeout 600 ./glideslope.exe --headless --gpu-driver direct3d12 --size 640x400 \
    --shot four-windows.bmp --shot-at 3600 --view behind --server 127.0.0.1 $port \
    --server-key "$key" > "$work/win-window.txt" 2>&1) &
(cd "$win" && timeout 600 ./glideslope_cli.exe connect 127.0.0.1:$port "$key" 40 --after 2 \
    --predict --heard four-win-cli.txt > /dev/null 2>&1) &
(cd "$lin" && LSAN_OPTIONS=exitcode=0 GLIDESLOPE_CACHE="$lin/downloads" timeout 600 \
    ./glideslope --headless --gpu-driver vulkan --size 640x400 --shot "$work/four-linux.bmp" \
    --shot-at 3600 --view behind --server "$host" $port --server-key "$key" \
    > "$work/lin-window.txt" 2>&1) &
(cd "$lin" && timeout 600 ./glideslope_cli connect "$host:$port" "$key" 40 --after 4 \
    --predict --heard "$work/lin-cli.txt" > /dev/null 2>&1) &
(cd "$lin" && timeout 600 ./glideslope_cli connect "$host:$port" "$key" 5 --after 25 \
    --heard "$work/fifth.txt" > /dev/null 2>&1) &
wait
tr -d '\r' < "$win/four-win-cli.txt" > "$work/win-cli.txt"
rm -f "$win/four-win-cli.txt"
cp "$win/four-windows.bmp" "$work/" 2>/dev/null

failed=0
fail() {
    echo "four_machines: $*" >&2
    failed=1
}

server="$(tr -d '\r' < "$work/server.txt")"
players="$(grep -c "a player's, banked" <<< "$server")"
[[ "$players" == 4 ]] || fail "the server flew $players players' aircraft, not 4"
grep -q "an AI's, banked" <<< "$server" || fail "the server flew no AI"

for side in win lin; do
    said="$(tr -d '\r' < "$work/$side-window.txt")"
    grep -q "the server gave this client aircraft [0-9]*, the c172p" <<< "$said" ||
        fail "$side: the client with the window was given no Cessna"
    drew="$(grep -c "drew aircraft [0-9]*, the c172p" <<< "$said")"
    [[ "$drew" == 4 ]] || fail "$side: the client with the window drew $drew others, not 4"
    worst="$(sed -n 's/.*predicted: [0-9]* corrections, the worst \([0-9.]*\) m, \([0-9]*\) too.*/\1 \2/p' <<< "$said")"
    read -r metres snapped <<< "$worst"
    [[ -n "$metres" && "$snapped" == 0 ]] && awk "BEGIN { exit !($metres < 20) }" ||
        fail "$side: the client with the window's corrections: '$worst'"
    error="$(sed -n 's/.*prediction error: [0-9]* updates compared, the worst \([0-9.]*\) m.*/\1/p' "$work/$side-cli.txt")"
    [[ -n "$error" ]] && awk "BEGIN { exit !($error < 10) }" ||
        fail "$side: glideslope_cli's prediction error: '$error'"
    echo "$side: $(grep -E 'predicted:' <<< "$said"); glideslope_cli's worst error $error m"
done
grep -q "refused, reason 5" "$work/fifth.txt" || fail "the fifth client was not refused as full"

echo "four_machines: what was said is in $work"
if ((failed)); then
    exit 1
fi
echo "four_machines: four clients on Windows and Linux flew with the AI, and the fifth was refused"
