#include "audio/device.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace glideslope::audio {

Device::Device() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        throw DeviceError(std::string("no audio: ") + SDL_GetError());
    }
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = mix_rate;
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr,
                                        nullptr);
    if (stream_ == nullptr) {
        const std::string why = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        throw DeviceError("no audio device: " + why);
    }
    SDL_ResumeAudioStreamDevice(stream_);
}

Device::~Device() {
    SDL_DestroyAudioStream(stream_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

std::string Device::driver() const {
    const char* name = SDL_GetCurrentAudioDriver();
    return name != nullptr ? name : "";
}

void Device::play(Mixer& mixer, double seconds) {
    if (!(seconds > 0.0)) {
        return;
    }
    owed_frames_ += seconds * static_cast<double>(mixer.rate());
    const double whole = std::floor(owed_frames_);
    owed_frames_ -= whole;
    const auto frames = static_cast<std::size_t>(whole);
    if (frames == 0) {
        return;
    }
    buffer_.resize(2 * frames);
    mixer.mix(buffer_.data(), frames);
    mixed_frames_ += frames;
    if (tap_ != nullptr) {
        tap_->insert(tap_->end(), buffer_.begin(), buffer_.end());
    }
    constexpr std::size_t frame_bytes = 2 * sizeof(float);
    const auto most = static_cast<std::size_t>(most_queued_s * mix_rate) * frame_bytes;
    const auto queued = static_cast<std::size_t>(std::max(SDL_GetAudioStreamQueued(stream_), 0));
    const std::size_t room = queued < most ? (most - queued) / frame_bytes : 0;
    const std::size_t put = std::min(frames, room);
    not_queued_frames_ += frames - put;
    if (put == 0) {
        fade_in_next_ = true;
        return;
    }
    const auto fade = std::min(put, static_cast<std::size_t>(fade_s * mix_rate));
    const auto ramp = [&](std::size_t from, bool in) {
        for (std::size_t i = 0; i < fade; ++i) {
            const float g = static_cast<float>(i) / static_cast<float>(fade);
            const std::size_t f = from + i;
            const float k = in ? g : 1.0f - g;
            buffer_[2 * f] *= k;
            buffer_[2 * f + 1] *= k;
        }
    };
    if (fade_in_next_) {
        ramp(0, true);
        fade_in_next_ = false;
    }
    if (put < frames) {
        ramp(put - fade, false);
        fade_in_next_ = true;
    }
    SDL_PutAudioStreamData(stream_, buffer_.data(), static_cast<int>(put * frame_bytes));
}

} // namespace glideslope::audio
