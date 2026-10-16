### Checklists for every aircraft, 2026-09-21 — item done 2026-09-23

Phase 5c's first item. **All sixteen aircraft carry a checklist for each of
the nine phases of flight - 144 checklists and 757 items - and every item
either names a state the aeroplane really has or is marked the pilot's to
confirm.** The item is not ticked, for a reason given at the end.

They are the aeroplane's own, not one aeroplane's copied round: the Cub is
swung by hand and has its nose weaved on the ground because nothing is
visible straight ahead, the S.23 taxies on water and finishes at a buoy, the
Mosquito has its superchargers and radiator flaps, and the F-22's cruise item
wants Mach without reheat. A turbofan names no mixture and no pitch lever
anywhere, and each of those files says in its header that the engine has
neither.

**The format** is `assets/aircraft/<id>.checklist`, beside the `.aircraft`
file, line-oriented with `#` for comments as every other data file here is;
`src/sim/checklist.hpp` owns it. Four commands: `phase`, and three kinds of
item - `check PROPERTY OP VALUE TEXT` where OP is `<=` or `>=`, `range
PROPERTY LOW HIGH TEXT` where a band is what shows the item done, and
`confirm TEXT` for what the simulation cannot see. The machine-readable part
comes first so the pilot's words can be free text to the end of the line,
which is how `start AIRSPEED THROTTLE` already reads. All nine phases must
appear, once, in the order they are flown, and none may be empty: a file that
has quietly lost a phase is refused rather than teaching less than it claims.

**A property being there does not mean the aeroplane has the thing.** The
first test asked each model for every property its checklists name, through
`Aircraft::property`, which throws for one the model has not got. That is
weaker than it looks: JSBSim's `FGFCS::bind` ties `fcs/flap-pos-deg`,
`fcs/flap-cmd-norm` and `fcs/speedbrake-pos-norm` for every aircraft whether
or not its model has that channel (`ext/jsbsim/src/models/FGFCS.cpp:725, 753,
758`), so they answer on a Piper Cub, which has no flaps at all, and on a
B-2, whose drag rudders answer the pedals. An item resting on one would never
tick and the test would have said nothing.

What separates a real channel from a phantom is whether the aircraft's own
flight model names the property, to drive it or to read it back. So each
aircraft's model files are read and a control property its own model never
mentions is refused. Three are named in the test as exceptions with their
reason - JSBSim ties `fcs/throttle-cmd-norm`, `fcs/mixture-cmd-norm` and
`fcs/advance-cmd-norm` once per engine, so a model that uses them need never
spell them - and mixture and propeller pitch are refused on an aircraft whose
engine is neither piston nor turboprop, which is the one way those two could
still be nonsense on a jet.

**Verification run.** Six tests, all registered:
`every_aircraft_has_a_checklist_for_every_phase_of_flight` (which states the
roster is sixteen and the phases nine, and fails if either moves),
`no_checklist_item_rests_on_a_control_its_aircrafts_model_has_not_got`,
`every_checklist_item_names_a_state_its_own_aircraft_has_or_is_the_pilots`,
`an_item_is_done_when_the_aircrafts_state_is_inside_its_band`,
`a_checklist_file_that_is_wrong_is_refused_and_says_where` (ten separate
refusals) and `the_nine_phases_of_flight_each_have_one_name_and_answer_to_it`.
The phantom-control test was watched to fail: a flap item put into the Cub's
file gave "j3cub's take-off: j3cub's flight model never mentions
fcs/flap-pos-deg, so the item could not tick", while the older property test
passed the same file - which is the gap, seen. 289 of 289 tests pass locally
at `-j4`, in 1014 s.

**Why the item is not ticked: the words' provenance.** The item asks for
checklists "from its handbook or pilot's notes". No such manual was read to
write these. They follow the ordinary practice for each type, in the order it
is flown, with their speeds and settings taken from this project's own
`assets/figures/<id>.xml` and from the flight models themselves where the
model is what decides. Every file says so in its own header, and
`docs/ASSETS.md` has a section, "Written here, from no outside source", that
records it rather than naming a source that was not used - the file's rule is
that terms are quoted from the source, not paraphrased from memory, and a
handbook cited but unread would be exactly that. **For three of the sixteen
it could not be done as written in any case: no flight manual is public for
the B-2A, the F-22A or the F-35A.**
