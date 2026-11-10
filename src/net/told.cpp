#include "net/told.hpp"

#include <utility>

namespace glideslope::net {

Told::Heard Told::hear(std::span<const std::uint8_t> body) {
    const std::optional<Message> kind = kind_of(body);
    if (!kind) {
        return Heard::nothing;
    }
    switch (*kind) {
    case Message::session: {
        Session m;
        if (read(body, m)) {
            session_ = std::move(m);
            return Heard::session;
        }
        return Heard::nothing;
    }
    case Message::lobby: {
        Lobby m;
        if (read(body, m)) {
            lobby_ = std::move(m);
            return Heard::lobby;
        }
        return Heard::nothing;
    }
    case Message::terrain_dataset: {
        TerrainDataset m;
        if (read(body, m)) {
            dataset_ = std::move(m);
            return Heard::dataset;
        }
        return Heard::nothing;
    }
    case Message::weather: {
        Weather m;
        if (!read(body, m)) {
            return Heard::nothing;
        }
        if (m.aloft_follows) {
            waiting_for_aloft_ = std::move(m);
            return Heard::nothing;
        }
        waiting_for_aloft_.reset();
        took(std::move(m), std::nullopt);
        return Heard::weather;
    }
    case Message::weather_aloft: {
        WeatherAloft m;
        // The forecast for the slot of the weather it completes, or none.
        if (!waiting_for_aloft_ || !read(body, m) || m.station != waiting_for_aloft_->station) {
            return Heard::nothing;
        }
        Weather w = std::move(*waiting_for_aloft_);
        waiting_for_aloft_.reset();
        took(std::move(w), std::move(m));
        return Heard::weather;
    }
    case Message::runway_conditions: {
        RunwayConditions m;
        if (!read(body, m)) {
            return Heard::nothing;
        }
        last_station_ = m.station;
        Station& s = stations_[m.station];
        s.runway_conditions = std::move(m);
        ++s.runway_conditions_heard;
        ++runway_conditions_heard_;
        return Heard::runway_conditions;
    }
    case Message::law_state: {
        LawState m;
        if (!read(body, m)) {
            return Heard::nothing;
        }
        law_state_ = std::move(m);
        ++law_states_heard_;
        return Heard::law_state;
    }
    case Message::aircraft:
    case Message::controller_swap:
    case Message::watch:
    case Message::copilot_route:
    case Message::take_over_refused:
    case Message::learnt_landing_refused:
    case Message::traffic_instruction:
        return Heard::nothing;
    }
    return Heard::nothing;
}

void Told::took(Weather weather, std::optional<WeatherAloft> aloft) {
    last_station_ = weather.station;
    Station& s = stations_[weather.station];
    s.before = std::move(s.weather);
    s.before_aloft = std::move(s.aloft);
    s.weather = std::move(weather);
    s.aloft = std::move(aloft);
    ++s.weathers;
    ++weathers_;
}

} // namespace glideslope::net
