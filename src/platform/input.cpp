#include "platform/input.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>

namespace glideslope::platform {

namespace {

const std::map<std::string, Control>& control_names() {
    static const std::map<std::string, Control> names{
        {"aileron", Control::aileron},
        {"elevator", Control::elevator},
        {"rudder", Control::rudder},
        {"pitch_trim", Control::pitch_trim},
        {"throttle", Control::throttle},
        {"mixture", Control::mixture},
        {"propeller", Control::propeller},
        {"flaps", Control::flaps},
        {"left_brake", Control::left_brake},
        {"right_brake", Control::right_brake},
        {"brakes", Control::brakes},
        {"speedbrake", Control::speedbrake}};
    return names;
}

const std::map<std::string, std::uint8_t>& hat_names() {
    static const std::map<std::string, std::uint8_t> names{
        {"up", hat_up}, {"right", hat_right}, {"down", hat_down}, {"left", hat_left}};
    return names;
}

bool centred_control(Control c) {
    return c == Control::aileron || c == Control::elevator || c == Control::rudder ||
           c == Control::pitch_trim;
}

// The controls a binding moves: `brakes` is both.
std::vector<double*> fields(Control c, sim::Controls& controls) {
    switch (c) {
    case Control::aileron: return {&controls.aileron};
    case Control::elevator: return {&controls.elevator};
    case Control::rudder: return {&controls.rudder};
    case Control::pitch_trim: return {&controls.pitch_trim};
    case Control::throttle: return {&controls.throttle};
    case Control::mixture: return {&controls.mixture};
    case Control::propeller: return {&controls.propeller};
    case Control::flaps: return {&controls.flaps};
    case Control::left_brake: return {&controls.left_brake};
    case Control::right_brake: return {&controls.right_brake};
    case Control::brakes: return {&controls.left_brake, &controls.right_brake};
    case Control::speedbrake: return {&controls.speedbrake};
    }
    return {};
}

double clamp_control(Control c, double v) {
    return centred_control(c) ? std::clamp(v, -1.0, 1.0) : std::clamp(v, 0.0, 1.0);
}

double axis_value(const Binding& b, double v) {
    return b.mode == Mode::centred ? v * b.amount : (v * b.amount + 1.0) / 2.0;
}

std::string hat_name(std::uint8_t direction) {
    for (const auto& [name, bit] : hat_names()) {
        if (bit == direction) {
            return name;
        }
    }
    return "?";
}

} // namespace

std::vector<Binding> parse_bindings(std::string_view text) {
    std::vector<Binding> bindings;
    std::istringstream lines{std::string(text)};
    int number = 0;
    for (std::string line; std::getline(lines, line);) {
        ++number;
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line.erase(hash);
        }
        std::istringstream words(line);
        std::vector<std::string> w;
        for (std::string word; words >> word;) {
            w.push_back(word);
        }
        if (w.empty()) {
            continue;
        }
        const auto bad = [&](const std::string& why) {
            return InputError("bindings line " + std::to_string(number) + ": " + why);
        };
        Binding b;
        if (w[0] == "flight_stick") {
            b.device = DeviceKind::flight_stick;
        } else if (w[0] == "throttle") {
            b.device = DeviceKind::throttle;
        } else {
            throw bad("no device kind " + w[0]);
        }
        if (w.size() < 3) {
            throw bad("too short");
        }
        char* end = nullptr;
        const long index = std::strtol(w[2].c_str(), &end, 10);
        if (*end != '\0' || index < 0 || index > 255) {
            throw bad("no input number " + w[2]);
        }
        b.index = static_cast<int>(index);
        std::size_t at = 3;
        if (w[1] == "axis") {
            b.source = Source::axis;
        } else if (w[1] == "button") {
            b.source = Source::button;
        } else if (w[1] == "hat") {
            b.source = Source::hat;
            if (w.size() < 4 || hat_names().count(w[3]) == 0) {
                throw bad("a hat needs up, down, left or right");
            }
            b.hat_direction = hat_names().at(w[3]);
            at = 4;
        } else {
            throw bad("no input kind " + w[1]);
        }
        if (w.size() <= at + 1 || control_names().count(w[at]) == 0) {
            throw bad("no control " + (w.size() > at ? w[at] : std::string()));
        }
        b.control = control_names().at(w[at]);
        const std::string& mode = w[at + 1];
        std::size_t expected = at + 2;
        if (mode == "centred" || mode == "lever" || mode == "step") {
            b.mode = mode == "centred" ? Mode::centred
                     : mode == "lever" ? Mode::lever
                                       : Mode::step;
            if (w.size() < at + 3) {
                throw bad(mode + " needs an amount");
            }
            b.amount = std::strtod(w[at + 2].c_str(), &end);
            if (*end != '\0') {
                throw bad("no amount " + w[at + 2]);
            }
            expected = at + 3;
        } else if (mode == "hold") {
            b.mode = Mode::hold;
        } else {
            throw bad("no mode " + mode);
        }
        if (w.size() != expected) {
            throw bad("more than a binding");
        }
        const bool axis_mode = b.mode == Mode::centred || b.mode == Mode::lever;
        if ((b.source == Source::axis) != axis_mode) {
            throw bad(
                "an axis is centred or a lever, and a button or hat holds or steps");
        }
        if (b.source == Source::hat && b.mode != Mode::step) {
            throw bad("a hat steps");
        }
        bindings.push_back(b);
    }
    return bindings;
}

ControlMapper::ControlMapper(std::vector<Binding> bindings)
    : bindings_(std::move(bindings)) {}

void ControlMapper::apply(const std::vector<DeviceState>& devices,
                          sim::Controls& controls) {
    for (std::size_t d = 0; d < devices.size(); ++d) {
        const DeviceState& device = devices[d];
        const DeviceState* before = d < previous_.size() ? &previous_[d] : nullptr;
        for (const Binding& b : bindings_) {
            if (b.device != device.kind) {
                continue;
            }
            const auto index = static_cast<std::size_t>(b.index);
            switch (b.source) {
            case Source::axis: {
                // Only when it moves - or is first read - so that a lever left
                // alone does not undo what buttons do to the same control.
                if (index >= device.axes.size()) {
                    break;
                }
                const bool moved = before == nullptr || index >= before->axes.size() ||
                                   before->axes[index] != device.axes[index];
                if (moved) {
                    for (double* f : fields(b.control, controls)) {
                        *f =
                            clamp_control(b.control, axis_value(b, device.axes[index]));
                    }
                }
                break;
            }
            case Source::button: {
                if (index >= device.buttons.size()) {
                    break;
                }
                const bool down = device.buttons[index];
                const bool was = before != nullptr && index < before->buttons.size() &&
                                 before->buttons[index];
                for (double* f : fields(b.control, controls)) {
                    if (b.mode == Mode::hold && down) {
                        *f = clamp_control(b.control, 1.0);
                    } else if (b.mode == Mode::hold && was) {
                        *f = clamp_control(b.control, 0.0); // let go
                    } else if (b.mode == Mode::step && down && !was) {
                        *f = clamp_control(b.control, *f + b.amount);
                    }
                }
                break;
            }
            case Source::hat: {
                if (index >= device.hats.size()) {
                    break;
                }
                const bool into = (device.hats[index] & b.hat_direction) != 0;
                const bool was = before != nullptr && index < before->hats.size() &&
                                 (before->hats[index] & b.hat_direction) != 0;
                if (into && !was) {
                    for (double* f : fields(b.control, controls)) {
                        *f = clamp_control(b.control, *f + b.amount);
                    }
                }
                break;
            }
            }
        }
    }
    previous_ = devices;
}

std::vector<std::string> ControlMapper::unbound(const DeviceState& device) const {
    const auto bound = [&](Source source, int index, std::uint8_t direction) {
        return std::any_of(bindings_.begin(), bindings_.end(), [&](const Binding& b) {
            return b.device == device.kind && b.source == source && b.index == index &&
                   (source != Source::hat || b.hat_direction == direction);
        });
    };
    std::vector<std::string> out;
    for (std::size_t i = 0; i < device.axes.size(); ++i) {
        if (!bound(Source::axis, static_cast<int>(i), 0)) {
            out.push_back("axis " + std::to_string(i));
        }
    }
    for (std::size_t i = 0; i < device.buttons.size(); ++i) {
        if (!bound(Source::button, static_cast<int>(i), 0)) {
            out.push_back("button " + std::to_string(i));
        }
    }
    for (std::size_t i = 0; i < device.hats.size(); ++i) {
        for (const std::uint8_t d : {hat_up, hat_right, hat_down, hat_left}) {
            if (!bound(Source::hat, static_cast<int>(i), d)) {
                out.push_back("hat " + std::to_string(i) + " " + hat_name(d));
            }
        }
    }
    return out;
}

struct Joysticks::Open {
    SDL_Joystick* joystick = nullptr;
    ~Open() {
        if (joystick != nullptr) {
            SDL_CloseJoystick(joystick);
        }
    }
};

Joysticks::Joysticks() {
    if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
        throw InputError("SDL's joystick subsystem is not initialised");
    }
}

Joysticks::~Joysticks() = default;

std::vector<DeviceState> Joysticks::read() {
    SDL_UpdateJoysticks();
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    std::vector<DeviceState> states;
    std::set<std::uint32_t> present;
    for (int i = 0; i < count; ++i) {
        const SDL_JoystickID id = ids[i];
        present.insert(id);
        auto& open = open_[id];
        if (!open) {
            open = std::make_unique<Open>();
            open->joystick = SDL_OpenJoystick(id);
        }
        SDL_Joystick* j = open->joystick;
        if (j == nullptr) {
            continue;
        }
        DeviceState s;
        s.kind = SDL_GetJoystickType(j) == SDL_JOYSTICK_TYPE_THROTTLE
                     ? DeviceKind::throttle
                     : DeviceKind::flight_stick;
        const char* name = SDL_GetJoystickName(j);
        s.name = name != nullptr ? name : "";
        for (int a = 0; a < SDL_GetNumJoystickAxes(j); ++a) {
            s.axes.push_back(
                std::clamp(SDL_GetJoystickAxis(j, a) / 32767.0, -1.0, 1.0));
        }
        for (int b = 0; b < SDL_GetNumJoystickButtons(j); ++b) {
            s.buttons.push_back(SDL_GetJoystickButton(j, b));
        }
        for (int h = 0; h < SDL_GetNumJoystickHats(j); ++h) {
            s.hats.push_back(SDL_GetJoystickHat(j, h));
        }
        states.push_back(std::move(s));
    }
    SDL_free(ids);
    for (auto it = open_.begin(); it != open_.end();) {
        it = present.count(it->first) == 0 ? open_.erase(it) : std::next(it);
    }
    return states;
}

const std::vector<KeyboardBinding>& keyboard_bindings() {
    static const std::vector<KeyboardBinding> keys{
        {Control::elevator, Mode::centred, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, "UP", "DOWN"},
        {Control::aileron, Mode::centred, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, "LEFT",
         "RIGHT"},
        {Control::rudder, Mode::centred, SDL_SCANCODE_Z, SDL_SCANCODE_X, "Z", "X"},
        {Control::throttle, Mode::lever, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_PAGEUP,
         "PAGE DOWN", "PAGE UP"},
        {Control::mixture, Mode::lever, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, ",", "."},
        {Control::propeller, Mode::lever, SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET,
         "LEFT BRACKET", "RIGHT BRACKET"},
        {Control::speedbrake, Mode::lever, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE, ";",
         "APOSTROPHE"},
        {Control::flaps, Mode::step, SDL_SCANCODE_R, SDL_SCANCODE_F, "R", "F"},
        {Control::brakes, Mode::hold, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_B, "", "B"},
    };
    return keys;
}

const std::vector<std::pair<Command, CommandKey>>& command_keys() {
    static const std::vector<std::pair<Command, CommandKey>> keys{
        {Command::help, {SDL_SCANCODE_F1, "F1", "THESE CONTROLS, SHOWN OR HIDDEN"}},
        {Command::swap_pilot, {SDL_SCANCODE_A, "A", "THE AI FLIES, OR YOU DO"}},
        {Command::learnt_landing, {SDL_SCANCODE_L, "L", "THE LEARNT LANDING (SERVER)"}},
        {Command::next_model, {SDL_SCANCODE_M, "M", "THE NEXT LANGUAGE MODEL (SERVER)"}},
        {Command::copilot, {SDL_SCANCODE_C, "C", "ASK THE COPILOT (SERVER)"}},
        {Command::take_over, {SDL_SCANCODE_T, "T", "TAKE OVER THE AIRCRAFT RIDDEN IN (SERVER)"}},
        {Command::ride_next, {SDL_SCANCODE_W, "W", "RIDE IN THE NEXT AIRCRAFT (SERVER)"}},
        {Command::view, {SDL_SCANCODE_V, "V", "THE NEXT VIEW"}},
    };
    return keys;
}

const CommandKey& command_key(Command command) {
    for (const auto& [c, key] : command_keys()) {
        if (c == command) {
            return key;
        }
    }
    throw InputError("no key for a command");
}

std::string help_name(Control control) {
    for (const auto& [name, c] : control_names()) {
        if (c == control) {
            std::string out;
            for (const char ch : name) {
                out.push_back(ch == '_' ? ' '
                                        : static_cast<char>(
                                              std::toupper(static_cast<unsigned char>(ch))));
            }
            return out;
        }
    }
    return "?";
}

std::string help_name(DeviceKind device) {
    return device == DeviceKind::throttle ? "THROTTLE" : "STICK";
}

std::string help_name(const Binding& b) {
    std::string out;
    switch (b.source) {
    case Source::axis:
        out = "AXIS " + std::to_string(b.index);
        break;
    case Source::button:
        out = "BUTTON " + std::to_string(b.index);
        break;
    case Source::hat:
        out = "HAT " + std::to_string(b.index) + " " + hat_name(b.hat_direction);
        break;
    }
    if (b.mode == Mode::hold) {
        out += " HELD";
    } else if (b.mode == Mode::step) {
        // The step to two places, its trailing noughts dropped: +0.1, -0.33.
        char text[32];
        std::snprintf(text, sizeof text, "%+.2f", b.amount);
        std::string step = text;
        while (step.back() == '0') {
            step.pop_back();
        }
        if (step.back() == '.') {
            step.pop_back();
        }
        out += " " + step;
    }
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

std::vector<std::string> controls_help(const std::vector<Binding>& bindings) {
    const auto padded = [](std::string text, std::size_t width) {
        text.resize(std::max(text.size() + 1, width), ' ');
        return text;
    };
    std::vector<std::string> lines;
    lines.push_back("KEYS");
    for (const auto& [command, key] : command_keys()) {
        lines.push_back(" " + padded(key.name, 28) + key.does);
    }
    for (const KeyboardBinding& k : keyboard_bindings()) {
        const std::string keys = k.mode == Mode::hold
                                     ? std::string(k.more_name)
                                     : std::string(k.less_name) + " " + k.more_name;
        lines.push_back(" " + padded(keys, 28) + help_name(k.control) +
                        (k.mode == Mode::hold   ? " HELD"
                         : k.mode == Mode::step ? " A NOTCH A PRESS"
                                                : ""));
    }
    for (const DeviceKind device : {DeviceKind::flight_stick, DeviceKind::throttle}) {
        lines.push_back(help_name(device));
        // A line a control, in the order the file first binds it.
        std::vector<Control> order;
        for (const Binding& b : bindings) {
            if (b.device == device &&
                std::find(order.begin(), order.end(), b.control) == order.end()) {
                order.push_back(b.control);
            }
        }
        for (const Control control : order) {
            std::string line = " " + padded(help_name(control), 13);
            bool first = true;
            for (const Binding& b : bindings) {
                if (b.device == device && b.control == control) {
                    line += (first ? "" : ", ") + help_name(b);
                    first = false;
                }
            }
            lines.push_back(line);
        }
    }
    return lines;
}

void KeyboardControls::apply(sim::Controls& controls, double seconds,
                             const bool* keys, int count) {
    if (keys == nullptr) {
        return;
    }
    const auto down = [&](int at) { return at > 0 && at < count && keys[at]; };
    std::size_t centred = 0;
    for (const KeyboardBinding& k : keyboard_bindings()) {
        const std::vector<double*> moved = fields(k.control, controls);
        switch (k.mode) {
        case Mode::centred: {
            // Moved while held, and to the middle when let go - but only
            // then, not every call, so a stick left alone is not overridden.
            const double v = (down(k.more) ? 0.5 : 0.0) - (down(k.less) ? 0.5 : 0.0);
            const bool held = down(k.more) || down(k.less);
            bool& was = centred_held_.at(centred++);
            if (held || was) {
                for (double* f : moved) {
                    *f = v;
                }
            }
            was = held;
            break;
        }
        case Mode::lever: {
            // Moved while held and left where they are put.
            const double v = (down(k.more) ? 1.0 : 0.0) - (down(k.less) ? 1.0 : 0.0);
            for (double* f : moved) {
                *f = std::clamp(*f + 0.5 * seconds * v, 0.0, 1.0);
            }
            break;
        }
        case Mode::step: {
            // A notch a press - on the key going down, not while held - from
            // the notch nearest where they are, so that a lever a stick left
            // between notches steps to one.
            const auto notch = [&](int key, bool& was, double by) {
                const bool now = down(key);
                if (now && !was) {
                    for (double* f : moved) {
                        *f = std::clamp(std::round(*f * 3.0) + by, 0.0, 3.0) / 3.0;
                    }
                }
                was = now;
            };
            notch(k.more, flaps_down_, 1.0);
            notch(k.less, flaps_up_, -1.0);
            break;
        }
        case Mode::hold:
            if (down(k.more) || brakes_) {
                for (double* f : moved) {
                    *f = down(k.more) ? 1.0 : 0.0;
                }
            }
            brakes_ = down(k.more);
            break;
        }
    }
}

} // namespace glideslope::platform
