#!/usr/bin/env python3
"""What each test costs on CI, measured from CI runs, for tools/ci_shard.cmake.

    tools/ci_test_costs.py [--finished] RUN_ID [RUN_ID ...]

reads every test shard's log of each run of the ci workflow through `gh`, and
writes tests/ci_costs/<preset>.txt: one "seconds name" a line, each test's
wall time rounded up to a whole second, measured as it ran - four at a time
(three on macOS) beside the other tests of its shard - and, given more than
one run, the longest of them, since a runner's speed varies run to run. Run it
on green runs when the tests have changed enough that the shards drift apart:
a shard's log warns of each test the table does not know, and each name in it
that is no longer a test.

With --finished, a run whose shards did not all pass is measured too: the
tests each of its shards finished, a shard cut off by its time limit
included. That is how a test new on a branch whose run ran out of time is
measured at all (#241's STOVL take-over tests, 2026-10-28); the header says
which runs were not green. A test still running when its shard was cut off
costs at least the time it had run, and the header names each preset's count
of such floors, so a later green run measures them whole.

The tests labelled gust_sweep run in no pull request's run, so their costs
come from a scheduled or dispatched run's shards: give one such run's id
beside the pull requests' (ci.yml's workflow_dispatch runs what a scheduled
run does).
"""
import collections
import datetime
import json
import math
import os
import re
import subprocess
import sys

REPO = 'GavinMGlynn/glideslope'
TEST = re.compile(r'Test\s+#\d+: (\S+) \.+.*?\s([\d.]+) sec')
# One word of platform, then the preset: Rocky 9's shards ("Rocky 9 linux-release -
# tests 1/3") do not match, so linux-release's table is Ubuntu's runners alone.
SHARD_JOB = re.compile(r'^\S+ (\S+) - tests \d+/\d+$')
# A log line's time, and a test's start in ctest's output.
STAMP = re.compile(r'^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d+)Z')
START = re.compile(r'Start\s+\d+: (\S+)')


def gh(*args):
    return subprocess.run(['gh', *args], check=True, capture_output=True).stdout


def measure(run, costs, finished, not_green, cut_off):
    jobs = json.loads(gh('run', 'view', run, '--repo', REPO, '--json', 'jobs'))['jobs']
    for job in jobs:
        m = SHARD_JOB.match(job['name'])
        if not m:
            continue
        if job['conclusion'] != 'success':
            if not finished:
                sys.exit(f"run {run}: {job['name']} did not pass; measure green runs,"
                         " or pass --finished")
            not_green.add(run)
        try:
            log = gh('api', '--allow-escape-sequences',
                     f"repos/{REPO}/actions/jobs/{job['databaseId']}/logs").decode(errors='replace')
        except subprocess.CalledProcessError:
            # A job whose runner was lost has no log (BlobNotFound): run
            # 43111884667's linux-debug shard 11, cancelled at 65 minutes.
            if not finished:
                raise
            print(f"run {run}: {job['name']} has no log; its tests are not measured",
                  file=sys.stderr)
            continue
        tests = costs[m.group(1)]
        started, last = {}, None
        for line in log.splitlines():
            when = STAMP.match(line)
            if when:
                last = datetime.datetime.fromisoformat(when.group(1)[:26])
            s = START.search(line)
            if s and when:
                started[s.group(1)] = last
            t = TEST.search(line)
            if t:
                cost = max(1, math.ceil(float(t.group(2))))
                tests[t.group(1)] = max(tests.get(t.group(1), 0), cost)
                started.pop(t.group(1), None)
        # **A test the shard was cut off in costs at least what it had run.**
        # Measured from finished tests alone, a test that never finishes inside
        # a shard is never measured, and is dealt at the default 60 s however
        # long it is: the gust_sweep tests, which no pull request's run has,
        # were, and every shard of every scheduled run from 2026-11-05 was cut
        # off (runs 42115457538 to 43111884667). So with --finished, a test
        # still running when its shard stopped costs the time it had run.
        if finished and job['conclusion'] != 'success' and last is not None:
            for name, since in started.items():
                cost = max(1, math.ceil((last - since).total_seconds()))
                if cost > tests.get(name, 0):
                    tests[name] = cost
                    cut_off[m.group(1)].add(name)


def main():
    runs = sys.argv[1:]
    finished = '--finished' in runs
    runs = [r for r in runs if r != '--finished']
    if not runs:
        sys.exit(__doc__)
    costs = collections.defaultdict(dict)
    not_green = set()
    cut_off = collections.defaultdict(set)
    for run in runs:
        measure(run, costs, finished, not_green, cut_off)
    if not costs:
        sys.exit('no test shards in runs ' + ' '.join(runs))
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tests', 'ci_costs')
    os.makedirs(here, exist_ok=True)
    for preset, tests in sorted(costs.items()):
        with open(os.path.join(here, preset + '.txt'), 'w', newline='\n') as f:
            unfinished = ''
            if not_green:
                unfinished = (f" ({', '.join(sorted(not_green))} not green: the tests its"
                              ' shards finished counted')
                if cut_off[preset]:
                    unfinished += (f', {len(cut_off[preset])} of them cut off and counted'
                                   ' at the time they had run')
                unfinished += ')'
            f.write(f"# Seconds each test took on CI, the longest of runs {', '.join(runs)}"
                    f'{unfinished}: tools/ci_test_costs.py --finished\n' if not_green else
                    f"# Seconds each test took on CI, the longest of runs {', '.join(runs)}:"
                    ' tools/ci_test_costs.py\n')
            for name in sorted(tests):
                f.write(f'{tests[name]} {name}\n')
        print(f'{preset}: {len(tests)} tests, {sum(tests.values())} s')


if __name__ == '__main__':
    main()
