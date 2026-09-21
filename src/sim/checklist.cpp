#include "sim/checklist.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>

namespace glideslope::sim {
namespace {

// The nine, with the names the data spells them by. One table, so the order
// the phases are flown in and the names they are read by cannot disagree.
constexpr std::array<std::pair<Phase, std::string_view>, 9> phases_named{{
    {Phase::before_start, "before-start"},
    {Phase::taxi, "taxi"},
    {Phase::take_off, "take-off"},
    {Phase::climb, "climb"},
    {Phase::cruise, "cruise"},
    {Phase::descent, "descent"},
    {Phase::approach, "approach"},
    {Phase::landing, "landing"},
    {Phase::after_landing, "after-landing"},
}};

// Every phase's name, for a message that has to say what was expected.
std::string every_phase_name() {
    std::string all;
    for (const auto& [phase, name] : phases_named) {
        all += (all.empty() ? "" : ", ") + std::string(name);
    }
    return all;
}

} // namespace

const std::vector<Phase>& all_phases() {
    static const std::vector<Phase> phases = [] {
        std::vector<Phase> out;
        for (const auto& [phase, name] : phases_named) {
            out.push_back(phase);
        }
        return out;
    }();
    return phases;
}

std::string phase_name(Phase phase) {
    for (const auto& [which, name] : phases_named) {
        if (which == phase) {
            return std::string(name);
        }
    }
    return {};
}

bool phase_of(std::string_view name, Phase& out) {
    for (const auto& [which, spelt] : phases_named) {
        if (spelt == name) {
            out = which;
            return true;
        }
    }
    return false;
}

const Checklist& AircraftChecklists::at(Phase phase) const {
    for (const Checklist& list : phases) {
        if (list.phase == phase) {
            return list;
        }
    }
    // parse_checklists refuses a file with a phase missing, so this cannot
    // happen for anything it made.
    throw ChecklistError(id + ".checklist has no " + phase_name(phase));
}

AircraftChecklists parse_checklists(const std::string& id, std::string_view text) {
    AircraftChecklists lists;
    lists.id = id;
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
            return ChecklistError(id + ".checklist, line " +
                                  std::to_string(line_number) + ": " + why);
        };
        const auto number = [&](const std::string& word, const char* what) {
            char* end = nullptr;
            const double v = std::strtod(word.c_str(), &end);
            if (word.empty() || *end != '\0') {
                throw wrong(std::string(what) + " must be a number, not \"" + word +
                            "\"");
            }
            return v;
        };
        // Everything from word `from` on, which is what the pilot reads.
        const auto words_from = [&](std::size_t from) {
            std::string out;
            for (std::size_t i = from; i < w.size(); ++i) {
                out += (out.empty() ? "" : " ") + w[i];
            }
            if (out.empty()) {
                throw wrong("an item must say what to do");
            }
            return out;
        };
        const auto into = [&]() -> Checklist& {
            if (lists.phases.empty()) {
                throw wrong("an item before any phase; say which phase it is for");
            }
            return lists.phases.back();
        };

        if (w[0] == "phase") {
            if (w.size() != 2) {
                throw wrong("phase NAME, one of: " + every_phase_name());
            }
            Phase phase{};
            if (!phase_of(w[1], phase)) {
                throw wrong("no phase \"" + w[1] + "\"; the phases are: " +
                            every_phase_name());
            }
            // In the order they are flown, each once. Anything else is a file
            // that has quietly lost a phase or gained one twice.
            const auto expected = all_phases()[lists.phases.size()];
            if (lists.phases.size() >= all_phases().size() || phase != expected) {
                throw wrong("the phases come in the order they are flown, so \"" +
                            phase_name(expected) + "\" was expected here, not \"" +
                            w[1] + "\"");
            }
            lists.phases.push_back(Checklist{phase, {}});
        } else if (w[0] == "check") {
            if (w.size() < 5) {
                throw wrong("check PROPERTY OP VALUE TEXT, where OP is <= or >=");
            }
            ChecklistItem item;
            item.property = w[1];
            const double value = number(w[3], "the figure");
            if (w[2] == ">=") {
                item.low = value;
                item.high = std::numeric_limits<double>::infinity();
            } else if (w[2] == "<=") {
                item.low = -std::numeric_limits<double>::infinity();
                item.high = value;
            } else {
                throw wrong("the comparison must be <= or >=, not \"" + w[2] + "\"");
            }
            item.text = words_from(4);
            into().items.push_back(std::move(item));
        } else if (w[0] == "range") {
            if (w.size() < 5) {
                throw wrong("range PROPERTY LOW HIGH TEXT");
            }
            ChecklistItem item;
            item.property = w[1];
            item.low = number(w[2], "the bottom of the band");
            item.high = number(w[3], "the top of the band");
            if (!(item.low <= item.high)) {
                throw wrong("the band's bottom must not be above its top");
            }
            item.text = words_from(4);
            into().items.push_back(std::move(item));
        } else if (w[0] == "confirm") {
            ChecklistItem item;
            item.text = words_from(1);
            into().items.push_back(std::move(item));
        } else {
            throw wrong("no command \"" + w[0] + "\"");
        }
    }
    if (lists.phases.size() != all_phases().size()) {
        throw ChecklistError(id + ".checklist must give all " +
                             std::to_string(all_phases().size()) +
                             " phases of flight: " + every_phase_name());
    }
    for (const Checklist& list : lists.phases) {
        if (list.items.empty()) {
            throw ChecklistError(id + ".checklist has nothing to do in \"" +
                                 phase_name(list.phase) +
                                 "\"; a phase with no items is a phase not written");
        }
    }
    return lists;
}

std::vector<AircraftChecklists> read_checklists(const std::filesystem::path& data) {
    std::vector<AircraftChecklists> all;
    const std::filesystem::path dir = data / "aircraft";
    std::error_code error;
    for (const auto& file : std::filesystem::directory_iterator(dir, error)) {
        if (file.path().extension() != ".checklist") {
            continue;
        }
        std::ifstream in(file.path(), std::ios::binary);
        if (!in) {
            throw ChecklistError("cannot read " + file.path().string());
        }
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        all.push_back(parse_checklists(file.path().stem().string(), text));
    }
    if (error) {
        throw ChecklistError("cannot read " + dir.string() + ": " + error.message());
    }
    std::sort(all.begin(), all.end(),
              [](const AircraftChecklists& a, const AircraftChecklists& b) {
                  return a.id < b.id;
              });
    return all;
}

AircraftChecklists find_checklists(const std::filesystem::path& data,
                                   const std::string& id) {
    const std::filesystem::path file = data / "aircraft" / (id + ".checklist");
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw ChecklistError("no checklists for \"" + id + "\": " + file.string() +
                             " is not there");
    }
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    return parse_checklists(id, text);
}

} // namespace glideslope::sim
