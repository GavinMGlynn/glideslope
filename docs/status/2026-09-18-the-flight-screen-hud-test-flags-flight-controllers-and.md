### The flight screen: HUD, test flags, flight controllers and frames, 2026-09-18 — items done

Proved in CI on every platform (run 35244380011) and by every package (run
35244379942), each uploading its frame.

**What is missing first:** there is still nothing to see but sky: no terrain,
no aircraft, no cockpit. The HUD is text and a horizon line. There is no
gamepad binding, no way to rebind a device but editing the file, and no
settings screen - `--screen` chooses between the flight and three test scenes.

**The flight screen** (`src/frontend/client/flight.hpp`), the client's default:
the Cessna standing on the DEM - tiles and geoid downloaded into the cache -
started over Sydney at 1,000 m, 100 kt, stepped at 120 Hz, seen from the
cockpit through a camera built from its position and attitude.

**The HUD** (`gfx/hud.hpp`): airspeed, altitude, heading, vertical speed,
pitch and bank as text, and a horizon line that pitches and banks. Text is a
5-by-7 pixel font drawn here, each font pixel a square of whole screen pixels,
over everything through a second pipeline with no depth test - so a frame can be
read back exactly (`gfx::read_text`).

**The test flags.** `--screen NAME`; `--shot FILE` and `--shot-at TICK`, which
now counts simulation ticks, with every frame advancing exactly two while
shooting whatever the clock says; `--trace`, a line of the flight's state after
every tick. Each is used by a ctest: the frame tests choose their scenes with
`--screen` and shoot at ticks, and `the_hud_shows_the_flights_state_at_the_tick_it_was_shot_on_<driver>`
flies 600 ticks with `--trace`, shoots the last, and has
`glideslope_hud_check` read every HUD line back out of the BMP and hold each
number to the traced state, within half its last digit, having checked the
trace holds ticks 1 to 600 in order.

**Flight controllers** (`platform/input.hpp`, library `glideslope_input`, apart
from `glideslope_platform` so that the CLI still links no SDL). `Joysticks`
reads every device SDL sees; `ControlMapper` turns them into controls by the
bindings in `assets/input/bindings.txt` - per kind of device SDL reports,
flight sticks and yokes or HOTAS throttles, each axis centred or a lever, each
button held or stepping, each hat direction stepping. An axis sets its control
when it moves, so a lever left alone does not undo a button. Pitch trim is a
new control. The keyboard still works beside them.

**Tested through SDL's virtual devices.** A virtual yoke and a virtual HOTAS
throttle, eight axes, sixteen buttons and a hat each: every one of the 56 inputs
alone moves a control, and none is unbound; a particular set of inputs sets the
controls they name to the values they should - stick forward is nose down, a
lever three quarters forward is 0.75, two presses are two notches of flap, the
brake held and let go - and those controls reach JSBSim's commands with the
signs JSBSim uses; a bindings file that cannot be read is refused by line.

**Frames from everywhere.** Packages now carry the client and SDL's licence.
Every package job runs the unpacked client headless on its platform's driver -
Vulkan through Mesa's lavapipe, installed in the stock Linux containers; Metal;
Direct3D 12 - flying 600 ticks of the flight, and uploads the frame. CI uploads
every frame its tests wrote.

**Watched to fail:** the HUD's airspeed shown one knot high ("SPD shows 79 at
tick 600 when the state is 77.896"); a binding removed from the file (the yoke
had an unbound input). The first run of the controller tests found that an axis
rewrote its control every read and so undid button steps to the same control -
flaps stayed at 0 - which is why axes now act only when they move.
