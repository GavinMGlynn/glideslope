#pragma once

// **The mix, played** (audio/mixer.hpp): SDL3's default playback device,
// through an audio stream - SDL converts to whatever the device wants. The
// mix is made by the simulated time flown, not by the device's asking: a
// flight flown faster or slower than the clock is heard as flown, and a test
// on SDL's dummy driver hears exactly what its steps made.
//
// **No more than `most_queued_s` waits to be played**: a mix made faster than
// the device plays it - a test's, or a frame loop catching up - is still
// made, and tapped, but what would queue past that is not given to SDL, and
// is counted (`not_queued_frames`). What is given is trimmed to fit, its last
// `fade_s` faded out and the next given faded in, so that the cut does not
// click.

#include "audio/mixer.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

struct SDL_AudioStream;

namespace glideslope::audio {

struct DeviceError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline constexpr int mix_rate = 48000;
inline constexpr double most_queued_s = 0.25;
inline constexpr double fade_s = 0.005;

class Device {
public:
    // Opens SDL's audio and its default playback device, on whichever driver
    // SDL picks - SDL_AUDIO_DRIVER names one. Throws DeviceError, with SDL's
    // reason, where there is none.
    Device();
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // The driver SDL opened: "dummy", "pulseaudio", "wasapi" ...
    std::string driver() const;

    // Mixes `seconds` more of `mixer` - simulated time flown - and queues it.
    void play(Mixer& mixer, double seconds);

    // **The test hook**: every frame mixed from now on, interleaved left and
    // right, appended to `into`; null stops it.
    void tap(std::vector<float>* into) { tap_ = into; }

    std::size_t mixed_frames() const { return mixed_frames_; }
    std::size_t not_queued_frames() const { return not_queued_frames_; }

private:
    SDL_AudioStream* stream_ = nullptr;
    double owed_frames_ = 0.0;
    std::vector<float> buffer_;
    std::vector<float>* tap_ = nullptr;
    std::size_t mixed_frames_ = 0;
    std::size_t not_queued_frames_ = 0;
    bool fade_in_next_ = false;
};

} // namespace glideslope::audio
