#include "sim/catalogue.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace glideslope::sim {

std::string_view name_of(AircraftClass of) {
    switch (of) {
    case AircraftClass::light_aircraft:
        return "light-aircraft";
    case AircraftClass::seaplane:
        return "seaplane";
    case AircraftClass::second_world_war:
        return "second-world-war";
    case AircraftClass::business_jet:
        return "business-jet";
    case AircraftClass::airliner:
        return "airliner";
    case AircraftClass::fighter:
        return "fighter";
    case AircraftClass::bomber:
        return "bomber";
    }
    return "";
}

std::optional<AircraftClass> class_from_name(std::string_view name) {
    if (name == "light-aircraft") {
        return AircraftClass::light_aircraft;
    }
    if (name == "seaplane") {
        return AircraftClass::seaplane;
    }
    if (name == "second-world-war") {
        return AircraftClass::second_world_war;
    }
    if (name == "business-jet") {
        return AircraftClass::business_jet;
    }
    if (name == "airliner") {
        return AircraftClass::airliner;
    }
    if (name == "fighter") {
        return AircraftClass::fighter;
    }
    if (name == "bomber") {
        return AircraftClass::bomber;
    }
    return std::nullopt;
}

CatalogueEntry parse_catalogue_entry(const std::string& id, std::string_view text) {
    CatalogueEntry e;
    e.id = id;
    bool started = false;
    bool classed = false;
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
            return CatalogueError(id + ".aircraft, line " +
                                  std::to_string(line_number) + ": " + why);
        };
        const auto number = [&](const std::string& word, const char* what, double low,
                                double high) {
            char* end = nullptr;
            const double v = std::strtod(word.c_str(), &end);
            if (word.empty() || *end != '\0' || !(v >= low && v <= high)) {
                throw wrong(std::string(what) + " must be a number from " +
                            std::to_string(low) + " to " + std::to_string(high) +
                            ", not \"" + word + "\"");
            }
            return v;
        };
        if (w[0] == "name") {
            if (w.size() < 2) {
                throw wrong("name TEXT");
            }
            e.name.clear();
            for (std::size_t i = 1; i < w.size(); ++i) {
                e.name += (i > 1 ? " " : "") + w[i];
            }
        } else if (w[0] == "model") {
            if (w.size() != 2) {
                throw wrong("model NAME");
            }
            e.model = w[1];
        } else if (w[0] == "start") {
            if (w.size() != 3) {
                throw wrong("start AIRSPEED_KT THROTTLE");
            }
            e.start_airspeed_kts = number(w[1], "the airspeed", 1.0, 1000.0);
            e.start_throttle = number(w[2], "the throttle", 0.0, 1.0);
            started = true;
        } else if (w[0] == "class") {
            if (w.size() != 2) {
                throw wrong("class NAME");
            }
            const auto of = class_from_name(w[1]);
            if (!of) {
                throw wrong("no class \"" + w[1] + "\"");
            }
            e.aircraft_class = *of;
            classed = true;
        } else if (w[0] == "seaplane") {
            if (w.size() != 1) {
                throw wrong("seaplane, alone");
            }
            e.seaplane = true;
        } else {
            throw wrong("no command \"" + w[0] + "\"");
        }
    }
    if (e.name.empty() || e.model.empty() || !started || !classed) {
        throw CatalogueError(id + ".aircraft must give the aircraft's name, model, "
                                  "start and class");
    }
    return e;
}

std::vector<CatalogueEntry> read_catalogue(const std::filesystem::path& data) {
    std::vector<CatalogueEntry> entries;
    const std::filesystem::path dir = data / "aircraft";
    std::error_code error;
    for (const auto& file : std::filesystem::directory_iterator(dir, error)) {
        if (file.path().extension() != ".aircraft") {
            continue;
        }
        std::ifstream in(file.path(), std::ios::binary);
        if (!in) {
            throw CatalogueError("cannot read " + file.path().string());
        }
        CatalogueEntry e =
            parse_catalogue_entry(file.path().stem().string(),
                                  std::string(std::istreambuf_iterator<char>(in), {}));
        const std::filesystem::path model =
            data / "jsbsim" / "aircraft" / e.model / (e.model + ".xml");
        if (!std::filesystem::exists(model)) {
            throw CatalogueError(e.id + ".aircraft names the model " + e.model +
                                 ", which is not in " + (data / "jsbsim").string());
        }
        entries.push_back(std::move(e));
    }
    if (error) {
        throw CatalogueError("cannot read " + dir.string() + ": " + error.message());
    }
    std::sort(
        entries.begin(), entries.end(),
        [](const CatalogueEntry& a, const CatalogueEntry& b) { return a.id < b.id; });
    return entries;
}

CatalogueEntry find_aircraft(const std::filesystem::path& data, const std::string& id) {
    for (CatalogueEntry& e : read_catalogue(data)) {
        if (e.id == id) {
            return e;
        }
    }
    throw CatalogueError("no aircraft " + id + " in " + (data / "aircraft").string());
}

std::optional<CatalogueEntry> known_aircraft(const std::filesystem::path& data,
                                             const std::string& id) {
    for (CatalogueEntry& e : read_catalogue(data)) {
        if (e.id == id) {
            return e;
        }
    }
    return std::nullopt;
}

} // namespace glideslope::sim
