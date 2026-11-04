"""The landing task as a gymnasium environment, and a flight flown by any
policy from a stated start.

Each episode begins somewhere on a final approach - between a sixth of a mile
and two and a half miles out, off the centreline, off the glidepath, off the
runway heading and off the speed, in a steady wind of up to fifteen knots
from anywhere but no more than five of it behind (`wind`; so since
2026-09-30, though this said thirteen across and eight down it until
2026-11-02) - and ends five seconds after the
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

**`nose_up`** (train.py `--nose-up`, 2026-11-04) costs that much at the
touch for each degree her nose is below level, to 15. The flight ends at
the touch, so how she rolls after it is never seen in training, and the
PA-28's policy touched at 8 degrees nose down - on her nosewheel - and in
the simulation's roll-out pitched to 15 degrees nose down and banked 20.
Without it (0) the reward is the one the C172P's and the C182's policies
were trained with.

**`nose_up_from_deg`** (train.py `--nose-up-from`, 2026-11-05) measures
`nose_up`'s degrees from that attitude, not from level: the FAA's Airplane
Flying Handbook (FAA-H-8083-3C, chapter 9) has a light aeroplane touch on
its main wheels with the nose held off, and the PA-28's policy, full of
fuel and flying the whole approach, touched flat at her approach speed,
1.3 degrees nose down, and in the simulation's roll-out pitched to 14 nose
down and banked 21. Without it (0) the cost is measured from level.

**`sink_over`** (train.py `--sink-over`, 2026-11-05) costs that much at
the touch for each ft/min she sinks over 200, to 1,000: the touch's own
sink term is shallow near the 300 ft/min limit (3.5 there, 7 at 230), and
the PA-28's policy, full of fuel, held her off to 41 kt from the hand-over
in strong wind and dropped her on at 300 to 370. Without it (0) the reward
is the one the C172P's and the C182's policies were trained with.

**`full_fuel`** (train.py `--full-fuel`, 2026-11-05): this fraction of the
flights with her tanks full, the edge of training's fuel that drew few of
them and failed her verification. Without it no random number is drawn for
it.
"""

from __future__ import annotations

import dataclasses
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

    def __init__(self, runway: L.Runway, approach: L.Approach, plane: L.Aeroplane,
                 speed_costs: bool = False, nose_up: float = 0.0, nose_up_from_deg: float = 0.0,
                 sink_over: float = 0.0):
        self.rw = runway
        self.nose_up = nose_up
        self.nose_up_from_deg = nose_up_from_deg
        self.sink_over = sink_over
        self.ap = approach
        self.plane = plane
        # The stabilized approach's speed costs (STABILIZED), or the reward
        # the committed policy was trained with (the default).
        self.speed_costs = speed_costs
        self.fdm = L.new_fdm(JSBSIM_ROOT, plane.model)
        self.started_in_the_wind = False
        # What she weighs with no fuel: the empty aeroplane and what is on
        # board, as the model has them.
        self.dry_lbs = self.fdm["inertia/empty-weight-lbs"] + sum(
            self.fdm[f"inertia/pointmass-weight-lbs[{i}]"] for i in range(5))

    def begin(self, start: L.Start) -> list[float]:
        # **A hand-over start in the wind gets an aeroplane of its own**, and so
        # does the start after it: L.into_the_wind's initial conditions, given
        # as ground velocities, are kept by JSBSim into the next start's, which
        # then began slipping and, a few starts on, tumbling.
        if start.controls_as_trimmed or self.started_in_the_wind:
            self.fdm = L.new_fdm(JSBSIM_ROOT, self.plane.model)
        self.started_in_the_wind = start.controls_as_trimmed
        L.initialise(self.fdm, self.rw, self.ap, start, self.plane)
        # **She weighs what the start says**: the fuel is the start's, not
        # what the last flight left.
        weight = self.fdm["inertia/weight-lbs"]
        wanted = self.dry_lbs + self.plane.tanks * L.fuel_of(start, self.plane)
        if abs(weight - wanted) > 0.01:
            raise RuntimeError(f"she weighs {weight} lb, and the start says {wanted}")
        self.previous = [0.0] * L.ACTIONS
        if start.controls_as_trimmed:
            # The elevator with what JSBSim trimmed on the pitch trim, which
            # write_controls holds at nothing.
            self.previous = [-(self.fdm["fcs/elevator-cmd-norm"] + self.fdm["fcs/pitch-trim-cmd-norm"]),
                             self.fdm["fcs/aileron-cmd-norm"],
                             self.fdm["fcs/rudder-cmd-norm"],
                             self.fdm["fcs/throttle-cmd-norm[0]"] * 2.0 - 1.0]
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
                reward -= self.nose_up * min(15.0, max(0.0, self.nose_up_from_deg - f.touch.pitch_deg))
                reward -= self.sink_over * min(1000.0, max(0.0, f.touch.sink_fpm - 200.0))
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


# **Where the approach autopilot hands her to the policy**: 35 ft over the
# runway (sim::LearntLander::policy_takes_her_ft), on the glidepath - a copy:
# a change there is a change here.
HANDOVER_FT = 35.0


def random_start(rng: np.random.Generator, plane: L.Aeroplane,
                 behind_left: float = 0.0, handover: float = 0.0,
                 ahead: float = 0.0, handover_across: float = 0.0) -> L.Start:
    # **From the approach autopilot's hand-over** (train.py's `--handover`,
    # 2026-11-04): this fraction of the flights from near where a session's
    # approach autopilot hands her to the policy - 25 to 45 ft over the
    # runway, within 2 m of the glidepath and 5 m of the centreline, up to
    # 8 degrees off the heading (her crab), at her speed to 3 kt either side,
    # trimmed down the glidepath, in training's wind - so that the round out
    # and the touch from there are learnt, not only reached. Without it no
    # random number is drawn for it, as for `behind_left`.
    # **Strong wind ahead** (train.py's `--ahead`, 2026-11-04): this fraction
    # of the flights as the rest, but in 10 to 15 kt from no more than 50
    # degrees either side of ahead, where the C182's policy of that day
    # drifted 6 to 8 m off the centreline from the gate's corners (15 ahead,
    # and 15 quartering ahead from the left). Without it no random number is
    # drawn for it.
    strong_ahead = ahead > 0.0 and rng.random() < ahead
    if handover > 0.0 and rng.random() < handover:
        # **In strong wind** (train.py's `--handover-across`, 2026-11-04):
        # this fraction of the hand-over's flights in `strong_wind`, where the
        # PA-28's policy of that day, handed over at 35 ft in 10 kt from the
        # left, sank 465 ft/min - and those at her speed down to 6 kt slow
        # and up to 3 kt fast, not 3 either side. With it every hand-over
        # start is made as the simulation hands her over: crabbed into the
        # wind besides the 8 degrees either side, unslipped, with her trimmed
        # controls as the last action (landing.py's `into_the_wind`). Without
        # it no random number is drawn for it.
        strong = handover_across > 0.0 and rng.random() < handover_across
        ap = plane.approach
        tan = math.tan(ap.glidepath_deg / L.DEGREES)
        above_m = rng.uniform(HANDOVER_FT - 10.0, HANDOVER_FT + 10.0) / L.FEET_PER_METRE
        return L.Start(
            out_m=above_m / tan - ap.aim_m,
            across_m=rng.uniform(-5.0, 5.0),
            high_m=rng.uniform(-2.0, 2.0),
            heading_offset_deg=rng.uniform(-8.0, 8.0),
            # In strong wind from 6 kt slow to 3 kt fast: the approach
            # autopilot hands her over up to 5 kt under her speed (the gate
            # corner tests).
            airspeed_kts=ap.vref_kts + (rng.uniform(-6.0, 3.0) if strong else rng.uniform(-3.0, 3.0)),
            fuel_lbs=float(rng.uniform(*plane.training_fuel_lbs)),
            controls_as_trimmed=handover_across > 0.0,
            **(strong_wind(rng) if strong else
               wind_ahead(rng) if strong_ahead else wind(rng)),
        )
    # **Behind and across from the left** (train.py's `--behind-left`): this
    # fraction of the flights from near the two corners of the learnt
    # landing's gate (sim::LearntGate) the policy of 2026-11-02 touched short
    # from with the wind behind and across from the left: nearest, furthest
    # left, lowest and slowest, turned either way. Drawn from anywhere in
    # the gate (tried 2026-11-03), so few of the flights were near them that
    # training moved them no nearer the runway. Without it no random number
    # is drawn for it, so that the committed parts train again as they did.
    if behind_left > 0.0 and rng.random() < behind_left:
        def low_end(lo: float, hi: float) -> float:
            # Up to a twentieth of the way in from its low end.
            return lo + rng.uniform(0.0, 0.05) * (hi - lo)
        return L.Start(
            out_m=low_end(1.6, 2.4) * L.METRES_PER_NM,
            across_m=low_end(-60.0, 60.0),
            high_m=low_end(-20.0, 20.0),
            heading_offset_deg=float(rng.choice([-1.0, 1.0])) * (5.0 - rng.uniform(0.0, 0.5)),
            airspeed_kts=plane.approach.vref_kts + low_end(-3.0, 8.0),
            fuel_lbs=float(rng.uniform(*plane.training_fuel_lbs)),
            **behind_and_across_from_the_left(rng),
        )
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
        airspeed_kts=plane.approach.vref_kts + rng.uniform(-3.0, 8.0),
        fuel_lbs=float(rng.uniform(*plane.training_fuel_lbs)),
        **(wind_ahead(rng) if strong_ahead else wind(rng)),
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


def strong_wind(rng: np.random.Generator) -> dict[str, float]:
    """**Strong wind at the hand-over** (random_start's `handover_across`):
    half the flights 8 to 15 kt across from either side, with up to 5 kt
    behind or ahead and no more than 15 in all; the rest 12 to 15 kt from
    anywhere `wind` blows from - the learnt landing's gate's edge winds."""
    if rng.random() < 0.5:
        cross = float(rng.choice([-1.0, 1.0])) * rng.uniform(8.0, 15.0)
        most = min(5.0, np.sqrt(15.0**2 - cross**2))
        return {"crosswind_kts": float(cross), "headwind_kts": float(rng.uniform(-most, most))}
    while True:
        speed = rng.uniform(12.0, 15.0)
        towards = rng.uniform(0.0, 2.0 * np.pi)
        cross, head = speed * np.sin(towards), speed * np.cos(towards)
        if head >= -5.0:
            return {"crosswind_kts": float(cross), "headwind_kts": float(head)}


def wind_ahead(rng: np.random.Generator) -> dict[str, float]:
    """10 to 15 kt from no more than 50 degrees either side of ahead
    (random_start's `ahead`)."""
    speed = rng.uniform(10.0, 15.0)
    towards = rng.uniform(-50.0, 50.0) / L.DEGREES
    return {"crosswind_kts": float(speed * np.sin(towards)),
            "headwind_kts": float(speed * np.cos(towards))}


def behind_and_across_from_the_left(rng: np.random.Generator) -> dict[str, float]:
    """**A wind behind and across from the left**, within `wind`'s: up to five
    knots behind, and across from the left as much as keeps it within fifteen
    in all - the quarter the policy of 2026-11-02 touched short in (two of its
    gate's corners at 5 behind and 14.1 across)."""
    head = rng.uniform(-5.0, -4.5)
    cross = rng.uniform(0.0, np.sqrt(15.0**2 - head**2))
    return {"crosswind_kts": float(cross), "headwind_kts": float(head)}


class LandingEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(self, model: str, seed: int = 0, speed_costs: bool = False,
                 behind_left: float = 0.0, handover: float = 0.0, ahead: float = 0.0,
                 nose_up: float = 0.0, handover_across: float = 0.0,
                 nose_up_from_deg: float = 0.0, full_fuel: float = 0.0, sink_over: float = 0.0):
        super().__init__()
        self.full_fuel = full_fuel
        self.handover_across = handover_across
        self.ahead = ahead
        self.behind_left = behind_left
        self.handover = handover
        plane = L.aeroplane(model)
        self.flier = Flier(L.Runway(), plane.approach, plane, speed_costs, nose_up, nose_up_from_deg,
                           sink_over)
        self.observation_space = gym.spaces.Box(-np.inf, np.inf, (L.OBSERVATIONS,), np.float64)
        self.action_space = gym.spaces.Box(-1.0, 1.0, (L.ACTIONS,), np.float32)
        self.rng = np.random.default_rng(seed)

    def reset(self, *, seed=None, options=None):
        if seed is not None:
            self.rng = np.random.default_rng(seed)
        start = random_start(self.rng, self.flier.plane, self.behind_left, self.handover,
                             self.ahead, self.handover_across)
        if self.full_fuel > 0.0 and self.rng.random() < self.full_fuel:
            start = dataclasses.replace(start, fuel_lbs=self.flier.plane.full_tank_lbs)
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
