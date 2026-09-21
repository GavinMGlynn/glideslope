#include "harness.hpp"

#include "platform/input.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <utility>
#include <filesystem>
#include <string>
#include <vector>

using glideslope::platform::Control;
using glideslope::platform::ControlMapper;
using glideslope::platform::KeyboardControls;
using glideslope::sim::Controls;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;
constexpr int key_count = 512;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// The keys are kept in a plain array rather than a std::vector<bool>, which
// is bit-packed and has no bool* to hand to SDL's shape of interface.
struct Keys {
    bool held[key_count] = {};
    void down(SDL_Scancode code) { held[static_cast<std::size_t>(code)] = true; }
    void up(SDL_Scancode code) { held[static_cast<std::size_t>(code)] = false; }
};

// Where a lever gets to after `seconds` of a key being held.
double after(SDL_Scancode code, double from, double Controls::*lever, double seconds) {
    KeyboardControls keyboard;
    Controls c;
    c.*lever = from;
    Keys keys;
    keys.down(code);
    const int steps = static_cast<int>(seconds * steps_per_second);
    for (int i = 0; i < steps; ++i) {
        keyboard.apply(c, 1.0 / steps_per_second, keys.held, key_count);
    }
    return c.*lever;
}

// The aircraft's engine speed after flying `seconds` with these controls,
// starting from its catalogue start in the air.
double rpm_after(const std::string& id, const Controls& flying, double seconds,
                 double& kcas) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 5000.0;
    ic.airspeed_kts = entry.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    aircraft.initialize(ic);
    const int steps = static_cast<int>(seconds * steps_per_second);
    for (int i = 0; i < steps; ++i) {
        aircraft.set_controls(flying);
        aircraft.step();
    }
    kcas = aircraft.state().airspeed_kts;
    return aircraft.property("propulsion/engine[0]/engine-rpm");
}

} // namespace

// **Every lever the keyboard names is moved by its keys, and stays where it
// is put.** A lever that sprang back when the key came up would be no use for
// leaning a mixture or coarsening a propeller.
GLIDESLOPE_TEST(the_keyboard_moves_every_lever_it_names_and_leaves_it_where_it_is_put) {
    struct Lever {
        std::string name;
        double Controls::*field;
        SDL_Scancode less;
        SDL_Scancode more;
    };
    const std::vector<Lever> levers{
        {"throttle", &Controls::throttle, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_PAGEUP},
        {"mixture", &Controls::mixture, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD},
        {"propeller", &Controls::propeller, SDL_SCANCODE_LEFTBRACKET,
         SDL_SCANCODE_RIGHTBRACKET}};

    std::size_t walked = 0;
    for (const Lever& lever : levers) {
        // Down from the middle, and up from it, a second of holding each.
        const double down = after(lever.less, 0.5, lever.field, 1.0);
        const double up = after(lever.more, 0.5, lever.field, 1.0);
        check(down < 0.5, lever.name + " goes down: " + std::to_string(down));
        check(up > 0.5, lever.name + " goes up: " + std::to_string(up));

        // It stops at its ends rather than running past them.
        check(after(lever.less, 0.5, lever.field, 10.0) == 0.0,
              lever.name + " stops at nought");
        check(after(lever.more, 0.5, lever.field, 10.0) == 1.0,
              lever.name + " stops at one");

        // And it stays where it was put once the key comes up.
        KeyboardControls keyboard;
        Controls c;
        c.*lever.field = 0.5;
        Keys keys;
        keys.down(lever.more);
        for (int i = 0; i < steps_per_second; ++i) {
            keyboard.apply(c, 1.0 / steps_per_second, keys.held, key_count);
        }
        const double left_at = c.*lever.field;
        keys.up(lever.more);
        for (int i = 0; i < 2 * steps_per_second; ++i) {
            keyboard.apply(c, 1.0 / steps_per_second, keys.held, key_count);
        }
        check(c.*lever.field == left_at,
              lever.name + " stayed at " + std::to_string(left_at) +
                  " when the key came up, and is now " +
                  std::to_string(c.*lever.field));
        ++walked;
    }
    check(walked == levers.size(), "every lever the keyboard names was walked");
    check(walked == 3, "there are three of them, not " + std::to_string(walked));
}

// **And a binding moves each of them too.** The propeller is the one that had
// none at all; this holds the whole set, so that losing one is noticed.
GLIDESLOPE_TEST(a_binding_moves_every_lever_the_controls_have) {
    std::ifstream in(data() / "input" / "bindings.txt", std::ios::binary);
    check(in.good(), "the bindings file is there");
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto bindings = glideslope::platform::parse_bindings(text);

    const std::vector<std::pair<Control, std::string>> levers{
        {Control::throttle, "throttle"},
        {Control::mixture, "mixture"},
        {Control::propeller, "propeller"}};
    std::size_t walked = 0;
    for (const auto& [control, name] : levers) {
        std::size_t bound = 0;
        for (const auto& b : bindings) {
            bound += b.control == control ? 1u : 0u;
        }
        check(bound > 0, name + " is bound to something");
        ++walked;
    }
    check(walked == 3, "every lever was looked for");

    // And the binding actually reaches the control: the quadrant's second
    // lever is the propeller, as the file says.
    ControlMapper mapper(bindings);
    glideslope::platform::DeviceState quadrant;
    quadrant.kind = glideslope::platform::DeviceKind::throttle;
    quadrant.axes.assign(8, 0.0);
    quadrant.buttons.assign(16, false);
    quadrant.hats.assign(1, 0);
    Controls c;
    quadrant.axes[1] = -1.0; // a lever forward reads -1
    mapper.apply({quadrant}, c);
    check(std::abs(c.propeller - 1.0) < 1e-9,
          "the quadrant's second lever forward puts the propeller at one, not " +
              std::to_string(c.propeller));
    quadrant.axes[1] = 1.0;
    mapper.apply({quadrant}, c);
    check(std::abs(c.propeller) < 1e-9,
          "and back puts it at nought, not " + std::to_string(c.propeller));
}

// **The Short S.23's airscrews in coarse pitch keep its engines inside their
// rating.** Flown with the levers where the flight deck would have them -
// coarse pitch, the mixture at NORMAL rather than through the gate to
// take-off boost - the Pegasus turns within the 2,600 rpm it is rated at.
// Before there was a lever to move, a pilot flew it in fine pitch through the
// gate and it turned 3,185.
GLIDESLOPE_TEST(the_short_s23_in_coarse_pitch_at_normal_boost_turns_within_its_rated_rpm) {
    Controls cruising;
    cruising.throttle = 1.0;
    cruising.mixture = 0.5;   // NORMAL, not through the gate
    cruising.propeller = 0.0; // COARSE
    double kcas = 0.0;
    const double rpm = rpm_after("short_s23", cruising, 90.0, kcas);
    std::printf("  short_s23 coarse, NORMAL: %.0f rpm at %.0f knots\n", rpm, kcas);
    check(rpm > 100.0, "the engines are turning at all");
    check(rpm <= 2600.0,
          "the Pegasus is rated at 2,600 rpm and turns " + std::to_string(rpm) +
              " in coarse pitch at normal boost");

    // And in fine pitch through the gate it turns faster, which is what the
    // lever is for: if these were the same the lever would do nothing.
    Controls climbing = cruising;
    climbing.mixture = 1.0;   // through the gate
    climbing.propeller = 1.0; // FINE
    double fine_kcas = 0.0;
    const double fine = rpm_after("short_s23", climbing, 90.0, fine_kcas);
    std::printf("  short_s23 fine, through the gate: %.0f rpm at %.0f knots\n", fine,
                fine_kcas);
    check(fine > rpm + 50.0,
          "fine pitch through the gate turns faster than coarse at normal: " +
              std::to_string(fine) + " against " + std::to_string(rpm));
}

// **The Mosquito's rpm follows its lever.** A constant-speed airscrew governs
// to the rpm the lever asks for, so moving the lever must move the engine.
GLIDESLOPE_TEST(the_mosquitos_rpm_follows_its_propeller_lever) {
    Controls flying;
    flying.throttle = 0.7;
    flying.mixture = 1.0;

    double kcas = 0.0;
    flying.propeller = 1.0;
    const double high = rpm_after("mosquito-fb6", flying, 60.0, kcas);
    flying.propeller = 0.4;
    const double low = rpm_after("mosquito-fb6", flying, 60.0, kcas);
    std::printf("  mosquito-fb6: lever at 1.0 gives %.0f rpm, at 0.4 gives %.0f\n",
                high, low);

    check(high > 100.0 && low > 100.0, "the engines are turning at both settings");
    check(high > low + 100.0,
          "the lever forward turns the airscrew faster: " + std::to_string(high) +
              " against " + std::to_string(low));
}
