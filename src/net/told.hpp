#pragma once

// **What a server has told a client about the session it is in** (REQUIREMENTS.md
// section 6.3): the session, the lobby, the terrain dataset it collides on and
// the weather it flies, each as the newest message of its kind said it.
//
// Both clients - `glideslope_cli connect` and the client with the window -
// hear the server's reliable messages through one of these, so that the two
// cannot read the same message differently.
//
// **A weather is taken whole or not at all.** A `WEATHER` that says a
// `WEATHER_ALOFT` follows is held until that arrives, so that a report is
// never flown for a moment without the forecast above it; the reliable layer
// delivers them in order, so the forecast is the next of the two to come. A
// `WEATHER_ALOFT` with no such `WEATHER` before it is not a weather, and is
// dropped.

#include "net/messages.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace glideslope::net {

class Told {
public:
    // What a message changed, if anything a caller acts on.
    enum class Heard { nothing, session, lobby, dataset, weather };

    // **Takes one reliable message**, whatever its kind; a kind this does not
    // keep - an aircraft's definition, a swap - is `nothing` here.
    Heard hear(std::span<const std::uint8_t> body);

    const std::optional<Session>& session() const { return session_; }
    const std::optional<Lobby>& lobby() const { return lobby_; }
    const std::optional<TerrainDataset>& dataset() const { return dataset_; }
    // The newest whole weather, and the forecast above it if it had one.
    const std::optional<Weather>& weather() const { return weather_; }
    const std::optional<WeatherAloft>& aloft() const { return aloft_; }
    // How many whole weathers have been heard: one on joining, and one more
    // for each change.
    int weathers() const { return weathers_; }

private:
    std::optional<Session> session_;
    std::optional<Lobby> lobby_;
    std::optional<TerrainDataset> dataset_;
    std::optional<Weather> weather_;
    std::optional<WeatherAloft> aloft_;
    std::optional<Weather> waiting_for_aloft_;
    int weathers_ = 0;
};

} // namespace glideslope::net
