#include "harness.hpp"

#include "frontend/lessons_screen.hpp"
#include "net/messages.hpp"
#include "sim/catalogue.hpp"
#include "sim/lesson.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

using glideslope::frontend::LessonsScreen;
using glideslope::test::check;

namespace {

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

bool has(const std::vector<std::string>& lines, const std::string& line) {
    return std::find(lines.begin(), lines.end(), line) != lines.end();
}

} // namespace

// **The lessons screen, in every state it has, for every class**: the list
// of her class's lessons and no other's, one chosen and moved round by F3,
// asked once; refused and why; settling; each stage of each lesson with its
// words from the client's own copy; handed over with a debrief of faults
// and with none; ended and why; and closed, showing nothing. Every class
// in the catalogue is walked, and every lesson of each through every stage
// it has; the count of each is stated.
GLIDESLOPE_TEST(the_lessons_screen_shows_every_state_for_every_lesson_of_every_class) {
    const auto all = glideslope::sim::read_lessons(data());
    std::set<glideslope::sim::AircraftClass> classes;
    std::vector<glideslope::sim::CatalogueEntry> first_of;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        if (classes.insert(entry.aircraft_class).second) {
            first_of.push_back(entry);
        }
    }
    std::set<LessonsScreen::State> seen;
    std::size_t lessons_walked = 0;
    std::size_t stages_walked = 0;
    for (const auto& entry : first_of) {
        LessonsScreen screen(all, entry.aircraft_class, entry.id);
        seen.insert(screen.state());
        check(screen.state() == LessonsScreen::State::closed && screen.lines().empty(),
              entry.id + ": the screen begins closed, showing nothing");
        const std::size_t of_class = static_cast<std::size_t>(
            std::count_if(all.begin(), all.end(),
                          [&](const auto& l) { return l.teaches == entry.aircraft_class; }));
        check(screen.count() == of_class && of_class > 0,
              entry.id + ": the list is her class's " + std::to_string(of_class) +
                  " lessons, not " + std::to_string(screen.count()));
        check(!screen.ask().has_value(), entry.id + ": nothing is asked while it is closed");
        screen.toggle();
        seen.insert(screen.state());
        auto lines = screen.lines();
        check(screen.state() == LessonsScreen::State::choosing &&
                  lines.size() == of_class + 2,
              entry.id + ": F2 shows the list, a heading, a line a lesson and the keys");
        check(lines[1] == "+ " + screen.lessons()[0].name,
              entry.id + ": the first lesson is chosen first");
        // F3 round the whole list and back to the first.
        for (std::size_t i = 1; i <= of_class; ++i) {
            screen.next();
            const std::size_t at = i % of_class;
            check(screen.lines()[1 + at] == "+ " + screen.lessons()[at].name,
                  entry.id + ": F3 chooses lesson " + std::to_string(at + 1));
        }
        for (const auto& lesson : screen.lessons()) {
            check(lesson.teaches == entry.aircraft_class,
                  entry.id + ": " + lesson.id + " is of her class");
            check(screen.choose(lesson.id), entry.id + ": " + lesson.id + " can be chosen");
            const auto asked = screen.ask();
            check(asked && *asked == lesson.id, entry.id + ": Return asks for " + lesson.id);
            check(!screen.ask().has_value(), entry.id + ": and only once");

            glideslope::net::LessonTold told;
            told.aircraft = 0;
            told.lesson = lesson.id;
            told.stages = static_cast<std::uint8_t>(lesson.stages.size());
            told.part = glideslope::net::LessonPart::settling;
            screen.told(told);
            seen.insert(screen.state());
            check(screen.state() == LessonsScreen::State::settling &&
                      has(screen.lines(), "Settling her before the lesson") &&
                      screen.lines()[0] == lesson.name,
                  lesson.id + ": settling, said");
            for (std::size_t k = 0; k < lesson.stages.size(); ++k) {
                told.part = glideslope::net::LessonPart::demonstrating;
                told.stage = static_cast<std::uint8_t>(k);
                screen.told(told);
                seen.insert(screen.state());
                lines = screen.lines();
                const auto& stage = lesson.stages[k];
                check(has(lines, "The instructor has control") &&
                          has(lines, "Stage " + std::to_string(k + 1) + " of " +
                                         std::to_string(lesson.stages.size()) + ": " +
                                         stage.name) &&
                          lines.size() == 3 + stage.doing.size(),
                      lesson.id + ": stage " + std::to_string(k + 1) +
                          " said with its name and its words");
                for (const std::string& doing : stage.doing) {
                    check(has(lines, "  " + doing), lesson.id + ": " + doing);
                }
                ++stages_walked;
            }
            told.part = glideslope::net::LessonPart::handed_over;
            told.stage = told.stages;
            told.lines.clear();
            screen.told(told);
            seen.insert(screen.state());
            lines = screen.lines();
            check(has(lines, "Handed over: you have control") &&
                      has(lines, "Debrief: " + std::to_string(lesson.stages.size()) + " of " +
                                     std::to_string(lesson.stages.size()) + " stages flown") &&
                      has(lines, "  Flown to the book: nothing to do differently"),
                  lesson.id + ": handed over, flown to the book");
            told.lines = {"too fast on final", "flared late"};
            screen.told(told);
            lines = screen.lines();
            check(has(lines, "  too fast on final") && has(lines, "  flared late") &&
                      !has(lines, "  Flown to the book: nothing to do differently"),
                  lesson.id + ": handed over, with what to do differently");
            told.lines = {"taken back from the instructor"};
            told.part = glideslope::net::LessonPart::ended;
            told.stage = 0;
            screen.told(told);
            seen.insert(screen.state());
            check(screen.state() == LessonsScreen::State::ended &&
                      screen.lines()[0] == "Lesson ended: " + lesson.name &&
                      has(screen.lines(), "taken back from the instructor"),
                  lesson.id + ": ended, and why");
            told.part = glideslope::net::LessonPart::refused;
            told.lines = {"no runway within 50 km of her to teach it at"};
            screen.told(told);
            seen.insert(screen.state());
            check(screen.state() == LessonsScreen::State::refused &&
                      screen.lines()[0] == "Lesson refused: " + lesson.name &&
                      has(screen.lines(), "no runway within 50 km of her to teach it at"),
                  lesson.id + ": refused, and why");
            ++lessons_walked;
        }
        // A lesson not of her class is not in the list, and cannot be chosen.
        const auto other = std::find_if(all.begin(), all.end(), [&](const auto& l) {
            return l.teaches != entry.aircraft_class;
        });
        check(other == all.end() || !screen.choose(other->id),
              entry.id + ": another class's lesson cannot be chosen");
        screen.toggle();
        check(screen.state() == LessonsScreen::State::closed && screen.lines().empty(),
              entry.id + ": F2 hides it, whatever it showed");
    }
    std::size_t all_stages = 0;
    for (const auto& lesson : all) {
        all_stages += lesson.stages.size();
    }
    std::printf("  %zu classes, %zu of %zu lessons, %zu of %zu stages, %zu of %zu states\n",
                first_of.size(), lessons_walked, all.size(), stages_walked, all_stages,
                seen.size(), LessonsScreen::states);
    check(first_of.size() == 7, "seven classes, not " + std::to_string(first_of.size()));
    check(lessons_walked == all.size() && all.size() == 42,
          "every lesson of every class walked: " + std::to_string(lessons_walked));
    check(stages_walked == all_stages, "every stage of every lesson walked");
    check(seen.size() == LessonsScreen::states, "every state of the screen was reached");
}
