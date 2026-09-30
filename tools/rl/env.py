"""The landing task as a gymnasium environment, and a flight flown by any
policy from a stated start.

Each episode begins somewhere on a final approach - between a sixth of a mile
and two and a half miles out, off the centreline, off the glidepath, off the
runway heading and off the speed, in a steady wind of up to thirteen knots
across the runway and up to eight down it - and ends five seconds after the
wheels first touch, or when the flight goes wrong.

**The reward** is a little for each tenth of a second flown near the
centreline, on the glidepath, tracking down the runway and at the speed, less
a little for moving the controls; and at the touch, most of it: for touching
at all, for sinking slowly, for touching near the centreline and on the
runway. After the touch, bouncing and banking cost. Going wrong costs.
"""

from __future__ import annotations

import math
import os

import gymnasium as gym
import numpy as np

import landing as L

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
JSBSIM_ROOT = os.path.join(REPO, "assets", "jsbsim")


def gauss(x: float, width: float) -> float:
    return math.exp(-((x / width) ** 2))


class Flier:
    """One aeroplane flown decision by decision: the part of the environment
    that a trained policy is evaluated with too."""

    def __init__(self, runway: L.Runway, approach: L.Approach):
        self.rw = runway
        self.ap = approach
        self.fdm = L.new_fdm(JSBSIM_ROOT)

    def begin(self, start: L.Start) -> list[float]:
        L.initialise(self.fdm, self.rw, self.ap, start)
        self.previous = [0.0] * L.ACTIONS
        self.flight = L.Flight()
        self.touch_above_m = 0.0
        self.touch_agl_ft = 0.0
        self.t = 0.0
        self.readings = L.read(self.fdm)
        return L.observe(self.readings, self.rw, self.ap, self.previous)

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
                )
                self.touch_time = self.t
                self.touch_above_m = w.above_m
                self.touch_agl_ft = self.fdm["position/h-agl-ft"]
                reward += self.touchdown_reward(f.touch)
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
        obs = L.observe(r, self.rw, self.ap, a)
        # Moving the controls about costs a little.
        reward -= 0.02 * sum((x - y) ** 2 for x, y in zip(a, self.previous))
        self.previous = a
        f.seconds = self.t
        if f.touched:
            rise = self.fdm["position/h-agl-ft"] - self.touch_agl_ft
            reward -= 0.5 * max(0.0, rise - 0.5)
            reward -= 0.05 * max(0.0, abs(r[3]) * L.DEGREES - 5.0)
            reward -= 0.02 * max(0.0, abs(w.across_m) - 3.0)
            if self.t - self.touch_time >= L.AFTER_TOUCH_S:
                f.ended = "touched"
                # Staying down the whole time is part of landing.
                if f.highest_after_touch_ft < 3.0 and f.worst_roll_after_touch_deg < 15.0:
                    reward += 10.0
                return obs, reward, True, False
            return obs, reward, False, False
        wrong = L.crashed(r, w)
        if wrong:
            f.ended = wrong
            return obs, reward - 50.0, True, False
        # Flying the approach well is worth a little every tenth of a second.
        glidepath_m = (w.along_m + self.ap.aim_m) * math.tan(self.ap.glidepath_deg / L.DEGREES)
        near_ground = min(1.0, max(0.0, w.above_m / 15.0))
        h = self.rw.heading_deg / L.DEGREES
        vn, ve = r[10] * L.FPS_TO_MPS, r[11] * L.FPS_TO_MPS
        track_error = math.atan2(ve * math.cos(h) - vn * math.sin(h), ve * math.sin(h) + vn * math.cos(h))
        reward += 0.025 * (
            gauss(w.across_m, 15.0)
            + near_ground * gauss(w.above_m - glidepath_m, 6.0)
            + gauss(track_error, 0.1)
            + near_ground * gauss(r[9] - self.ap.vref_kts, 6.0)
        )
        if self.t >= L.LONGEST_S:
            f.ended = "out of time"
            return obs, reward, True, True
        return obs, reward, False, False

    @staticmethod
    def touchdown_reward(t: L.Touch) -> float:
        r = 20.0
        r += 30.0 * max(-2.0, 1.0 - t.sink_fpm / 300.0)
        r += 30.0 * max(-2.0, 1.0 - abs(t.across_m) / 5.0)
        r += 10.0 if 0.0 <= t.along_m <= 1200.0 else -20.0
        return r


def random_start(rng: np.random.Generator) -> L.Start:
    if rng.random() < 0.5:
        out = rng.uniform(1800.0, 4600.0)
    else:
        out = rng.uniform(300.0, 1800.0)
    across_limit = min(100.0, 0.04 * out)
    high_limit = min(25.0, 0.008 * out)
    return L.Start(
        out_m=out,
        across_m=rng.uniform(-across_limit, across_limit),
        high_m=rng.uniform(-high_limit, high_limit),
        heading_offset_deg=rng.uniform(-8.0, 8.0),
        airspeed_kts=59.8 + rng.uniform(-3.0, 8.0),
        crosswind_kts=rng.uniform(-13.0, 13.0),
        headwind_kts=rng.uniform(-2.0, 8.0),
    )


class LandingEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(self, seed: int = 0):
        super().__init__()
        self.flier = Flier(L.Runway(), L.Approach())
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
            }
        return (
            np.asarray(obs, dtype=np.float64),
            float(reward),
            bool(done and not truncated),
            bool(truncated),
            info,
        )
