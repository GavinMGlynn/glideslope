#include "sim/lesson.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

namespace glideslope::sim {
namespace {

std::vector<std::string> words_of(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> out;
    std::string w;
    while (in >> w) {
        out.push_back(w);
    }
    return out;
}

std::string joined(const std::vector<std::string>& w, std::size_t from) {
    std::string out;
    for (std::size_t i = from; i < w.size(); ++i) {
        out += (i > from ? " " : "") + w[i];
    }
    return out;
}

} // namespace

double figure_of(const LessonNumber& number, const LessonSpeeds& speeds) {
    if (!number.named()) {
        return number.literal;
    }
    if (number.reference == "rotate") {
        return speeds.rotate_kts + number.offset;
    }
    if (number.reference == "climb") {
        return speeds.climb_kts + number.offset;
    }
    if (number.reference == "vref") {
        return speeds.vref_kts + number.offset;
    }
    return number.literal;
}

bool read_number(std::string_view text, LessonNumber& out) {
    out = LessonNumber{};
    if (text.empty()) {
        return false;
    }
    for (const char* name : {"rotate", "climb", "vref"}) {
        const std::string_view head(name);
        if (text.size() >= head.size() && text.substr(0, head.size()) == head) {
            const std::string_view rest = text.substr(head.size());
            out.reference = std::string(head);
            if (rest.empty()) {
                out.offset = 0.0;
                return true;
            }
            if (rest[0] != '+' && rest[0] != '-') {
                return false;
            }
            try {
                std::size_t used = 0;
                out.offset = std::stod(std::string(rest), &used);
                return used == rest.size();
            } catch (const std::exception&) {
                return false;
            }
        }
    }
    try {
        std::size_t used = 0;
        out.literal = std::stod(std::string(text), &used);
        return used == text.size();
    } catch (const std::exception&) {
        return false;
    }
}

Lesson parse_lesson(const std::string& id, std::string_view text) {
    Lesson lesson;
    lesson.id = id;
    std::istringstream in{std::string(text)};
    std::string line;
    int number = 0;
    const auto wrong = [&](const std::string& want) {
        return LessonError(id + ".lesson, line " + std::to_string(number) + ": " +
                           want);
    };

    while (std::getline(in, line)) {
        ++number;
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        const std::vector<std::string> w = words_of(line);
        if (w.empty()) {
            continue;
        }
        // Everything but `name` and `teaches` belongs to a stage.
        const auto in_stage = [&]() -> LessonStage& {
            if (lesson.stages.empty()) {
                throw wrong("\"" + w[0] + "\" before any stage");
            }
            return lesson.stages.back();
        };

        if (w[0] == "name") {
            if (w.size() < 2) {
                throw wrong("name TEXT");
            }
            lesson.name = joined(w, 1);
        } else if (w[0] == "teaches") {
            if (w.size() != 2) {
                throw wrong("teaches CLASS");
            }
            const auto of = class_from_name(w[1]);
            if (!of) {
                throw wrong("no class \"" + w[1] + "\"");
            }
            lesson.teaches = *of;
        } else if (w[0] == "stage") {
            if (w.size() < 2) {
                throw wrong("stage TEXT");
            }
            LessonStage stage;
            stage.name = joined(w, 1);
            lesson.stages.push_back(std::move(stage));
        } else if (w[0] == "do") {
            if (w.size() < 2) {
                throw wrong("do TEXT");
            }
            in_stage().doing.push_back(joined(w, 1));
        } else if (w[0] == "until") {
            if (w.size() != 4 || (w[2] != "<=" && w[2] != ">=")) {
                throw wrong("until PROPERTY <=|>= VALUE");
            }
            LessonStage& stage = in_stage();
            if (!stage.until_property.empty()) {
                throw wrong("a stage ends once, and this one already has an until");
            }
            stage.until_property = w[1];
            stage.until_at_least = w[2] == ">=";
            if (!read_number(w[3], stage.until_value)) {
                throw wrong("until wants a figure, not \"" + w[3] + "\"");
            }
        } else if (w[0] == "hold") {
            if (w.size() < 5) {
                throw wrong("hold PROPERTY LOW HIGH TEXT");
            }
            LessonWatch watch;
            watch.property = w[1];
            if (!read_number(w[2], watch.low) || !read_number(w[3], watch.high)) {
                throw wrong("hold wants two figures for its band");
            }
            // A band of two plain numbers must run the right way. One written
            // against the aeroplane own speeds cannot be checked here,
            // because the aeroplane is not known yet; a test flies them.
            if (!watch.low.named() && !watch.high.named() &&
                !(watch.low.literal <= watch.high.literal)) {
                throw wrong("hold band runs backwards");
            }
            watch.banded = true;
            watch.fault = joined(w, 4);
            in_stage().holds.push_back(std::move(watch));
        } else if (w[0] == "need") {
            if (w.size() < 5 || (w[2] != "<=" && w[2] != ">=")) {
                throw wrong("need PROPERTY <=|>= VALUE TEXT");
            }
            LessonWatch watch;
            watch.property = w[1];
            watch.at_least = w[2] == ">=";
            if (!read_number(w[3], watch.low)) {
                throw wrong("need wants a figure, not \"" + w[3] + "\"");
            }
            watch.high = watch.low;
            watch.fault = joined(w, 4);
            in_stage().needs.push_back(std::move(watch));
        } else {
            throw wrong("no command \"" + w[0] + "\"");
        }
    }

    if (lesson.name.empty()) {
        throw LessonError(id + ".lesson must give the lesson's name");
    }
    if (lesson.stages.empty()) {
        throw LessonError(id + ".lesson must have at least one stage");
    }
    for (const LessonStage& stage : lesson.stages) {
        if (stage.until_property.empty()) {
            throw LessonError(id + ".lesson: the stage \"" + stage.name +
                              "\" never ends: it needs an until");
        }
        if (stage.doing.empty()) {
            throw LessonError(id + ".lesson: the stage \"" + stage.name +
                              "\" says nothing to do");
        }
    }
    return lesson;
}

std::vector<Lesson> read_lessons(const std::filesystem::path& data) {
    std::vector<Lesson> out;
    const std::filesystem::path where = data / "lessons";
    if (!std::filesystem::is_directory(where)) {
        return out;
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(where)) {
        if (entry.path().extension() == ".lesson") {
            files.push_back(entry.path());
        }
    }
    // In name order, so that a roster is the same on every machine.
    std::sort(files.begin(), files.end());
    for (const std::filesystem::path& file : files) {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            throw LessonError("cannot read " + file.string());
        }
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        out.push_back(parse_lesson(file.stem().string(), text));
    }
    return out;
}

} // namespace glideslope::sim
