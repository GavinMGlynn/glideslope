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
        weather_ = std::move(m);
        aloft_.reset();
        ++weathers_;
        return Heard::weather;
    }
    case Message::weather_aloft: {
        WeatherAloft m;
        if (!waiting_for_aloft_ || !read(body, m)) {
            return Heard::nothing;
        }
        weather_ = std::move(*waiting_for_aloft_);
        waiting_for_aloft_.reset();
        aloft_ = std::move(m);
        ++weathers_;
        return Heard::weather;
    }
    case Message::aircraft:
    case Message::controller_swap:
    case Message::watch:
    case Message::copilot_route:
    case Message::take_over_refused:
    case Message::learnt_landing_refused:
        return Heard::nothing;
    }
    return Heard::nothing;
}

} // namespace glideslope::net
