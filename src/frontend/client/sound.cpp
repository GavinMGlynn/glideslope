#include "sound.hpp"

#include <cstdio>
#include <utility>

namespace glideslope::client {

Sound::Sound(std::filesystem::path data) : data_(std::move(data)) {}

const audio::SoundSet* Sound::set_of(const std::string& id) {
    auto it = sets_.find(id);
    if (it == sets_.end()) {
        std::optional<audio::SoundSet> set;
        try {
            set = audio::read_sound_set(data_, id);
        } catch (const audio::SoundSetError& e) {
            std::fprintf(stderr, "glideslope: %s is not heard: %s\n", id.c_str(), e.what());
        }
        it = sets_.emplace(id, std::move(set)).first;
    }
    return it->second ? &*it->second : nullptr;
}

void Sound::play(const Flight& flight, const std::vector<Other>& others, std::uint8_t own,
                 double seconds) {
    const std::string& id = flight.aircraft().id;
    const audio::SoundSet* set = set_of(id);
    if (set == nullptr) {
        return;
    }
    if (!mixer_ || flown_ != id) {
        mixer_ = std::make_unique<audio::Mixer>(*set, audio::mix_rate);
        flown_ = id;
    }
    audio::Heard heard = flight.heard(*set);
    const sim::AircraftState s = flight.state();
    const Axes listener = axes_at(flight.centre(), s.heading_deg, s.pitch_deg, s.roll_deg);
    for (const Other& o : others) {
        if (o.number == own || o.aircraft_id.empty()) {
            continue;
        }
        const audio::SoundSet* theirs = set_of(o.aircraft_id);
        if (theirs == nullptr) {
            continue;
        }
        const double dx = o.centre.x - listener.position.x;
        const double dy = o.centre.y - listener.position.y;
        const double dz = o.centre.z - listener.position.z;
        const auto along = [&](const world::Ecef& axis) {
            return dx * axis.x + dy * axis.y + dz * axis.z;
        };
        heard.others.push_back({o.number, theirs, along(listener.right), -along(listener.down),
                                along(listener.forward)});
    }
    mixer_->hear(heard);
    device_.play(*mixer_, seconds);
}

} // namespace glideslope::client
