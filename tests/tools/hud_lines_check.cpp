// hud_lines_check - the HUD's lines say who is flying and where every control
// is, in every case, word for word.
//
//   glideslope_hud_lines_check
//
// The frame tests read the HUD back out of a shot and hold it to the flight
// model, but a shot only has what its flight had: the flaps up, and the gear
// down or fixed. This walks what they cannot reach - flaps part and all the
// way down, the gear up, down and fixed, and each way of saying who is flying -
// through gfx::hud_lines, and holds every line to what it must say. Every case
// is counted, and the count held to the number there are. Exits 0 if all is
// well, 1 with the first case wrong.

#include "gfx/hud.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace {

struct Case {
    const char* what;
    bool ai;
    std::string autopilot;
    glideslope::gfx::ControlsShown controls;
    std::vector<std::string> said; // the lines after the flight's own six
};

} // namespace

int main() {
    glideslope::gfx::ControlsShown cruise;
    cruise.aileron = 0.3;
    cruise.elevator = -0.05;
    cruise.rudder = 0.0;
    cruise.throttle = 0.7;
    cruise.flaps = 0.0;
    cruise.gear = 0.0;
    glideslope::gfx::ControlsShown landing = cruise;
    landing.aileron = -1.0;
    landing.elevator = 1.0;
    landing.rudder = -0.25;
    landing.throttle = 0.0;
    landing.flaps = 1.0;
    landing.gear = 1.0;
    glideslope::gfx::ControlsShown fixed = cruise;
    fixed.flaps = 0.33;
    fixed.gear.reset();

    const std::vector<Case> cases = {
        {"the pilot, gear up", false, "", cruise,
         {"FLYING PILOT", "STICK +0.30 -0.05", "RUDDER +0.00", "THROTTLE 0.70", "FLAPS 0.00",
          "GEAR UP"}},
        {"the AI holding, flaps and gear down", true, "HOLD", landing,
         {"FLYING AI HOLD", "STICK -1.00 +1.00", "RUDDER -0.25", "THROTTLE 0.00",
          "FLAPS 1.00", "GEAR DOWN"}},
        {"the AI to a waypoint, fixed gear, flaps part down", true, "NAV THE_HEADS", fixed,
         {"FLYING AI NAV THE HEADS", "STICK +0.30 -0.05", "RUDDER +0.00", "THROTTLE 0.70",
          "FLAPS 0.33"}},
    };
    std::size_t walked = 0;
    for (const Case& c : cases) {
        glideslope::gfx::HudReadings r;
        r.ai_flying = c.ai;
        r.autopilot = c.autopilot;
        r.controls = c.controls;
        const std::vector<std::string> lines = glideslope::gfx::hud_lines(r);
        // The flight's six first - speed, altitude, heading, climb, pitch,
        // bank - then who is flying and the controls, and nothing more.
        if (lines.size() != 6 + c.said.size()) {
            std::fprintf(stderr, "%s: %zu lines, not %zu\n", c.what, lines.size(),
                         6 + c.said.size());
            return 1;
        }
        for (std::size_t i = 0; i < c.said.size(); ++i) {
            if (lines[6 + i] != c.said[i]) {
                std::fprintf(stderr, "%s: line %zu says \"%s\", not \"%s\"\n", c.what, 7 + i,
                             lines[6 + i].c_str(), c.said[i].c_str());
                return 1;
            }
        }
        std::printf("%s: as it should be\n", c.what);
        ++walked;
    }
    // Without an aircraft to read them from, no controls at all.
    glideslope::gfx::HudReadings none;
    if (glideslope::gfx::hud_lines(none).size() != 7) {
        std::fprintf(stderr, "with no controls to show, the HUD showed some\n");
        return 1;
    }
    ++walked;
    if (walked != cases.size() + 1) {
        std::fprintf(stderr, "%zu cases of %zu were walked\n", walked, cases.size() + 1);
        return 1;
    }
    std::printf("all %zu cases of who is flying and the controls read as they should\n",
                walked);
    return 0;
}
