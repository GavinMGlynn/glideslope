### A tail-wheel aeroplane starts standing on her three wheels, 2026-10-14 — item done

**What is still not true first.** This is the first of two causes found for
the open gear item of #195, "a rolling aeroplane near a stop can be thrown on
her nose by her gear in one step". The second, in a 15 kt crosswind near a
stop, is the alpha-rate change of #198, landed before this one. **The full
suite was not run**: only the 161 ground tests below.

**The cause.** `Aircraft::initialize` started every aeroplane at pitch 0. A
tail-wheel aeroplane started level balances on her main wheels with her tail
in the air and her centre of gravity 9 degrees behind them (the Cub; 21 at
her standing attitude). Idle thrust from JSBSim's running start (1,790 rpm)
and full brakes tipped her forward. Built with fused multiply-adds (Rocky
10's GCC defaults to `-march=x86-64-v3`) she rocked back on to her tail;
built without them - CI's Rocky 9 and Ubuntu jobs, and here with
`-march=x86-64-v2`, which reproduced CI run 38472468661's -89.751562 degrees
to the digit - she went over on her nose in the stand before the walking-pace
tests of #195 began. The Mosquito tipped to -5.8 degrees and struck her
airframe 0.85 s after the same start.

**The fix.** A start at rest (`airspeed_kts` 0) with no pitch asked sets a
tail-wheel aeroplane, not a flying boat, at `Aircraft::stance`'s standing
pitch: the Cub 12.10 degrees, the Mosquito 12.31. It is worked out after
JSBSim's first `RunIC` and set with a second, because before it
`inertia/cg-x-in` reads 0 and the Mosquito was taken for a nose-wheel
aeroplane. Nose-wheel aeroplanes start as before.

**Verification.**
`every_tail_wheel_aeroplane_started_at_rest_stands_on_all_three_wheels_at_her_stance_pitch`:
each landplane whose stance is on a tail wheel (two: the Cub and the
Mosquito; thirteen nose-wheel aeroplanes and the flying boat named) starts
within a degree of her standing pitch and stands ten seconds at idle on full
brakes, stick central, within two degrees of it on all three wheels. The Cub
stood between 11.58 and 12.13 degrees, the Mosquito between 11.19 and 12.31.
Seen red with the fix disabled: the Cub started at 0, went from -89.5 to 88.6
degrees with no wheel touching, and the Mosquito tipped to -5.84. Applied to
#195's branch on the no-FMA build, its three failing Cub tests (calm, wet and
the gust) pass. The 161 ground tests matching cub, mosquito, take-off, taxi,
stand, walking, roll-out, lesson, brakes, stopping, departure and rotation
pass in both the `-march=x86-64-v2` build and the default build, but for 8
that need the client and server programs, which neither build had made. **The
selftest hash does not move**: on main after #198, `de9bf01f96427593` in
the default build and `fefe2a7f5d0f52a5` in the `-march=x86-64-v2` build,
before and after (it flies a C172P in the air). Replayed onto #198, the 191
tests matching the Cub, Mosquito, tail wheel, crosswind, standing, stopping,
braking, taxi, roll-out, take-off, departure, rotation and committed-model
checks pass in both builds, 9 skipped for want of a language model's key.
