"""The C172P's final approach and landing, as a reinforcement-learning task.

A small gym-style environment of glideslope's own around JSBSim's Python
bindings - no third-party gym wrapper - flying the repository's committed
flight model (assets/jsbsim, the C172P) with the settings the simulation
uses: 120 Hz steps, the flaps and the initial trim as sim::Aircraft sets
them, the controls written to the properties sim::Aircraft::set_controls
writes.

**What the agent sees and does is defined here and in src/sim/learnt.cpp,
and the two must agree exactly.** `READINGS` are the JSBSim properties read,
`observe` turns them into the observation, and `controls` turns an action into
control positions. tools/rl/export.py records readings, observations and
actions from these functions into tests/data/rl/, and a C++ test holds the
simulation's versions to them.

The agent decides ten times a second - every twelfth 120 Hz step - and holds
its controls between decisions. The simulation still steps at 120 Hz.

Only the standard library and numpy are needed to import this file; gymnasium
is imported by `LandingEnv`, which is what training uses.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np

DEGREES = 180.0 / math.pi
FEET_PER_METRE = 3.280839895013123
METRES_PER_NM = 1852.0
FPS_TO_MPS = 0.3048
STEPS_PER_SECOND = 120

# The JSBSim properties the agent's observation is made from, in this order.
READINGS = (
    "position/lat-geod-deg",
    "position/long-gc-deg",
    "position/h-sl-ft",
    "attitude/phi-rad",
    "attitude/theta-rad",
    "attitude/psi-rad",
    "velocities/p-rad_sec",
    "velocities/q-rad_sec",
    "velocities/r-rad_sec",
    "velocities/vc-kts",
    "velocities/v-north-fps",
    "velocities/v-east-fps",
    "velocities/v-down-fps",
    "aero/alpha-rad",
    "aero/beta-rad",
    "gear/wow",
)

OBSERVATIONS = 21
ACTIONS = 4


@dataclass
class Runway:
    """A datum, as sim::Runway is: the threshold, its elevation, the landing
    direction and the length from the threshold on."""

    threshold_lat_deg: float = -33.9461
    threshold_lon_deg: float = 151.1772
    elevation_ft: float = 0.0
    heading_deg: float = 70.0
    length_m: float = 3000.0


@dataclass
class Approach:
    """How the approach is flown, which the policy file carries: the speed
    it is trained at, the landing flap, the glidepath and where it aims."""

    vref_kts: float = 59.8  # 1.3 times the C172P's published landing stall
    flaps: float = 1.0
    glidepath_deg: float = 3.0
    aim_m: float = 300.0  # past the threshold, as sim::ApproachSpeeds aims
    decision_steps: int = 12


def metres_per_degree_latitude(latitude_deg: float) -> float:
    lat = latitude_deg / DEGREES
    return (
        111132.92
        - 559.82 * math.cos(2.0 * lat)
        + 1.175 * math.cos(4.0 * lat)
        - 0.0023 * math.cos(6.0 * lat)
    )


def metres_per_degree_longitude(latitude_deg: float) -> float:
    lat = latitude_deg / DEGREES
    return (
        111412.84 * math.cos(lat)
        - 93.5 * math.cos(3.0 * lat)
        + 0.118 * math.cos(5.0 * lat)
    )


def remainder(x: float, y: float) -> float:
    """C's std::remainder: x - n*y, n the integer nearest x/y, ties to even."""
    return math.remainder(x, y)


@dataclass
class Where:
    """Where the aeroplane is with respect to the runway, as sim::Lander has
    it: `along_m` positive before the threshold, `across_m` positive right of
    the centreline, `above_m` above the threshold's elevation."""

    along_m: float
    across_m: float
    above_m: float


def where(r: list[float] | tuple[float, ...], rw: Runway) -> Where:
    north_m = (r[0] - rw.threshold_lat_deg) * metres_per_degree_latitude(
        rw.threshold_lat_deg
    )
    east_m = (r[1] - rw.threshold_lon_deg) * metres_per_degree_longitude(
        rw.threshold_lat_deg
    )
    h = rw.heading_deg / DEGREES
    past_m = east_m * math.sin(h) + north_m * math.cos(h)
    across_m = east_m * math.cos(h) - north_m * math.sin(h)
    above_m = (r[2] - rw.elevation_ft) / FEET_PER_METRE
    return Where(-past_m, across_m, above_m)


def observe(
    r: list[float] | tuple[float, ...],
    rw: Runway,
    ap: Approach,
    previous: list[float] | tuple[float, ...],
) -> list[float]:
    """The observation, from `READINGS`' values and the last action taken.
    Unnormalised: the policy file carries the normalisation."""
    w = where(r, rw)
    h = rw.heading_deg / DEGREES
    glidepath_m = (w.along_m + ap.aim_m) * math.tan(ap.glidepath_deg / DEGREES)
    heading_error = remainder(r[5] - h, 2.0 * math.pi)
    vn = r[10] * FPS_TO_MPS
    ve = r[11] * FPS_TO_MPS
    v_along = ve * math.sin(h) + vn * math.cos(h)
    v_across = ve * math.cos(h) - vn * math.sin(h)
    climb = -r[12] * FPS_TO_MPS
    return [
        w.along_m / 1000.0,
        w.across_m / 30.0,
        (w.above_m - glidepath_m) / 10.0,
        w.above_m / 30.0,
        heading_error,
        v_across / 5.0,
        v_along / 30.0,
        climb / 3.0,
        (r[9] - ap.vref_kts) / 10.0,
        r[3],
        r[4],
        r[6],
        r[7],
        r[8],
        r[13],
        r[14],
        1.0 if r[15] > 0.5 else 0.0,
        float(previous[0]),
        float(previous[1]),
        float(previous[2]),
        float(previous[3]),
    ]


def clip_action(a) -> list[float]:
    return [min(1.0, max(-1.0, float(x))) for x in a]


def controls(a, ap: Approach) -> dict[str, float]:
    """An action's control positions, as sim::Controls: elevator nose-up
    positive, the throttle 0 to 1; the flaps at the landing flap, no trim, no
    brakes. The JSBSim properties are written as sim::Aircraft::set_controls
    writes them."""
    a = clip_action(a)
    return {
        "elevator": a[0],
        "aileron": a[1],
        "rudder": a[2],
        "throttle": (a[3] + 1.0) / 2.0,
        "flaps": ap.flaps,
    }


def write_controls(fdm, c: dict[str, float]) -> None:
    fdm["fcs/elevator-cmd-norm"] = -c["elevator"]
    fdm["fcs/aileron-cmd-norm"] = c["aileron"]
    fdm["fcs/rudder-cmd-norm"] = c["rudder"]
    fdm["fcs/throttle-cmd-norm[0]"] = min(1.0, max(0.0, c["throttle"]))
    fdm["fcs/mixture-cmd-norm[0]"] = 1.0
    fdm["fcs/advance-cmd-norm[0]"] = 1.0
    fdm["fcs/flap-cmd-norm"] = c["flaps"]
    fdm["fcs/left-brake-cmd-norm"] = 0.0
    fdm["fcs/right-brake-cmd-norm"] = 0.0
    fdm["fcs/pitch-trim-cmd-norm"] = -0.0


@dataclass
class Start:
    """Where a flight begins: `out_m` before the threshold on the extended
    centreline, `across_m` right of it, `high_m` above the glidepath, and a
    steady wind - `crosswind_kts` from the left of the landing direction,
    `headwind_kts` down it."""

    out_m: float = 2.0 * METRES_PER_NM
    across_m: float = 0.0
    high_m: float = 0.0
    heading_offset_deg: float = 0.0
    airspeed_kts: float | None = None
    crosswind_kts: float = 0.0
    headwind_kts: float = 0.0


def wind_ned_fps(rw: Runway, s: Start) -> tuple[float, float]:
    """The wind's north and east components, in feet a second, blowing
    towards: a crosswind from the left blows towards the right of the landing
    direction, as tests/unit/test_lander.cpp has it, and a headwind towards
    the aeroplane."""
    h = rw.heading_deg / DEGREES
    towards_right = h + math.pi / 2.0
    mps_cross = s.crosswind_kts * 0.514444
    mps_head = s.headwind_kts * 0.514444
    north = mps_cross * math.cos(towards_right) - mps_head * math.cos(h)
    east = mps_cross * math.sin(towards_right) - mps_head * math.sin(h)
    return north * FEET_PER_METRE, east * FEET_PER_METRE


def start_position(rw: Runway, ap: Approach, s: Start) -> tuple[float, float, float]:
    h = rw.heading_deg / DEGREES
    north_m = -s.out_m * math.cos(h) - s.across_m * math.sin(h)
    east_m = -s.out_m * math.sin(h) + s.across_m * math.cos(h)
    lat = rw.threshold_lat_deg + north_m / metres_per_degree_latitude(rw.threshold_lat_deg)
    lon = rw.threshold_lon_deg + east_m / metres_per_degree_longitude(rw.threshold_lat_deg)
    alt_ft = rw.elevation_ft + (
        (s.out_m + ap.aim_m) * math.tan(ap.glidepath_deg / DEGREES) + s.high_m
    ) * FEET_PER_METRE
    return lat, lon, alt_ft


def new_fdm(jsbsim_root: str):
    import jsbsim

    jsbsim.FGJSBBase().debug_lvl = 0
    fdm = jsbsim.FGFDMExec(jsbsim_root)
    fdm.set_debug_level(0)
    if not fdm.load_model("c172p"):
        raise RuntimeError("JSBSim could not load the c172p from " + jsbsim_root)
    fdm.set_dt(1.0 / STEPS_PER_SECOND)
    return fdm


def initialise(fdm, rw: Runway, ap: Approach, s: Start) -> None:
    """As sim::Aircraft::initialize does with `flaps`, `flight_path_deg` and
    `trim` set: the flaps run out while JSBSim trims, the engine started, and
    the longitudinal trim."""
    lat, lon, alt_ft = start_position(rw, ap, s)
    fdm["ic/lat-geod-deg"] = lat
    fdm["ic/long-gc-deg"] = lon
    fdm["ic/terrain-elevation-ft"] = rw.elevation_ft
    fdm["ic/h-sl-ft"] = alt_ft
    fdm["ic/psi-true-deg"] = rw.heading_deg + s.heading_offset_deg
    fdm["ic/theta-deg"] = 0.0
    fdm["ic/phi-deg"] = 0.0
    fdm["ic/vc-kts"] = ap.vref_kts if s.airspeed_kts is None else s.airspeed_kts
    fdm["ic/gamma-deg"] = -ap.glidepath_deg
    fdm["fcs/flap-cmd-norm"] = ap.flaps
    fdm.set_trim_status(True)
    ok = fdm.run_ic()
    fdm.set_trim_status(False)
    if not ok:
        raise RuntimeError("JSBSim refused the initial conditions")
    fdm["propulsion/set-running"] = -1
    try:
        fdm.do_trim(1)  # tLongitudinal
    except Exception:
        # As sim::Aircraft does where JSBSim cannot trim: started again,
        # untrimmed, from the same conditions.
        fdm.set_trim_status(True)
        ok = fdm.run_ic()
        fdm.set_trim_status(False)
        if not ok:
            raise RuntimeError("JSBSim refused the initial conditions after a failed trim")
        fdm["propulsion/set-running"] = -1
    north, east = wind_ned_fps(rw, s)
    fdm["atmosphere/wind-north-fps"] = north
    fdm["atmosphere/wind-east-fps"] = east
    fdm["atmosphere/wind-down-fps"] = 0.0


def read(fdm) -> list[float]:
    return [float(fdm[p]) for p in READINGS]


@dataclass
class Touch:
    sink_fpm: float = 0.0
    across_m: float = 0.0
    along_m: float = 0.0  # from the threshold, positive down the runway
    pitch_deg: float = 0.0


@dataclass
class Flight:
    """One flight, flown from a start by `policy` (a function from the
    observation to an action), to five seconds after the wheels first
    touched, or to its end."""

    touched: bool = False
    touch: Touch = field(default_factory=Touch)
    ended: str = ""
    highest_after_touch_ft: float = 0.0
    worst_roll_after_touch_deg: float = 0.0
    least_pitch_after_touch_deg: float = 0.0
    seconds: float = 0.0


# How long after the touch a flight is flown on, to see it stays down.
AFTER_TOUCH_S = 5.0
# The longest a flight is let run.
LONGEST_S = 300.0


def crashed(r: list[float], w: Where) -> str:
    if abs(r[3]) > 45.0 / DEGREES:
        return "banked past 45 degrees"
    if r[4] < -25.0 / DEGREES or r[4] > 30.0 / DEGREES:
        return "pitched past its limits"
    if abs(w.across_m) > 400.0:
        return "more than 400 m off the centreline"
    if w.along_m < -3000.0:
        return "past the runway's end in the air"
    if w.above_m > 600.0:
        return "climbed away"
    return ""


def verification_starts() -> list[tuple[str, Start]]:
    """**The starts the policy is judged from**, the same set as
    tests/unit/test_learnt.cpp flies: a final-approach gate two miles out,
    on the centreline and 50 m either side of it, on the glidepath and 15 m
    (about fifty feet) above and below it, in calm air and in a ten-knot
    crosswind from either side - 3 x 3 x 3 = 27 starts."""
    out = []
    for wind in (0.0, 10.0, -10.0):
        for across in (-50.0, 0.0, 50.0):
            for high in (-15.0, 0.0, 15.0):
                name = f"across {across:+.0f} m, high {high:+.0f} m, crosswind {wind:+.0f} kt"
                out.append(
                    (name, Start(out_m=2.0 * METRES_PER_NM, across_m=across, high_m=high,
                                 crosswind_kts=wind))
                )
    return out
