#pragma once

// **What the mix is told of a flight** (audio/mixer.hpp), read from her
// flight model: each engine running or not, at what drive - her engine's RPM
// for a piston, N1 for a turbine (audio/sound_set.hpp) - and power; her
// airspeed, gear, flaps, whether a wheel (or her hull) is on the surface and
// how fast she sinks, and her angle of attack. Other aircraft are the
// caller's to add.

#include "audio/mixer.hpp"
#include "audio/sound_set.hpp"
#include "sim/aircraft.hpp"

namespace glideslope::audio {

Heard heard_of(const sim::Aircraft& aircraft, const SoundSet& sounds);

} // namespace glideslope::audio
