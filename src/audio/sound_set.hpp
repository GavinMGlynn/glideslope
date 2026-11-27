#pragma once

// **An aircraft's sounds, as data** (assets/sound/<id>.sound, one for every
// aircraft in the catalogue, by its id): what the mix (audio/mixer.hpp) makes
// her engines, her gear and flap motors, her touchdown and her stall warning
// sound like. Every sound is synthesised from these numbers - nothing is
// recorded yet; Phase 15 brings recordings (docs/COMPLETION_PLAN.md).
//
//   engine piston CYLINDERS GAIN
//                     a four-stroke piston engine: her note's fundamental is
//                     her firing rate, RPM / 60 x CYLINDERS / 2, from the
//                     flight model's engine-rpm
//   engine turbine FAN_RPM BLADES GAIN
//                     a turbine: her fundamental is her fan's turning rate,
//                     N1 / 100 x FAN_RPM / 60, from the flight model's n1,
//                     with the whine of BLADES blades passing above it
//   wind GAIN REF_KT  the air past her, rising with airspeed: half its
//                     loudness at REF_KT
//   gear MOTOR_HZ GAIN
//                     her gear's motor while it moves; only an aircraft whose
//                     gear retracts gives it
//   flaps MOTOR_HZ GAIN
//                     her flaps' motor while they move; only an aircraft
//                     whose flap lever works flaps gives it
//   touchdown GAIN    her wheels or her hull meeting the surface
//   stall-warning ALPHA_DEG HORN_HZ GAIN
//                     her stall warning, sounding from ALPHA_DEG of angle of
//                     attack while she is off the ground
//   heard-as-other DRIVE
//                     another aircraft's engine is heard at this drive - RPM
//                     for a piston, N1 percent for a turbine - because what a
//                     state update carries of her says nothing of her engine
//
// with `#` beginning a comment. Gains are of the whole mix's full scale.

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace glideslope::audio {

struct SoundSetError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class EngineKind { piston, turbine };

struct Motor {
    double hz = 0.0;
    double gain = 0.0;
};

struct SoundSet {
    std::string id;
    EngineKind engine = EngineKind::piston;
    int cylinders = 0;       // piston
    double fan_rpm = 0.0;    // turbine, at 100% N1
    int blades = 0;          // turbine
    double engine_gain = 0.0;
    double wind_gain = 0.0;
    double wind_ref_kt = 0.0;
    std::optional<Motor> gear; // only where her gear retracts
    std::optional<Motor> flaps; // only where her flap lever works flaps
    double touchdown_gain = 0.0;
    double stall_alpha_deg = 0.0;
    Motor stall_horn;
    double heard_as_other_drive = 0.0;

    // **Her note's fundamental at `drive`**: RPM for a piston, N1 percent for
    // a turbine.
    double fundamental_hz(double drive) const;
};

SoundSet parse_sound_set(const std::string& id, std::string_view text);
// data/sound/<id>.sound; throws SoundSetError where it is missing or wrong.
SoundSet read_sound_set(const std::filesystem::path& data, const std::string& id);

} // namespace glideslope::audio
