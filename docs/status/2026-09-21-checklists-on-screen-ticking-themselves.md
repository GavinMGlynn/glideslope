### Checklists on screen, ticking themselves, 2026-09-21 — item done

Phase 5c's second item. **`--checklist PHASE` puts that phase's list down the
right of the screen, and it ticks itself as the aeroplane flies.**

**An item ticks at the first tick its state shows it done, and stays ticked.**
A checklist records that a thing was done, not that it is still true: flaps
set for take-off and raised on the climb does not untick the take-off list.
`sim::ChecklistRun` holds one `ItemProgress` per item of every phase - the
tick it was first seen done at - and the run is kept for every phase at once,
so turning to a list shows what the aeroplane has already done rather than an
empty page. An item the simulation cannot see is the pilot's, and ticks only
when the pilot says so; an item the aircraft has not got the property for is
left alone rather than ending the flight, because that is a fault for a test
to catch and not a reason to stop flying.

**On screen** it is drawn small down the top right, as the credits are drawn
small along the bottom, so a long item reads without crowding the numbers
down the left. The first line is the phase and how much of it is done; each
line after begins "X" for a ticked item or "-" for one still to do. The font
has capitals, digits and a little punctuation and no more, so the words go in
capitals and what it lacks - a comma - is dropped; an item too long for the
line is cut rather than wrapped, so that every line but the first begins with
a mark and the two can never be told apart.

**Verification run.** Four tests fly it and one reads it off a frame:

- `every_checklist_item_the_cessna_can_see_ticks_when_its_state_first_shows_it_done`
  flies a take-off from a runway and, for every item of the taxi, take-off,
  climb and landing lists, holds the tick recorded against the first tick the
  item was really done - and holds that, flown by the book, every item of the
  three lists it flies that the aeroplane can see is ticked. Watched to fail
  with the recorded tick moved by one: "taxi item 0 (Engine running) ticked at
  1, but was first done at 0".
- `an_item_once_ticked_stays_ticked_though_the_aeroplane_moves_on` finds an
  item the take-off undoes - the taxi list's "throttle back to a walking
  pace", true at rest and false at full power - and holds it ticked. Watched
  to fail with unticking allowed: "was done at tick 0, stopped being so at
  144, and must still be ticked".
- `flown_with_the_flaps_left_up_the_landing_flaps_item_stays_unticked_and_is_flagged`
  and `the_pilot_ticks_their_own_items_and_only_their_own`.
- `the_checklist_on_screen_is_the_one_the_flight_is_working_through_on_<driver>`
  shoots a frame, reads the block back glyph by glyph and holds every line to
  what the same run said it drew, and refuses a phase of flight there is none
  of by name. Watched to fail twice: with the drawing moved three pixels, line
  one read "???????? ???" instead of "TAKE-OFF 4/6"; and with the heading's
  count made one too many, "the checklist says 5 done but 4 items are ticked"
  - that second one is a real independent check, because the tool counts the
  marks itself rather than trusting the heading.

294 of 294 tests pass locally at `-j4`, in 1036 s.

**The verification names a phase there is none of.** It asks for "the
Cessna's before-take-off, take-off and climb"; the nine phases are
`FEATURES.md`'s - before start, taxi, take-off, climb, cruise, descent,
approach, landing, after landing - and there is no before-take-off among
them. Taxi is what was flown in its place, and it is the list that holds the
run-up.
