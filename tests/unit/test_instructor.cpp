// The instructor as the simulation's (sim/instructor.hpp): every lesson there
// is begun where it begins, demonstrated by the AI pilot to an empty debrief,
// and handed over to the pilot with no step in any control.

#include "harness.hpp"

#include "sim/catalogue.hpp"
#include "sim/instructor.hpp"
#include "sim/lesson.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using glideslope::sim::Demonstration;
using glideslope::sim::Instructor;
using glideslope::sim::Lesson;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

glideslope::sim::Runway a_runway() {
    glideslope::sim::Runway r;
    r.name = "the runway";
    r.threshold_lat_deg = -33.9461;
    r.threshold_lon_deg = 151.1772;
    r.elevation_ft = 0.0;
    r.heading_deg = 70.0;
    r.length_m = 3000.0;
    return r;
}

// The most any control moved between two steps.
double worst_step(const glideslope::sim::Controls& was, const glideslope::sim::Controls& now) {
    const auto before = was.as_list();
    const auto after = now.as_list();
    double worst = 0.0;
    for (std::size_t i = 0; i < before.size(); ++i) {
        worst = std::max(worst, std::abs(after[i] - before[i]));
    }
    return worst;
}

// **Who demonstrates a lesson**: the first aeroplane in the roster of its
// class that can be taught it (sim::cannot_be_taught), or nothing.
std::optional<glideslope::sim::CatalogueEntry> taught_by(const Lesson& lesson) {
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        if (entry.aircraft_class == lesson.teaches &&
            glideslope::sim::cannot_be_taught(
                lesson, glideslope::sim::lesson_speeds(data(), entry.model))
                .empty()) {
            return entry;
        }
    }
    return std::nullopt;
}

struct Shown {
    std::size_t completed = 0;
    std::size_t stages = 0;
    std::vector<std::string> debrief;
    bool handed_over = false;
    double handed_at_s = 0.0;
    double worst_to_pilot = 0.0;
    double worst_settled = 0.0;
};

// **One lesson, from its beginning to three seconds after the hand-over**,
// for at most fifteen minutes of simulated time: the pilot's hands held
// where the instructor left them at rest, nowhere near where the AI had the
// controls.
Shown show(const Lesson& lesson, const glideslope::sim::CatalogueEntry& entry) {
    Instructor instructor(
        data(), entry, lesson, a_runway(),
        std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; },
            [water = entry.seaplane](double, double) { return water; }));
    Shown out;
    out.stages = lesson.stages.size();
    glideslope::sim::Controls last;
    bool first = true;
    std::int64_t after = -1;
    for (std::int64_t tick = 0; tick < 900 * steps_per_second; ++tick) {
        const glideslope::sim::Controls now = instructor.step();
        if (!first && instructor.handed_over_at() >= 0) {
            const double step = worst_step(last, now);
            if (instructor.handed_over_at() == tick) {
                out.worst_to_pilot = step;
            } else {
                out.worst_settled = std::max(out.worst_settled, step);
            }
        }
        first = false;
        last = now;
        if (instructor.part() == Instructor::Part::handed_over && after < 0) {
            after = tick + 3 * steps_per_second;
        }
        if (after >= 0 && tick >= after) {
            break;
        }
    }
    out.handed_over = instructor.part() == Instructor::Part::handed_over;
    out.handed_at_s =
        static_cast<double>(instructor.handed_over_at()) / static_cast<double>(steps_per_second);
    out.debrief = instructor.run().debrief_lines();
    out.completed = instructor.run().completed();
    return out;
}

// **Every class's lesson for one exercise, demonstrated and handed over.**
// The space is the lessons that say they are demonstrated so; each is
// flown by the first aeroplane of its class that can be taught it, and a
// lesson no aeroplane can be taught is named, never quietly skipped.
void every_lesson_demonstrated(Demonstration exercise) {
    const double a_hands_pace = 2.0 / steps_per_second + 0.004;
    std::size_t lessons = 0;
    std::size_t shown_count = 0;
    std::vector<std::string> left_out;
    for (const Lesson& lesson : glideslope::sim::read_lessons(data())) {
        if (lesson.demonstration != exercise) {
            continue;
        }
        ++lessons;
        const auto entry = taught_by(lesson);
        if (!entry) {
            left_out.push_back(lesson.id);
            continue;
        }
        const Shown shown = show(lesson, *entry);
        std::printf("  %-36s %-13s %zu/%zu stages, handed over at %.1f s, worst step "
                    "%.4f over, %.4f settled\n",
                    lesson.id.c_str(), entry->id.c_str(), shown.completed, shown.stages,
                    shown.handed_at_s, shown.worst_to_pilot, shown.worst_settled);
        for (const std::string& said : shown.debrief) {
            std::printf("      %s\n", said.c_str());
        }
        check(shown.completed == shown.stages,
              lesson.id + ": " + entry->id + " flew " + std::to_string(shown.completed) +
                  " of " + std::to_string(shown.stages) + " stages");
        check(shown.debrief.empty(), lesson.id + ": " + entry->id +
                                         " demonstrated it inside the lesson's limits, and "
                                         "said " +
                                         std::to_string(shown.debrief.size()) + " things");
        check(shown.handed_over, lesson.id + ": the controls were handed over");
        check(shown.worst_to_pilot <= a_hands_pace,
              lesson.id + ": " + entry->id + " stepped " +
                  std::to_string(shown.worst_to_pilot) + " handing over");
        check(shown.worst_settled <= a_hands_pace,
              lesson.id + ": " + entry->id + " moved a control " +
                  std::to_string(shown.worst_settled) + " in a step while the pilot held it");
        ++shown_count;
    }
    for (const std::string& id : left_out) {
        std::printf("      left out - %s: no aeroplane of its class can be taught it\n",
                    id.c_str());
    }
    std::printf("  %s: %zu of the %zu lessons demonstrated\n", glideslope::sim::name_of(exercise),
                shown_count, lessons);
    // Seven classes, a lesson each.
    check(lessons == 7, std::string("seven classes' ") + glideslope::sim::name_of(exercise) +
                            " lessons, and there are " + std::to_string(lessons));
    check(shown_count == lessons, "every lesson demonstrated: " + std::to_string(shown_count) +
                                      " of " + std::to_string(lessons));
}

} // namespace

// **Every lesson says how the instructor demonstrates it, and has an
// aeroplane to teach it.** Forty-two lessons: six exercises for each of the
// seven classes.
GLIDESLOPE_TEST(every_lesson_says_how_the_instructor_demonstrates_it_and_has_an_aeroplane_to_teach_it) {
    const auto lessons = glideslope::sim::read_lessons(data());
    std::map<std::string, std::size_t> by_exercise;
    std::size_t taught = 0;
    for (const Lesson& lesson : lessons) {
        check(lesson.demonstration.has_value(),
              lesson.id + " says how the instructor demonstrates it");
        if (!lesson.demonstration) {
            continue;
        }
        ++by_exercise[glideslope::sim::name_of(*lesson.demonstration)];
        // Its file is named for its class and its exercise.
        const std::string named = std::string(glideslope::sim::name_of(lesson.teaches)) + "-" +
                                  glideslope::sim::name_of(*lesson.demonstration);
        check(lesson.id == named, lesson.id + " is demonstrated as " + named);
        const auto entry = taught_by(lesson);
        check(entry.has_value(), lesson.id + " has an aeroplane of its class to teach it");
        taught += entry ? 1U : 0U;
    }
    std::printf("  %zu lessons, %zu with an aeroplane to teach them\n", lessons.size(), taught);
    check(lessons.size() == 42, "42 lessons, and there are " + std::to_string(lessons.size()));
    check(taught == lessons.size(), "every lesson has an aeroplane to teach it");
    check(by_exercise.size() == 6, "six exercises, and there are " +
                                       std::to_string(by_exercise.size()));
}

GLIDESLOPE_TEST(the_instructor_demonstrates_every_classs_take_off_lesson_and_hands_it_over) {
    every_lesson_demonstrated(Demonstration::take_off);
}

GLIDESLOPE_TEST(the_instructor_demonstrates_every_classs_approach_lesson_and_hands_it_over) {
    every_lesson_demonstrated(Demonstration::approach);
}

GLIDESLOPE_TEST(the_instructor_demonstrates_every_classs_circuit_lesson_and_hands_it_over) {
    every_lesson_demonstrated(Demonstration::circuit);
}

GLIDESLOPE_TEST(the_instructor_demonstrates_every_classs_climb_and_descent_lesson_and_hands_it_over) {
    every_lesson_demonstrated(Demonstration::climb_and_descent);
}

GLIDESLOPE_TEST(the_instructor_demonstrates_every_classs_stall_lesson_and_hands_it_over) {
    every_lesson_demonstrated(Demonstration::stalls);
}

GLIDESLOPE_TEST(the_instructor_demonstrates_every_classs_turns_lesson_and_hands_it_over) {
    every_lesson_demonstrated(Demonstration::turns);
}

// **A lesson that does not say how it is demonstrated is refused**, saying
// what it lacks, and an `instructor` line naming no exercise is refused
// where it is written.
GLIDESLOPE_TEST(a_lesson_the_instructor_cannot_demonstrate_is_refused_and_says_why) {
    const char* without = "name A lesson\nteaches light-aircraft\nstage One\ndo It\n"
                          "until position/h-agl-ft >= 500\n";
    const Lesson lesson = glideslope::sim::parse_lesson("without", without);
    check(!lesson.demonstration.has_value(), "a lesson need not be demonstrable to be read");
    bool refused = false;
    try {
        const auto entry = glideslope::sim::find_aircraft(data(), "c172p");
        Instructor instructor(data(), entry, lesson, a_runway(),
                              std::make_shared<glideslope::sim::FunctionTerrain>(
                                  [](double, double) { return 0.0; }));
    } catch (const glideslope::sim::LessonError& e) {
        refused = std::string(e.what()).find("instructor EXERCISE") != std::string::npos;
    }
    check(refused, "the instructor refuses a lesson that does not say how it is demonstrated");
    bool said_where = false;
    try {
        (void)glideslope::sim::parse_lesson(
            "wrong", "name A lesson\nteaches light-aircraft\ninstructor aerobatics\n");
    } catch (const glideslope::sim::LessonError& e) {
        said_where = std::string(e.what()).find("line 3") != std::string::npos;
    }
    check(said_where, "an instructor line naming no exercise is refused at its line");
}
