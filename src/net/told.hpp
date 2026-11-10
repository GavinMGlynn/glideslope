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
//
// **A server's weather is several stations', each in its slot**
// (net::Weather::station): each slot's is kept as its own, its newest whole
// weather and the one before, and its runways.

#include "net/messages.hpp"
#include "net/state.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <span>

namespace glideslope::net {

class Told {
public:
    // What a message changed, if anything a caller acts on.
    enum class Heard { nothing, session, lobby, dataset, weather, runway_conditions, law_state };

    // **Takes one reliable message**, whatever its kind; a kind this does not
    // keep - an aircraft's definition, a swap - is `nothing` here.
    Heard hear(std::span<const std::uint8_t> body);

    const std::optional<Session>& session() const { return session_; }
    const std::optional<Lobby>& lobby() const { return lobby_; }
    const std::optional<TerrainDataset>& dataset() const { return dataset_; }

    // **What one slot has been told**.
    struct Station {
        // The newest whole weather, and the forecast above it if it had one.
        std::optional<Weather> weather;
        std::optional<WeatherAloft> aloft;
        // **The whole weather heard before the newest**, and its forecast:
        // what the newest blends from, if it still blends. A server tells a
        // client joining mid-blend this first, so that one making its air
        // from the newest alone - a flight made since - can blend from it.
        std::optional<Weather> before;
        std::optional<WeatherAloft> before_aloft;
        // How many whole weathers this slot has heard.
        int weathers = 0;
        // **The newest runway conditions**, and how many have been heard.
        // They are for the weather that took over when they say: a client
        // flying another drops them.
        std::optional<RunwayConditions> runway_conditions;
        int runway_conditions_heard = 0;
    };
    // Every slot told anything, by its number.
    const std::map<std::uint8_t, Station>& stations() const { return stations_; }
    // The slot the newest whole weather, or runway conditions, was for.
    std::uint8_t last_station() const { return last_station_; }
    // The newest whole weather heard, of any slot, and its forecast.
    const std::optional<Weather>& weather() const { return station(last_station_).weather; }
    const std::optional<WeatherAloft>& aloft() const { return station(last_station_).aloft; }
    // How many whole weathers have been heard, of every slot: one on joining
    // for each, and one more for each change.
    int weathers() const { return weathers_; }
    // The newest runway conditions heard, of any slot, and how many in all.
    const std::optional<RunwayConditions>& runway_conditions() const {
        return station(last_station_).runway_conditions;
    }
    int runway_conditions_heard() const { return runway_conditions_heard_; }
    // **The newest STOVL law state** (`LAW_STATE`), and how many have been
    // heard: for the aircraft it names, at the step it names.
    const std::optional<LawState>& law_state() const { return law_state_; }
    int law_states_heard() const { return law_states_heard_; }

private:
    std::optional<Session> session_;
    std::optional<Lobby> lobby_;
    std::optional<TerrainDataset> dataset_;
    const Station& station(std::uint8_t slot) const {
        static const Station none;
        const auto it = stations_.find(slot);
        return it == stations_.end() ? none : it->second;
    }
    // A whole weather heard for its slot.
    void took(Weather weather, std::optional<WeatherAloft> aloft);
    std::map<std::uint8_t, Station> stations_;
    std::uint8_t last_station_ = 0;
    std::optional<Weather> waiting_for_aloft_;
    int weathers_ = 0;
    int runway_conditions_heard_ = 0;
    std::optional<LawState> law_state_;
    int law_states_heard_ = 0;
};

} // namespace glideslope::net
