"""Trains the C172P landing policy: PPO, from stable-baselines3, on the
environment in env.py. Run it with the packages in requirements.txt, in a
virtual environment outside the repository:

    python3 -m venv ~/.venvs/glideslope-rl
    ~/.venvs/glideslope-rl/bin/pip install -r tools/rl/requirements.txt \
        --extra-index-url https://download.pytorch.org/whl/cpu
    nice -n 10 ~/.venvs/glideslope-rl/bin/python tools/rl/train.py --seed 7 --envs 8

Eight environments, niced: the machine is shared with builds and CI runs,
and a timing test elsewhere has failed under the load of more.

Checkpoints go to --out (outside the repository, by default under
~/.cache/glideslope-rl); tools/rl/export.py turns one into the policy file the
simulation reads, and tools/rl/evaluate.py flies a policy file: from the
held-out starts (--held-out) that checkpoints are chosen on, or from the
verification's starts.
"""

from __future__ import annotations

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numpy as np
import torch
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.vec_env import SubprocVecEnv, VecNormalize

from env import LandingEnv


class Landings(BaseCallback):
    """Prints how the landings are going, once a rollout, and saves a
    checkpoint every `every` steps."""

    def __init__(self, out: str, every: int):
        super().__init__()
        self.out = out
        self.every = every
        self.next_save = every
        self.recent: list[dict] = []
        self.started = time.time()

    def _on_training_start(self) -> None:
        # Resumed, the next checkpoint is the next whole `every` from here.
        self.next_save = (self.num_timesteps // self.every + 1) * self.every

    def _on_step(self) -> bool:
        for info in self.locals["infos"]:
            if "landing" in info:
                self.recent.append(info["landing"])
        if self.num_timesteps >= self.next_save:
            self.save(f"step{self.num_timesteps}")
            self.next_save += self.every
        return True

    def save(self, name: str) -> None:
        self.model.save(os.path.join(self.out, name))
        self.model.get_vec_normalize_env().save(os.path.join(self.out, name + ".vecnorm"))

    def _on_rollout_end(self) -> None:
        r = self.recent[-400:]
        if not r:
            return
        touched = [x for x in r if x["touched"]]
        good = [
            x for x in touched
            if x["sink_fpm"] < 300 and abs(x["across_m"]) <= 5 and 0 <= x["along_m"] <= 1200
        ]
        sinks = [x["sink_fpm"] for x in touched]
        ends: dict[str, int] = {}
        for x in r:
            ends[x["ended"]] = ends.get(x["ended"], 0) + 1
        print(
            f"{self.num_timesteps:>10} steps {time.time() - self.started:7.0f} s: "
            f"{len(touched)}/{len(r)} touched, {len(good)} within limits; "
            f"sink median {np.median(sinks) if sinks else float('nan'):.0f} fpm, across median "
            f"{np.median([abs(x['across_m']) for x in touched]) if touched else float('nan'):.1f} m; ends {ends}",
            flush=True,
        )
        self.recent = self.recent[-400:]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--steps", type=int, default=20_000_000)
    ap.add_argument("--envs", type=int, default=8)
    ap.add_argument("--out", default=os.path.expanduser("~/.cache/glideslope-rl/run"))
    ap.add_argument("--resume", default="")
    ap.add_argument("--lr", type=float, default=None,
                    help="resuming: this learning rate, and a clip range of 0.1")
    ap.add_argument("--log-std", type=float, default=None,
                    help="resuming: hold the action noise at this log standard deviation")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    torch.set_num_threads(1)

    def make(i: int):
        return lambda: LandingEnv(seed=args.seed * 1000 + i)

    venv = SubprocVecEnv([make(i) for i in range(args.envs)])
    if args.resume:
        venv = VecNormalize.load(args.resume.removesuffix(".zip") + ".vecnorm", venv)
        # A fine-tune steps more gently than a start from nothing.
        custom = {} if args.lr is None else {"learning_rate": args.lr, "clip_range": 0.1}
        model = PPO.load(args.resume, env=venv, device="cpu", custom_objects=custom)
        if args.log_std is not None:
            # **The noise held small and still**, so that the mean action -
            # what the simulation flies - is what training flew. With PPO's
            # own noise (a standard deviation of 0.15 to 0.27) the sampled
            # flights landed on the centreline and the mean ones 15 to 20 m
            # right of it.
            model.policy.log_std.data.fill_(args.log_std)
            model.policy.log_std.requires_grad_(False)
    else:
        venv = VecNormalize(venv, norm_obs=True, norm_reward=True, clip_obs=10.0, gamma=0.995)
        model = PPO(
            "MlpPolicy",
            venv,
            n_steps=1024,
            batch_size=4096,
            n_epochs=10,
            learning_rate=3e-4,
            gamma=0.995,
            gae_lambda=0.95,
            clip_range=0.2,
            ent_coef=0.0,
            policy_kwargs=dict(net_arch=dict(pi=[64, 64], vf=[128, 128]), log_std_init=-0.5),
            seed=args.seed,
            device="cpu",
            verbose=0,
        )
    cb = Landings(args.out, every=1_000_000)
    model.learn(total_timesteps=args.steps, callback=cb, reset_num_timesteps=not args.resume)
    cb.save("final")


if __name__ == "__main__":
    main()
