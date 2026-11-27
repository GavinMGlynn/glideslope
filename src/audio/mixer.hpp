#pragma once

// **The mix**: what the listener hears, synthesised sample by sample from
// what the flight says (`Heard`) and each aircraft's sound set
// (audio/sound_set.hpp). It knows nothing of SDL or of the flight model -
// audio/heard.hpp reads a flight into a Heard, and audio/device.hpp hands the
// mix to SDL - so that a test can drive it, step by step, as a flight does.
//
// Stereo, interleaved left and right, 32-bit float at `rate` frames a second.
// The listener sits in her own aircraft: its sounds are in the middle, and
// another aircraft's are panned by where it is from her and fade with
// distance.
//
// **The events are heard from what changes between one Heard and the next**:
// her gear's motor runs while her gear moves, her flaps' while they do, her
// touchdown is a thump from the step a wheel (or her hull) first meets the
// surface, louder the faster she sinks - once she has been off it for
// `touchdown_after_off_s` or more, so that a wheel chattering on the
// rollout is not a landing, and a bounce that long is - and her stall
// warning sounds while her angle of attack is at or past her set's for her
// flaps, off the ground.

#include "audio/sound_set.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace glideslope::audio {

struct EngineHeard {
    bool running = false;
    double drive = 0.0; // RPM for a piston, N1 percent for a turbine
    double power = 0.0; // her throttle, 0 to 1
};

// Another aircraft, from the listener: metres along her own aircraft's
// right, up and forward.
struct OtherHeard {
    std::uint32_t key = 0; // the same aircraft keeps the same key
    const SoundSet* sounds = nullptr;
    double right_m = 0.0;
    double up_m = 0.0;
    double forward_m = 0.0;
};

struct Heard {
    std::vector<EngineHeard> engines;
    double airspeed_kts = 0.0;
    double gear = 1.0;   // 0 up, 1 down
    double flaps = 0.0;  // 0 up, 1 fully down
    bool on_surface = false;
    double sink_fps = 0.0;
    double alpha_deg = 0.0;
    std::vector<OtherHeard> others;
};

// Another aircraft's loudness at `distance_m`, of what she sounds at 50 m and
// nearer; nothing past 20 km.
double other_gain_at(double distance_m);
inline constexpr double other_full_within_m = 50.0;
inline constexpr double other_heard_within_m = 20000.0;
inline constexpr double touchdown_after_off_s = 0.25;

class Mixer {
public:
    // `own` is the listener's aircraft's sound set; it must outlive the mixer.
    Mixer(const SoundSet& own, int rate);

    // What the flight says now: heard from the next sample mixed.
    void hear(const Heard& heard);

    // Mixes `frames` stereo frames into `out` (2 x frames floats), going on
    // from the last.
    void mix(float* out, std::size_t frames);

    int rate() const { return rate_; }

    // What is sounding now, for a test to say what it heard.
    bool gear_moving() const { return gear_moving_; }
    bool flaps_moving() const { return flaps_moving_; }
    bool stall_warning() const { return stall_on_; }
    std::size_t touchdowns() const { return touchdowns_; }

private:
    struct Voice {
        double phase = 0.0;  // of the fundamental, 0 to 1
        double hz = 0.0;     // smoothed
        double amp = 0.0;    // smoothed
        double rumble = 0.0; // a low-passed noise's state
    };
    struct Tone {
        double phase = 0.0;
        double amp = 0.0;
    };
    double noise();
    double engine_sample(Voice& v, const SoundSet& set, double hz, double amp);
    double motor_sample(Tone& t, const Motor& m, bool on);

    const SoundSet& own_;
    int rate_;
    double smooth_; // the one-pole smoothing a sample, 20 ms
    Heard heard_;
    bool heard_once_ = false;
    std::vector<Voice> engines_;
    std::map<std::uint32_t, Voice> others_;
    double wind_lp_ = 0.0;
    double wind_amp_ = 0.0;
    Tone gear_tone_, flap_tone_, horn_;
    bool gear_moving_ = false;
    bool flaps_moving_ = false;
    bool stall_on_ = false;
    std::size_t touchdowns_ = 0;
    // Frames mixed since she last was on it, counted whole so that a quarter
    // second is exactly one.
    std::size_t off_surface_frames_ = static_cast<std::size_t>(-1) / 2;
    double thump_ = 0.0;     // its envelope, decaying
    double thump_phase_ = 0.0;
    double thump_lp_ = 0.0;
    std::uint32_t seed_ = 0x9e3779b9u;
};

} // namespace glideslope::audio
