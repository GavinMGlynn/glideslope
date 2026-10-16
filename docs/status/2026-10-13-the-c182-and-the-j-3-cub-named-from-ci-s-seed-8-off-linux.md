### The C182 and the J-3 Cub named from CI's seed 8 off Linux; two of fourteen judged, twelve named, 2026-10-13 — item still open

**What is missing first.** Twelve of fourteen are named in the gusts test;
only the Learjet 35A and the PA-28 are judged. CI run 38028434284 (#164
rebuilt onto the new stack, head a4a49380) failed only seed 8, and only off
Linux, by the same cross-platform effect as before: on macOS the C182
touched at 352.5 ft/min, not 25 inside the 360 limit, and on Windows
release and clang the J-3 Cub touched at 359.7. Both passed every seed on
Linux with the margin. Each is named from that run, as the 787-8,
A320 and C172P were from 38006839738.

**Verification:** the eight gust tests' named list (`not_yet_landed_in_gusts`)
gains the C182 and the J-3 Cub, with the run and the figure each; the
test's comment and the item's text say two of fourteen. No change to the
lander; the selftest hash does not move.
