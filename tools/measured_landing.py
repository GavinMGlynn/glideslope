"""measured_landing.py - the measured landing distance line for a figures file.

    python3 tools/measured_landing.py BUILD/glideslope_cli AIRCRAFT

For an aircraft that publishes no landing distance, `glideslope_cli land`
hands her to the approach autopilot at a gate 2 nm out, on a level runway at
sea level in calm air with every tank full, and says where she stopped past
the threshold. This prints the line her figures file carries: that distance
2 per cent over, for the drift between machines, rounded up to 100 ft -

    <landing_distance ft="..." basis="measured" factor="1">

It is the AI's own landing in those conditions, with a small allowance: not
a regulatory distance and not a minimum. The test
`the_<aircraft>_lands_within_her_measured_landing_distance`
(tests/cmake/measured_landing.cmake) flies it again and is the check.
"""

import math
import re
import subprocess
import sys

ALLOWANCE = 1.02
FT_PER_M = 1 / 0.3048


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: measured_landing.py GLIDESLOPE_CLI AIRCRAFT")
    cli, aircraft = sys.argv[1], sys.argv[2]
    run = subprocess.run([cli, "land", aircraft], capture_output=True, text=True, check=False)
    sys.stderr.write(run.stdout)
    stopped = re.search(r"stopped (\d+) m past the threshold", run.stdout)
    if run.returncode != 0 or stopped is None:
        raise SystemExit(f"measured_landing: {aircraft} did not land and stop on the runway "
                         f"(exit {run.returncode})\n{run.stderr}")
    metres = int(stopped.group(1))
    ft = math.ceil(metres * ALLOWANCE * FT_PER_M / 100) * 100
    print(f"{aircraft}: stopped {metres} m past the threshold")
    print(f'<landing_distance ft="{ft}" basis="measured" factor="1">')


if __name__ == "__main__":
    main()
