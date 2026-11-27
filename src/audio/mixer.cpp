#include "audio/mixer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace glideslope::audio {

namespace {

constexpr double two_pi = 2.0 * std::numbers::pi;

// A one-pole low-pass's coefficient for `hz` at `rate`.
double low_pass(double hz, int rate) {
    return 1.0 - std::exp(-two_pi * hz / static_cast<double>(rate));
}

} // namespace

double other_gain_at(double distance_m) {
    if (!(distance_m < other_heard_within_m)) {
        return 0.0;
    }
    return distance_m <= other_full_within_m ? 1.0 : other_full_within_m / distance_m;
}

Mixer::Mixer(const SoundSet& own, int rate)
    : own_(own), rate_(rate), smooth_(1.0 - std::exp(-1.0 / (0.02 * rate))) {}

void Mixer::hear(const Heard& heard) {
    if (heard_once_) {
        gear_moving_ = own_.gear.has_value() && std::abs(heard.gear - heard_.gear) > 1e-6;
        flaps_moving_ = own_.flaps.has_value() && std::abs(heard.flaps - heard_.flaps) > 1e-6;
        if (heard.on_surface && !heard_.on_surface && static_cast<double>(off_surface_frames_) >=
                                                             touchdown_after_off_s * rate_) {
            // Louder the faster she comes down: full at 10 ft/s, and never
            // less than a quarter of it.
            thump_ = own_.touchdown_gain * std::clamp(heard.sink_fps / 10.0, 0.25, 1.0);
            ++touchdowns_;
        }
    }
    if (heard.on_surface) {
        off_surface_frames_ = 0;
    }
    stall_on_ = !heard.on_surface &&
                heard.alpha_deg >= own_.stall_warning_alpha_deg(heard.flaps);
    engines_.resize(heard.engines.size());
    for (auto it = others_.begin(); it != others_.end();) {
        const bool still = std::any_of(heard.others.begin(), heard.others.end(),
                                       [&](const OtherHeard& o) { return o.key == it->first; });
        it = still ? std::next(it) : others_.erase(it);
    }
    heard_ = heard;
    heard_once_ = true;
}

double Mixer::noise() {
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return static_cast<double>(seed_) / 2147483648.0 - 1.0;
}

// One sample of an engine's note: its fundamental strongest, three
// harmonics above it, and for a turbine her blades' whine; a rumble of noise
// under it all. Its pitch and loudness move to `hz` and `amp` over 20 ms.
double Mixer::engine_sample(Voice& v, const SoundSet& set, double hz, double amp) {
    v.hz += smooth_ * (hz - v.hz);
    v.amp += smooth_ * (amp - v.amp);
    v.phase += v.hz / static_cast<double>(rate_);
    v.phase -= std::floor(v.phase);
    const double x = two_pi * v.phase;
    double s = std::sin(x) + 0.5 * std::sin(2.0 * x) + 0.33 * std::sin(3.0 * x) +
               0.25 * std::sin(4.0 * x);
    if (set.engine == EngineKind::turbine) {
        s += 0.3 * std::sin(x * static_cast<double>(set.blades));
    }
    v.rumble += low_pass(300.0, rate_) * (noise() - v.rumble);
    s += 0.6 * v.rumble;
    return v.amp * s;
}

double Mixer::motor_sample(Tone& t, const Motor& m, bool on) {
    t.amp += smooth_ * ((on ? m.gain : 0.0) - t.amp);
    t.phase += m.hz / static_cast<double>(rate_);
    t.phase -= std::floor(t.phase);
    const double x = two_pi * t.phase;
    return t.amp * (std::sin(x) + 0.3 * std::sin(3.0 * x));
}

void Mixer::mix(float* out, std::size_t frames) {
    const double centre = std::sqrt(0.5);
    const double engines = static_cast<double>(std::max<std::size_t>(heard_.engines.size(), 1));
    const double x = heard_.airspeed_kts / own_.wind_ref_kt;
    const double wind_target = own_.wind_gain * x * x / (1.0 + x * x);
    const double wind_cut = low_pass(150.0 + 6.0 * std::max(heard_.airspeed_kts, 0.0), rate_);
    const double thump_decay = std::exp(-1.0 / (0.12 * rate_));
    const double thump_cut = low_pass(200.0, rate_);
    if (!heard_.on_surface) {
        off_surface_frames_ += frames;
    }
    for (std::size_t f = 0; f < frames; ++f) {
        double middle = 0.0;
        for (std::size_t i = 0; i < engines_.size(); ++i) {
            const EngineHeard& e = heard_.engines[i];
            const double amp =
                e.running ? own_.engine_gain * (0.35 + 0.65 * std::clamp(e.power, 0.0, 1.0)) /
                                std::sqrt(engines)
                          : 0.0;
            middle += engine_sample(engines_[i], own_, own_.fundamental_hz(e.drive), amp);
        }
        wind_amp_ += smooth_ * (wind_target - wind_amp_);
        wind_lp_ += wind_cut * (noise() - wind_lp_);
        middle += wind_amp_ * 2.0 * wind_lp_;
        if (own_.gear) {
            middle += motor_sample(gear_tone_, *own_.gear, gear_moving_);
        }
        if (own_.flaps) {
            middle += motor_sample(flap_tone_, *own_.flaps, flaps_moving_);
        }
        middle += motor_sample(horn_, own_.stall_horn, stall_on_);
        if (thump_ > 1e-7) {
            thump_phase_ += 55.0 / static_cast<double>(rate_);
            thump_phase_ -= std::floor(thump_phase_);
            thump_lp_ += thump_cut * (noise() - thump_lp_);
            middle += thump_ * (0.6 * std::sin(two_pi * thump_phase_) + 2.0 * thump_lp_);
            thump_ *= thump_decay;
        } else {
            thump_ = 0.0;
        }
        double left = centre * middle;
        double right = centre * middle;
        for (const OtherHeard& o : heard_.others) {
            if (o.sounds == nullptr) {
                continue;
            }
            const double d = std::sqrt(o.right_m * o.right_m + o.up_m * o.up_m +
                                       o.forward_m * o.forward_m);
            const double pan = d > 1e-6 ? std::clamp(o.right_m / d, -1.0, 1.0) : 0.0;
            const double angle = (pan + 1.0) * std::numbers::pi / 4.0;
            const double s =
                engine_sample(others_[o.key], *o.sounds,
                              o.sounds->fundamental_hz(o.sounds->heard_as_other_drive),
                              o.sounds->engine_gain * other_gain_at(d));
            left += std::cos(angle) * s;
            right += std::sin(angle) * s;
        }
        out[2 * f] = static_cast<float>(std::clamp(left, -1.0, 1.0));
        out[2 * f + 1] = static_cast<float>(std::clamp(right, -1.0, 1.0));
    }
}

} // namespace glideslope::audio
