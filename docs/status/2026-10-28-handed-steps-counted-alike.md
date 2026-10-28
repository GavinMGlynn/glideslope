### The handed aircraft's steps counted alike on a server behind real time, 2026-10-28

**What was wrong.** A server stopped on a count of steps with the AI
(`--steps-after-hand-over`) let go of the steps it owed (`CatchingUp::forgive`)
but not of those it had let slip, and the end of the run takes both, so that
a server flies the time it was given. So a server that had let any slip flew
them after its stop, and every pair still flying - the handed aircraft's
included - was measured over that many more steps than the count. Windows
debug on CI, 2026-10-28 (run 40686462655, PR #262): stopped at 14,400, the
handed aircraft measured over 14,407. Slipping arrived with #256
(`net::CatchingUp`); before it nothing slipped and `owed = 0` was all of it,
so this was not possible before #256.

**The fix.** `forgive()` lets go of what slipped as well as what is owed: a
run stopped where it is is past its count at either. Only
`--steps-after-hand-over` forgives; a run that ends any other way still
takes what it owes and what it let slip.

**Verification.**

- `a_run_stopped_where_it_is_owes_no_step_it_let_slip` (unit): held up 4 s,
  60 steps slipped; forgiven, nothing owed and nothing slipped. Seen to fail
  with `slipped_` kept: "0 owed and 60 slipped are left for the end to take".
- The spiral hand-over test's own script, its server held up 1 to 4 s every
  20 simulated seconds (`--test-held-up 20 4000`), Linux debug: before the
  fix red, "c172p (AI 1) and c172p (slot 0), over 14460 steps" - the 4 s
  hold-up's 60 slipped steps; after it green, both pairs with the handed
  aircraft over 14400, and the two AI aircraft's pair 60 steps shorter
  (14,649 against 14,709).
- **Built into the committed test, not left to a runner's luck** (the
  review of #263): `an_aircraft_its_player_rolled_into_a_spiral_..._from_either`
  now runs its server with `--test-held-up 20 4000` and fails unless the
  stop let some steps slip. With the old `forgive()`: red, "stopped 9 steps
  owed and 60 let slip", the handed pair over 14,460. With the fix: green,
  "8 owed and 60 let slip", both handed pairs over 14,400, separation never
  lost.
- The stop's line now says what was owed and let slip before they are
  forgiven, so a log still shows how far behind the server was.
- Unchanged and green:
  `an_aircraft_handed_to_the_ai_is_measured_against_every_other_ai_aircraft_and_kept_apart_from_them`,
  the server's flag tests and the held-input unit tests.

No plan item: this is a test operator's count, not anything a player sees;
`docs/PROJECT_STATUS.md` is unchanged, since what works is.
