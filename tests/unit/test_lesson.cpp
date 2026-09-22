#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/autopilot.hpp"
#include "sim/lander.hpp"
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

namespace {

constexpr double degrees = 57.29577951308232;
constexpr double metres_per_nm = 1852.0;
constexpr double feet_per_metre = 3.280839895013123;

double metres_per_degree_latitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111132.92 - 559.82 * std::cos(2.0 * lat) + 1.175 * std::cos(4.0 * lat) -
           0.0023 * std::cos(6.0 * lat);
}

double metres_per_degree_longitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111412.84 * std::cos(lat) - 93.5 * std::cos(3.0 * lat) +
           0.118 * std::cos(5.0 * lat);
}

struct Approached {
    std::vector<std::string> debrief;
    std::size_t completed = 0;
    std::size_t stages = 0;
    double least_kts = 1e9;
    double most_kts = -1e9;
    double vref_kts = 0.0;
    // Per stage, so a band can be set from the stage it belongs to rather
    // than from the whole flight.
    std::vector<double> stage_least;
    std::vector<double> stage_most;
};

// **Two miles out on the glidepath, down to a stop.** `fast_by_kts` is flown
// by telling the approach autopilot a reference speed the aeroplane has not
// got: it then flies a correct approach at the wrong speed, which is what an
// approach flown fast is. The lesson still resolves `vref` from the
// aeroplane's own published figures, so it sees the difference.
Approached fly_the_approach(const std::string& id, double fast_by_kts) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);
    auto flown_with = published;
    flown_with.vref_kts += fast_by_kts;

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));

    const double out_m = 2.0 * metres_per_nm;
    const double heading = runway.heading_deg / degrees;
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg =
        runway.threshold_lat_deg + (-out_m * std::cos(heading)) /
                                       metres_per_degree_latitude(runway.threshold_lat_deg);
    ic.longitude_deg =
        runway.threshold_lon_deg + (-out_m * std::sin(heading)) /
                                       metres_per_degree_longitude(runway.threshold_lat_deg);
    ic.altitude_ft = runway.elevation_ft + (out_m + published.aim_m) *
                                               std::tan(3.0 / degrees) * feet_per_metre;
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = flown_with.vref_kts;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);

    const auto lessons = glideslope::sim::read_lessons(data());
    const auto it = std::find_if(lessons.begin(), lessons.end(), [](const Lesson& l) {
        return l.id == "light-approach-and-landing";
    });
    check(it != lessons.end(), "the approach lesson is in the data");
    const auto departure = glideslope::sim::departure_speeds(data(), entry.model);
    LessonRun run(*it, glideslope::sim::LessonSpeeds{departure.rotate_kts,
                                                     departure.climb_kts,
                                                     published.vref_kts});

    glideslope::sim::Lander lander(aircraft, runway, flown_with);
    Approached out;
    out.vref_kts = published.vref_kts;
    out.stage_least.assign(it->stages.size(), 1e9);
    out.stage_most.assign(it->stages.size(), -1e9);
    for (int tick = 0; tick < 600 * steps_per_second && !run.finished(); ++tick) {
        aircraft.set_controls(lander.fly());
        aircraft.step();
        const std::size_t which = run.stage();
        run.update(aircraft, tick);
        const double kts = aircraft.property("velocities/vc-kts");
        if (which < out.stage_least.size()) {
            out.stage_least[which] = std::min(out.stage_least[which], kts);
            out.stage_most[which] = std::max(out.stage_most[which], kts);
        }
        if (aircraft.property("position/h-agl-ft") > 5.0) {
            out.least_kts = std::min(out.least_kts, kts);
            out.most_kts = std::max(out.most_kts, kts);
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    out.stages = it->stages.size();
    return out;
}

} // namespace

// **Flown by the book, the approach lesson leaves an empty debrief** - for
// every light aeroplane, each down its own glidepath at its own speed.
GLIDESLOPE_TEST(the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief) {
    std::size_t walked = 0;
    for (const std::string& id : light_aircraft()) {
        const Approached flown = fly_the_approach(id, 0.0);
        std::printf("  %-6s vref %.0f:", id.c_str(), flown.vref_kts);
        for (std::size_t i = 0; i < flown.stage_least.size(); ++i) {
            if (flown.stage_most[i] > 0.0) {
                std::printf("  stage %zu %.0f-%.0f", i, flown.stage_least[i],
                            flown.stage_most[i]);
            }
        }
        std::printf("  (%zu of %zu stages)\n", flown.completed, flown.stages);
        for (const std::string& said : flown.debrief) {
            std::printf("    %s\n", said.c_str());
        }
        check(flown.completed == flown.stages,
              id + " got through all its stages, not " +
                  std::to_string(flown.completed));
        check(flown.debrief.empty(),
              id + " flown by the book says nothing, and it said " +
                  std::to_string(flown.debrief.size()) + " things");
        ++walked;
    }
    check(walked == 4, "all four light aeroplanes were landed");
}

// **An approach flown fast is named in the debrief**, and a correct one is
// not. The aeroplane flies a perfectly good approach - it is simply doing it
// at a speed it has no business using.
GLIDESLOPE_TEST(an_approach_flown_fast_is_named_in_the_debrief) {
    const Approached fast = fly_the_approach("c172p", 20.0);
    std::printf("  c172p vref %.0f, flown fast: %.0f to %.0f knots\n", fast.vref_kts,
                fast.least_kts, fast.most_kts);
    for (const std::string& said : fast.debrief) {
        std::printf("    %s\n", said.c_str());
    }
    check(!fast.debrief.empty(), "an approach twenty knots fast is not faultless");
    const bool named =
        std::any_of(fast.debrief.begin(), fast.debrief.end(),
                    [](const std::string& s) { return s == "Hold the approach speed"; });
    check(named, "and the debrief says to hold the approach speed");
}

namespace {

// An aeroplane trimmed out in level flight at `agl_ft`, ready to be flown by
// the autopilot.
struct InFlight {
    std::unique_ptr<glideslope::sim::Aircraft> aircraft;
    glideslope::sim::LessonSpeeds speeds;
    double start_agl_ft = 0.0;
    double start_heading_deg = 0.0;
};

InFlight airborne(const std::string& id, double agl_ft) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    InFlight out;
    out.aircraft = std::make_unique<glideslope::sim::Aircraft>(data() / "jsbsim",
                                                               entry.model);
    out.aircraft->set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.altitude_ft = agl_ft;
    ic.terrain_elevation_ft = 0.0;
    ic.heading_deg = 90.0;
    ic.airspeed_kts = entry.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    out.aircraft->initialize(ic);
    const auto departure = glideslope::sim::departure_speeds(data(), entry.model);
    const auto approach = glideslope::sim::approach_speeds(data(), entry.model);
    out.speeds = {departure.rotate_kts, departure.climb_kts, approach.vref_kts};
    out.start_agl_ft = agl_ft;
    out.start_heading_deg = 90.0;
    return out;
}

Lesson the_lesson(const std::string& id) {
    const auto lessons = glideslope::sim::read_lessons(data());
    const auto it = std::find_if(lessons.begin(), lessons.end(),
                                 [&](const Lesson& l) { return l.id == id; });
    check(it != lessons.end(), id + " is in the data");
    return *it;
}

struct Result {
    std::vector<std::string> debrief;
    std::size_t completed = 0;
    std::size_t stages = 0;
    double lowest_agl_ft = 1e9;
    double steepest_bank_deg = 0.0;
};

// **A turn, flown by the autopilot.** `sink_fpm` other than zero makes her
// lose height through it, which is the fault this lesson is for.
Result fly_a_turn(const std::string& id, double sink_fpm) {
    InFlight f = airborne(id, 3000.0);
    const Lesson lesson = the_lesson("light-turns");
    LessonRun run(lesson, f.speeds);

    glideslope::sim::Controls controls;
    controls.throttle = 0.7;
    glideslope::sim::Autopilot autopilot(*f.aircraft, controls);
    glideslope::sim::AutopilotModes modes = autopilot.modes();
    modes.altitude_ft = f.start_agl_ft;
    modes.heading_deg = f.start_heading_deg;
    autopilot.set(modes);

    Result out;
    out.stages = lesson.stages.size();
    // A moment to settle, then turn ninety degrees left.
    for (int tick = 0; tick < 240 * steps_per_second && !run.finished(); ++tick) {
        if (tick == 10 * steps_per_second) {
            modes.heading_deg = f.start_heading_deg - 90.0;
            if (sink_fpm != 0.0) {
                modes.altitude_ft.reset();
                modes.vertical_speed_fpm = sink_fpm;
            }
            autopilot.set(modes);
        }
        f.aircraft->set_controls(autopilot.fly());
        f.aircraft->step();
        if (tick >= 10 * steps_per_second) {
            run.update(*f.aircraft, tick);
            out.lowest_agl_ft =
                std::min(out.lowest_agl_ft, f.aircraft->property("position/h-agl-ft"));
            out.steepest_bank_deg = std::min(
                out.steepest_bank_deg, f.aircraft->property("attitude/phi-deg"));
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

} // namespace

// **A turn flown by the book keeps its height**, and the lesson says nothing.
GLIDESLOPE_TEST(the_turns_lesson_flown_by_the_book_leaves_an_empty_debrief) {
    std::size_t walked = 0;
    for (const std::string& id : light_aircraft()) {
        const Result flown = fly_a_turn(id, 0.0);
        std::printf("  %-6s banked to %.0f degrees, lowest %.0f ft of 3000, "
                    "%zu of %zu stages\n",
                    id.c_str(), flown.steepest_bank_deg, flown.lowest_agl_ft,
                    flown.completed, flown.stages);
        for (const std::string& said : flown.debrief) {
            std::printf("    %s\n", said.c_str());
        }
        check(flown.completed == flown.stages,
              id + " got through the turn, " + std::to_string(flown.completed) +
                  " of " + std::to_string(flown.stages) + " stages");
        check(flown.debrief.empty(),
              id + " flown by the book says nothing, and it said " +
                  std::to_string(flown.debrief.size()));
        ++walked;
    }
    check(walked == 4, "all four were turned");
}

// **A turn that loses height is named for losing height.** The bank is the
// same; what is different is that she is let descend through it.
GLIDESLOPE_TEST(a_turn_that_loses_height_is_named_in_the_debrief) {
    const Result sinking = fly_a_turn("c172p", -1200.0);
    std::printf("  c172p sinking: lowest %.0f ft of 3000, banked to %.0f\n",
                sinking.lowest_agl_ft, sinking.steepest_bank_deg);
    for (const std::string& said : sinking.debrief) {
        std::printf("    %s\n", said.c_str());
    }
    check(!sinking.debrief.empty(), "losing height in a turn is not faultless");
    const bool named = std::any_of(
        sinking.debrief.begin(), sinking.debrief.end(), [](const std::string& s) {
            return s == "Hold your height through the turn";
        });
    check(named, "and the debrief says to hold your height");
}

namespace {

// **A climb, a level-off and a descent, flown by the autopilot.**
// `fast_by_kts` other than zero flies the climb at a speed that is not the
// climbing speed, which is the fault this lesson is for.
Result fly_a_climb_and_descent(const std::string& id, double fast_by_kts) {
    InFlight f = airborne(id, 3000.0);
    const Lesson lesson = the_lesson("light-climb-and-descent");
    LessonRun run(lesson, f.speeds);

    glideslope::sim::Controls controls;
    controls.throttle = 0.8;
    glideslope::sim::Autopilot autopilot(*f.aircraft, controls);
    glideslope::sim::AutopilotModes modes = autopilot.modes();
    // Climbing on vertical speed alone while she settles: the height to
    // level off at is set once the lesson has actually begun, because the
    // lesson asks for nine hundred feet from *there* and she will have
    // climbed some way before then.
    modes.heading_deg = f.start_heading_deg;
    modes.vertical_speed_fpm = 600.0;
    modes.airspeed_kts = f.speeds.climb_kts + fast_by_kts;
    autopilot.set(modes);

    Result out;
    out.stages = lesson.stages.size();
    bool descending = false;
    // **The lesson begins when the exercise does.** She starts at her cruise
    // speed and the autopilot has to slow her to the climbing speed and get
    // the climb established; a lesson that began on the first tick would be
    // judging the settling, not the climb. Thirty seconds is comfortably
    // more than any of the four needs.
    const int settling = 30 * steps_per_second;
    for (int tick = 0; tick < 600 * steps_per_second && !run.finished(); ++tick) {
        f.aircraft->set_controls(autopilot.fly());
        f.aircraft->step();
        if (tick < settling) {
            continue;
        }
        if (tick == settling) {
            modes.altitude_ft = f.aircraft->property("position/h-agl-ft") + 1100.0;
            autopilot.set(modes);
        }
        run.update(*f.aircraft, tick);
        const double agl = f.aircraft->property("position/h-agl-ft");
        out.lowest_agl_ft = std::min(out.lowest_agl_ft, agl);
        // Once she is levelled off up there, bring her back down.
        if (!descending && run.stage() >= 2) {
            descending = true;
            modes.altitude_ft = agl - 700.0;
            modes.vertical_speed_fpm = 600.0;
            modes.airspeed_kts = f.speeds.climb_kts + 25.0;
            autopilot.set(modes);
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

} // namespace

// **Climbing and descending by the book leaves an empty debrief**, for every
// light aeroplane, each at its own climbing speed.
GLIDESLOPE_TEST(the_climb_lesson_flown_by_the_book_leaves_an_empty_debrief) {
    std::size_t walked = 0;
    for (const std::string& id : light_aircraft()) {
        const Result flown = fly_a_climb_and_descent(id, 0.0);
        std::printf("  %-6s %zu of %zu stages\n", id.c_str(), flown.completed,
                    flown.stages);
        for (const std::string& said : flown.debrief) {
            std::printf("    %s\n", said.c_str());
        }
        check(flown.completed == flown.stages,
              id + " got through the climb and the descent, " +
                  std::to_string(flown.completed) + " of " +
                  std::to_string(flown.stages));
        check(flown.debrief.empty(),
              id + " flown by the book says nothing, and it said " +
                  std::to_string(flown.debrief.size()));
        ++walked;
    }
    check(walked == 4, "all four climbed and descended");
}

// **A climb flown at the wrong speed is named for it.**
GLIDESLOPE_TEST(a_climb_at_the_wrong_speed_is_named_in_the_debrief) {
    const Result fast = fly_a_climb_and_descent("c172p", 30.0);
    for (const std::string& said : fast.debrief) {
        std::printf("    %s\n", said.c_str());
    }
    check(!fast.debrief.empty(), "a climb thirty knots fast is not faultless");
    const bool named = std::any_of(
        fast.debrief.begin(), fast.debrief.end(), [](const std::string& s) {
            return s == "Hold the climbing speed";
        });
    check(named, "and the debrief says to hold the climbing speed");
}
