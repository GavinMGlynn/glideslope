#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/autopilot.hpp"
#include "sim/controller.hpp"
#include "sim/lander.hpp"
#include "sim/lesson.hpp"
#include "sim/lesson_run.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <set>
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

// The lesson this aeroplane class has for `exercise`, or nothing if its
// class has none. Not every class can have every lesson: four of the six
// name speeds an aeroplane publishes, and eleven of the sixteen publish none.
std::optional<Lesson> lesson_for(const glideslope::sim::CatalogueEntry& entry,
                                 const std::string& exercise) {
    const std::string want =
        std::string(glideslope::sim::name_of(entry.aircraft_class)) + "-" + exercise;
    const auto lessons = glideslope::sim::read_lessons(data());
    const auto it = std::find_if(lessons.begin(), lessons.end(),
                                 [&](const Lesson& l) { return l.id == want; });
    if (it == lessons.end()) {
        return std::nullopt;
    }
    return *it;
}

// Every aeroplane in the roster whose class has a lesson for `exercise`.
// A class without one is named, so a lesson quietly lost from the data shows
// up as a class that stopped being taught rather than as a test that got
// shorter.
std::vector<std::string> everyone_taught(const std::string& exercise) {
    std::vector<std::string> out;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        if (lesson_for(entry, exercise)) {
            out.push_back(entry.id);
        }
    }
    return out;
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
        {"a reference there is none of", "name N\nstage S\ndo X\nuntil a >= vne\n"},
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
    std::vector<std::string> where; // the stage each fault happened in
    std::size_t completed = 0;
    std::size_t stages = 0;
    // The attitude the aeroplane actually held once it was off the ground,
    // which is what the lesson's band has to be set from rather than guessed.
    double least_theta_deg = 1e9;
    double most_theta_deg = -1e9;
    // The speed she was doing as the roll ended, which is what the lesson
    // judges "rotating early" on.
    double off_at_kts = 0.0;
    // And the speed through the climb away, which is what its band is set
    // from.
    double climb_least_kts = 1e9;
    double climb_most_kts = -1e9;
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

    // **Its own class's lesson, not the light aircraft's.** This lookup was
    // hardcoded to `light-aircraft-take-off` and flew the Mosquito against
    // the Cessna's lesson, whose climbing band is a literal 45 to 120 knots -
    // so a Mosquito climbing correctly at 150 was told to hold its climbing
    // speed. The lesson was right and the test was handing it the wrong one.
    const auto found = lesson_for(entry, "take-off");
    check(found.has_value(), entry.id + " has a take-off lesson for its class");
    const auto it = &*found;
    LessonRun run(*found, glideslope::sim::LessonSpeeds{speeds.rotate_kts,
                                                        speeds.climb_kts});

    glideslope::sim::Departure departure(aircraft, runway, speeds);
    double out_least = 1e9;
    double out_most = -1e9;
    double out_off_at_kts = 0.0;
    double out_climb_least = 1e9;
    double out_climb_most = -1e9;
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
            if (run.stage() == 2) {
                const double kts = aircraft.property("velocities/vc-kts");
                out_climb_least = std::min(out_climb_least, kts);
                out_climb_most = std::max(out_climb_most, kts);
            }
        }
    }
    std::vector<std::string> where;
    for (const glideslope::sim::Fault& fault : run.debrief()) {
        where.push_back(fault.stage);
    }
    return {run.debrief_lines(), where, run.completed(), it->stages.size(), out_least,
            out_most, out_off_at_kts, out_climb_least, out_climb_most};
}

} // namespace

// **Flown by the book, the take-off lesson leaves an empty debrief** - for
// every light aeroplane in the roster, not one of them.
GLIDESLOPE_TEST(the_take_off_lesson_flown_by_the_book_leaves_an_empty_debrief) {
    const auto taught = everyone_taught("take-off");
    std::size_t walked = 0;
    for (const std::string& id : taught) {
        const Flown flown = fly_the_take_off(id, 0.0, 1.0);
        check(flown.completed == flown.stages,
              id + " got through all " + std::to_string(flown.stages) +
                  " stages, not " + std::to_string(flown.completed));
        std::printf("  %-13s off at %3.0f kt, pitch %5.1f to %5.1f, climbed at "
                    "%3.0f to %3.0f kt\n",
                    id.c_str(), flown.off_at_kts, flown.least_theta_deg,
                    flown.most_theta_deg, flown.climb_least_kts,
                    flown.climb_most_kts);
        for (std::size_t i = 0; i < flown.debrief.size(); ++i) {
            std::printf("  %s: [%s] %s\n", id.c_str(),
                        i < flown.where.size() ? flown.where[i].c_str() : "?",
                        flown.debrief[i].c_str());
        }
        check(flown.debrief.empty(),
              id + " flown by the book has nothing in its debrief, and it has " +
                  std::to_string(flown.debrief.size()));
        ++walked;
    }
    check(walked == taught.size(),
          "every aeroplane whose class teaches a take-off was flown");
    check(walked == 5, "five of them: the four light aircraft and the Mosquito");
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
    check(glideslope::sim::read_number("stall", number) && number.named() &&
              number.reference == "stall",
          "stall is one of them");
    // **`stall` and `start` both begin with \"st\"**, and the first match
    // wins, so both must still read as themselves.
    check(glideslope::sim::read_number("start-150", number) && number.named() &&
              number.reference == "start" && std::abs(number.offset + 150.0) < 1e-9,
          "and start is not swallowed by stall");
    check(glideslope::sim::read_number("stall+6", number) && number.named() &&
              number.reference == "stall" && std::abs(number.offset - 6.0) < 1e-9,
          "nor stall by start");
    check(!glideslope::sim::read_number("vne", number), "a name there is none of");
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
                                 [](const Lesson& l) { return l.id == "light-aircraft-take-off"; });
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

    const auto found = lesson_for(entry, "approach-and-landing");
    check(found.has_value(), entry.id + " has an approach lesson for its class");
    const auto it = &*found;
    // **An approach lesson names no climbing speed, so not publishing one is
    // no reason to refuse the lesson.** The Learjet publishes a stall speed
    // and no rate of climb; letting that throw escape would have denied it an
    // approach it can perfectly well fly.
    double rotate = 0.0;
    double climb = 0.0;
    try {
        const auto departure = glideslope::sim::departure_speeds(data(), entry.model);
        rotate = departure.rotate_kts;
        climb = departure.climb_kts;
    } catch (const std::exception&) {
    }
    LessonRun run(*it, glideslope::sim::LessonSpeeds{
                           rotate, climb, published.vref_kts,
                           published.vref_kts / 1.3});

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
    const auto taught = everyone_taught("approach-and-landing");
    std::size_t walked = 0;
    for (const std::string& id : taught) {
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
    check(walked == taught.size(), "every aeroplane taught an approach was landed");
    check(walked == 6,
          "six: the four light aircraft, the Mosquito and the Learjet");
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
    // **An aeroplane that publishes no figures still has lessons.** Eleven of
    // the sixteen publish no stall speed and most publish no rate of climb,
    // and `departure_speeds` and `approach_speeds` throw rather than guess -
    // which is right, a reference speed invented is a reference speed that
    // means nothing. A lesson that names none of them, as the turns lesson
    // does, is flown all the same; one that names them would fail to resolve
    // and a test would catch it.
    try {
        const auto departure = glideslope::sim::departure_speeds(data(), entry.model);
        out.speeds.rotate_kts = departure.rotate_kts;
        out.speeds.climb_kts = departure.climb_kts;
    } catch (const std::exception&) {
    }
    try {
        const auto approach = glideslope::sim::approach_speeds(data(), entry.model);
        out.speeds.vref_kts = approach.vref_kts;
        // The published stall is the reference speed divided by the 1.3 that
        // made it, which is how `sim::approach_speeds` built it.
        out.speeds.stall_kts = approach.vref_kts / 1.3;
    } catch (const std::exception&) {
    }
    out.start_agl_ft = agl_ft;
    out.start_heading_deg = 90.0;
    return out;
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
    const auto found = lesson_for(glideslope::sim::find_aircraft(data(), id), "turns");
    check(found.has_value(), id + " has a turns lesson for its class");
    const Lesson lesson = *found;
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
    const auto found =
        lesson_for(glideslope::sim::find_aircraft(data(), id), "climb-and-descent");
    check(found.has_value(), id + " has a climb lesson for its class");
    const Lesson lesson = *found;
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
    const auto taught = everyone_taught("climb-and-descent");
    std::size_t walked = 0;
    for (const std::string& id : taught) {
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
    check(walked == taught.size(), "every aeroplane taught a climb flew one");
    check(walked == 5, "five: the four light aircraft and the Mosquito");
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

namespace {

// **A stall, entered the way one is entered**: throttle closed, the height
// held, the nose rising as the speed decays until the wing gives up. The
// recovery is the control column forward and full power. `sloppy` recovers
// late and lazily, which is what loses the height this lesson is about.
Result fly_a_stall(const std::string& id, bool sloppy) {
    const auto stall_entry = glideslope::sim::find_aircraft(data(), id);
    // **A stall is practised where its aeroplane practises it.** A light
    // aeroplane decelerates to the stall in a few hundred feet; a clean jet
    // at idle descends while it slows, and doing that from five thousand feet
    // puts it in the ground before it stalls.
    const bool light = stall_entry.aircraft_class ==
                       glideslope::sim::AircraftClass::light_aircraft;
    InFlight f = airborne(id, light ? 5000.0 : 20000.0);
    const auto found = lesson_for(stall_entry, "stalls");
    check(found.has_value(), id + " has a stalls lesson for its class");
    const Lesson lesson = *found;
    LessonRun run(lesson, f.speeds);

    glideslope::sim::Controls controls;
    controls.throttle = 0.6;
    glideslope::sim::Autopilot autopilot(*f.aircraft, controls);
    glideslope::sim::AutopilotModes modes = autopilot.modes();
    modes.heading_deg = f.start_heading_deg;
    modes.altitude_ft = f.start_agl_ft;
    autopilot.set(modes);

    Result out;
    out.stages = lesson.stages.size();
    const int settling = 20 * steps_per_second;
    bool recovering = false;
    bool recovery_set = false;
    // **The sloppy recovery is the same recovery, started late.** It is timed
    // from the moment the lesson calls the stall rather than from a speed,
    // because an aeroplane mushing in a stall does not go on slowing - wait
    // for a speed six knots below the stall and it never comes, the nose
    // stays up, and she descends all the way to the ground. That is not a
    // late recovery, it is no recovery, and it taught the test nothing.
    const int dawdle = sloppy ? 35 * steps_per_second : 0;
    std::int64_t stalled_at = -1;
    for (int tick = 0; tick < 600 * steps_per_second && !run.finished(); ++tick) {
        glideslope::sim::Controls c = autopilot.fly();
        if (tick >= settling && !recovering) {
            // Throttle closed: the autopilot holds the height by raising the
            // nose, and she slows towards the stall of her own accord.
            c.throttle = 0.0;
            if (run.stage() >= 1) {
                if (stalled_at < 0) {
                    stalled_at = tick;
                }
                if (tick - stalled_at >= dawdle) {
                    recovering = true;
                }
            }
        }
        if (recovering) {
            // **The recovery is flown by the autopilot, not by a fixed
            // control position.** A control column held forward by the same
            // amount recovers a Cessna and flies a Learjet into the ground:
            // the aeroplanes differ by a factor of three in speed and far
            // more in inertia. Asking for a speed well above the stall and a
            // gentle descent is the same instruction to every aeroplane, and
            // each one flies it with its own controls.
            if (!recovery_set) {
                recovery_set = true;
                modes.altitude_ft.reset();
                modes.vertical_speed_fpm = -600.0;
                modes.airspeed_kts = f.speeds.stall_kts * 1.5;
                autopilot.set(modes);
            }
            c = autopilot.fly();
            c.throttle = 1.0;
        }
        f.aircraft->set_controls(c);
        f.aircraft->step();
        if (tick >= settling) {
            run.update(*f.aircraft, tick);
            out.lowest_agl_ft =
                std::min(out.lowest_agl_ft, f.aircraft->property("position/h-agl-ft"));
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

} // namespace

// **A stall entered and recovered properly loses little height**, and the
// lesson says nothing about it.
GLIDESLOPE_TEST(the_stalls_lesson_flown_by_the_book_leaves_an_empty_debrief) {
    const auto taught = everyone_taught("stalls");
    std::size_t walked = 0;
    for (const std::string& id : taught) {
        const Result flown = fly_a_stall(id, false);
        std::printf("  %-13s lowest %6.0f ft, %zu of %zu stages\n", id.c_str(),
                    flown.lowest_agl_ft, flown.completed, flown.stages);
        for (const std::string& said : flown.debrief) {
            std::printf("    %s\n", said.c_str());
        }
        check(flown.completed == flown.stages,
              id + " stalled and recovered, " + std::to_string(flown.completed) +
                  " of " + std::to_string(flown.stages) + " stages");
        check(flown.debrief.empty(),
              id + " flown by the book says nothing, and it said " +
                  std::to_string(flown.debrief.size()));
        ++walked;
    }
    check(walked == taught.size(), "every aeroplane taught a stall was stalled");
    check(walked == 6,
          "six: the four light aircraft, the Mosquito and the Learjet");
}

// **A stall recovered late and lazily loses height, and is named for it.**
GLIDESLOPE_TEST(a_stall_recovered_badly_is_named_in_the_debrief) {
    const Result sloppy = fly_a_stall("c172p", true);
    std::printf("  c172p recovered badly: lowest %.0f ft of 5000\n",
                sloppy.lowest_agl_ft);
    for (const std::string& said : sloppy.debrief) {
        std::printf("    %s\n", said.c_str());
    }
    check(!sloppy.debrief.empty(), "a lazy recovery is not faultless");
    const bool named = std::any_of(
        sloppy.debrief.begin(), sloppy.debrief.end(), [](const std::string& s) {
            return s == "Recover with the least height you can";
        });
    check(named, "and the debrief says so");
}

namespace {

// The turns lesson written for this aeroplane's own class.
Lesson turns_for(const glideslope::sim::CatalogueEntry& entry) {
    const std::string want =
        std::string(glideslope::sim::name_of(entry.aircraft_class)) + "-turns";
    const auto lessons = glideslope::sim::read_lessons(data());
    const auto it = std::find_if(lessons.begin(), lessons.end(),
                                 [&](const Lesson& l) { return l.id == want; });
    check(it != lessons.end(), want + " is in the data");
    return *it;
}

// **Any aeroplane, turned ninety degrees left by the autopilot.** Flown at
// three thousand feet, where every class of them has power in hand: near its
// ceiling a light aeroplane cannot sustain a thirty degree bank and the
// autopilot banks to its limit anyway, which is a fault of the autopilot and
// recorded as its own item.
Result turn_any(const std::string& id) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    InFlight f = airborne(id, 3000.0);
    const Lesson lesson = turns_for(entry);
    LessonRun run(lesson, f.speeds);

    glideslope::sim::Controls controls;
    controls.throttle = 0.7;
    glideslope::sim::Autopilot autopilot(*f.aircraft, controls);
    glideslope::sim::AutopilotModes modes = autopilot.modes();
    modes.altitude_ft = f.start_agl_ft;
    modes.heading_deg = f.start_heading_deg;
    modes.airspeed_kts = entry.start_airspeed_kts;
    autopilot.set(modes);

    Result out;
    out.stages = lesson.stages.size();
    const int settling = 20 * steps_per_second;
    for (int tick = 0; tick < 400 * steps_per_second && !run.finished(); ++tick) {
        if (tick == settling) {
            modes.heading_deg = f.start_heading_deg - 90.0;
            autopilot.set(modes);
        }
        f.aircraft->set_controls(autopilot.fly());
        f.aircraft->step();
        if (tick >= settling) {
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

// **Every aeroplane in the roster turns to its own class's lesson.** Not one
// of each class: all sixteen, because a lesson that suits the A320 and not
// the A380 is a lesson that teaches one of them wrongly.
GLIDESLOPE_TEST(every_aeroplane_flies_the_turns_lesson_of_its_own_class) {
    const auto roster = glideslope::sim::read_catalogue(data());
    check(roster.size() == 16, "sixteen aeroplanes");
    std::size_t walked = 0;
    std::set<std::string> classes_seen;
    for (const auto& entry : roster) {
        const Result flown = turn_any(entry.id);
        std::printf("  %-13s %-17s banked %6.1f, height %5.0f of 3000, %zu/%zu\n",
                    entry.id.c_str(),
                    std::string(glideslope::sim::name_of(entry.aircraft_class)).c_str(),
                    flown.steepest_bank_deg, flown.lowest_agl_ft, flown.completed,
                    flown.stages);
        for (const std::string& said : flown.debrief) {
            std::printf("      %s\n", said.c_str());
        }
        check(flown.completed == flown.stages,
              entry.id + " got round the turn, " + std::to_string(flown.completed) +
                  " of " + std::to_string(flown.stages));
        check(flown.debrief.empty(),
              entry.id + " flown by the book says nothing, and it said " +
                  std::to_string(flown.debrief.size()));
        classes_seen.insert(std::string(glideslope::sim::name_of(entry.aircraft_class)));
        ++walked;
    }
    check(walked == 16, "every aeroplane was turned, not " + std::to_string(walked));
    check(classes_seen.size() == glideslope::sim::aircraft_class_count,
          "and all seven classes were covered, not " +
              std::to_string(classes_seen.size()));
}

namespace {

// The largest a control moved in one step, over every control there is.
// The seventeen controls, in the order `Controls::as_list()` gives them, so
// a step can name the control it happened in rather than only its size.
const char* control_name(std::size_t i) {
    static const char* names[] = {
        "elevator",   "aileron",     "rudder",        "throttle",
        "mixture",    "flaps",       "left brake",    "right brake",
        "pitch trim", "propeller",   "gear",          "supercharger",
        "throttle offset 0",         "throttle offset 1",
        "cooling flap 0",            "cooling flap 1",
        "speedbrake"};
    return i < std::size(names) ? names[i] : "?";
}

std::size_t which_stepped(const glideslope::sim::Controls& was,
                          const glideslope::sim::Controls& now) {
    const auto before = was.as_list();
    const auto after = now.as_list();
    std::size_t worst = 0;
    double most = -1.0;
    for (std::size_t i = 0; i < before.size(); ++i) {
        const double d = std::abs(after[i] - before[i]);
        if (d > most) {
            most = d;
            worst = i;
        }
    }
    return worst;
}

double worst_step(const glideslope::sim::Controls& was,
                  const glideslope::sim::Controls& now) {
    const auto before = was.as_list();
    const auto after = now.as_list();
    double worst = 0.0;
    for (std::size_t i = 0; i < before.size(); ++i) {
        worst = std::max(worst, std::abs(after[i] - before[i]));
    }
    return worst;
}

struct Demonstrated {
    std::vector<std::string> debrief;
    std::size_t completed = 0;
    std::size_t stages = 0;
    double worst_to_pilot = 0.0;
    double worst_to_ai = 0.0;
    // The fastest any control moved while the pilot had it: their hands
    // move at a hand's pace, and so must the aeroplane's controls.
    double worst_settled = 0.0;
    // The furthest her nose got from the runway heading on the take-off
    // roll, which is what a band on keeping straight has to be set from.
    double worst_swing_deg = 0.0;
};

// **The instructor flies the demonstration, hands over, and takes it back.**
// The aircraft is flown through a `sim::Controller`, which is what a swap
// actually is: the same aeroplane, listening to somebody else. Part-way
// through the lesson the controls go to the pilot - whose hands are nowhere
// near where the AI had them - and later come back.
Demonstrated demonstrate(const std::string& id, const std::string& exercise) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    InFlight f = airborne(id, 3000.0);
    const auto found = lesson_for(entry, exercise);
    check(found.has_value(), id + " has a " + exercise + " lesson");
    LessonRun run(*found, f.speeds);

    glideslope::sim::Controls held;
    held.throttle = 0.7;
    glideslope::sim::Controller controller(*f.aircraft, held);
    controller.to_ai();
    check(controller.autopilot() != nullptr, "the AI has an autopilot to be told");
    glideslope::sim::AutopilotModes modes = controller.autopilot()->modes();
    modes.altitude_ft = f.start_agl_ft;
    modes.heading_deg = f.start_heading_deg;
    modes.airspeed_kts = entry.start_airspeed_kts;
    controller.autopilot()->set(modes);

    // **The pilot's hands are somewhere else entirely**, which is the point:
    // a handover that stepped would step by this much.
    // Different from where the AI has them - which is the point - but a
    // position a pilot might actually hold. A first draft used full back
    // stick and a closed throttle for thirty seconds, which does not test a
    // handover: it tests engaging an autopilot on an aeroplane that is
    // already beyond its own pitch limits, and it stepped 0.56 doing it.
    // **Different from where the AI has them, and flying the aeroplane.**
    // Two earlier drafts held enough aileron to roll her into a spiral - the
    // second reached pitch -26 and bank -62 in fifteen seconds - and then
    // measured the autopilot recovering from it and called that a jolt. An
    // autopilot handed an aeroplane sixteen degrees outside the pitch
    // envelope it is allowed to command *must* move the controls. That is a
    // finding of its own and is a tail; it is not what this test is for.
    glideslope::sim::Controls pilot;
    pilot.throttle = 0.55;
    pilot.elevator = 0.02;
    pilot.aileron = 0.0;
    pilot.rudder = 0.0;
    controller.set_pilot(pilot);

    Demonstrated out;
    out.stages = found->stages.size();
    const int settling = 15 * steps_per_second;
    // **Demonstrated first, then handed over**, which is the order the item
    // names. An earlier draft handed over part-way through and judged a
    // demonstration that had reached one stage of three - an empty debrief
    // then says almost nothing.
    int hand_over = -1;
    int take_back = -1;
    // **The controls come back three seconds later.** The item is about the
    // two swaps, not about what the aeroplane does between them: a pilot
    // handed a banked aeroplane who holds neutral aileron gets a spiral, as
    // two earlier drafts of this test discovered, and then the autopilot is
    // being handed a diving turn rather than an aeroplane.

    glideslope::sim::Controls last = held;
    bool first = true;
    bool demonstrated = false;
    for (int tick = 0; tick < 400 * steps_per_second; ++tick) {
        // The demonstration is over when the lesson is; the swaps follow it.
        if (!demonstrated && run.finished()) {
            demonstrated = true;
            hand_over = tick + steps_per_second;
            take_back = hand_over + 3 * steps_per_second;
        }
        if (demonstrated && tick > take_back + 2 * steps_per_second) {
            break;
        }
        if (tick == settling) {
            modes.heading_deg = f.start_heading_deg - 90.0;
            controller.autopilot()->set(modes);
        }
        if (tick == hand_over) {
            controller.to_pilot();
        }
        if (tick == take_back) {
            // **Taking the controls back is one thing; deciding what to do
            // with them is another.** The autopilot engages holding what the
            // aeroplane is doing, which is what makes the swap step-free. A
            // first draft commanded a ninety-degree turn in the same frame
            // and measured a step of 1.29 - most of a control's travel - and
            // that was the command, not the handover. The instructor takes
            // the aeroplane first and turns it a second later.
            controller.to_ai();
            check(controller.autopilot() != nullptr, "the AI has its autopilot back");
        }
        if (tick == take_back + steps_per_second) {
            controller.autopilot()->set(modes);
        }
        const glideslope::sim::Controls now = controller.fly();
        if (!first) {
            const double step = worst_step(last, now);
            // **The swap is one frame, and that is what "no step" is about.**
            // The frames after it are the new pilot flying - the autopilot
            // correcting what it has been handed, or a person moving their
            // hands - and a control moving then is not a step, it is
            // somebody flying. Measuring a whole second after the swap
            // measures the flying and calls it a jolt.
            if (tick == hand_over) {
                out.worst_to_pilot = step;
            } else if (tick == take_back) {
                out.worst_to_ai = step;
                std::printf("      %s at the swap: pitch %.1f, bank %.1f, %.0f kt\n",
                            id.c_str(), f.aircraft->property("attitude/theta-deg"),
                            f.aircraft->property("attitude/phi-deg"),
                            f.aircraft->property("velocities/vc-kts"));
            } else if (hand_over > 0 && tick > hand_over && tick < take_back) {
                // While the pilot has it, the controls travel towards their
                // hands at a hand's pace and no faster.
                out.worst_settled = std::max(out.worst_settled, step);
            }
        }
        first = false;
        last = now;
        f.aircraft->set_controls(now);
        f.aircraft->step();
        if (tick >= settling && !demonstrated) {
            run.update(*f.aircraft, tick);
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

} // namespace

// **The instructor demonstrates, hands over, and takes back with no step in
// any control.** A pilot's hand moves a control through its full travel in a
// second - `sim/controller.hpp` says so - which at 120 Hz is about 0.017 of
// its travel a step. Every one of the seventeen controls is measured at both
// swaps.
GLIDESLOPE_TEST(an_instructor_hands_over_and_takes_back_with_no_step_in_any_control) {
    // A control moving at a pilot's hand pace, with a little room for the
    // step the swap itself lands on.
    const double a_hands_pace = 2.0 / steps_per_second + 0.004;
    std::size_t walked = 0;
    const auto four = everyone_taught("turns");
    check(!four.empty(), "some aeroplane is taught turns");
    for (const std::string& id : four) {
        const Demonstrated shown = demonstrate(id, "turns");
        std::printf("  %-13s demonstration %zu/%zu stages, worst step %.4f to the "
                    "pilot, %.4f back, %.4f settled\n",
                    id.c_str(), shown.completed, shown.stages, shown.worst_to_pilot,
                    shown.worst_to_ai, shown.worst_settled);
        for (const std::string& said : shown.debrief) {
            std::printf("      %s\n", said.c_str());
        }
        check(shown.debrief.empty(),
              id + " flew the demonstration inside the lesson's limits, and said " +
                  std::to_string(shown.debrief.size()) + " things");
        check(shown.worst_to_pilot <= a_hands_pace,
              id + " stepped " + std::to_string(shown.worst_to_pilot) +
                  " handing over, and a hand moves " + std::to_string(a_hands_pace));
        check(shown.worst_to_ai <= a_hands_pace,
              id + " stepped " + std::to_string(shown.worst_to_ai) +
                  " taking back, and a hand moves " + std::to_string(a_hands_pace));
        check(shown.worst_settled <= a_hands_pace,
              id + " moved a control " + std::to_string(shown.worst_settled) +
                  " in one step while the pilot held it, and a hand moves " +
                  std::to_string(a_hands_pace));
        ++walked;
    }
    check(walked == four.size(),
          "every aeroplane taught the exercise demonstrated it: " +
              std::to_string(walked) + " of " + std::to_string(four.size()));
}

namespace {

// **A take-off demonstrated, then handed over.** The aeroplane stands on the
// runway and the AI pilot flies it off - which it could not do at all until
// `Controller` was given the take-off autopilot - and the lesson watches.
Demonstrated demonstrate_a_take_off(const std::string& id) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto speeds = glideslope::sim::departure_speeds(data(), entry.model);

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

    const auto found = lesson_for(entry, "take-off");
    check(found.has_value(), id + " has a take-off lesson");
    LessonRun run(*found, glideslope::sim::LessonSpeeds{speeds.rotate_kts,
                                                        speeds.climb_kts, 0.0, 0.0});

    glideslope::sim::Controls standing;
    glideslope::sim::Controller controller(aircraft, standing);
    controller.to_ai_take_off(runway, speeds);
    check(controller.departure() != nullptr, "the AI pilot has a take-off to fly");

    glideslope::sim::Controls pilot;
    pilot.throttle = 0.6;
    pilot.elevator = 0.0;
    controller.set_pilot(pilot);

    Demonstrated out;
    out.stages = found->stages.size();
    int hand_over = -1;
    int take_back = -1;
    glideslope::sim::Controls last = standing;
    bool first = true;
    bool demonstrated = false;
    for (int tick = 0; tick < 400 * steps_per_second; ++tick) {
        if (!demonstrated && run.finished()) {
            demonstrated = true;
            hand_over = tick + steps_per_second;
            take_back = hand_over + 3 * steps_per_second;
        }
        if (demonstrated && take_back > 0 && tick > take_back + steps_per_second) {
            break;
        }
        if (tick == hand_over) {
            controller.to_pilot();
        }
        if (tick == take_back) {
            controller.to_ai();
        }
        const glideslope::sim::Controls now = controller.fly();
        if (!first) {
            const double step = worst_step(last, now);
            if (tick == hand_over) {
                out.worst_to_pilot = step;
            } else if (tick == take_back) {
                out.worst_to_ai = step;
            }
        }
        first = false;
        last = now;
        aircraft.set_controls(now);
        aircraft.step();
        if (!demonstrated) {
            run.update(aircraft, tick);
            if (run.stage() == 0) {
                out.worst_swing_deg = std::max(
                    out.worst_swing_deg,
                    std::abs(std::remainder(
                        aircraft.property("attitude/psi-deg") - runway.heading_deg,
                        360.0)));
            }
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

} // namespace

// **The instructor demonstrates a take-off, then hands over.** Until
// `sim::Controller` was given the take-off autopilot its AI could only be
// handed an aeroplane already flying, so a take-off had no demonstration to
// hand over from.
GLIDESLOPE_TEST(an_instructor_demonstrates_a_take_off_and_hands_it_over) {
    const double a_hands_pace = 2.0 / steps_per_second + 0.004;
    const auto flown = everyone_taught("take-off");
    check(!flown.empty(), "some aeroplane is taught take-offs");
    std::size_t walked = 0;
    for (const std::string& id : flown) {
        const Demonstrated shown = demonstrate_a_take_off(id);
        std::printf("  %-13s take-off %zu/%zu stages, worst step %.4f over, "
                    "%.4f back, swung %.1f deg on the roll\n",
                    id.c_str(), shown.completed, shown.stages, shown.worst_to_pilot,
                    shown.worst_to_ai, shown.worst_swing_deg);
        for (const std::string& said : shown.debrief) {
            std::printf("      %s\n", said.c_str());
        }
        check(shown.completed == shown.stages,
              id + " flew the whole take-off, " + std::to_string(shown.completed) +
                  " of " + std::to_string(shown.stages) + " stages");
        check(shown.debrief.empty(),
              id + " demonstrated it inside the lesson's limits, and said " +
                  std::to_string(shown.debrief.size()) + " things");
        check(shown.worst_to_pilot <= a_hands_pace,
              id + " stepped " + std::to_string(shown.worst_to_pilot) +
                  " handing over");
        check(shown.worst_to_ai <= a_hands_pace,
              id + " stepped " + std::to_string(shown.worst_to_ai) + " taking back");
        ++walked;
    }
    check(walked == flown.size(),
          "every aeroplane taught the exercise demonstrated it: " +
              std::to_string(walked) + " of " + std::to_string(flown.size()));
}

namespace {

// **An approach demonstrated, then handed over.** Two miles out on the
// glidepath, flown down by the AI pilot through a `Controller` - which is
// what makes it a demonstration rather than a frontend flying a Lander.
Demonstrated demonstrate_an_approach(const std::string& id) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);

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
    ic.airspeed_kts = published.vref_kts;
    ic.engine_running = true;
    ic.gear = 1.0;
    aircraft.initialize(ic);

    const auto found = lesson_for(entry, "approach-and-landing");
    check(found.has_value(), id + " has an approach lesson");
    double rotate = 0.0;
    double climb = 0.0;
    try {
        const auto d = glideslope::sim::departure_speeds(data(), entry.model);
        rotate = d.rotate_kts;
        climb = d.climb_kts;
    } catch (const std::exception&) {
    }
    LessonRun run(*found, glideslope::sim::LessonSpeeds{rotate, climb,
                                                        published.vref_kts,
                                                        published.vref_kts / 1.3});

    glideslope::sim::Controls flying;
    flying.throttle = 0.4;
    flying.gear = 1.0;
    glideslope::sim::Controller controller(aircraft, flying);
    controller.to_ai_approach(runway, published);
    check(controller.lander() != nullptr, "the AI pilot has an approach to fly");

    glideslope::sim::Controls pilot;
    pilot.throttle = 0.25;
    pilot.gear = 1.0;
    controller.set_pilot(pilot);

    Demonstrated out;
    out.stages = found->stages.size();
    int hand_over = -1;
    int take_back = -1;
    glideslope::sim::Controls last = flying;
    bool first = true;
    bool demonstrated = false;
    for (int tick = 0; tick < 600 * steps_per_second; ++tick) {
        if (!demonstrated && run.finished()) {
            demonstrated = true;
            hand_over = tick + steps_per_second / 2;
            take_back = hand_over + steps_per_second;
        }
        if (demonstrated && take_back > 0 && tick > take_back + steps_per_second / 2) {
            break;
        }
        if (tick == hand_over) {
            controller.to_pilot();
        }
        if (tick == take_back) {
            controller.to_ai();
        }
        const glideslope::sim::Controls now = controller.fly();
        if (!first) {
            const double step = worst_step(last, now);
            if (tick == hand_over) {
                out.worst_to_pilot = step;
            } else if (tick == take_back) {
                out.worst_to_ai = step;
                if (step > 0.05) {
                    std::printf("      %s stepped %.3f in the %s, at %.0f kt, "
                                "pitch %.1f, elevator %.3f to %.3f, agl %.0f\n",
                                id.c_str(), step, control_name(which_stepped(last, now)),
                                aircraft.property("velocities/vc-kts"),
                                aircraft.property("attitude/theta-deg"),
                                last.elevator, now.elevator,
                                aircraft.property("position/h-agl-ft"));
                }
            }
        }
        first = false;
        last = now;
        aircraft.set_controls(now);
        aircraft.step();
        if (!demonstrated) {
            run.update(aircraft, tick);
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

} // namespace

// **The instructor demonstrates an approach, then hands over.**
GLIDESLOPE_TEST(an_instructor_demonstrates_an_approach_and_hands_it_over) {
    const double a_hands_pace = 2.0 / steps_per_second + 0.004;
    const auto flown = everyone_taught("approach-and-landing");
    check(!flown.empty(), "some aeroplane is taught approaches");
    std::size_t walked = 0;
    for (const std::string& id : flown) {
        const Demonstrated shown = demonstrate_an_approach(id);
        std::printf("  %-13s approach %zu/%zu stages, worst step %.4f over, "
                    "%.4f back\n",
                    id.c_str(), shown.completed, shown.stages, shown.worst_to_pilot,
                    shown.worst_to_ai);
        for (const std::string& said : shown.debrief) {
            std::printf("      %s\n", said.c_str());
        }
        check(shown.completed == shown.stages,
              id + " flew the whole approach, " + std::to_string(shown.completed) +
                  " of " + std::to_string(shown.stages) + " stages");
        check(shown.debrief.empty(),
              id + " demonstrated it inside the lesson's limits, and said " +
                  std::to_string(shown.debrief.size()) + " things");
        check(shown.worst_to_pilot <= a_hands_pace,
              id + " stepped " + std::to_string(shown.worst_to_pilot) +
                  " handing over");
        check(shown.worst_to_ai <= a_hands_pace,
              id + " stepped " + std::to_string(shown.worst_to_ai) + " taking back");
        ++walked;
    }
    check(walked == flown.size(),
          "every aeroplane taught the exercise demonstrated it: " +
              std::to_string(walked) + " of " + std::to_string(flown.size()));
}

namespace {

// **A demonstration flown in the air, through a `Controller`.** The climb and
// the stall differ from the turns only in what the instructor asks the AI
// pilot for, so everything else is shared: the aeroplane, the lesson
// watching, the two swaps, and what is measured at them.
//
// `begin` sets what the autopilot holds before the exercise starts. `fly` is
// called every tick of the demonstration, with the ticks since it began, and
// is where the instructor tells the AI what to do next. **Neither of them
// touches a control.** The climb and the stall were both flown by driving an
// autopilot directly and reaching into the controls it returned - the stall
// closed the throttle by hand - and an exercise flown that way has no
// controller to hand over, which is why this item could not be ticked.
using Begin = std::function<void(glideslope::sim::AutopilotModes&, const InFlight&)>;
using Fly = std::function<void(glideslope::sim::Autopilot&, const LessonRun&,
                               const InFlight&, int)>;

Demonstrated demonstrate_in_the_air(const std::string& id, const std::string& exercise,
                                    double start_ft, int settling_s, const Begin& begin,
                                    const Fly& fly) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    InFlight f = airborne(id, start_ft);
    const auto found = lesson_for(entry, exercise);
    check(found.has_value(), id + " has a " + exercise + " lesson for its class");
    LessonRun run(*found, f.speeds);

    glideslope::sim::Controls held;
    held.throttle = 0.7;
    glideslope::sim::Controller controller(*f.aircraft, held);
    controller.to_ai();
    check(controller.autopilot() != nullptr, "the AI has an autopilot to be told");
    glideslope::sim::AutopilotModes modes = controller.autopilot()->modes();
    modes.heading_deg = f.start_heading_deg;
    modes.altitude_ft = f.start_agl_ft;
    modes.airspeed_kts = entry.start_airspeed_kts;
    begin(modes, f);
    controller.autopilot()->set(modes);

    // The pilot's hands: somewhere a pilot might actually hold them, and
    // nowhere near where the AI has the aeroplane's controls.
    glideslope::sim::Controls pilot;
    pilot.throttle = 0.55;
    pilot.elevator = 0.02;
    controller.set_pilot(pilot);

    Demonstrated out;
    out.stages = found->stages.size();
    const int settling = settling_s * steps_per_second;
    int hand_over = -1;
    int take_back = -1;
    glideslope::sim::Controls last = held;
    bool first = true;
    bool demonstrated = false;
    for (int tick = 0; tick < 900 * steps_per_second; ++tick) {
        // Demonstrated first, then handed over - the order the item names.
        if (!demonstrated && tick > settling && run.finished()) {
            demonstrated = true;
            hand_over = tick + steps_per_second;
            take_back = hand_over + 3 * steps_per_second;
        }
        if (demonstrated && tick > take_back + 2 * steps_per_second) {
            break;
        }
        if (tick >= settling && !demonstrated) {
            fly(*controller.autopilot(), run, f, tick - settling);
        }
        if (tick == hand_over) {
            controller.to_pilot();
        }
        if (tick == take_back) {
            controller.to_ai();
            check(controller.autopilot() != nullptr, "the AI has its autopilot back");
        }
        const glideslope::sim::Controls now = controller.fly();
        if (!first) {
            // The swap is one frame. The frames after it are somebody flying.
            const double step = worst_step(last, now);
            if (tick == hand_over) {
                out.worst_to_pilot = step;
            } else if (tick == take_back) {
                out.worst_to_ai = step;
            } else if (hand_over > 0 && tick > hand_over && tick < take_back) {
                out.worst_settled = std::max(out.worst_settled, step);
            }
        }
        first = false;
        last = now;
        f.aircraft->set_controls(now);
        f.aircraft->step();
        if (tick >= settling && !demonstrated) {
            run.update(*f.aircraft, tick);
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

// Up at the climbing speed, level off, and back down: the instructor asks for
// a height, and then for a lower one once she is levelled off up there.
Demonstrated demonstrate_a_climb(const std::string& id) {
    return demonstrate_in_the_air(
        id, "climb-and-descent", 3000.0, 30,
        [](glideslope::sim::AutopilotModes& m, const InFlight& f) {
            // Climbing on rate alone while she settles: the height to level
            // off at is set once the lesson has begun, because the lesson
            // asks for nine hundred feet from *there*.
            m.altitude_ft.reset();
            m.vertical_speed_fpm = 600.0;
            m.airspeed_kts = f.speeds.climb_kts;
        },
        [descending = false](glideslope::sim::Autopilot& ap, const LessonRun& run,
                             const InFlight& f, int since) mutable {
            glideslope::sim::AutopilotModes m = ap.modes();
            const double agl = f.aircraft->property("position/h-agl-ft");
            if (since == 0) {
                m.altitude_ft = agl + 1100.0;
                ap.set(m);
            } else if (!descending && run.stage() >= 2) {
                descending = true;
                m.altitude_ft = agl - 700.0;
                m.vertical_speed_fpm = 600.0;
                m.airspeed_kts = f.speeds.climb_kts + 25.0;
                ap.set(m);
            }
        });
}

// The clean stall, wings level, power off - and the recovery.
//
// **The instructor asks for a speed, not for a throttle.** Asking the
// autopilot to hold a speed below the stall closes the throttle for it and
// holds the height by raising the nose, which is the entry; asking for half
// as much again as the stall speed opens the throttle and puts the nose down,
// which is the recovery. The version this replaces reached into the controls
// and set the throttle to 0 and then to 1 by hand, which no controller can
// hand over.
Demonstrated demonstrate_a_stall(const std::string& id, double start_ft) {
    return demonstrate_in_the_air(
        id, "stalls", start_ft, 20, [](glideslope::sim::AutopilotModes&, const InFlight&) {},
        [recovering = false](glideslope::sim::Autopilot& ap, const LessonRun& run,
                             const InFlight& f, int since) mutable {
            glideslope::sim::AutopilotModes m = ap.modes();
            if (since == 0) {
                m.airspeed_kts = f.speeds.stall_kts - 10.0;
                ap.set(m);
            } else if (!recovering && run.stage() >= 1) {
                recovering = true;
                m.altitude_ft.reset();
                m.vertical_speed_fpm = -600.0;
                // **The speed asked for has to clear the one the lesson is
                // waiting for.** The recovery stage ends at the climbing
                // speed, and half as much again as the stall speed is *below*
                // it in a Cessna - 72 knots against 76 - so she settled there
                // and the stage never ended. The version this replaces got
                // past it by holding the throttle wide open by hand, which
                // was the throttle flying the aeroplane rather than the AI.
                m.airspeed_kts = std::max(f.speeds.stall_kts * 1.5,
                                          f.speeds.climb_kts + 10.0);
                ap.set(m);
            }
        });
}

void report_and_check(const std::string& id, const std::string& what,
                      const Demonstrated& shown) {
    // A control moving at a pilot's hand pace, with a little room for the
    // step the swap itself lands on.
    const double a_hands_pace = 2.0 / steps_per_second + 0.004;
    std::printf("  %-13s %s %zu/%zu stages, worst step %.4f over, %.4f back, "
                "%.4f settled\n",
                id.c_str(), what.c_str(), shown.completed, shown.stages,
                shown.worst_to_pilot, shown.worst_to_ai, shown.worst_settled);
    for (const std::string& said : shown.debrief) {
        std::printf("      %s\n", said.c_str());
    }
    check(shown.completed == shown.stages,
          id + " flew every stage of the " + what + ": " +
              std::to_string(shown.completed) + " of " + std::to_string(shown.stages));
    check(shown.debrief.empty(),
          id + " flew the " + what + " inside the lesson's limits, and said " +
              std::to_string(shown.debrief.size()) + " things");
    check(shown.worst_to_pilot <= a_hands_pace,
          id + " stepped " + std::to_string(shown.worst_to_pilot) +
              " handing over, and a hand moves " + std::to_string(a_hands_pace));
    check(shown.worst_to_ai <= a_hands_pace,
          id + " stepped " + std::to_string(shown.worst_to_ai) +
              " taking back, and a hand moves " + std::to_string(a_hands_pace));
    check(shown.worst_settled <= a_hands_pace,
          id + " moved a control " + std::to_string(shown.worst_settled) +
              " in one step while the pilot held it, and a hand moves " +
              std::to_string(a_hands_pace));
}

} // namespace

// **The instructor demonstrates a climb and descent, then hands over.** Every
// aeroplane whose class is taught the exercise flies it.
GLIDESLOPE_TEST(an_instructor_demonstrates_a_climb_and_descent_and_hands_it_over) {
    const auto taught = everyone_taught("climb-and-descent");
    check(!taught.empty(), "some aeroplane is taught climbs and descents");
    std::size_t walked = 0;
    for (const std::string& id : taught) {
        report_and_check(id, "climb", demonstrate_a_climb(id));
        ++walked;
    }
    check(walked == taught.size(),
          "every aeroplane taught the exercise demonstrated it: " +
              std::to_string(walked) + " of " + std::to_string(taught.size()));
}

// **The instructor demonstrates a stall, then hands over.** Every aeroplane
// whose class is taught the exercise flies it.
GLIDESLOPE_TEST(an_instructor_demonstrates_a_stall_and_hands_it_over) {
    const auto taught = everyone_taught("stalls");
    check(!taught.empty(), "some aeroplane is taught stalls");
    std::size_t walked = 0;
    for (const std::string& id : taught) {
        // A stall is practised where its aeroplane practises it: a clean jet
        // at idle descends a long way while it slows.
        const bool light = glideslope::sim::find_aircraft(data(), id).aircraft_class ==
                           glideslope::sim::AircraftClass::light_aircraft;
        report_and_check(id, "stall", demonstrate_a_stall(id, light ? 5000.0 : 20000.0));
        ++walked;
    }
    check(walked == taught.size(),
          "every aeroplane taught the exercise demonstrated it: " +
              std::to_string(walked) + " of " + std::to_string(taught.size()));
}

namespace {

// Where she is along the runway, in nautical miles: positive beyond the
// threshold in the landing direction, negative before it. A circuit has to
// know this and the aeroplane's own state cannot say it.
double along_the_runway_nm(const glideslope::sim::Runway& r,
                           const glideslope::sim::Aircraft& a) {
    const double north_m = (a.property("position/lat-geod-deg") - r.threshold_lat_deg) *
                           metres_per_degree_latitude(r.threshold_lat_deg);
    const double east_m = (a.property("position/long-gc-deg") - r.threshold_lon_deg) *
                          metres_per_degree_longitude(r.threshold_lat_deg);
    const double h = r.heading_deg / degrees;
    return (north_m * std::cos(h) + east_m * std::sin(h)) / metres_per_nm;
}

bool pointing_at(const glideslope::sim::Aircraft& a, double heading_deg) {
    return std::abs(std::remainder(heading_deg - a.property("attitude/psi-deg"), 360.0)) <
           10.0;
}

struct Circuit {
    std::vector<std::string> debrief;
    std::size_t completed = 0;
    std::size_t stages = 0;
    bool stopped = false;
    double highest_agl_ft = 0.0;
    // **What the bands are set from, rather than guessed at.** The speed
    // through the climb out and down final, and the height held downwind,
    // measured over every aeroplane that flies the lesson.
    double slowest_climb_out_kts = 1e9;
    double fastest_climb_out_kts = 0.0;
    double lowest_downwind_ft = 1e9;
    double highest_downwind_ft = 0.0;
    double slowest_final_kts = 1e9;
    double fastest_final_kts = 0.0;
};

// **The AI pilot flies a whole circuit.** The take-off autopilot flies her
// off, the plain autopilot flies the pattern - a heading and a height a leg -
// and the approach autopilot brings her back to the same runway she left.
Circuit fly_a_circuit(const std::string& id, bool trace, double sink_downwind_ft = 0.0) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto dep = glideslope::sim::departure_speeds(data(), entry.model);
    const auto app = glideslope::sim::approach_speeds(data(), entry.model);

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

    const auto found = lesson_for(entry, "circuit");
    check(found.has_value(), id + " has a circuit lesson for its class");
    LessonRun run(*found, glideslope::sim::LessonSpeeds{dep.rotate_kts, dep.climb_kts,
                                                        app.vref_kts,
                                                        app.vref_kts / 1.3});

    glideslope::sim::Controls standing;
    glideslope::sim::Controller controller(aircraft, standing);
    // The climb out: straight ahead to six hundred feet.
    controller.to_ai_take_off(runway, dep, 600.0);

    // **A faster aeroplane flies a bigger circuit, and the two numbers that
    // make it are one number.** The downwind leg is left at the distance
    // where a three-degree glidepath passes through circuit height, so the
    // approach autopilot is handed an aeroplane on its path rather than
    // above or below it. A Mosquito flown round the Cessna's thousand-foot
    // circuit and handed the approach at the Cessna's distance arrived low,
    // still turning, and put itself into the ground.
    const double circuit_ft =
        std::clamp(1000.0 + (app.vref_kts - 60.0) * 8.0, 1000.0, 1600.0);
    // A three-degree slope rises about 318 feet in a nautical mile, and the
    // extra third of a mile leaves her a little above the path at the hand
    // over, which is the side to be on.
    const double leave_downwind_nm = circuit_ft / 318.0 + 0.33;
    enum class Leg { climbing_out, crosswind, downwind, approach };
    Leg leg = Leg::climbing_out;
    bool sank = false;

    Circuit out;
    out.stages = found->stages.size();
    std::size_t stage_was = 0;
    for (int tick = 0; tick < 900 * steps_per_second && !run.finished(); ++tick) {
        const double agl = aircraft.property("position/h-agl-ft");
        const double along = along_the_runway_nm(runway, aircraft);
        if (leg == Leg::climbing_out && agl >= 600.0) {
            leg = Leg::crosswind;
            controller.to_ai();
            glideslope::sim::AutopilotModes m = controller.autopilot()->modes();
            m.heading_deg = runway.heading_deg - 90.0;
            m.altitude_ft = runway.elevation_ft + circuit_ft;
            m.airspeed_kts = dep.climb_kts;
            m.vertical_speed_fpm = 600.0;
            controller.autopilot()->set(m);
        } else if (leg == Leg::crosswind &&
                   pointing_at(aircraft, runway.heading_deg - 90.0) &&
                   agl >= circuit_ft - 100.0) {
            leg = Leg::downwind;
            glideslope::sim::AutopilotModes m = controller.autopilot()->modes();
            m.heading_deg = runway.heading_deg - 180.0;
            m.airspeed_kts = app.vref_kts + 20.0;
            controller.autopilot()->set(m);
        } else if (leg == Leg::downwind && sink_downwind_ft > 0.0 && along <= -1.0 &&
                   !sank) {
            // **The fault: she sinks along the downwind leg.** Not a low
            // circuit - that is a different fault and the band is measured
            // from where the leg began, on purpose - but a leg begun at
            // circuit height and not held there.
            sank = true;
            std::printf("      sinking %.0f ft at %.2f nm, stage %zu, agl %.0f\n",
                        sink_downwind_ft, along, run.stage(), agl);
            glideslope::sim::AutopilotModes m = controller.autopilot()->modes();
            m.altitude_ft = *m.altitude_ft - sink_downwind_ft;
            controller.autopilot()->set(m);
        } else if (leg == Leg::downwind && along <= -leave_downwind_nm) {
            leg = Leg::approach;
            controller.to_ai_approach(runway, app);
        }
        aircraft.set_controls(controller.fly());
        aircraft.step();
        run.update(aircraft, tick);
        out.highest_agl_ft = std::max(out.highest_agl_ft, agl);
        const double kcas = aircraft.property("velocities/vc-kts");
        if (run.stage() == 1) {
            out.slowest_climb_out_kts = std::min(out.slowest_climb_out_kts, kcas);
            out.fastest_climb_out_kts = std::max(out.fastest_climb_out_kts, kcas);
        } else if (run.stage() == 4) {
            out.lowest_downwind_ft = std::min(out.lowest_downwind_ft, agl);
            out.highest_downwind_ft = std::max(out.highest_downwind_ft, agl);
        } else if (run.stage() == 6) {
            out.slowest_final_kts = std::min(out.slowest_final_kts, kcas);
            out.fastest_final_kts = std::max(out.fastest_final_kts, kcas);
        }
        if (trace && (run.stage() != stage_was || tick % (30 * steps_per_second) == 0)) {
            stage_was = run.stage();
            std::printf("      %6.1f s  stage %zu  leg %d  agl %5.0f  along %+5.2f nm  "
                        "hdg %3.0f  %3.0f kt\n",
                        static_cast<double>(tick) / steps_per_second, run.stage(),
                        static_cast<int>(leg), agl, along,
                        aircraft.property("attitude/psi-deg"),
                        aircraft.property("velocities/vc-kts"));
        }
    }
    out.stopped = std::abs(aircraft.property("velocities/vg-fps")) < 1.0 &&
                  aircraft.property("gear/wow") > 0.5;
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

} // namespace

// **A circuit flown by the book leaves an empty debrief**, for every light
// aeroplane: off the runway, round the pattern left-hand at circuit height,
// and back on to the same runway.
GLIDESLOPE_TEST(the_circuit_lesson_flown_by_the_book_leaves_an_empty_debrief) {
    const auto taught = everyone_taught("circuit");
    check(!taught.empty(), "some aeroplane is taught the circuit");
    std::size_t walked = 0;
    for (const std::string& id : taught) {
        const Circuit flown = fly_a_circuit(id, false);
        std::printf("  %-13s circuit %zu/%zu stages, highest %.0f ft, %s\n", id.c_str(),
                    flown.completed, flown.stages, flown.highest_agl_ft,
                    flown.stopped ? "stopped on the runway" : "NOT STOPPED");
        const auto d = glideslope::sim::departure_speeds(data(), 
            glideslope::sim::find_aircraft(data(), id).model);
        const auto a = glideslope::sim::approach_speeds(data(),
            glideslope::sim::find_aircraft(data(), id).model);
        std::printf("      climb out %.0f-%.0f kt (climb %.0f), downwind %.0f-%.0f ft, "
                    "final %.0f-%.0f kt (vref %.0f)\n",
                    flown.slowest_climb_out_kts, flown.fastest_climb_out_kts,
                    d.climb_kts, flown.lowest_downwind_ft, flown.highest_downwind_ft,
                    flown.slowest_final_kts, flown.fastest_final_kts, a.vref_kts);
        for (const std::string& said : flown.debrief) {
            std::printf("      %s\n", said.c_str());
        }
        check(flown.completed == flown.stages,
              id + " flew every stage of the circuit: " +
                  std::to_string(flown.completed) + " of " +
                  std::to_string(flown.stages));
        check(flown.stopped, id + " finished the circuit stopped on the runway");
        check(flown.debrief.empty(),
              id + " flew the circuit inside the lesson's limits, and said " +
                  std::to_string(flown.debrief.size()) + " things");
        ++walked;
    }
    check(walked == taught.size(),
          "every aeroplane taught the circuit flew it: " + std::to_string(walked) +
              " of " + std::to_string(taught.size()));
}

// **A circuit flown with one fault has that fault in its debrief, and no
// other.** She is let sink two hundred and fifty feet along the downwind leg,
// which is the one thing the lesson watches there.
GLIDESLOPE_TEST(a_circuit_flown_low_downwind_is_named_in_the_debrief) {
    const Circuit sunk = fly_a_circuit("c172p", false, 250.0);
    std::printf("  c172p sinking downwind: %zu/%zu stages, downwind %.0f-%.0f ft\n",
                sunk.completed, sunk.stages, sunk.lowest_downwind_ft,
                sunk.highest_downwind_ft);
    for (const std::string& said : sunk.debrief) {
        std::printf("      %s\n", said.c_str());
    }
    check(sunk.debrief.size() == 1,
          "the debrief holds one thing, and it said " +
              std::to_string(sunk.debrief.size()));
    check(sunk.debrief.front().find("circuit height") != std::string::npos,
          "and the one thing is the circuit height: " + sunk.debrief.front());
}
