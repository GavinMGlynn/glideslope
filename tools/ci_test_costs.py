#!/usr/bin/env python3
"""What each test costs on CI, measured from a CI run, for tools/ci_shard.cmake.

    tools/ci_test_costs.py RUN_ID

reads every test shard's log of that run of the ci workflow through `gh`, and
writes tests/ci_costs/<preset>.txt: one "seconds name" a line, each test's
wall time rounded up to a whole second, measured as it ran - four at a time
(three on macOS) beside the other tests of its shard. Run it on a green run
when the tests have changed enough that the shards drift apart; a test it does
not know is counted at the script's default and named in the shard's log.
"""
import collections
import json
import math
import os
import re
import subprocess
import sys

REPO = 'GavinMGlynn/glideslope'
TEST = re.compile(r'Test\s+#\d+: (\S+) \.+.*?\s([\d.]+) sec')
SHARD_JOB = re.compile(r'^\S+ (\S+) - tests \d+/\d+$')


def gh(*args):
    return subprocess.run(['gh', *args], check=True, capture_output=True).stdout


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    run = sys.argv[1]
    jobs = json.loads(gh('run', 'view', run, '--repo', REPO, '--json', 'jobs'))['jobs']
    costs = collections.defaultdict(dict)
    for job in jobs:
        m = SHARD_JOB.match(job['name'])
        if not m:
            continue
        if job['conclusion'] != 'success':
            sys.exit(f"{job['name']} did not pass; measure a green run")
        log = gh('api', '--allow-escape-sequences',
                 f"repos/{REPO}/actions/jobs/{job['databaseId']}/logs").decode(errors='replace')
        for line in log.splitlines():
            t = TEST.search(line)
            if t:
                costs[m.group(1)][t.group(1)] = max(1, math.ceil(float(t.group(2))))
    if not costs:
        sys.exit(f'run {run} has no test shards')
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tests', 'ci_costs')
    os.makedirs(here, exist_ok=True)
    for preset, tests in sorted(costs.items()):
        with open(os.path.join(here, preset + '.txt'), 'w', newline='\n') as f:
            f.write(f'# Seconds each test took on CI, run {run}: tools/ci_test_costs.py\n')
            for name in sorted(tests):
                f.write(f'{tests[name]} {name}\n')
        print(f'{preset}: {len(tests)} tests, {sum(tests.values())} s')


if __name__ == '__main__':
    main()
