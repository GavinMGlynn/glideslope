### The propeller and mixture levers a pilot can reach, 2026-09-21 — tail done

The tail found making the Short S.23: the controls had a propeller lever and
a mixture that the figure flights set, and no key or binding worked either,
so a pilot flew every aeroplane full rich with its airscrews at their
highest. The S.23 turned 3,185 rpm and gave 1,185 hp where its Pegasus is
rated at 2,600 and 920.

**`propeller` is now a control a binding can name**, beside the mixture that
already was one. Every input of both devices was already bound - the file
says so and a test walks all 56 - so this is a rebinding, not an addition:
**the quadrant's three levers now sit in the order the hand finds them**,
throttle, propeller, mixture, on its first three axes, and the rudder comes
off the quadrant, where a rudder does not belong. Two buttons on each device
step the propeller.

**The keyboard works them too**: the comma and full stop for the mixture, the
square brackets for the propeller, each moving at the throttle's rate. **A
lever stays where it is left**, as the throttle does - one that sprang back
to rich when the key came up would be no use for leaning.

**The keyboard moved house to be testable.** It was a class inside the
client's `main.cpp`, asking SDL for the key state itself, and the tests link
no client code. It is `platform::KeyboardControls` now, given the key state
rather than fetching it, which is where the joystick mapping already lives
and is what let the keys be held to the same standard as the bindings.

**What it is worth, measured:**

| Aeroplane | Levers | Engine |
| --- | --- | --- |
| Short S.23 | Coarse pitch, mixture at NORMAL | **2,169 rpm**, inside its rated 2,600 |
| Short S.23 | Fine pitch, mixture through the gate | 2,871 rpm - the take-off setting, and now a choice |
| Mosquito FB VI | Lever fully forward | 2,530 rpm |
| Mosquito FB VI | Lever at four tenths | 2,283 rpm |

The S.23's take-off setting still turns above its rating, which is what a
take-off setting is for and why it has a gate; what the tail was about is
that a pilot had no way to come off it, and now has one.

**Verification run.** Four tests:
`the_keyboard_moves_every_lever_it_names_and_leaves_it_where_it_is_put`
(which walks all three levers, each down, up, to both stops, and held after
the key comes up), `a_binding_moves_every_lever_the_controls_have`,
`the_short_s23_in_coarse_pitch_at_normal_boost_turns_within_its_rated_rpm`
and `the_mosquitos_rpm_follows_its_propeller_lever`. Watched to fail with the
propeller taken out of the mapper's `fields`: "and back puts it at nought,
not 1.000000".

**And the walk of every input found the new control before a person did.**
`every_axis_button_and_hat_of_a_virtual_stick_and_throttle_moves_the_controls`
failed with "a yoke button 11 moved no control": the binding was right, and
the test could not see it. Its own list of controls to compare had nine and
not the propeller, so a binding that moved only that one looked like it moved
nothing, and it started the propeller at its default of 1, where a step
upward has nowhere to go. Both put right, and it still walks all 56 inputs of
both devices. It was a filtered run - `ctest -R 'input|binding'` - that
missed this; the whole suite found it.

323 of 323 tests pass locally at `-j4`, in 978 s.
