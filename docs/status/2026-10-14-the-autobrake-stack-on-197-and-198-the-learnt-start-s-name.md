### The autobrake stack on #197 and #198: the learnt start's name off, 2026-10-14 — item still open

**What is still missing, first**: the autobrake item stays open on its named
cases, the Mosquito's swing and the Cub in 15 kt across. The Cub still goes
over there on this base (least pitch -87.8 degrees, rolled 180), as #198
found in its sweep. **The full suite was not run**: the 162 tests below.

**What changed.** #192 and #195 were rebased onto main after #197 (a
tail-wheel aeroplane starts at her stance pitch) and #198 (the alpha-rate
terms on the dynamic pressure without its sideways part); only the log
conflicted, and both sides were kept. CI run 38472468661 had failed five
tests on the stack: the Cub's calm, wet and gust walking-pace cases (her
level start, #197), the wet side grip's every-jet slide, and the learnt
half-fuel landings. Now the half-fuel test's named start (-50 m across, -15 m
high, 10 kt from the left) lands within the limits, and the test failed
asking for its name to come off. It is off: the test is main's again, every
one of the 27 starts judged, and the means of naming one, used by nothing
else, went with it.

**Verification.** The 162 tests matching wet, brake, walking, autobrake,
gust, crosswind, learnt, rollout, stop and committed - the five that failed
on CI among them - pass in the default linux-release build and in one built
with `-march=x86-64-v2` (no fused multiply-adds, as CI builds), 3 skipped for
want of a language model's key. They ran on the stack's top (#195) only:
#192's head alone was not tested locally, and its CI run is the first test of
it on this base. In the `-march=x86-64-v2` run one
client-server test could not listen on its port while another agent's tests
ran, and passed run alone. **The selftest hash does not move** from main's:
`de9bf01f96427593` (linux-release), `fefe2a7f5d0f52a5` (`-march=x86-64-v2`).
