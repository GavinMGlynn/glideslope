### A checklist band the aeroplane can reach, 2026-09-21 — tail done

The tail found writing the checklists: the tests proved a property was real
and that the aircraft's own model drove it, but not that the figure was one
the aeroplane could get to. A flap band of 33 to 35 degrees on a type whose
flaps stop at 32 passed everything and would never have ticked once.

**A lever and what it moves are held differently**, because they are
different things. A command is the lever, and its travel is known without
flying anything - 0 to 1, or -1 to 1 for the ones that go both ways. A
position is where the aeroplane has got to, which only its own model knows,
so its levers are worked through their travel and what the position really
covers is measured.

**The aeroplane is not flown to find out**, and three attempts taught why.
Driving every lever to its stop at once - full throttle against full brakes,
the trim hard over - put a tail-wheel aeroplane on its nose and off the end
of its own aerodynamic tables. Retracting the undercarriage while the
aeroplane stood on it did the same. Both end the flight inside JSBSim, which
asserts rather than extrapolating. Only the configuration levers are worked,
with the brakes holding the aeroplane still, and the undercarriage is read
from the model instead: an aeroplane that never names `gear/gear-cmd-norm`
or `gear/gear-pos-norm` has no undercarriage channel and its wheels stay
down. `<retractable>` on a leg is not the marker - the Mosquito's legs do not
carry it and its undercarriage still comes up.

**A flying boat needs water under it.** Given dry land its hull tells the
truth about dry land, and every hydrodynamic item looked unreachable: the
S.23's "the hull in the water and carrying the boat" wanted
`hydro/active-norm` at least 0.5 and measured 0 throughout. That was the
test's fault, not the checklist's.

**Verification run.**
`every_checklist_band_is_one_its_aeroplanes_controls_can_reach`, registered,
watched to fail with the Cessna's landing flaps put at 33 to 35 degrees:
"c172p's landing ... wants fcs/flap-pos-deg between 33 and 35, but its levers
only move it between 0 and 30". It takes 68 seconds. 300 of 300 tests
pass locally at `-j4`, in 1039 s.

**What GCC cannot see.** The two constants this left unused when its trace
came out were invisible to both Linux builds: GCC does not warn about an
unused constant at namespace scope in C++, not under `-Wall -Wextra` and not
under `-Wunused-const-variable` at any level, which it honours for C alone.
Clang does, so macOS and Windows clang-cl went red together on a commit that
both Linux jobs had passed. Every first-party file is now checked by hand
under clang before a push, which is review rather than a check, and a tail
records that.
