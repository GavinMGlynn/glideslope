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
// runways near it; the route it last sent; and whether the engine runs, from
// the aircraft's condition (`net::Condition::engine_stopped`).
//
// **Nothing here holds up the thread that calls it.** Its ground - a geoid,
// a DEM and the world's runways, which may be downloaded - is made on a
// thread of its own as it is made, and what it is told is worked out on the
// question's own thread (copilot::Copilot::ask): a DEM of its own, not the
// client's, because a `world::Dem` is not for two threads, and the tiles
// are shared through the cache on disk.
//
// **It flies only what the player gave it.** The player asks (`ask`): the
// copilot is engaged, and its route, sent, has the server hand the aircraft
// to its AI. From then on it looks again `routine_s` after each answer
// **while the server says the AI flies the aircraft**, and as soon as it can
// when the engine stops, whoever flies it. Taken back by the player, it stands by: nothing more is asked,
// an answer still to come is not sent, and it is engaged again only when the
// player asks again. Its answer is taken `thinking_s` seconds after it was
// asked on the session's clock, or when it comes if that is later, and never
// waits for it.
//
// **Going away is not held up**: the model's request is abandoned and every
// fetch of its ground given up (world::FetchesGivenUp), and each is waited
// for only as long as giving up takes. **Its ground not had** as it is made -
// a fetch or a file that fails - it says so once and is gone for the
// session; what one question is told not worked out - a DEM tile not
// fetched, say - is that question unanswered, and asked again.
//
// **The route it knows is the one it sent**: the server says nothing back
// of a route it refused, so a route this side took and the server did not
// is taken here as flown - which the checks being the same on both sides,
// with the same figures and the same DEM, makes rare.

#include "copilot/copilot.hpp"
#include "net/messages.hpp"
#include "net/state.hpp"
#include "world/runways.hpp"

#include <atomic>
#include <filesystem>
#include <future>
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

    // The player asks: at the next look, and engaged from then.
    void ask() {
        wanted_ = "the pilot has asked";
        engaged_ = true;
    }
    bool engaged() const {
        return engaged_;
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
    struct Ground;
    // Worked out on the question's thread: see above.
    copilot::Situation situation(double simulation_s, const net::AircraftState& own,
                                 const std::string& event, std::vector<sim::Waypoint> route,
                                 const std::shared_ptr<Ground>& ground);

    PlayersCopilotOptions o_;
    std::unique_ptr<copilot::Copilot> helper_;
    std::string provider_;
    // The DEM, the geoid and the runways, as this machine has them: made on
    // a thread of their own, and used only on the question's.
    std::shared_future<std::shared_ptr<Ground>> ground_;
    bool engaged_ = false;
    bool gone_ = false; // its ground could not be had: see look()
    std::string asked_about_; // the question out, or last asked
    int asked_again_ = 0;
    std::atomic<bool> going_{false};
    bool ai_flying_ = false;
    bool engine_said_ = false;
    std::optional<std::string> wanted_;
    std::optional<double> asked_at_s_;
    std::optional<double> answered_at_s_;
    std::vector<sim::Waypoint> route_;
    std::vector<std::string> said_;
    int questions_ = 0;
    int answers_ = 0;
};

} // namespace glideslope::frontend
