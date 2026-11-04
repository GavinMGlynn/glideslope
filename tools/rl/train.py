"""Trains a light aircraft's landing policy (--aircraft, tools/rl/aircraft/): PPO, from stable-baselines3, on the
environment in env.py. Run it with the packages in requirements.txt, in a
virtual environment outside the repository:

    python3 -m venv ~/.venvs/glideslope-rl
    ~/.venvs/glideslope-rl/bin/pip install -r tools/rl/requirements.txt \
        --extra-index-url https://download.pytorch.org/whl/cpu
    nice -n 10 ~/.venvs/glideslope-rl/bin/python tools/rl/train.py --aircraft c172p --seed 7 --envs 8

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

from env import GAMMA, LandingEnv


class Landings(BaseCallback):
    """Prints how the landings are going, once a rollout, and saves a
    checkpoint every `every` steps."""

    def __init__(self, out: str, every: int, critic_first: int = 0):
        super().__init__()
        self.out = out
        self.every = every
        self.critic_first = critic_first
        self.actor_held_until = 0
        self.next_save = every
        self.recent: list[dict] = []
        self.started = time.time()

    def _on_training_start(self) -> None:
        # Resumed, the next checkpoint is the next whole `every` from here.
        self.next_save = (self.num_timesteps // self.every + 1) * self.every
        if self.critic_first > 0:
            # **The critic first**: the actor held still while the value
            # function learns the starts' new mix, so that the first steps of
            # the policy are not taken on a critic's guesses about flights it
            # was never trained on (2026-11-03: without it every run from the
            # committed policy flew its own training's starts worse).
            self.actor_held_until = self.num_timesteps + self.critic_first
            self.hold_actor(True)

    def hold_actor(self, held: bool) -> None:
        policy = self.model.policy
        for p in list(policy.mlp_extractor.policy_net.parameters()) + \
                list(policy.action_net.parameters()):
            p.requires_grad_(not held)
        print(f"{self.num_timesteps:>10} steps: the actor {'held' if held else 'free'}", flush=True)

    def _on_rollout_start(self) -> None:
        if self.actor_held_until and self.num_timesteps >= self.actor_held_until:
            self.actor_held_until = 0
            self.hold_actor(False)

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
        unstable = sum(1 for x in r if x.get("most_unstable_s", 0.0) >= 2.0)
        sinks = [x["sink_fpm"] for x in touched]
        ends: dict[str, int] = {}
        for x in r:
            ends[x["ended"]] = ends.get(x["ended"], 0) + 1
        print(
            f"{self.num_timesteps:>10} steps {time.time() - self.started:7.0f} s: "
            f"{len(touched)}/{len(r)} touched, {len(good)} within limits, {unstable} unstabilized; "
            f"sink median {np.median(sinks) if sinks else float('nan'):.0f} fpm, across median "
            f"{np.median([abs(x['across_m']) for x in touched]) if touched else float('nan'):.1f} m; ends {ends}",
            flush=True,
        )
        self.recent = self.recent[-400:]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--aircraft", required=True,
                    help="the aircraft trained on: tools/rl/aircraft/AIRCRAFT.txt")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--steps", type=int, default=20_000_000)
    ap.add_argument("--envs", type=int, default=8)
    ap.add_argument("--out", default=os.path.expanduser("~/.cache/glideslope-rl/run"))
    ap.add_argument("--resume", default="")
    ap.add_argument("--lr", type=float, default=None,
                    help="resuming: this learning rate, and a clip range of 0.1")
    ap.add_argument("--log-std", type=float, default=None,
                    help="resuming: hold the action noise at this log standard deviation")
    ap.add_argument("--every", type=int, default=1_000_000,
                    help="a checkpoint every this many decisions")
    ap.add_argument("--speed-costs", action="store_true",
                    help="the stabilized approach's speed costs (env.py's STABILIZED); "
                         "without it, the reward the committed policy was trained with")
    ap.add_argument("--behind-left", type=float, default=0.0,
                    help="this fraction of the flights from inside the learnt landing's gate, "
                         "the wind behind and across from the left (env.py's random_start); "
                         "none without it")
    ap.add_argument("--handover", type=float, default=0.0,
                    help="this fraction of the flights from near where the approach autopilot hands "
                         "her to the policy, 35 ft over the runway (env.py's random_start); none without it")
    ap.add_argument("--ahead", type=float, default=0.0,
                    help="this fraction of the flights in 10 to 15 kt from within 50 degrees of ahead "
                         "(env.py's random_start); none without it")
    ap.add_argument("--handover-across", type=float, default=0.0,
                    help="of the flights from the hand-over, this fraction in strong wind "
                         "(env.py's strong_wind); none without it")
    ap.add_argument("--nose-up", type=float, default=0.0,
                    help="at the touch, this much reward lost for each degree her nose is below "
                         "level, to 15 (env.py's Flier); none without it")
    ap.add_argument("--hold-norm", action="store_true",
                    help="resuming: hold the checkpoint's observation and reward normalisation")
    ap.add_argument("--critic-first", type=int, default=0,
                    help="resuming: train only the value function for this many decisions first")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    torch.set_num_threads(1)

    def make(i: int):
        return lambda: LandingEnv(args.aircraft, seed=args.seed * 1000 + i, speed_costs=args.speed_costs,
                                  behind_left=args.behind_left, handover=args.handover,
                                  ahead=args.ahead, nose_up=args.nose_up,
                                  handover_across=args.handover_across)

    venv = SubprocVecEnv([make(i) for i in range(args.envs)])
    if args.resume:
        venv = VecNormalize.load(args.resume.removesuffix(".zip") + ".vecnorm", venv)
        venv.gamma = GAMMA
        if args.hold_norm:
            # **The normalisation held as the checkpoint has it**, so that a
            # change of the starts' mix does not move what every input means
            # to the policy before a step of training has.
            venv.training = False
        # A fine-tune steps more gently than a start from nothing.
        custom = {"gamma": GAMMA}
        if args.lr is not None:
            custom |= {"learning_rate": args.lr, "clip_range": 0.1}
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
        venv = VecNormalize(venv, norm_obs=True, norm_reward=True, clip_obs=10.0, gamma=GAMMA)
        model = PPO(
            "MlpPolicy",
            venv,
            n_steps=1024,
            batch_size=4096,
            n_epochs=10,
            learning_rate=3e-4,
            gamma=GAMMA,
            gae_lambda=0.95,
            clip_range=0.2,
            ent_coef=0.0,
            policy_kwargs=dict(net_arch=dict(pi=[64, 64], vf=[128, 128]), log_std_init=-0.5),
            seed=args.seed,
            device="cpu",
            verbose=0,
        )
    cb = Landings(args.out, every=args.every, critic_first=args.critic_first)
    model.learn(total_timesteps=args.steps, callback=cb, reset_num_timesteps=not args.resume)
    cb.save("final")


if __name__ == "__main__":
    main()
