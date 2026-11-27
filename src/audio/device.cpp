#include "audio/device.hpp"

#include <SDL3/SDL.h>

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
    const int bytes = static_cast<int>(buffer_.size() * sizeof(float));
    const int most = static_cast<int>(most_queued_s * mix_rate) * 2 *
                     static_cast<int>(sizeof(float));
    if (SDL_GetAudioStreamQueued(stream_) + bytes > most) {
        not_queued_frames_ += frames;
        return;
    }
    SDL_PutAudioStreamData(stream_, buffer_.data(), bytes);
}

} // namespace glideslope::audio
