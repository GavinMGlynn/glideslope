#pragma once

// **The window client heard** (src/audio/): the aircraft flown, by her own
// sound set (data/sound/<id>.sound), and every other aircraft on a server by
// hers, placed by where it is from the listener and fading with distance.
// The mix is made by the simulated time flown each pass, so that what is
// heard keeps the flight's own pace.

#include "audio/device.hpp"
#include "audio/mixer.hpp"
#include "audio/sound_set.hpp"
#include "flight.hpp"
#include "online.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::client {

class Sound {
public:
    // Opens the default playback device (audio::Device); throws
    // audio::DeviceError where there is none.
    explicit Sound(std::filesystem::path data);

    std::string driver() const { return device_.driver(); }

    // `seconds` more of the mix, of `flight` as it is now and of `others`
    // but the one numbered `own` - its own, among them while the AI flies it
    // on a server. An aircraft whose sound set cannot be read is not heard,
    // and said once.
    void play(const Flight& flight, const std::vector<Other>& others, std::uint8_t own,
              double seconds);

private:
    const audio::SoundSet* set_of(const std::string& id);

    std::filesystem::path data_;
    audio::Device device_;
    std::map<std::string, std::optional<audio::SoundSet>> sets_;
    std::string flown_;
    std::unique_ptr<audio::Mixer> mixer_;
};

} // namespace glideslope::client
