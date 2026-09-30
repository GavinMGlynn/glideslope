#pragma once

// **A player's copilot, on the player's own machine** (REQUIREMENTS.md
// section 5, decided 2026-09-30): a language model asked by the player's
// client with the player's own key, which never leaves it. What it answers is
// checked here as it is anywhere (copilot/copilot.hpp) and sent to the
// server as a route (`COPILOT_ROUTE`) - an input, which the server checks
// again against its own aircraft and flies with its own AI pilot, or refuses.
//
// **What it is told** comes from the server's state updates: where the
// aircraft is, its heading, its speed over the ground (the updates carry no
// airspeed) and its vertical speed; the ground under it from the DEM and its
// height above the sea from the geoid, both as this machine has them; the
// runways near it; and the route it last sent. **Not whether the engine
// runs**, which no update says: a player's copilot is told it does.
//
// **When it is asked**: when the player asks (`ask`), and then a minute after
// each answer taken, if `routine_s` is set. Its answer is taken `thinking_s`
// seconds after it was asked on the session's clock, or when it comes if that
// is later, and never waits for it.

#include "copilot/copilot.hpp"
#include "net/messages.hpp"
#include "net/state.hpp"
#include "world/runways.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::frontend {

struct PlayersCopilotOptions {
    std::string aircraft; // the catalogue's id, "c172p"
    std::string task;     // what the player asked
    std::string provider = "openai";
    std::string model;    // the provider's default when empty
    std::string record;   // keep what was asked and answered here
    std::string playback; // or ask nothing, and play this back
    double thinking_s = 10.0;
    double routine_s = 0.0; // 0: asked only when the player asks
};

class PlayersCopilot {
public:
    // Throws copilot::ProviderError for a provider without its key: refused,
    // never faked.
    PlayersCopilot(const std::filesystem::path& data, PlayersCopilotOptions options);
    ~PlayersCopilot();
    PlayersCopilot(const PlayersCopilot&) = delete;
    PlayersCopilot& operator=(const PlayersCopilot&) = delete;

    // The player asks: at the next look.
    void ask() {
        wanted_ = "the pilot has asked";
    }

    // **A look, between two frames**, with the newest update's clock and the
    // player's own aircraft in it. Returns a route to send when an answer is
    // due and changes the route. Never waits for the model.
    std::optional<net::CopilotRoute> look(double simulation_s, const net::AircraftState& own);

    // What has happened since last asked, a line each, for the log.
    std::vector<std::string> said();

    int questions() const {
        return questions_;
    }
    int answers() const {
        return answers_;
    }
    std::string provider() const;

private:
    copilot::Situation situation(double simulation_s, const net::AircraftState& own,
                                 const std::string& event) const;

    PlayersCopilotOptions o_;
    std::unique_ptr<copilot::Copilot> helper_;
    std::string provider_;
    // The DEM and the geoid, as this machine has them.
    struct Ground;
    std::unique_ptr<Ground> ground_;
    std::vector<world::RunwayEnd> runways_;
    std::optional<std::string> wanted_;
    std::optional<double> asked_at_s_;
    std::optional<double> answered_at_s_;
    std::vector<sim::Waypoint> route_;
    std::vector<std::string> said_;
    int questions_ = 0;
    int answers_ = 0;
};

} // namespace glideslope::frontend
