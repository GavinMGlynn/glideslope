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
            try {
                stage.until_value = std::stod(w[3]);
            } catch (const std::exception&) {
                throw wrong("until wants a number, not \"" + w[3] + "\"");
            }
        } else if (w[0] == "hold") {
            if (w.size() < 5) {
                throw wrong("hold PROPERTY LOW HIGH TEXT");
            }
            LessonWatch watch;
            watch.property = w[1];
            try {
                watch.low = std::stod(w[2]);
                watch.high = std::stod(w[3]);
            } catch (const std::exception&) {
                throw wrong("hold wants two numbers for its band");
            }
            if (!(watch.low <= watch.high)) {
                throw wrong("hold's band runs backwards");
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
            try {
                watch.low = std::stod(w[3]);
            } catch (const std::exception&) {
                throw wrong("need wants a number, not \"" + w[3] + "\"");
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
