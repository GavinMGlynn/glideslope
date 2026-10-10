"""The landing task as a gymnasium environment, and a flight flown by any
policy from a stated start.

Each episode begins somewhere on a final approach - between a sixth of a mile
and two and a half miles out, off the centreline, off the glidepath, off the
runway heading and off the speed, in a steady wind of up to thirteen knots
across the runway and up to eight down it - and ends five seconds after the
wheels first touch, or when the flight goes wrong.

**The reward** is mostly at the touch: for touching at all, for sinking
slowly, for touching near the centreline and on the runway. Before it, the
approach is shaped by a potential - near the centreline, on the glidepath,
tracking down the runway, at the speed - and moving the controls about costs
a little. After it, bouncing and banking cost. Going wrong costs.

**The default reward is the one the committed policy was trained with**, so
that train.py with the command and seed its header records trains it again.
With it the approach's speed is shaped by the potential alone, which changes
no optimum (Ng, Harada and Russell, 1999) and fades to nothing at the runway:
the committed policy dives down the glidepath on power and passes 500 ft up
to 35 kt fast.

**`speed_costs=True`** (train.py `--speed-costs`) adds the stabilized
approach's speed costs (`STABILIZED`, tried 2026-10-10 and not enough - see
docs/PROJECT_STATUS.md): the speed off a band inside the gate's +10/-5 kt
from 500 ft down to the flare, every tenth of a second, and going wrong
charged what the speed could still have cost. No committed policy was
trained with them.
"""

from __future__ import annotations

import math
import os

import gymnasium as gym
import numpy as np

import landing as L

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
JSBSIM_ROOT = os.path.join(REPO, "assets", "jsbsim")


# The discount, as train.py's. **0.999, not 0.995**: from the gate the touch
# is some 1,300 decisions away, and 0.995 to that power is 0.0015 - the
# touch's reward, the centreline's most of all, was invisible for all but
# the last twenty seconds. 0.999 to the power of the last 200 is 0.82.
GAMMA = 0.999

# **The stabilized approach the speed is held to**: from `gate_ft` over the
# runway down to `down_to_ft`, each tenth of a second outside `fast_kts` over
# or `slow_kts` under the approach speed costs `per_kt` for each knot
# outside, to `most_kts`, and outside the gate's own band (+10/-5 kt,
# sim::StabilizedApproach) `outside_gate` more. The band is narrower than the
# gate's, a margin for the mean action and for the simulation's own flight
# of it.
#
# **Down to where the simulation's flare begins, not the gate's 50 ft**:
# sim::Lander flares the C172P at a quarter of a foot a knot of its approach
# speed, 15 ft, with its wheels hanging some 5 ft below where its height is
# measured from - 20 ft. Costed only to 50 ft (tried 2026-10-10,
# docs/PROJECT_STATUS.md), the policy learnt to flare from 100 ft and was 10
# kt slow by 50, the gate still judging it.
#
# **Outside the gate's band outweighs the softest touch.** The touch's sink
# term (`touchdown_reward`) is 40 exp(-(sink/250)^2) - sink/50: 40 at none,
# 31 at 106 ft/min (the slow flare's), 3.5 at 300 - at most 36.5 between
# the softest touch and the limit. The gate sends round after two seconds
# running, twenty decisions: at 2.0 a decision those cost 40, more than any
# touch could gain, and ten seconds of a slow flare 200.
#
# **Going wrong is never a way out of it.** Charged some 2.6 a decision for
# 700 decisions, a policy 25 kt fast from 500 ft would rather go wrong in the
# air for its 70 (tried 2026-10-10: from the committed policy at 0.05 a knot
# and learning rate 1e-4 its landings within the limits fell from 399 of 400
# to 179 within 1.2 million decisions). Going wrong now costs as well the
# most the speed could still have cost, at `least_fpm` from where it went
# wrong (`speed_cost_left`), and the knot costs 0.02 again.
STABILIZED = dict(gate_ft=500.0, down_to_ft=20.0, fast_kts=5.0, slow_kts=2.0, per_kt=0.02,
                  most_kts=30.0, outside_gate=2.0, least_fpm=300.0, judged_down_to_ft=50.0,
                  judged_fast_kts=10.0, judged_slow_kts=5.0)


def speed_cost_left(above_ft: float) -> float:
    """The most the speed can cost from `above_ft` down (STABILIZED): every
    decision outside the gate's band and `most_kts` off, descending at no
    more than `least_fpm`."""
    st = STABILIZED
    feet = max(0.0, min(above_ft, st["gate_ft"]) - st["down_to_ft"])
    decisions = feet / (st["least_fpm"] / 600.0)
    return decisions * (st["per_kt"] * st["most_kts"] + st["outside_gate"])


def outside_band(kts: float, vref_kts: float, fast_kts: float, slow_kts: float) -> float:
    """How many knots `kts` is outside +`fast_kts`/-`slow_kts` of `vref_kts`."""
    return max(0.0, kts - vref_kts - fast_kts) + max(0.0, vref_kts - slow_kts - kts)


class Flier:
    """One aeroplane flown decision by decision: the part of the environment
    that a trained policy is evaluated with too."""

    def __init__(self, runway: L.Runway, approach: L.Approach, speed_costs: bool = False):
        self.rw = runway
        self.ap = approach
        # The stabilized approach's speed costs (STABILIZED), or the reward
        # the committed policy was trained with (the default).
        self.speed_costs = speed_costs
        self.fdm = L.new_fdm(JSBSIM_ROOT)
        # What she weighs with no fuel: the empty aeroplane and what is on
        # board, as the model has them.
        self.dry_lbs = self.fdm["inertia/empty-weight-lbs"] + sum(
            self.fdm[f"inertia/pointmass-weight-lbs[{i}]"] for i in range(5))

    def begin(self, start: L.Start) -> list[float]:
        L.initialise(self.fdm, self.rw, self.ap, start)
        # **She weighs what the start says**: the fuel is the start's, not
        # what the last flight left.
        weight = self.fdm["inertia/weight-lbs"]
        wanted = self.dry_lbs + L.TANKS * start.fuel_lbs
        if abs(weight - wanted) > 0.01:
            raise RuntimeError(f"she weighs {weight} lb, and the start says {wanted}")
        self.previous = [0.0] * L.ACTIONS
        self.flight = L.Flight()
        self.touch_above_m = 0.0
        self.touch_agl_ft = 0.0
        self.t = 0.0
        self.unstable_steps = 0
        self.readings = L.read(self.fdm)
        w = L.where(self.readings, self.rw)
        self.phi = self.potential(self.readings, w)
        self.integral = L.remember(0.0, w.across_m, self.ap)
        return L.observe(self.readings, self.rw, self.ap, self.previous, self.integral)

    def potential(self, r: list[float], w: L.Where) -> float:
        """How well placed the aeroplane is, 0 at best: near the centreline,
        on the glidepath (less so near the ground, where the flare leaves
        it), tracking down the runway, at the speed, wings level, and not
        floating past where the glidepath meets the runway."""
        glidepath_m = (w.along_m + self.ap.aim_m) * math.tan(self.ap.glidepath_deg / L.DEGREES)
        fade = min(1.0, max(0.0, w.above_m / 15.0))
        h = self.rw.heading_deg / L.DEGREES
        vn, ve = r[10] * L.FPS_TO_MPS, r[11] * L.FPS_TO_MPS
        track = math.atan2(ve * math.cos(h) - vn * math.sin(h),
                           ve * math.sin(h) + vn * math.cos(h))
        return -(
            min(abs(w.across_m), 300.0) / 10.0
            + 4.0 * (1.0 - math.exp(-((w.across_m / 8.0) ** 2)))
            + fade * min(abs(w.above_m - glidepath_m), 60.0) / 4.0
            + min(abs(track), 1.0) / 0.2
            + fade * min(abs(r[9] - self.ap.vref_kts), 30.0) / 5.0
            + max(0.0, -w.along_m - self.ap.aim_m) / 50.0
            + abs(r[3]) / 0.5
        )

    def decide(self, action) -> tuple[list[float], float, bool, bool]:
        """Flies one decision: returns the observation, the reward, whether
        the flight is over, and whether it ended by running out of time."""
        a = L.clip_action(action)
        c = L.controls(a, self.ap)
        L.write_controls(self.fdm, c)
        reward = 0.0
        f = self.flight
        for _ in range(self.ap.decision_steps):
            self.fdm.run()
            self.t += 1.0 / L.STEPS_PER_SECOND
            wow = self.fdm["gear/wow"] > 0.5
            if wow and not f.touched:
                r = L.read(self.fdm)
                w = L.where(r, self.rw)
                f.touched = True
                f.touch = L.Touch(
                    sink_fpm=-self.fdm["velocities/h-dot-fps"] * 60.0,
                    across_m=w.across_m,
                    along_m=-w.along_m,
                    pitch_deg=r[4] * L.DEGREES,
                    heading_error_deg=L.remainder(r[5] * L.DEGREES - self.rw.heading_deg, 360.0),
                )
                self.touch_time = self.t
                self.touch_above_m = w.above_m
                self.touch_agl_ft = self.fdm["position/h-agl-ft"]
                reward += self.touchdown_reward(f.touch)
            if not f.touched:
                self.judge_the_gate()
            if f.touched:
                rise_ft = self.fdm["position/h-agl-ft"] - self.touch_agl_ft
                f.highest_after_touch_ft = max(f.highest_after_touch_ft, rise_ft)
                roll = abs(self.fdm["attitude/phi-deg"])
                f.worst_roll_after_touch_deg = max(f.worst_roll_after_touch_deg, roll)
                f.least_pitch_after_touch_deg = min(
                    f.least_pitch_after_touch_deg, self.fdm["attitude/theta-deg"]
                )
        r = L.read(self.fdm)
        self.readings = r
        w = L.where(r, self.rw)
        self.integral = L.remember(self.integral, w.across_m, self.ap)
        obs = L.observe(r, self.rw, self.ap, a, self.integral)
        # Moving the controls about costs a little.
        reward -= 0.02 * sum((x - y) ** 2 for x, y in zip(a, self.previous))
        self.previous = a
        f.seconds = self.t
        if f.touched:
            rise = self.fdm["position/h-agl-ft"] - self.touch_agl_ft
            reward -= 0.5 * min(10.0, max(0.0, rise - 0.5))
            reward -= 0.05 * max(0.0, abs(r[3]) * L.DEGREES - 5.0)
            reward -= 0.02 * min(30.0, max(0.0, abs(w.across_m) - 3.0))
            if self.t - self.touch_time >= L.AFTER_TOUCH_S:
                f.ended = "touched"
                # Staying down after the touch was part of landing when the
                # policy flew five seconds past it. With the flight ending at
                # the touch (AFTER_TOUCH_S = 0) nothing can rise or bank in
                # time, so this is ten added to every touch: a constant, kept
                # because the committed policy was trained with it.
                if f.highest_after_touch_ft < 3.0 and f.worst_roll_after_touch_deg < 15.0:
                    reward += 10.0
                return obs, reward, True, False
            return obs, reward, False, False
        wrong = L.crashed(r, w)
        if wrong:
            f.ended = wrong
            # **And the most the speed could still have cost** (STABILIZED),
            # so that ending the flight is never a way out of it.
            if self.speed_costs:
                reward -= speed_cost_left(w.above_m * L.FEET_PER_METRE)
            return obs, reward - 70.0, True, False
        # Flying the approach well is shaped by a potential (Ng, Harada and
        # Russell, 1999): the reward is how much better the aeroplane is
        # placed than a tenth of a second ago, which cannot be farmed by
        # flying on and on, and leaves the touch the prize.
        phi = self.potential(r, w)
        reward += GAMMA * phi - self.phi
        self.phi = phi
        # **Off its speed from 500 ft down costs, every tenth of a second**
        # (STABILIZED): a cost, so nothing is gained by flying on.
        above_ft = w.above_m * L.FEET_PER_METRE
        st = STABILIZED
        if self.speed_costs and st["down_to_ft"] <= above_ft <= st["gate_ft"]:
            off = outside_band(r[9], self.ap.vref_kts, st["fast_kts"], st["slow_kts"])
            reward -= st["per_kt"] * min(off, st["most_kts"])
            if outside_band(r[9], self.ap.vref_kts, st["judged_fast_kts"], st["judged_slow_kts"]) > 0.0:
                reward -= st["outside_gate"]
        # **Off the centreline on short final costs, every tenth of a second**:
        # inside 1,500 m of the threshold, a hundredth for each metre off it,
        # to fifty. A cost, so nothing is gained by flying on; the touch short
        # of the threshold costs more than any of it.
        if w.along_m < 1500.0:
            reward -= 0.01 * min(abs(w.across_m), 50.0)
        if self.t >= L.LONGEST_S:
            f.ended = "out of time"
            return obs, reward, True, True
        return obs, reward, False, False

    def judge_the_gate(self) -> None:
        """The stabilized gate's judgement, step by step, as sim::unstabilized
        makes it of the speed: the longest it found her outside +10/-5 kt
        between 500 and 50 ft, running, for the training's log and the
        evaluation. Rewards nothing itself."""
        st = STABILIZED
        above_ft = (self.fdm["position/h-sl-ft"] - self.rw.elevation_ft)
        kts = self.fdm["velocities/vc-kts"]
        off = (st["judged_down_to_ft"] <= above_ft <= st["gate_ft"] and
               outside_band(kts, self.ap.vref_kts, st["judged_fast_kts"], st["judged_slow_kts"]) > 0.0)
        self.unstable_steps = self.unstable_steps + 1 if off else 0
        f = self.flight
        f.most_unstable_s = max(f.most_unstable_s, self.unstable_steps / L.STEPS_PER_SECOND)

    @staticmethod
    def touchdown_reward(t: L.Touch) -> float:
        # Smooth all the way out, so that a touch far off still says which
        # way is better: a bowl near the limits, and a slope beyond them.
        r = 40.0
        r += 40.0 * math.exp(-((t.sink_fpm / 250.0) ** 2)) - min(max(t.sink_fpm, 0.0), 1500.0) / 50.0
        r += 60.0 * math.exp(-((t.across_m / 4.0) ** 2)) - min(abs(t.across_m), 150.0) / 3.0
        # The crab taken off before the wheels meet the runway: pointing down
        # it, not sliding across it.
        r -= 1.0 * min(abs(t.heading_error_deg), 15.0)
        # On the runway, and well past its threshold: a hundred metres in.
        # Short of it is off the runway, which is worse than any touch on it
        # and worse than going wrong in the air - the further short the worse.
        if t.along_m < 0.0:
            r -= 120.0 + min(-t.along_m, 1000.0) / 10.0
        elif t.along_m <= 1200.0:
            r += 10.0 * min(1.0, t.along_m / 100.0)
        else:
            r -= 20.0
        return r


def random_start(rng: np.random.Generator) -> L.Start:
    # Three in four from a final-approach gate, where the whole approach is
    # flown; the rest from nearer in, where the flare is learnt sooner.
    if rng.random() < 0.75:
        out = rng.uniform(2800.0, 4600.0)
    else:
        out = rng.uniform(300.0, 2800.0)
    across_limit = min(100.0, 0.04 * out)
    high_limit = min(25.0, 0.008 * out)
    return L.Start(
        out_m=out,
        across_m=rng.uniform(-across_limit, across_limit),
        high_m=rng.uniform(-high_limit, high_limit),
        heading_offset_deg=rng.uniform(-8.0, 8.0),
        airspeed_kts=59.8 + rng.uniform(-3.0, 8.0),
        fuel_lbs=float(rng.uniform(*L.TRAINING_FUEL_LBS)),
        **wind(rng),
    )


def wind(rng: np.random.Generator) -> dict[str, float]:
    """A steady wind of up to fifteen knots from anywhere, but no more than
    five knots of it behind - which is as much tailwind as a light
    aeroplane's handbook lands with."""
    while True:
        speed = rng.uniform(0.0, 15.0)
        towards = rng.uniform(0.0, 2.0 * np.pi)
        cross, head = speed * np.sin(towards), speed * np.cos(towards)
        if head >= -5.0:
            return {"crosswind_kts": float(cross), "headwind_kts": float(head)}


class LandingEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(self, seed: int = 0, speed_costs: bool = False):
        super().__init__()
        self.flier = Flier(L.Runway(), L.Approach(), speed_costs)
        self.observation_space = gym.spaces.Box(-np.inf, np.inf, (L.OBSERVATIONS,), np.float64)
        self.action_space = gym.spaces.Box(-1.0, 1.0, (L.ACTIONS,), np.float32)
        self.rng = np.random.default_rng(seed)

    def reset(self, *, seed=None, options=None):
        if seed is not None:
            self.rng = np.random.default_rng(seed)
        start = random_start(self.rng)
        obs = self.flier.begin(start)
        return np.asarray(obs, dtype=np.float64), {}

    def step(self, action):
        obs, reward, done, truncated = self.flier.decide(action)
        info = {}
        if done:
            f = self.flier.flight
            info["landing"] = {
                "touched": f.touched,
                "sink_fpm": f.touch.sink_fpm,
                "across_m": f.touch.across_m,
                "along_m": f.touch.along_m,
                "ended": f.ended,
                "most_unstable_s": f.most_unstable_s,
            }
        return (
            np.asarray(obs, dtype=np.float64),
            float(reward),
            bool(done and not truncated),
            bool(truncated),
            info,
        )
