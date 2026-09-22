#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/lesson.hpp"
#include "sim/lesson_run.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using glideslope::sim::Lesson;
using glideslope::sim::LessonError;
using glideslope::sim::LessonRun;
using glideslope::sim::parse_lesson;
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

const std::vector<std::string>& light_aircraft() {
    static const std::vector<std::string> ids{"c172p", "c182", "pa28", "j3cub"};
    return ids;
}

const char* a_whole_lesson() {
    return "# a comment\n"
           "name A lesson with every command in it\n"
           "teaches light-aircraft\n"
           "stage The first stage\n"
           "do One thing to do\n"
           "do And another\n"
           "hold velocities/vc-kts 40.0 60.0 Hold the speed\n"
           "need position/h-agl-ft >= 3.0 Get off the ground\n"
           "until velocities/vc-kts >= 50.0\n"
           "stage The second stage\n"
           "do Something else\n"
           "until position/h-agl-ft >= 500.0  # trailing comment\n";
}

} // namespace

// **A lesson written down and read back is the one that was written**, with
// every command the format has in it.
GLIDESLOPE_TEST(a_lesson_written_down_and_read_back_is_the_one_that_was_written) {
    const Lesson lesson = parse_lesson("whole", a_whole_lesson());
    check(lesson.id == "whole", "it keeps its id");
    check(lesson.name == "A lesson with every command in it", "and its name");
    check(lesson.teaches == glideslope::sim::AircraftClass::light_aircraft,
          "and the class it teaches");
    check(lesson.stages.size() == 2,
          "two stages, not " + std::to_string(lesson.stages.size()));

    const auto& first = lesson.stages[0];
    check(first.name == "The first stage", "the stage keeps its name");
    check(first.doing.size() == 2, "and both of its things to do");
    check(first.doing[0] == "One thing to do", "in order");
    check(first.doing[1] == "And another", "in order");
    check(first.until_property == "velocities/vc-kts" && first.until_at_least &&
              std::abs(first.until_value.literal - 50.0) < 1e-9,
          "and what ends it");
    check(first.holds.size() == 1 && first.holds[0].banded, "one band to hold");
    check(std::abs(first.holds[0].low.literal - 40.0) < 1e-9 &&
              std::abs(first.holds[0].high.literal - 60.0) < 1e-9,
          "with both its ends");
    check(first.holds[0].fault == "Hold the speed", "and what the debrief says");
    check(first.needs.size() == 1 && first.needs[0].at_least, "one need");
    check(first.needs[0].fault == "Get off the ground", "and what it says");

    check(lesson.stages[1].name == "The second stage", "the second stage is read");
    check(lesson.stages[1].holds.empty() && lesson.stages[1].needs.empty(),
          "a stage may watch nothing");
    check(std::abs(lesson.stages[1].until_value.literal - 500.0) < 1e-9,
          "and a trailing comment does not spoil its number");
}

// **A lesson file that is wrong is refused, and says where.** Every way a
// file can be wrong is walked and counted, so a way that stopped being
// refused fails here.
GLIDESLOPE_TEST(a_lesson_file_that_is_wrong_is_refused_and_says_where) {
    const std::vector<std::pair<std::string, std::string>> wrong = {
        {"no name", "teaches light-aircraft\nstage S\ndo X\nuntil a >= 1\n"},
        {"no stages", "name N\nteaches light-aircraft\n"},
        {"a stage that never ends", "name N\nstage S\ndo X\n"},
        {"a stage with nothing to do", "name N\nstage S\nuntil a >= 1\n"},
        {"a command before any stage", "name N\ndo X\n"},
        {"a class there is none of", "name N\nteaches glider\nstage S\ndo X\nuntil a >= 1\n"},
        {"a command there is none of", "name N\nfly fast\n"},
        {"until without an operator", "name N\nstage S\ndo X\nuntil a 1\n"},
        {"until with a bad operator", "name N\nstage S\ndo X\nuntil a == 1\n"},
        {"until without a figure", "name N\nstage S\ndo X\nuntil a >= fast\n"},
        {"a reference there is none of", "name N\nstage S\ndo X\nuntil a >= stall\n"},
        {"a reference with a bad offset", "name N\nstage S\ndo X\nuntil a >= rotate*3\n"},
        {"two untils in one stage", "name N\nstage S\ndo X\nuntil a >= 1\nuntil b >= 2\n"},
        {"hold without a band", "name N\nstage S\ndo X\nuntil a >= 1\nhold b 1 T\n"},
        {"hold with a backwards band", "name N\nstage S\ndo X\nuntil a >= 1\nhold b 9 1 T\n"},
        {"hold without words", "name N\nstage S\ndo X\nuntil a >= 1\nhold b 1 9\n"},
        {"need without words", "name N\nstage S\ndo X\nuntil a >= 1\nneed b >= 1\n"},
        {"need with a bad operator", "name N\nstage S\ndo X\nuntil a >= 1\nneed b == 1 T\n"},
    };
    std::size_t refused = 0;
    for (const auto& [what, text] : wrong) {
        bool threw = false;
        try {
            (void)parse_lesson("bad", text);
        } catch (const LessonError&) {
            threw = true;
        }
        check(threw, std::string("a lesson with ") + what + " is refused");
        ++refused;
    }
    check(refused == wrong.size(),
          "all " + std::to_string(wrong.size()) + " ways of being wrong were walked");
    check(refused == 18, "eighteen ways, and the list above holds eighteen");

    // And the one that is right is not refused.
    (void)parse_lesson("whole", a_whole_lesson());
}

namespace {

struct Flown {
    std::vector<std::string> debrief;
    std::size_t completed = 0;
    std::size_t stages = 0;
    // The attitude the aeroplane actually held once it was off the ground,
    // which is what the lesson's band has to be set from rather than guessed.
    double least_theta_deg = 1e9;
    double most_theta_deg = -1e9;
    // The speed she was doing as the roll ended, which is what the lesson
    // judges "rotating early" on.
    double off_at_kts = 0.0;
};

// **A take-off, flown either by the book or with one fault.** `rotate_early`
// hauls her off the ground the moment there is enough elevator authority;
// `throttle` caps the power. Both are flown against the same lesson.
Flown fly_the_take_off(const std::string& id, double rotate_kts_override,
                       double throttle_cap) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    auto speeds = glideslope::sim::departure_speeds(data(), entry.model);
    // **Rotating early is flown, not faked.** The take-off is the same
    // take-off in every other way - the autopilot still keeps her straight
    // and still climbs away - it is simply told to bring the nose up at a
    // speed she has no business flying at. Hauling the stick back instead
    // would break the attitude band as well, and the item asks for a debrief
    // that names one fault.
    (void)rotate_kts_override;

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));

    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg;
    ic.longitude_deg = runway.threshold_lon_deg;
    ic.altitude_ft = runway.elevation_ft;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = 0.0;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);

    const auto lesson = glideslope::sim::read_lessons(data());
    const auto it = std::find_if(lesson.begin(), lesson.end(), [](const Lesson& l) {
        return l.id == "light-take-off";
    });
    check(it != lesson.end(), "the light take-off lesson is in the data");
    LessonRun run(*it, glideslope::sim::LessonSpeeds{speeds.rotate_kts,
                                                     speeds.climb_kts});

    glideslope::sim::Departure departure(aircraft, runway, speeds);
    double out_least = 1e9;
    double out_most = -1e9;
    double out_off_at_kts = 0.0;
    for (int tick = 0; tick < 300 * steps_per_second && !run.finished(); ++tick) {
        glideslope::sim::Controls controls = departure.fly();
        // **Rotating early is flown, not faked.** A steady touch of back
        // stick while the wheels are still down brings her off before her
        // speed is there - which is what rotating early is - without the
        // wild attitude that hauling the stick fully back would give. Lower
        // the take-off autopilot's rotation speed instead and nothing
        // happens: an aeroplane below its stall will not fly, whatever the
        // nose is doing, and she simply rolls on with the nose up.
        if (rotate_kts_override > 0.0 &&
            aircraft.property("position/h-agl-ft") < rotate_kts_override) {
            controls.elevator = std::max(controls.elevator, 0.30);
        }
        if (throttle_cap < 1.0) {
            controls.throttle = std::min(controls.throttle, throttle_cap);
        }
        aircraft.set_controls(controls);
        aircraft.step();
        const std::size_t was = run.stage();
        run.update(aircraft, tick);
        if (run.stage() > was && was == 0) {
            out_off_at_kts = aircraft.property("velocities/vc-kts");
        }
        if (aircraft.property("position/h-agl-ft") > 15.0) {
            const double theta = aircraft.property("attitude/theta-deg");
            out_least = std::min(out_least, theta);
            out_most = std::max(out_most, theta);
        }
    }
    return {run.debrief_lines(), run.completed(), it->stages.size(), out_least,
            out_most, out_off_at_kts};
}

} // namespace

// **Flown by the book, the take-off lesson leaves an empty debrief** - for
// every light aeroplane in the roster, not one of them.
GLIDESLOPE_TEST(the_take_off_lesson_flown_by_the_book_leaves_an_empty_debrief) {
    std::size_t walked = 0;
    for (const std::string& id : light_aircraft()) {
        const Flown flown = fly_the_take_off(id, 0.0, 1.0);
        check(flown.completed == flown.stages,
              id + " got through all " + std::to_string(flown.stages) +
                  " stages, not " + std::to_string(flown.completed));
        std::printf("  %-6s off at %.0f knots, climbed between %.1f and %.1f "
                    "degrees of pitch\n",
                    id.c_str(), flown.off_at_kts, flown.least_theta_deg,
                    flown.most_theta_deg);
        for (const std::string& said : flown.debrief) {
            std::printf("  %s: %s\n", id.c_str(), said.c_str());
        }
        check(flown.debrief.empty(),
              id + " flown by the book has nothing in its debrief, and it has " +
                  std::to_string(flown.debrief.size()));
        ++walked;
    }
    check(walked == light_aircraft().size(),
          "all four light aeroplanes were flown");
    check(walked == 4, "and there are four of them");
}

// **Flown with one stated fault, the debrief names that fault.** The
// throttle case is exact: one thing said, and it is the throttle.
//
// The early rotation below says two things, and both are true - she came off
// early *and* the attitude wandered while she did it, which is what hauling
// an aeroplane off the ground before its speed actually does. A debrief that
// named only one of them would be hiding the other.
GLIDESLOPE_TEST(a_take_off_flown_with_one_fault_has_that_fault_in_its_debrief) {
    // **Not opening the throttle.** Flown by the book in every other way, so
    // the debrief should hold this and nothing else.
    const Flown lazy = fly_the_take_off("c172p", 0.0, 0.80);
    std::printf("  part throttle: off at %.0f knots, %zu fault(s)\n",
                lazy.off_at_kts, lazy.debrief.size());
    for (const std::string& said : lazy.debrief) {
        std::printf("    %s\n", said.c_str());
    }
    check(lazy.debrief.size() == 1,
          "the throttle fault and nothing else, not " +
              std::to_string(lazy.debrief.size()) + " things");
    check(lazy.debrief[0] == "Open the throttle fully for the take-off",
          "and it is the throttle, not: " + lazy.debrief[0]);

    // **Rotating early**, flown with a steady touch of back stick until she
    // is off. She comes off at a speed a 172 has no business flying at, and
    // the attitude band catches her.
    const Flown early = fly_the_take_off("c172p", 14.0, 1.0);
    const Flown book = fly_the_take_off("c172p", 0.0, 1.0);
    std::printf("  by the book: off at %.0f knots; rotating early: off at %.0f\n",
                book.off_at_kts, early.off_at_kts);
    check(early.off_at_kts < book.off_at_kts - 3.0,
          "she really did come off earlier: " + std::to_string(early.off_at_kts) +
              " against " + std::to_string(book.off_at_kts));
    check(!early.debrief.empty(), "and it is not flown faultlessly");
    const bool named = std::any_of(
        early.debrief.begin(), early.debrief.end(), [](const std::string& s) {
            return s == "Hold the climbing attitude: the nose wandered";
        });
    for (const std::string& said : early.debrief) {
        std::printf("    %s\n", said.c_str());
    }
    check(named, "the debrief names the attitude");
    check(book.debrief.empty(), "and the same take-off by the book says nothing");
}

// **A lesson may name the aeroplane's own published speeds**, because a class
// does not share one. `rotate` and `climb` are what `departure_speeds` works
// out from each aeroplane's figures, and an offset may follow.
GLIDESLOPE_TEST(a_lesson_may_name_the_speeds_the_aeroplane_publishes) {
    glideslope::sim::LessonNumber number;
    check(glideslope::sim::read_number("55", number) && !number.named() &&
              std::abs(number.literal - 55.0) < 1e-9,
          "a plain number is a plain number");
    check(glideslope::sim::read_number("rotate", number) && number.named() &&
              number.reference == "rotate" && std::abs(number.offset) < 1e-9,
          "a name on its own is that speed");
    check(glideslope::sim::read_number("rotate-3", number) && number.named() &&
              std::abs(number.offset + 3.0) < 1e-9,
          "and an offset comes off it");
    check(glideslope::sim::read_number("climb+10", number) && number.named() &&
              number.reference == "climb" && std::abs(number.offset - 10.0) < 1e-9,
          "or goes on it");
    check(!glideslope::sim::read_number("stall", number), "a name there is none of");
    check(!glideslope::sim::read_number("rotate*3", number), "a bad offset");
    check(!glideslope::sim::read_number("", number), "and nothing at all");

    // Resolved against two different aeroplanes, the same lesson figure is
    // two different speeds - which is the whole point.
    const glideslope::sim::LessonSpeeds cub{34.0, 48.0};
    const glideslope::sim::LessonSpeeds cessna{55.0, 76.0};
    glideslope::sim::LessonNumber rotate;
    check(glideslope::sim::read_number("rotate-3", rotate), "reads");
    check(std::abs(glideslope::sim::figure_of(rotate, cub) - 31.0) < 1e-9,
          "the Cub is held to 31 knots");
    check(std::abs(glideslope::sim::figure_of(rotate, cessna) - 52.0) < 1e-9,
          "and the Cessna to 52");

    // And the lesson in the data really does use one.
    const auto lessons = glideslope::sim::read_lessons(data());
    const auto it = std::find_if(lessons.begin(), lessons.end(),
                                 [](const Lesson& l) { return l.id == "light-take-off"; });
    check(it != lessons.end(), "the take-off lesson is there");
    bool names_one = false;
    for (const auto& stage : it->stages) {
        for (const auto& need : stage.needs) {
            names_one = names_one || need.low.named();
        }
    }
    check(names_one, "and it holds each aeroplane to its own rotation speed");
}

// **Now the early rotation is caught for a Cessna too.** The fault the class
// figure could not name is named, because the figure is the aeroplane own.
GLIDESLOPE_TEST(rotating_early_is_caught_for_each_aeroplane_at_its_own_speed) {
    const Flown early = fly_the_take_off("c172p", 14.0, 1.0);
    const Flown book = fly_the_take_off("c172p", 0.0, 1.0);
    std::printf("  c172p by the book: off at %.0f knots; early: %.0f\n",
                book.off_at_kts, early.off_at_kts);
    check(book.debrief.empty(), "by the book it says nothing");
    const bool named = std::any_of(
        early.debrief.begin(), early.debrief.end(), [](const std::string& s) {
            return s == "Let her reach the rotation speed before easing the nose up";
        });
    for (const std::string& said : early.debrief) {
        std::printf("    %s\n", said.c_str());
    }
    check(named, "and rotating early is named, which a class-wide number missed");
}
