#include "platform/input.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
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
        {"flaps", Control::flaps},
        {"left_brake", Control::left_brake},
        {"right_brake", Control::right_brake},
        {"brakes", Control::brakes}};
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
    case Control::flaps: return {&controls.flaps};
    case Control::left_brake: return {&controls.left_brake};
    case Control::right_brake: return {&controls.right_brake};
    case Control::brakes: return {&controls.left_brake, &controls.right_brake};
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

} // namespace glideslope::platform
