### A pilot's key and lever for reverse thrust, 2026-10-29 - item done

**What is still not true first.**
- **The HUD does not show the reverse lever.** It shows the stick, rudder,
  throttle, flaps, gear and speedbrake; the mixture, propeller and STOVL
  selector are not on it either. The F1 help lists the new key and buttons,
  as it lists every binding.
- **A joystick's reverse is all or nothing.** Its button holds maximum
  reverse and lets it go stowed; idle reverse (0.1) is reached from the
  keyboard only. Every input of a device of eight axes, sixteen buttons and a
  hat was already bound, so the reverse took the button that duplicated the
  brakes (button 15 of each device; button 0 still holds the brakes).
- **The reverse key is the slash, unshifted.** No sim has a convention both
  share: X-Plane's thrust reverse toggle is Shift + / (its key-command lists),
  MSFS reverses on F2 held (F1 here is the help), and FlightGear's reversers
  are per aircraft (its wiki's example uses Delete, its old keyboard map
  Ctrl-B for Concorde). Glideslope's keys are modifier-free pairs, so the
  slash raises the reverse lever and the backslash, beside it on the
  keyboard's other edge, stows it. Neither key was bound.
- **Keys are bound by position, not by what is printed on them** (SDL
  scancodes). On a UK or other ISO keyboard the backslash's scancode is the
  `#` key beside Enter, as the brackets, semicolon and apostrophe are
  already wherever a US keyboard has them.

**What works.** `platform::Control` has `reverse`, the bindings file reads
`reverse`, and the keyboard's table has a lever pair for it: `/` raises the
reverse lever and `\` lowers it, at half its travel a second, as the other
levers move - from stowed to idle reverse in a fifth of a second, to maximum
in two. Each device's button 15 (`assets/input/bindings.txt`) holds it at 1.
The lever was already on the wire (`docs/TRANSPORT.md`) and on the flight
model; only the AI moved it before.

**On an aircraft without reversers it does nothing**: `Aircraft::set_controls`
sets `fcs/reverse-cmd-norm` only on a model that has it (the five
airliners), so on the other eleven the lever moves in the controls and
nothing reads it. On an airliner the reversers deploy only with the weight
on the wheels; lifted in the air, the reversing engines' forward thrust is
held at idle, as the levers' interlock holds it (`tools/airliner.py`), and
comes back when it is stowed.

**Verification.**
- `the_keyboard_moves_every_lever_it_names_and_leaves_it_where_it_is_put`
  walks five lever pairs now (was four), and fails unless that is every
  lever pair in the keyboard's table.
- `a_binding_moves_every_lever_the_controls_have` looks for five levers in
  the bindings file (was four), and presses each device's button 15 on its
  own mapper: reverse 0 before, 1 held, 0 let go, on both kinds of device
  (2 of 2).
- `every_axis_button_and_hat_of_a_virtual_stick_and_throttle_moves_the_controls`
  compares the reverse lever too; it still walks all 56 inputs.
- `the_help_on_screen_names_every_binding_in_the_bindings_file_every_key_and_every_command`
  counts eleven key bindings (was ten).
- `the_controls_help_is_laid_out_inside_every_frame_size_and_says_so_where_it_is_cut`
  laid out a fixed 43 lines, said to be as many and as long as the
  committed bindings' help; it now reads that help and lays out as many
  lines - 45 - with the first as long as its longest, 86 characters (the
  fixed lines stopped at 79), and fails unless what it lays out is that
  many lines and that long.

**Seen red.** With the keyboard's reverse pair taken out of its table and
both devices' button 15 put back to the brakes, rebuilt and reconfigured
(the bindings are copied at configure time), both tests failed:
`a_binding_moves_every_lever_the_controls_have` at "reverse is bound to
something", and
`the_keyboard_moves_every_lever_it_names_and_leaves_it_where_it_is_put` at
"reverse goes down: 0.500000". Both were put back and pass again, with every
test whose name touches a lever, a binding, the keyboard, the help or the
wire (33 of them, the help drawn and read back off a Vulkan frame among
them); the whole suite is CI's to run.

From the review, each new check was then seen red on its own:
- **The button's hold**: with button 15 bound to `reverse step 0.25` on both
  devices instead of `hold`, reconfigured, the reverse is still bound, and
  only the 0, 1, 0 check failed: "STICK's last button: reverse 0.000000
  before, 0.250000 held, 0.250000 let go - not 0, 1, 0".
- **The help's count**: with the layout put back to a fixed 43 lines,
  `glideslope_help_check layout` failed: "laying out 43 lines, the longest
  87, for help of 45, the longest 86".
Both were reverted, and the 24 tests on levers, bindings, the keyboard and
the help pass again. README's list of keys and buttons has the reverse.
