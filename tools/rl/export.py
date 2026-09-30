"""Writes a trained checkpoint as the policy file the simulation reads, and
records the parity fixture the simulation's test holds its observation and
action to.

    ~/.venvs/glideslope-rl/bin/python tools/rl/export.py CHECKPOINT \
        --seed 1 --steps 20000000

CHECKPOINT is a stable-baselines3 .zip written by train.py, with its
.vecnorm beside it. Writes assets/rl/c172p-landing.txt and
tests/data/rl/c172p-landing-parity.txt.

**The policy is the mean of the trained action distribution** - the network
from the observation, normalised as in training, through the policy's hidden
layers to the action - with no sampling. The file is checked against the
checkpoint's own network (PyTorch, in single precision) before it is written.
"""

from __future__ import annotations

import argparse
import os
import sys
from importlib import metadata

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numpy as np
import torch

import landing as L
import policy_file
from env import Flier, REPO
from evaluate import fly

POLICY = os.path.join(REPO, "assets", "rl", "c172p-landing.txt")
PARITY = os.path.join(REPO, "tests", "data", "rl", "c172p-landing-parity.txt")


def number(x: float) -> str:
    return repr(float(x))


def from_checkpoint(path: str, header: list[str]) -> policy_file.Policy:
    from stable_baselines3 import PPO
    from stable_baselines3.common.vec_env import VecNormalize

    model = PPO.load(path, device="cpu")
    with open(path.removesuffix(".zip") + ".vecnorm", "rb") as f:
        import pickle

        vecnorm: VecNormalize = pickle.load(f)
    rms = vecnorm.obs_rms
    p = policy_file.Policy(header=header)
    p.obs_mean = [float(v) for v in rms.mean]
    p.obs_scale = [float(1.0 / np.sqrt(v + vecnorm.epsilon)) for v in rms.var]
    p.obs_clip = float(vecnorm.clip_obs)
    net = model.policy.mlp_extractor.policy_net
    linears = [m for m in net if isinstance(m, torch.nn.Linear)]
    for lin in linears:
        p.layers.append(
            policy_file.Layer(lin.weight.detach().double().tolist(),
                              lin.bias.detach().double().tolist(), "tanh")
        )
    out = model.policy.action_net
    p.layers.append(
        policy_file.Layer(out.weight.detach().double().tolist(),
                          out.bias.detach().double().tolist(), "linear")
    )
    # Checked against the checkpoint's own network.
    rng = np.random.default_rng(0)
    for _ in range(50):
        obs = rng.normal(size=L.OBSERVATIONS) * 2.0 + np.array(p.obs_mean)
        norm = np.clip((obs - np.array(p.obs_mean)) * np.array(p.obs_scale), -p.obs_clip, p.obs_clip)
        with torch.no_grad():
            mean = model.policy.get_distribution(
                torch.as_tensor(norm, dtype=torch.float32).unsqueeze(0)
            ).distribution.mean.numpy()[0]
        mine = p.act(list(obs))
        theirs = np.clip(mean, -1.0, 1.0)
        assert np.max(np.abs(np.array(mine) - theirs)) < 1e-4, (mine, theirs)
    return p


def parity(policy: policy_file.Policy) -> list[str]:
    """Readings, observations and actions from flights of the policy: every
    thirtieth decision of three of the verification's flights and of one
    begun 150 m short of the threshold, and every fifth decision with the
    wheels on the ground."""
    rw = L.Runway()
    cases: list[tuple[list[float], list[float], list[float], list[float]]] = []
    starts = [s for _, s in L.verification_starts()]
    starts = [starts[0], starts[13], starts[26], L.Start(out_m=150.0, crosswind_kts=5.0)]
    for start in starts:
        n = [0]

        def record(flier: Flier, obs: list[float]) -> None:
            n[0] += 1
            on_ground = flier.readings[15] > 0.5
            if n[0] % 30 == 1 or (on_ground and n[0] % 5 == 0):
                cases.append((list(flier.previous), list(flier.readings), list(obs),
                              policy.act(obs)))

        fly(policy, start, record=record)
    lines = [
        "# The simulation's observation and action, held to the training's.",
        "# Made by tools/rl/export.py, from flights of assets/rl/c172p-landing.txt",
        "# in JSBSim's Python bindings. Each case: the previous action (4), the",
        "# readings (16, tools/rl/landing.py's READINGS), the observation (21)",
        "# and the policy's action (4). Then each of the verification's flights:",
        "# touched (1 or 0), the sink ft/min, across m, along m, the most it rose,",
        "# banked and pitched down (ft, degrees) in the five seconds after.",
        "runway " + " ".join(number(v) for v in (rw.threshold_lat_deg, rw.threshold_lon_deg,
                                                  rw.elevation_ft, rw.heading_deg, rw.length_m)),
        f"cases {len(cases)}",
    ]
    for prev, readings, obs, act in cases:
        lines.append("case " + " ".join(number(v) for v in prev + readings + obs + act))
    # And each of the verification's flights, whole, as JSBSim's Python
    # bindings flew it: where it touched, and how it stayed down.
    starts = L.verification_starts()
    lines.append(f"flights {len(starts)}")
    for i, (_, start) in enumerate(starts):
        f = fly(policy, start)
        lines.append(
            f"flight {i} " + " ".join(
                number(v) for v in (1.0 if f.touched else 0.0, f.touch.sink_fpm, f.touch.across_m,
                                    f.touch.along_m, f.highest_after_touch_ft,
                                    f.worst_roll_after_touch_deg, f.least_pitch_after_touch_deg)
            )
        )
    return lines


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("checkpoint")
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--steps", required=True, help="the training steps the checkpoint took")
    ap.add_argument("--note", default="")
    ap.add_argument("--policy", default=POLICY, help="where to write the policy")
    ap.add_argument("--parity", default=PARITY, help="where to write the parity cases")
    args = ap.parse_args()
    versions = ", ".join(
        f"{p} {metadata.version(p)}"
        for p in ("jsbsim", "gymnasium", "stable_baselines3", "torch", "numpy")
    )
    header = [
        "The C172P's final approach and landing, learnt by reinforcement learning.",
        "Made by tools/rl/train.py (PPO) and written by tools/rl/export.py;",
        "do not edit - train again and export.",
        f"Seed {args.seed}; {args.steps} training steps (decisions, ten a second).",
        f"Packages: {versions}.",
        "Flown by src/sim/learnt.cpp. Observation and action: tools/rl/landing.py.",
    ]
    if args.note:
        header.append(args.note)
    p = from_checkpoint(args.checkpoint, header)
    os.makedirs(os.path.dirname(os.path.abspath(args.policy)), exist_ok=True)
    policy_file.write(args.policy, p)
    again = policy_file.read(args.policy)
    assert again.layers[0].weights == p.layers[0].weights
    os.makedirs(os.path.dirname(os.path.abspath(args.parity)), exist_ok=True)
    with open(args.parity, "w", newline="\n") as f:
        f.write("\n".join(parity(again)) + "\n")
    print("wrote", args.policy, "and", args.parity)


if __name__ == "__main__":
    main()
