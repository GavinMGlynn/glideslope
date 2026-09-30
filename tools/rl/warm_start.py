"""Makes a checkpoint for the 25-observation landing from one trained on the
first 21 observations, the new inputs weighted nothing.

    ~/.venvs/glideslope-rl/bin/python tools/rl/warm_start.py OLD.zip NEW.zip

The observation grew by four - the drift angle, the wind across and along
the runway as the instruments estimate it, and the remembered drift - and
they were appended, so the first 21 mean what they meant. The new network
is the old one with a zero column for each new input in the first layer of
the policy and of the value function: it acts exactly as the old one did
until training finds a use for them. The observation normalisation is the
old one's, and for the new inputs a mean of 0 and a variance of 1, which
training then measures. train.py --resume NEW.zip trains on from it.
"""

from __future__ import annotations

import os
import pickle
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numpy as np
import torch
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv, VecNormalize

import landing as L
from env import LandingEnv


def main() -> None:
    old_path, new_path = sys.argv[1], sys.argv[2]
    old = PPO.load(old_path, device="cpu")
    with open(old_path.removesuffix(".zip") + ".vecnorm", "rb") as f:
        old_norm: VecNormalize = pickle.load(f)
    n_old = old.observation_space.shape[0]
    assert n_old == 21 and L.OBSERVATIONS == 25, (n_old, L.OBSERVATIONS)

    venv = VecNormalize(DummyVecEnv([lambda: LandingEnv(0)]), norm_obs=True,
                        norm_reward=True, clip_obs=10.0, gamma=0.995)
    new = PPO("MlpPolicy", venv, n_steps=1024, batch_size=4096, n_epochs=10,
              learning_rate=3e-4, gamma=0.995, gae_lambda=0.95, clip_range=0.2,
              ent_coef=0.0,
              policy_kwargs=dict(net_arch=dict(pi=[64, 64], vf=[128, 128])),
              seed=old.seed, device="cpu", verbose=0)
    old_state = old.policy.state_dict()
    new_state = new.policy.state_dict()
    for name, value in old_state.items():
        target = new_state[name]
        if value.shape == target.shape:
            target.copy_(value)
        else:
            # A first layer: the old inputs' columns, and nothing for the new.
            assert value.shape[1] == n_old and target.shape[1] == L.OBSERVATIONS, name
            target.zero_()
            target[:, :n_old] = value
    new.policy.load_state_dict(new_state)

    rms = venv.obs_rms
    rms.mean = np.concatenate([old_norm.obs_rms.mean, np.zeros(L.OBSERVATIONS - n_old)])
    rms.var = np.concatenate([old_norm.obs_rms.var, np.ones(L.OBSERVATIONS - n_old)])
    rms.count = old_norm.obs_rms.count
    venv.ret_rms = old_norm.ret_rms
    new.num_timesteps = old.num_timesteps
    new.save(new_path)
    venv.save(new_path.removesuffix(".zip") + ".vecnorm")

    # It acts as the old one did, on the old inputs.
    rng = np.random.default_rng(0)
    for _ in range(20):
        obs = rng.normal(size=L.OBSERVATIONS)
        a_old, _ = old.predict(obs[:n_old], deterministic=True)
        a_new, _ = new.predict(obs, deterministic=True)
        assert np.allclose(a_old, a_new, atol=1e-5), (a_old, a_new)
    print("wrote", new_path, "from", old_path, "at", old.num_timesteps, "steps")


if __name__ == "__main__":
    main()
