#pragma once

// Flight controllers - joysticks, yokes, HOTAS throttles - to the aircraft's
// controls.
//
// Two halves, so that each can be tested alone: Joysticks reads every device
// SDL can see into plain DeviceStates, and ControlMapper turns DeviceStates into
// sim::Controls by bindings read from data (assets/input/bindings.txt).

#include "sim/aircraft.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::platform {

struct InputError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// What kind of flight controller SDL says a device is. Sticks and yokes, and
// anything SDL cannot name, are flight_stick.
enum class DeviceKind { flight_stick, throttle };

enum class Control {
    aileron,
    elevator,
    rudder,
    pitch_trim,
    throttle,
    mixture,
    // The rpm lever of a constant-speed propeller, and the pitch lever of a
    // two-pitch one: 0 its lowest rpm or coarse pitch, 1 its highest or fine.
    propeller,
    flaps,
    left_brake,
    right_brake,
    brakes, // both
};

enum class Source { axis, button, hat };

enum class Mode { centred, lever, hold, step };

// SDL's hat directions, as bits.
enum HatDirection : std::uint8_t {
    hat_up = 1,
    hat_right = 2,
    hat_down = 4,
    hat_left = 8
};

struct Binding {
    DeviceKind device = DeviceKind::flight_stick;
    Source source = Source::axis;
    int index = 0;
    std::uint8_t hat_direction = 0;
    Control control = Control::aileron;
    Mode mode = Mode::centred;
    double amount = 1.0; // the scale for an axis, the step for a press
};

// Parses a bindings file. Throws InputError naming the line of anything it
// cannot read.
std::vector<Binding> parse_bindings(std::string_view text);

struct DeviceState {
    DeviceKind kind = DeviceKind::flight_stick;
    std::string name;
    std::vector<double> axes; // -1 .. 1
    std::vector<bool> buttons;
    std::vector<std::uint8_t> hats; // HatDirection bits
};

class ControlMapper {
public:
    explicit ControlMapper(std::vector<Binding> bindings);

    // Applies the devices to `controls`. An axis sets its control when it moves,
    // and when first read - not every call, so a lever left alone does not undo
    // what a button did to the same control. A button bound to hold holds its
    // control at 1 and lets it back to 0 when released; a press - of a button,
    // or a hat into a direction - steps its control once. Devices are told
    // apart by their place in `devices`, which should not change from one call
    // to the next.
    void apply(const std::vector<DeviceState>& devices, sim::Controls& controls);

    // The inputs of `device` that no binding reaches: "axis 9", "button 17",
    // "hat 0 up".
    std::vector<std::string> unbound(const DeviceState& device) const;

private:
    std::vector<Binding> bindings_;
    std::vector<DeviceState> previous_;
};

// **The keyboard, beside any flight controller.** The arrows fly it, Z and X
// work the rudder, Page Up and Page Down the throttle, the comma and full
// stop the mixture, and the square brackets the propeller; B holds the
// brakes.
//
// A key moves its control while held and lets it go when released, so a stick
// left alone is not overridden every frame. **The levers hold where they are
// left**, as the throttle does: a mixture that sprang back to rich the moment
// the key came up would be no use for leaning. An aeroplane with no propeller
// lever or no mixture ignores them.
//
// It is given the key state rather than asking for it, so that it can be
// worked without a window - which is how it is tested.
class KeyboardControls {
public:
    // `keys` is SDL's keyboard state, indexed by scancode, of `count` entries.
    void apply(sim::Controls& controls, double seconds, const bool* keys,
               int count);

private:
    bool elevator_ = false;
    bool aileron_ = false;
    bool rudder_ = false;
    bool brakes_ = false;
};

// Every flight controller SDL can see, opened as they appear. Needs SDL's
// joystick subsystem.
class Joysticks {
public:
    Joysticks();
    ~Joysticks();
    Joysticks(const Joysticks&) = delete;
    Joysticks& operator=(const Joysticks&) = delete;

    // The state of every device now connected, in the order SDL lists them.
    std::vector<DeviceState> read();

private:
    struct Open;
    std::map<std::uint32_t, std::unique_ptr<Open>> open_;
};

} // namespace glideslope::platform
