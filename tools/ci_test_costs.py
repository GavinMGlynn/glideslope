#!/usr/bin/env python3
"""What each test costs on CI, measured from CI runs, for tools/ci_shard.cmake.

    tools/ci_test_costs.py RUN_ID [RUN_ID ...]

reads every test shard's log of each run of the ci workflow through `gh`, and
writes tests/ci_costs/<preset>.txt: one "seconds name" a line, each test's
wall time rounded up to a whole second, measured as it ran - four at a time
(three on macOS) beside the other tests of its shard - and, given more than
one run, the longest of them, since a runner's speed varies run to run. Run it
on green runs when the tests have changed enough that the shards drift apart:
a shard's log warns of each test the table does not know, and each name in it
that is no longer a test.
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


def measure(run, costs):
    jobs = json.loads(gh('run', 'view', run, '--repo', REPO, '--json', 'jobs'))['jobs']
    for job in jobs:
        m = SHARD_JOB.match(job['name'])
        if not m:
            continue
        if job['conclusion'] != 'success':
            sys.exit(f"run {run}: {job['name']} did not pass; measure green runs")
        log = gh('api', '--allow-escape-sequences',
                 f"repos/{REPO}/actions/jobs/{job['databaseId']}/logs").decode(errors='replace')
        for line in log.splitlines():
            t = TEST.search(line)
            if t:
                cost = max(1, math.ceil(float(t.group(2))))
                tests = costs[m.group(1)]
                tests[t.group(1)] = max(tests.get(t.group(1), 0), cost)


def main():
    runs = sys.argv[1:]
    if not runs:
        sys.exit(__doc__)
    costs = collections.defaultdict(dict)
    for run in runs:
        measure(run, costs)
    if not costs:
        sys.exit('no test shards in runs ' + ' '.join(runs))
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tests', 'ci_costs')
    os.makedirs(here, exist_ok=True)
    for preset, tests in sorted(costs.items()):
        with open(os.path.join(here, preset + '.txt'), 'w', newline='\n') as f:
            f.write(f"# Seconds each test took on CI, the longest of runs {', '.join(runs)}:"
                    ' tools/ci_test_costs.py\n')
            for name in sorted(tests):
                f.write(f'{tests[name]} {name}\n')
        print(f'{preset}: {len(tests)} tests, {sum(tests.values())} s')


if __name__ == '__main__':
    main()
