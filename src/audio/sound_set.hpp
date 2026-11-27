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
//   stall-warning HORN_HZ GAIN
//                     her stall warning, sounding while she is off the
//                     ground at or past the angle of attack her own flight
//                     model's lift curve gives (sim/lift_peak.hpp), worked
//                     out as her set is loaded for her to be flown
//                     (`own_sound_set`)
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
    Motor stall_horn;
    // Where her warning sounds, degrees of alpha, clean and with her flaps
    // fully down - between them by her flaps (`stall_warning_alpha_deg`).
    // Not in her file: `own_sound_set` reads them from her flight model.
    double stall_alpha_clean_deg = 90.0;
    double stall_alpha_flaps_deg = 90.0;
    double stall_warning_alpha_deg(double flaps_down) const {
        return stall_alpha_clean_deg + flaps_down * (stall_alpha_flaps_deg - stall_alpha_clean_deg);
    }
    double heard_as_other_drive = 0.0;

    // **Her note's fundamental at `drive`**: RPM for a piston, N1 percent for
    // a turbine.
    double fundamental_hz(double drive) const;
};

SoundSet parse_sound_set(const std::string& id, std::string_view text);
// data/sound/<id>.sound; throws SoundSetError where it is missing or wrong.
SoundSet read_sound_set(const std::filesystem::path& data, const std::string& id);
// **Hers, for her to be flown**: read as above, and her stall warning's
// angles read from her flight model (sim::lift_peak), clean and flaps down,
// at her catalogue airspeed and her warning margin (sim::stall_warning_margin).
SoundSet own_sound_set(const std::filesystem::path& data, const std::string& id,
                       const std::string& model, double airspeed_kts);

} // namespace glideslope::audio
