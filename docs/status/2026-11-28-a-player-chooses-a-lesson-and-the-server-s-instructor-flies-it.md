### A player chooses a lesson in the window client, and the server's instructor flies it, 2026-11-28 — second part of an open item

**Still open: a player cannot fly a lesson herself.** After the hand-over
her own flying is not staged or judged, and the debrief shown is the
demonstration's. This is the second part of the Phase 12c item (now "No
player can fly a lesson herself"), whose first part - the instructor in the
simulation, `glideslope_cli lesson` - landed in #426. Its verification as
written (a player chooses a lesson in the window client, the instructor flies
it first and hands over, and the debrief is shown) is met; the item stays
`[ ]` because FEATURES.md's lessons are flown by the player, stage by stage,
and debriefed on her flying.

**What now works**

- **The instructor wraps the server's own aircraft and controller.**
  `sim::Instructor` has a second constructor, taking an `Aircraft&` and the
  owner's `std::unique_ptr<Controller>&`: it puts her where the lesson begins
  and loads her for it, and makes the AI's controller into the owner's slot,
  which keeps it. Its step is split into `fly` (the hand-over, what the
  instructor asks for, the controls flown) and `stepped` (the lesson judged),
  so that the server steps the aircraft itself; `step` is the two around the
  aircraft's step, as before, for the command line and the tests. The
  instructor is the AI controller; handing over is `to_pilot` on that same
  controller - a controller swap and nothing more.
- **The server teaches a player's own aircraft a lesson asked for**
  (`Fleet::teach`): looked up among the lessons it read at start, refused
  for another client's aircraft, a lesson it has not got, another class's
  (`c172p is light-aircraft, and fighter-turns teaches fighter`), one she
  cannot be taught, or no runway within 50 km. **Where**: the longest runway
  within 50 km of her, of the collision ground's runways, from its end
  nearer her, its elevation the ground's there (YSSY 16R from the plan's
  start); a flying boat's on water, the first 3,000 m of it under her from
  north round to the right, looked at every 100 m. While the instructor has her, a state update says the AI flies
  her; she gives way to nobody in the separation (as an operator's course
  does) and the AI traffic gives way to her. At the hand-over the server
  announces `CONTROLLER_SWAP` to `PERSON` and lets the instructor go; her
  controller flies her person's inputs from the next step. A lesson ends
  before its hand-over, said, when her person takes her back, hands her to
  the AI, asks for the learnt landing or sends a copilot's route, takes over
  another aircraft, asks for another lesson or leaves, or when she is
  wrecked.
- **Protocol 22 (`16`)**: `ASK_LESSON` (`0F`, a client's: its aircraft and
  the lesson's id) and `LESSON` (`10`, the server's, to her client alone:
  the part - `REFUSED`, `SETTLING`, `DEMONSTRATING`, `HANDED_OVER`, `ENDED` -
  the stage or the count flown, the count of stages, and the lines: why, or
  the debrief). `docs/TRANSPORT.md` gives both byte for byte with every
  refusal; `docs/THREATS.md` names `ASK_LESSON` as the fourth message with a
  client-to-server surface.
- **The window client's lessons screen** (`frontend::LessonsScreen`, drawn by
  `gfx::lesson_lines` in the checklist's place, wrapped at its width): F2
  shows it - the lessons of her aeroplane's class, one chosen - F3 chooses
  the next, Return asks for it; `--lesson ID` chooses one by flag and asks
  for it. It then shows what the server says: refused and why; the
  instructor settling her; each stage as the instructor flies it, with the
  stage's words from the client's own copy of the lesson; handed over -
  "you have control" - with the debrief, or "flown to the book"; or ended
  and why. The three keys are in the controls' help (eleven keys now).
- **`--test-pace` up to 16**: a server ahead of real time, for the
  every-lesson test, where nothing is predicted until the hand-over.

**Verification**

- `the_lessons_screen_shows_every_state_for_every_lesson_of_every_class`
  (unit): seven classes, 42 of 42 lessons, every stage of every lesson, and
  7 of 7 states reached, each with its words.
- `every_refusal_the_document_names_for_a_lesson_is_refused`: 188 refusals
  built past the writer and 11 edges read, as TRANSPORT.md lists them; and
  the writer cuts a debrief to six lines of 160 bytes. The message tests
  walk sixteen kinds now (256 pairs, every truncation, every trailing byte,
  every single-byte change, filled to the limits).
- `every_state_of_the_lessons_screen_is_drawn_and_reads_back_off_its_frame`:
  a server and the window client in a Cessna, the turns lesson chosen by
  flag; every state shot the first time it is drawn - closed, choosing,
  settling, demonstrating, handed over, then (`--test-lesson-states`) a
  lesson the server refuses, and the turns again, taken back from the
  instructor as it demonstrates: ended. Each frame read back glyph by glyph
  (`glideslope_lesson_check`) against what the client said it drew, 7 of 7,
  and each holds the words its state says.
- `every_lesson_from_N_by_sevens_is_chosen_in_the_window_client_demonstrated_handed_over_and_debriefed`,
  seven pieces of six, one lesson of each exercise in each: every lesson `glideslope_cli lessons` lists, in the aeroplane
  that teaches it, chosen by flag in the window client; the client must be
  told the instructor flying every stage in turn and the hand-over with
  every stage flown, and must draw the screen handed over with the debrief,
  read back off its frame; the server must say it handed her over. Each
  piece asserts how many it flew of how many it holds, and the six hold the
  42. The server runs at 8 times real time: the circuit, 892 s of flying, in
  112 s. On linux-debug, all 42 were flown and debriefed (the six pieces in
  10 min side by side, and the flying boat's and the wartime twin's 12 again
  after the water runway below); the flying boat's take-off, circuit and
  approach first failed, wrecked alighting on YSSY's tarmac - which is why a
  flying boat's lesson is now taught on water. **Every circuit but the light
  aeroplane's debriefs "Hold the circuit height downwind"** on the server,
  where the unit tests' flat sea-level ground flies them to the book: the
  debrief is shown as it is, and the test does not require it empty.
- **Seen to fail**, each reverted: the reader taking seven debrief lines -
  `seven debrief lines is refused` red; the screen showing `ENDED` as
  refused - the screen unit test red; the HUD drawing the checklist in the
  lesson's place - the shot test red at `the lessons screen, choosing, on the
  frame is not the one drawn`; the server telling a stage only when the part
  changes - `airliner-approach-and-landing: the client was never told the
  instructor flies stage 2 of 3`. (A reader taking a part of 5 was not
  caught: the part's switch refuses it anyway, so that break was no break.)

**From the review of #434**

- The step that finishes the last stage told her client "stage N of N" -
  a stage at the count, which the reader refuses, so the client dropped it.
  Nothing is said on that step now; the server says each `LESSON` it sends,
  and the every-lesson test holds every stage told below the count and
  their number to the lesson's. Seen red first: `airliner-approach-and-landing:
  the server told 4 stages flown of the 3 there are`.
- Handed to the AI while the instructor flew, she kept the instructor's
  last asking (a stall's entry flown on): the lesson ended and `hand`
  returned, the AI already flying. Now the AI is given her (`to_ai`, holding
  what she is doing). The screen test hands her to the AI in a third
  demonstration and holds the server to `aircraft N handed to the AI`; seen
  red with the fix taken out.
- The instructor reads every figure she is flown by before she is loaded,
  moved or given a controller, so the wrapping constructor's "throws before
  either is touched" is so (but for JSBSim refusing the start).
- A learnt landing or a copilot's route refused no longer ends a lesson;
  only one accepted does.
- A refusal for another client's aircraft carries no count of stages, and
  TRANSPORT.md now says a count of 0 is a lesson not looked up.
- THREATS.md says what repeated `ASK_LESSON`s cost the step thread, and the
  only bound on them: the request rate.
- The two tails are items in the plan's Phase 12c.

**From the full run 46082094869**

- The tests' ports were outside the tests' block (25010 to 25022): the
  lessons-screen test is on 24855 now, and the six pieces on 24857, 24861,
  24875, 24889, 24907 and 24915, free ports of the block.
- Their two servers' `--timeout 30` are counted in the timeouts check's
  totals (79 and 67).
- Three pieces on Ubuntu debug failed at their flying boat's lesson -
  approach, circuit or stalls - with the window client's "cannot reach":
  its handshake gave up after 5 s with the server listening and its ready
  file written, while the other five lessons of each piece had joined; on
  this machine every join takes under 0.1 s, at either pace. The ports were
  each piece's own, so it was no clash. The cause is not pinned down: the
  runner was loaded with six pieces' clients and servers at 8 times real
  time. The test's clients now try the handshake for 120 s
  (`--connect-wait`, a new test flag, 5 s by default), as nothing else
  gives up on a slow machine sooner than it needs, and a failure to join
  prints the server's output, so a repeat says what the server did.

**From the full run 46106569461**

- Six pieces of every sixth lesson put all seven circuits in one piece,
  which ctest cut off at 900 s on Ubuntu debug with its sixth circuit
  flying. The list is each class's six exercises in turn, so seven pieces
  of every seventh hold one lesson of each exercise apiece - one circuit
  each - and each asserts its six, 42 between them (a piece outside the
  split is refused). Each has a time limit of 1,800 s. On this machine,
  inside the full linux-debug suite at -j4, they took 172, 200, 216, 235,
  219, 183 and 182 s: the slowest has 7.7 times its time to spare, and CI's
  debug runners were about 2.3 times slower than this machine on the six
  pieces before (2,128 s against about 920).
- `tests/ci_costs/` remeasured with `tools/ci_test_costs.py --finished` over
  the three runs before and 46106569461, and the 596 tests those runs did
  not run (the gust sweep's, the nightly's) merged back at their earlier
  figures; macos-debug, which the runs had none of, unchanged. The seven
  pieces' figures are estimates - the six old pieces' total over seven, a
  fifth more - until a run measures them.

**Decisions**

- **She is moved because her person asked.** The owner's rule that the
  server never moves a person's aircraft (2026-11-11) is about other
  traffic; a lesson begins where it begins, and is asked for.
- **The debrief shown at the hand-over is the demonstration's**, as
  `glideslope_cli lesson`'s is: the instructor's flying, judged. Judging the
  person's own attempt after the hand-over is not built.

**Tails**

- The lesson's loading (her figures' measured weight) is not told to her
  client: `AIRCRAFT` still names the loading she joined with, so after the
  hand-over her prediction may fly another weight, and is put right.
- The person's own attempt after the hand-over is not judged or debriefed.
