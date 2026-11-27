#include "audio/sound_set.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace glideslope::audio {

double SoundSet::fundamental_hz(double drive) const {
    if (engine == EngineKind::piston) {
        return drive / 60.0 * static_cast<double>(cylinders) / 2.0;
    }
    return drive / 100.0 * fan_rpm / 60.0;
}

SoundSet parse_sound_set(const std::string& id, std::string_view text) {
    SoundSet s;
    s.id = id;
    bool engine = false;
    bool wind = false;
    bool touchdown = false;
    bool stall = false;
    bool other = false;
    std::istringstream in{std::string(text)};
    int line_number = 0;
    for (std::string line; std::getline(in, line);) {
        ++line_number;
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
        const auto wrong = [&](const std::string& why) {
            return SoundSetError(id + ".sound, line " + std::to_string(line_number) + ": " +
                                 why);
        };
        const auto number = [&](std::size_t i, const char* what, double low, double high) {
            if (i >= w.size()) {
                throw wrong(std::string("no ") + what);
            }
            char* end = nullptr;
            const double v = std::strtod(w[i].c_str(), &end);
            if (*end != '\0' || !(v >= low && v <= high)) {
                throw wrong(std::string(what) + " must be a number from " +
                            std::to_string(low) + " to " + std::to_string(high) + ", not \"" +
                            w[i] + "\"");
            }
            return v;
        };
        const auto count = [&](std::size_t n, const char* form) {
            if (w.size() != n) {
                throw wrong(std::string("expected ") + form);
            }
        };
        if (w[0] == "engine" && w.size() >= 2 && w[1] == "piston") {
            count(4, "engine piston CYLINDERS GAIN");
            s.engine = EngineKind::piston;
            s.cylinders = static_cast<int>(number(2, "cylinders", 1, 48));
            s.engine_gain = number(3, "gain", 0, 1);
            engine = true;
        } else if (w[0] == "engine" && w.size() >= 2 && w[1] == "turbine") {
            count(5, "engine turbine FAN_RPM BLADES GAIN");
            s.engine = EngineKind::turbine;
            s.fan_rpm = number(2, "fan rpm", 100, 100000);
            s.blades = static_cast<int>(number(3, "blades", 1, 100));
            s.engine_gain = number(4, "gain", 0, 1);
            engine = true;
        } else if (w[0] == "wind") {
            count(3, "wind GAIN REF_KT");
            s.wind_gain = number(1, "gain", 0, 1);
            s.wind_ref_kt = number(2, "reference speed", 1, 2000);
            wind = true;
        } else if (w[0] == "gear") {
            count(3, "gear MOTOR_HZ GAIN");
            s.gear = Motor{number(1, "motor frequency", 20, 20000), number(2, "gain", 0, 1)};
        } else if (w[0] == "flaps") {
            count(3, "flaps MOTOR_HZ GAIN");
            s.flaps = Motor{number(1, "motor frequency", 20, 20000), number(2, "gain", 0, 1)};
        } else if (w[0] == "touchdown") {
            count(2, "touchdown GAIN");
            s.touchdown_gain = number(1, "gain", 0, 1);
            touchdown = true;
        } else if (w[0] == "stall-warning") {
            count(4, "stall-warning ALPHA_DEG HORN_HZ GAIN");
            s.stall_alpha_deg = number(1, "angle of attack", 1, 60);
            s.stall_horn = Motor{number(2, "horn frequency", 20, 20000), number(3, "gain", 0, 1)};
            stall = true;
        } else if (w[0] == "heard-as-other") {
            count(2, "heard-as-other DRIVE");
            s.heard_as_other_drive = number(1, "drive", 1, 100000);
            other = true;
        } else {
            throw wrong("unknown line \"" + w[0] + "\"");
        }
    }
    const auto missing = [&](bool had, const char* what) {
        if (!had) {
            throw SoundSetError(id + ".sound gives no " + what);
        }
    };
    missing(engine, "engine");
    missing(wind, "wind");
    missing(touchdown, "touchdown");
    missing(stall, "stall-warning");
    missing(other, "heard-as-other");
    return s;
}

SoundSet read_sound_set(const std::filesystem::path& data, const std::string& id) {
    const std::filesystem::path path = data / "sound" / (id + ".sound");
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw SoundSetError("cannot read " + path.string());
    }
    return parse_sound_set(id, std::string(std::istreambuf_iterator<char>(in), {}));
}

} // namespace glideslope::audio
