#include "harness.hpp"

#include "platform/input.hpp"
#include "sim/aircraft.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

using glideslope::platform::Binding;
using glideslope::platform::ControlMapper;
using glideslope::platform::DeviceKind;
using glideslope::platform::DeviceState;
using glideslope::platform::InputError;
using glideslope::platform::Joysticks;
using glideslope::sim::Controls;
using glideslope::test::check;
using glideslope::test::fail;

namespace {

std::vector<Binding> committed_bindings() {
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                         "../assets/input/bindings.txt",
                     std::ios::binary);
    check(static_cast<bool>(in), "can read assets/input/bindings.txt");
    return glideslope::platform::parse_bindings(
        std::string(std::istreambuf_iterator<char>(in), {}));
}

// Every field of the controls, named, for comparing.
std::vector<std::pair<const char*, double>> fields(const Controls& c) {
    return {{"elevator", c.elevator},     {"aileron", c.aileron},
            {"rudder", c.rudder},         {"throttle", c.throttle},
            {"mixture", c.mixture},       {"propeller", c.propeller},
            {"flaps", c.flaps},           {"left_brake", c.left_brake},
            {"right_brake", c.right_brake}, {"pitch_trim", c.pitch_trim}};
}

std::string differences(const Controls& a, const Controls& b) {
    std::string out;
    const auto fa = fields(a);
    const auto fb = fields(b);
    for (std::size_t i = 0; i < fa.size(); ++i) {
        if (fa[i].second != fb[i].second) {
            out += std::string(fa[i].first) + " " + std::to_string(fa[i].second) +
                   "->" + std::to_string(fb[i].second) + " ";
        }
    }
    return out;
}

// A virtual flight controller, attached for as long as it lives.
class VirtualDevice {
public:
    VirtualDevice(SDL_JoystickType type, const char* name, int axes, int buttons,
                  int hats) {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = static_cast<Uint16>(type);
        desc.naxes = static_cast<Uint16>(axes);
        desc.nbuttons = static_cast<Uint16>(buttons);
        desc.nhats = static_cast<Uint16>(hats);
        desc.name = name;
        id_ = SDL_AttachVirtualJoystick(&desc);
        check(id_ != 0,
              std::string("SDL attaches a virtual device: ") + SDL_GetError());
        joystick_ = SDL_OpenJoystick(id_);
        check(joystick_ != nullptr, std::string("and opens it: ") + SDL_GetError());
    }
    ~VirtualDevice() {
        SDL_CloseJoystick(joystick_);
        SDL_DetachVirtualJoystick(id_);
    }
    void axis(int i, double v) {
        SDL_SetJoystickVirtualAxis(joystick_, i,
                                   static_cast<Sint16>(std::lround(v * 32767)));
    }
    void button(int i, bool down) {
        SDL_SetJoystickVirtualButton(joystick_, i, down);
    }
    void hat(int i, Uint8 v) {
        SDL_SetJoystickVirtualHat(joystick_, i, v);
    }

private:
    SDL_JoystickID id_ = 0;
    SDL_Joystick* joystick_ = nullptr;
};

struct Sdl {
    Sdl() {
        check(SDL_Init(SDL_INIT_JOYSTICK),
              std::string("SDL's joysticks start: ") + SDL_GetError());
    }
    ~Sdl() {
        SDL_Quit();
    }
};

} // namespace

GLIDESLOPE_TEST(
    every_axis_button_and_hat_of_a_virtual_stick_and_throttle_moves_the_controls) {
    const Sdl sdl;
    ControlMapper mapper(committed_bindings());
    Joysticks joysticks;
    int walked = 0;

    for (const auto& [type, kind, name] :
         {std::tuple{SDL_JOYSTICK_TYPE_FLIGHT_STICK, DeviceKind::flight_stick,
                     "a yoke"},
          std::tuple{SDL_JOYSTICK_TYPE_THROTTLE, DeviceKind::throttle,
                     "a HOTAS throttle"}}) {
        VirtualDevice device(type, name, 8, 16, 1);
        std::vector<DeviceState> states = joysticks.read();
        check(states.size() == 1 && states[0].kind == kind &&
                  states[0].axes.size() == 8 && states[0].buttons.size() == 16 &&
                  states[0].hats.size() == 1,
              std::string(name) +
                  " is read with its 8 axes, 16 buttons and hat, as its kind");
        check(mapper.unbound(states[0]).empty(),
              std::string(name) + ": every input is bound");

        // Each input alone, from a neutral start: it must move a control.
        const auto walk = [&](const std::string& what,
                              const std::function<void()>& press,
                              const std::function<void()>& release) {
            Controls controls;
            controls.flaps = 0.5;      // room to step both ways
            controls.pitch_trim = 0.0; // and here
            controls.throttle = 0.5;
            controls.mixture = 0.5;
            controls.propeller = 0.5; // and here: it rests at 1, with no room up
            mapper.apply(joysticks.read(), controls);
            const Controls before = controls;
            press();
            mapper.apply(joysticks.read(), controls);
            const std::string moved = differences(before, controls);
            if (moved.empty()) {
                fail(std::string(name) + " " + what + " moved no control");
            }
            release();
            mapper.apply(joysticks.read(), controls);
            ++walked;
        };
        for (int a = 0; a < 8; ++a) {
            walk(
                "axis " + std::to_string(a), [&] { device.axis(a, 0.6); },
                [&] { device.axis(a, 0.0); });
        }
        for (int b = 0; b < 16; ++b) {
            walk(
                "button " + std::to_string(b), [&] { device.button(b, true); },
                [&] { device.button(b, false); });
        }
        for (const Uint8 d : {Uint8{SDL_HAT_UP}, Uint8{SDL_HAT_RIGHT},
                              Uint8{SDL_HAT_DOWN}, Uint8{SDL_HAT_LEFT}}) {
            walk(
                "hat direction " + std::to_string(d), [&] { device.hat(0, d); },
                [&] { device.hat(0, SDL_HAT_CENTERED); });
        }
    }
    check(walked == 2 * (8 + 16 + 4), "all 56 inputs of both devices were walked");
}

GLIDESLOPE_TEST(
    bound_inputs_set_the_controls_they_name_and_the_aircraft_receives_them) {
    const Sdl sdl;
    ControlMapper mapper(committed_bindings());
    Joysticks joysticks;
    VirtualDevice stick(SDL_JOYSTICK_TYPE_FLIGHT_STICK, "a stick", 8, 16, 1);

    // The stick right and forward, the throttle lever at three quarters
    // forward, the brake held, flaps down two notches.
    stick.axis(0, 0.5);
    stick.axis(1, -0.25);
    stick.axis(2, -0.5);
    stick.axis(3, 0.2);
    stick.axis(4, -1.0);
    stick.axis(5, -1.0);
    stick.axis(6, -1.0);
    stick.axis(7, -1.0);
    Controls controls;
    mapper.apply(joysticks.read(), controls);
    for (int notch = 0; notch < 2; ++notch) {
        stick.button(2, true);
        mapper.apply(joysticks.read(), controls);
        stick.button(2, false);
        mapper.apply(joysticks.read(), controls);
    }
    stick.button(0, true);
    stick.hat(0, SDL_HAT_DOWN);
    mapper.apply(joysticks.read(), controls);

    const auto near = [](double a, double b) { return std::abs(a - b) < 1e-4; };
    check(near(controls.aileron, 0.5),
          "aileron right 0.5: " + std::to_string(controls.aileron));
    check(near(controls.elevator, -0.25), "stick forward is nose down");
    check(near(controls.throttle, 0.75),
          "the lever three quarters forward is 0.75 throttle");
    check(near(controls.rudder, 0.2), "rudder 0.2");
    check(near(controls.mixture, 1.0), "mixture lever fully forward is rich");
    check(near(controls.flaps, 2.0 / 3.0),
          "two notches of flap: " + std::to_string(controls.flaps));
    check(controls.left_brake == 1.0 && controls.right_brake == 1.0,
          "both brakes held");
    check(near(controls.pitch_trim, 0.05), "the hat trims nose up");
    stick.button(0, false);
    mapper.apply(joysticks.read(), controls);
    check(controls.left_brake == 0.0 && controls.right_brake == 0.0,
          "the brakes let go");

    // What the mapper set is what JSBSim is given.
    glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    glideslope::sim::InitialConditions ic;
    ic.altitude_ft = 3000;
    ic.airspeed_kts = 100;
    aircraft.initialize(ic);
    controls.left_brake = 0.7;
    aircraft.set_controls(controls);
    check(near(aircraft.property("fcs/aileron-cmd-norm"), 0.5) &&
              near(aircraft.property("fcs/elevator-cmd-norm"), 0.25) &&
              near(aircraft.property("fcs/rudder-cmd-norm"), 0.2) &&
              near(aircraft.property("fcs/throttle-cmd-norm[0]"), 0.75) &&
              near(aircraft.property("fcs/mixture-cmd-norm[0]"), 1.0) &&
              near(aircraft.property("fcs/flap-cmd-norm"), 2.0 / 3.0) &&
              near(aircraft.property("fcs/left-brake-cmd-norm"), 0.7) &&
              near(aircraft.property("fcs/pitch-trim-cmd-norm"), -0.05),
          "every control reaches its JSBSim command, with the elevator and trim signs "
          "JSBSim uses");
}

GLIDESLOPE_TEST(a_bindings_file_that_cannot_be_read_is_refused_by_line) {
    const auto refused = [](const char* text, const char* says) {
        try {
            glideslope::platform::parse_bindings(text);
        } catch (const InputError& e) {
            return std::string(e.what()).find(says) != std::string::npos;
        }
        return false;
    };
    check(refused("joystick axis 0 aileron centred 1", "line 1: no device kind"),
          "unknown kind");
    check(refused("\n\nflight_stick axis 0 ailerons centred 1", "line 3: no control"),
          "unknown control");
    check(refused("flight_stick button 0 brakes centred 1", "an axis is centred"),
          "a button cannot be centred");
    check(refused("flight_stick hat 0 sideways flaps step 1", "a hat needs up"),
          "no such hat way");
    check(refused("flight_stick axis 0 aileron lever", "lever needs an amount"),
          "no amount");
    check(refused("flight_stick axis 0 aileron lever 1 2", "more than a binding"),
          "extra words");
    check(glideslope::platform::parse_bindings("# only a comment\n\n").empty(),
          "comments and blank lines are nothing");
}
