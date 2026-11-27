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
#include <set>
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

// **The lessons the roster's aeroplanes are not taught, each with its
// reason.** A lesson belongs to a class and the figures to the aeroplane, so a
// class's lesson can name a figure one of its aeroplanes has not got
// (sim::cannot_be_taught). Named here so that an aeroplane quietly dropped
// from a lesson fails rather than shortens the test.
const std::map<std::string, std::string>& left_out() {
    static const std::map<std::string, std::string> named = {
        // Her take-off speeds are measured from her model, for a plan, not
        // a book's (sim::book_departure_speeds): no rotation or climbing
        // speed for a lesson.
        {"747-400 airliner-take-off", "names rotate, which she has not got"},
        {"747-400 airliner-circuit", "names rotate, which she has not got"},
        {"747-400 airliner-climb-and-descent", "names climb, which she has not got"},
        {"f22 fighter-take-off", "names rotate, which she has not got"},
        {"f22 fighter-circuit", "names climb, which she has not got"},
        {"f22 fighter-climb-and-descent", "names climb, which she has not got"},
    };
    return named;
}

std::string pair_of(const std::string& aircraft, const std::string& lesson) {
    return aircraft + " " + lesson;
}

// **The roster, by hand**: one test per aeroplane, so that they run side by
// side; the test below asserts it is the catalogue's whole roster.
const std::vector<std::string>& roster_tested() {
    static const std::vector<std::string> ids = {
        "737-300", "747-400", "787-8", "a320", "a380", "b2", "c172p", "c182",
        "f15c", "f22", "f35b", "j3cub", "learjet35a", "mosquito-fb6", "pa28", "short_s23"};
    return ids;
}

// **Every lesson of her class, demonstrated in her and handed over**, or
// left out with its reason (left_out); the two together are every lesson
// of her class, six.
void every_lesson_of_her_class(const std::string& id) {
    const double a_hands_pace = 2.0 / steps_per_second + 0.004;
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const auto speeds = glideslope::sim::lesson_speeds(data(), entry.model);
    std::size_t lessons = 0;
    std::size_t flown = 0;
    std::size_t named_out = 0;
    for (const Lesson& lesson : glideslope::sim::read_lessons(data())) {
        if (lesson.teaches != entry.aircraft_class) {
            continue;
        }
        ++lessons;
        const std::string why = glideslope::sim::cannot_be_taught(lesson, speeds);
        const auto named = left_out().find(pair_of(id, lesson.id));
        if (!why.empty()) {
            std::printf("  %-36s left out: %s\n", lesson.id.c_str(), why.c_str());
            check(named != left_out().end(),
                  id + " cannot be taught " + lesson.id + " (" + why + "), and is not named");
            named_out += named != left_out().end() ? 1U : 0U;
            continue;
        }
        check(named == left_out().end(),
              id + " is named as left out of " + lesson.id + ", and can be taught it");
        const Shown shown = show(lesson, entry);
        std::printf("  %-36s %zu/%zu stages, handed over at %.1f s, worst step "
                    "%.4f over, %.4f settled\n",
                    lesson.id.c_str(), shown.completed, shown.stages, shown.handed_at_s,
                    shown.worst_to_pilot, shown.worst_settled);
        for (const std::string& said : shown.debrief) {
            std::printf("      %s\n", said.c_str());
        }
        check(shown.completed == shown.stages,
              lesson.id + ": " + id + " flew " + std::to_string(shown.completed) + " of " +
                  std::to_string(shown.stages) + " stages");
        check(shown.debrief.empty(), lesson.id + ": " + id +
                                         " demonstrated it inside the lesson's limits, and "
                                         "said " +
                                         std::to_string(shown.debrief.size()) + " things");
        check(shown.handed_over, lesson.id + ": the controls were handed over");
        check(shown.worst_to_pilot <= a_hands_pace,
              lesson.id + ": " + id + " stepped " + std::to_string(shown.worst_to_pilot) +
                  " handing over");
        check(shown.worst_settled <= a_hands_pace,
              lesson.id + ": " + id + " moved a control " +
                  std::to_string(shown.worst_settled) + " in a step while the pilot held it");
        ++flown;
    }
    std::printf("  %s: %zu of the %zu lessons of her class demonstrated, %zu named as left "
                "out\n",
                id.c_str(), flown, lessons, named_out);
    check(lessons == 6, id + "'s class has six lessons, and has " + std::to_string(lessons));
    check(flown + named_out == lessons,
          id + ": every lesson of her class demonstrated or named: " +
              std::to_string(flown + named_out) + " of " + std::to_string(lessons));
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

// **Every (lesson, aeroplane) pair**: the whole roster, each aeroplane
// taught every lesson of her class or named as left out of it, and the
// pairs counted against the lessons there are.
GLIDESLOPE_TEST(every_aeroplane_has_its_instructor_test_and_every_lesson_pair_is_flown_or_named) {
    std::set<std::string> catalogue;
    std::size_t pairs = 0;
    std::size_t taught = 0;
    const auto lessons = glideslope::sim::read_lessons(data());
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        catalogue.insert(entry.id);
        const auto speeds = glideslope::sim::lesson_speeds(data(), entry.model);
        for (const Lesson& lesson : lessons) {
            if (lesson.teaches != entry.aircraft_class) {
                continue;
            }
            ++pairs;
            const std::string why = glideslope::sim::cannot_be_taught(lesson, speeds);
            if (why.empty()) {
                ++taught;
            } else {
                std::printf("      left out - %s: %s\n", pair_of(entry.id, lesson.id).c_str(),
                            why.c_str());
            }
        }
    }
    const std::set<std::string> tested(roster_tested().begin(), roster_tested().end());
    check(tested == catalogue, "an instructor test for every aeroplane in the roster, and no "
                               "other: " +
                                   std::to_string(tested.size()) + " for " +
                                   std::to_string(catalogue.size()));
    std::printf("  %zu (lesson, aeroplane) pairs: %zu demonstrated, %zu named as left out\n",
                pairs, taught, left_out().size());
    check(pairs == 6 * catalogue.size(),
          "six lessons for every aeroplane: " + std::to_string(pairs) + " pairs");
    check(taught + left_out().size() == pairs,
          "every pair demonstrated or named: " + std::to_string(taught + left_out().size()) +
              " of " + std::to_string(pairs));
}

GLIDESLOPE_TEST(the_737_300_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("737-300");
}

GLIDESLOPE_TEST(the_747_400_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("747-400");
}

GLIDESLOPE_TEST(the_787_8_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("787-8");
}

GLIDESLOPE_TEST(the_a320_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("a320");
}

GLIDESLOPE_TEST(the_a380_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("a380");
}

GLIDESLOPE_TEST(the_b2_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("b2");
}

GLIDESLOPE_TEST(the_c172p_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("c172p");
}

GLIDESLOPE_TEST(the_c182_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("c182");
}

GLIDESLOPE_TEST(the_f15c_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("f15c");
}

GLIDESLOPE_TEST(the_f22_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("f22");
}

GLIDESLOPE_TEST(the_f35b_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("f35b");
}

GLIDESLOPE_TEST(the_j3cub_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("j3cub");
}

GLIDESLOPE_TEST(the_learjet35a_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("learjet35a");
}

GLIDESLOPE_TEST(the_mosquito_fb6_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("mosquito-fb6");
}

GLIDESLOPE_TEST(the_pa28_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("pa28");
}

GLIDESLOPE_TEST(the_short_s23_is_demonstrated_every_lesson_of_her_class_and_handed_over) {
    every_lesson_of_her_class("short_s23");
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
    // **A lesson for another class is refused**: a fighter's stall lesson
    // is a fighter's, its bands a jet's, and a Cessna is not judged by them.
    bool wrong_class = false;
    try {
        const auto lessons = glideslope::sim::read_lessons(data());
        const auto fighter = std::find_if(lessons.begin(), lessons.end(), [](const Lesson& l) {
            return l.id == "fighter-stalls";
        });
        check(fighter != lessons.end(), "there is a fighter's stall lesson");
        Instructor instructor(data(), glideslope::sim::find_aircraft(data(), "c172p"), *fighter,
                              a_runway(),
                              std::make_shared<glideslope::sim::FunctionTerrain>(
                                  [](double, double) { return 0.0; }));
    } catch (const glideslope::sim::LessonError& e) {
        wrong_class = std::string(e.what()) ==
                      "c172p is light-aircraft, and fighter-stalls teaches fighter";
        std::printf("  refused: %s\n", e.what());
    }
    check(wrong_class, "the instructor refuses a lesson for another class, saying so");
    bool said_where = false;
    try {
        (void)glideslope::sim::parse_lesson(
            "wrong", "name A lesson\nteaches light-aircraft\ninstructor aerobatics\n");
    } catch (const glideslope::sim::LessonError& e) {
        said_where = std::string(e.what()).find("line 3") != std::string::npos;
    }
    check(said_where, "an instructor line naming no exercise is refused at its line");
}
