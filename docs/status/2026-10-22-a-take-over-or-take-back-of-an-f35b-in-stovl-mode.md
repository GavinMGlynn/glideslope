### A take-over or take-back of an F-35B in STOVL mode, predicted from the server's law, 2026-10-22

**What is still not true first.**

- **The 0.858 degrees was not seen again.** The plan tail said a player's
  F-35B taken back in STOVL mode with her law let go had the two laws
  seeded 0.858 degrees of pitch apart. Built through a server (a route
  sent while the AI flies her, which converts her back and lets her law
  go, then taken back with the selector still on), the server makes her a
  new law on the take-back step and tells it with `LAW_STATE` (#240), and
  the client begins on it: 0.009 degrees. With the message taken away, the
  client's own law was 0.306 degrees off, not 0.858; the figure was seen
  before #240 told a law on a take-back, and nothing here reproduces it.
- **The windowed client is still compiled, not run**, on a take-over as on
  a join. It keys the law by aircraft and by count (`Online::fresh_law`),
  so the drop fixed below in `glideslope_cli` does not arise there; no
  test shows it.
- **The take-over and the take-back are events (2026-10-23, from the
  review)**: `--when-hovering` makes `--take-over-at 0` and
  `--take-back-at 0` wait until the aircraft concerned - the AI's, and the
  client's own handed to it - has flown under half a metre a second over the ground for three
  seconds of the session's clock. Only the hand-over (150 s, jet-borne and
  slowing) is still a time. The test asserts she was told hovering (stage 3).

**A plan names her loading** (owner's decision, 2026-10-22). `loading
NAME` in a plan file, one her figures file names (`sim::named_loading`),
as a player's `--loading` does. `sim::refuse_what_it_cannot_fly` refuses
one they do not name, naming it and those they do; the parser refuses a
loading with no name, of two words, or a second one. Flown at it: the
server's AI aircraft, a player given the plan's aircraft who asks for no
loading, `glideslope_cli fly-plan`, and the windowed client's own plan
where `--loading` names none. Documented in src/sim/plan.hpp, where the
plan format is; FEATURES.md says an AI pilot's F-35B hovers too.

**What happens to her law across a hand-over.** A player's F-35B handed to
the AI (`hand`) is held in STOVL mode: the AI is given no plan, and holds
the hover she is slowing to at the height asked. Her law is let go only if
the AI is given a plan or a route that is not at its vertical landing; it
then converts her back and flies it wing-borne.

**The fix.** Taking over an AI's F-35B whose law flies her, the server
sends `LAW_STATE` reliably ahead of the update that says she is now the
client's. `glideslope_cli` still predicted the aircraft it was leaving, and
dropped the law (`law_told` returned while any prediction ran); it then
waited for it for ever, drawing her from the updates. It now keeps the law
it is told, and a take-over keeps it if it is for the aircraft taken. Seen
red before the fix: never begun on the law, her pitch 43.3 degrees off.

**The tests.**

- `a_plans_loading_is_one_her_figures_name_or_the_plan_is_refused_by_it`
  (test_navigator.cpp): every named loading of the 16 aircraft (41) is
  flown from a plan; `no_such_loading` refused for each, by name; the F-35B
  refusal names her loadings; the three ways the line is misread. Seen red
  with the check removed.
- tests/cmake/server_stovl_take_over.cmake, three scenes, each through a
  100 ms relay, 750 updates compared from the law, the bounds of
  server_stovl_vertical_landing.cmake (pitch half a degree, median 60 mm,
  95th percentile 3.4 m, worst 5.6 m):
  - `a_client_taking_over_an_ai_f35b_hovering_predicts_her_from_the_servers_law_state`:
    the AI's F-35B, planned at `vertical_landing`, hovering over its pad,
    told stage 3: pitch within 0.005 degrees, median 4 mm, worst 6 mm.
  - `a_client_taking_back_its_f35b_hovering_from_the_ai_predicts_her_from_the_servers_law_state`:
    handed over at 150 s jet-borne, taken back once hovering, told
    stage 3: pitch 0.004 degrees, median 3 mm, worst 4 mm. **Seen red**
    with the server not telling and the client not waiting: 3.632 degrees.
  - `a_client_taking_back_its_f35b_from_the_ai_wing_borne_in_stovl_mode_predicts_the_law_the_server_makes_her`:
    a route converts her back; taken back at 300 s at 250 kt, the law made
    afresh and told, stage 0: pitch 0.009 degrees, median 6 mm, worst
    1.085 m. Untold: 0.306 degrees (and red on the told check).
- `glideslope_cli` gains `--until-compared-on-law N`.

**On the wire**: nothing changed; TRANSPORT.md is as #240 left it.

**Verification** (linux-release): the three multi-process tests and the
plan test green; the reds above seen and reverted.

**Rebased on #240 as it now stands** (2026-10-28), itself on main: no
conflicts. Locally (linux-debug, `ctest -j4`, no display): 1391 tests, all
pass but those skipped without a GPU, a live model call or Windows.
