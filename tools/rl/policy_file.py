"""The trained policy as a text file: what the simulation reads.

A small multilayer perceptron - the mean of the trained policy's action
distribution - with the observation normalisation it was trained with and the
approach it flies. src/sim/learnt.cpp reads the same format; this module
evaluates it in double precision, as the simulation does, so what the scripts
measure is what the file flies.

    # comment lines, as many as the header needs
    glideslope-policy 1
    aircraft c172p
    task landing
    decision_steps 12
    vref_kts 59.8
    flaps 1
    glidepath_deg 3
    aim_m 300
    observations 21
    actions 4
    obs_mean <21 numbers>
    obs_scale <21 numbers>
    obs_clip 10
    layers 3
    layer 21 64 tanh
    <64 lines of 21 weights, one line per output>
    bias <64 numbers>
    layer 64 64 tanh
    ...
    layer 64 4 linear
    ...

The action is the last layer's output, each clipped to -1..1. Numbers are
written with seventeen significant digits, so a double read back is the double
written.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

from landing import ACTIONS, OBSERVATIONS, Approach

FORMAT = "glideslope-policy 1"


@dataclass
class Layer:
    weights: list[list[float]]  # [out][in]
    bias: list[float]
    activation: str  # "tanh" or "linear"


@dataclass
class Policy:
    aircraft: str = "c172p"
    approach: Approach = field(default_factory=Approach)
    obs_mean: list[float] = field(default_factory=lambda: [0.0] * OBSERVATIONS)
    obs_scale: list[float] = field(default_factory=lambda: [1.0] * OBSERVATIONS)
    obs_clip: float = 10.0
    layers: list[Layer] = field(default_factory=list)
    header: list[str] = field(default_factory=list)

    def act(self, obs: list[float]) -> list[float]:
        x = [
            min(self.obs_clip, max(-self.obs_clip, (o - m) * s))
            for o, m, s in zip(obs, self.obs_mean, self.obs_scale)
        ]
        for layer in self.layers:
            y = []
            for row, b in zip(layer.weights, layer.bias):
                total = b
                for w, v in zip(row, x):
                    total += w * v
                y.append(math.tanh(total) if layer.activation == "tanh" else total)
            x = y
        return [min(1.0, max(-1.0, v)) for v in x]


def number(x: float) -> str:
    return repr(float(x)) if math.isfinite(x) else "nan"


def write(path: str, p: Policy) -> None:
    ap = p.approach
    lines = ["# " + h if h else "#" for h in p.header]
    lines += [
        FORMAT,
        f"aircraft {p.aircraft}",
        "task landing",
        f"decision_steps {ap.decision_steps}",
        f"vref_kts {number(ap.vref_kts)}",
        f"flaps {number(ap.flaps)}",
        f"glidepath_deg {number(ap.glidepath_deg)}",
        f"aim_m {number(ap.aim_m)}",
        f"observations {OBSERVATIONS}",
        f"actions {ACTIONS}",
        "obs_mean " + " ".join(number(v) for v in p.obs_mean),
        "obs_scale " + " ".join(number(v) for v in p.obs_scale),
        f"obs_clip {number(p.obs_clip)}",
        f"layers {len(p.layers)}",
    ]
    for layer in p.layers:
        lines.append(f"layer {len(layer.weights[0])} {len(layer.weights)} {layer.activation}")
        for row in layer.weights:
            lines.append(" ".join(number(v) for v in row))
        lines.append("bias " + " ".join(number(v) for v in layer.bias))
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def read(path: str) -> Policy:
    with open(path) as f:
        raw = [l.rstrip("\n") for l in f]
    header = [l[2:] if l.startswith("# ") else l[1:] for l in raw if l.startswith("#")]
    lines = [l for l in raw if l and not l.startswith("#")]
    if lines[0] != FORMAT:
        raise ValueError(f"{path} is not a {FORMAT} file")
    it = iter(lines[1:])
    kv = {}
    p = Policy(header=header)
    for line in it:
        key, _, rest = line.partition(" ")
        kv[key] = rest
        if key == "layers":
            break
    p.aircraft = kv["aircraft"]
    p.approach = Approach(
        vref_kts=float(kv["vref_kts"]),
        flaps=float(kv["flaps"]),
        glidepath_deg=float(kv["glidepath_deg"]),
        aim_m=float(kv["aim_m"]),
        decision_steps=int(kv["decision_steps"]),
    )
    p.obs_mean = [float(v) for v in kv["obs_mean"].split()]
    p.obs_scale = [float(v) for v in kv["obs_scale"].split()]
    p.obs_clip = float(kv["obs_clip"])
    for _ in range(int(kv["layers"])):
        _, n_in, n_out, act = next(it).split()
        weights = [[float(v) for v in next(it).split()] for _ in range(int(n_out))]
        bias = [float(v) for v in next(it).split()[1:]]
        assert all(len(r) == int(n_in) for r in weights) and len(bias) == int(n_out)
        p.layers.append(Layer(weights, bias, act))
    return p
