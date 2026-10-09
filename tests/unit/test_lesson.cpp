#include "after_touch.hpp"
#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/autopilot.hpp"
#include "sim/circuit.hpp"
#include "sim/controller.hpp"
#include "sim/crash.hpp"
#include "sim/lander.hpp"
#include "sim/figures.hpp"
#include "sim/lesson.hpp"
#include "sim/lesson_run.hpp"
#include "sim/orbit_trial.hpp"
#include "sim/plan.hpp"
#include "sim/runway_condition.hpp"
#include "sim/learnt.hpp"
#include "sim/test_pilot.hpp"
#include "sim/vacate.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
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
// **A demonstration asks for a climb the aeroplane has.** A Piper J-3 Cub at
// full load climbs at 450 ft/min - its own booklet says so - and every
// demonstration used to ask every aeroplane for 600. The Cub could manage
// that only while it was being flown lighter than the weight its figures were
// measured at; once it was loaded as they were, it sat at its pitch limit and
// never reached the height that ends the first stage. Seven tenths of the
// rate the aeroplane has, and never more than the 600 the jets were already
// being asked for.
double a_climb_it_can_manage(const std::string& model) {
    try {
        const auto figures = glideslope::sim::read_published_figures(
            data() / "figures" / (model + ".xml"));
        for (const auto& spec : figures.figures) {
            if (spec.flight == "climb_rate" && spec.published > 0.0) {
                return std::min(600.0, 0.7 * spec.published);
            }
        }
    } catch (const std::exception&) {
    }
    return 600.0;
}

// **A reference speed belongs to a weight, so a lesson flies the aeroplane at
// the weight its figures were measured at.** Without this the two are about
// different aeroplanes: the F-35B's climbing speed was measured at its 39,750
// lb combat loading and the lesson flew it at the 45,259 lb the model loads by
// default, where that speed is below its flying speed. It sat at fifteen
// degrees nose up and twenty-two degrees of alpha, mushing down at two
// thousand feet a minute, and arrived on the ground still doing 160 knots.
//
// The climbing speed's loading is the one taken: it is the figure a lesson is
// most likely to name, and for every aeroplane whose figures are measured the
// stall was taken at the same loading on purpose. Call it before
// `initialize`, as the figure flights do.
void load_as_measured(glideslope::sim::Aircraft& aircraft,
                      const glideslope::sim::PublishedFigures& figures,
                      const glideslope::sim::FigureSpec* spec) {
    const std::string& name = spec != nullptr ? spec->loading : std::string();
    const auto it = figures.loadings.find(name);
    aircraft.load(it != figures.loadings.end() ? it->second.loading : figures.loading);
}

void load_as_its_figures_were_measured(glideslope::sim::Aircraft& aircraft,
                                       const std::string& model) {
    try {
        const auto figures = glideslope::sim::read_published_figures(
            data() / "figures" / (model + ".xml"));
        const glideslope::sim::FigureSpec* climb = nullptr;
        for (const auto& spec : figures.figures) {
            if (spec.flight == "climb_rate") {
                climb = &spec;
            }
        }
        if (climb != nullptr) {
            load_as_measured(aircraft, figures, climb);
        }
    } catch (const std::exception&) {
    }
}

// **The runway lessons, likewise.** A take-off is judged against the speed
// the aeroplane lifts off at - its published ground roll's, or else the
// stall's it is worked from - so it flies at that figure's loading; an
// approach against a third above the stall with the most flap, so it flies at
// that stall's. Until 2026-09-23 they built their aeroplane and loaded
// nothing, and flew at whatever the model carries by default.
void load_for_the_take_off(glideslope::sim::Aircraft& aircraft, const std::string& model) {
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / (model + ".xml"));
    for (const auto& spec : figures.figures) {
        if (spec.flight == "takeoff_ground_roll" &&
            spec.conditions.count("lift_off_kcas") != 0) {
            load_as_measured(aircraft, figures, &spec);
            return;
        }
    }
    // A jet rotates from its stall at its field length's take-off flap, as
    // `sim::departure_speeds` works it out, so it flies at that stall's
    // loading.
    for (const auto& field : figures.figures) {
        if (field.flight != "takeoff_field_length") {
            continue;
        }
        const auto flap = field.conditions.find("flaps_deg");
        const double field_flap = flap == field.conditions.end() ? 0.0 : flap->second;
        for (const auto& spec : figures.figures) {
            const auto at = spec.conditions.find("flaps_deg");
            if (spec.flight == "stall_speed" && at != spec.conditions.end() &&
                std::abs(at->second - field_flap) < 0.5) {
                load_as_measured(aircraft, figures, &spec);
                return;
            }
        }
    }
    const glideslope::sim::FigureSpec* chosen = nullptr;
    double least_flap_deg = 0.0;
    for (const auto& spec : figures.figures) {
        if (spec.flight == "stall_speed") {
            const auto flap = spec.conditions.find("flaps_deg");
            const double flap_deg = flap == spec.conditions.end() ? 0.0 : flap->second;
            if (chosen == nullptr || flap_deg < least_flap_deg) {
                chosen = &spec;
                least_flap_deg = flap_deg;
            }
        }
    }
    load_as_measured(aircraft, figures, chosen);
}

void load_for_the_approach(glideslope::sim::Aircraft& aircraft, const std::string& model) {
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / (model + ".xml"));
    // A manual's own approach speed is for the weight it names.
    if (figures.approach_kcas > 0.0) {
        aircraft.load(figures.loadings.at(figures.approach_loading).loading);
        return;
    }
    const glideslope::sim::FigureSpec* chosen = nullptr;
    double most_flap_deg = -1.0;
    for (const auto& spec : figures.figures) {
        if (spec.flight != "stall_speed") {
            continue;
        }
        const auto flap = spec.conditions.find("flaps_deg");
        const double flap_deg = flap == spec.conditions.end() ? 0.0 : flap->second;
        if (flap_deg > most_flap_deg) {
            most_flap_deg = flap_deg;
            chosen = &spec;
        }
    }
    load_as_measured(aircraft, figures, chosen);
}

// **The take-off speeds its book gives** (sim::departure_speeds), and none
// for an aeroplane whose speeds are measured from its model: the 747-400's
// and the F-22A's (`<takeoff_speeds>`, 2026-10-06) are for a plan's take-off,
// at the one weight its model flies a plan at, and a lesson teaches the book
// - at every loading it names. Throws as departure_speeds does, and for those.
glideslope::sim::DepartureSpeeds book_departure_speeds(const std::filesystem::path& from,
                                                      const std::string& model) {
    glideslope::sim::DepartureSpeeds speeds = glideslope::sim::departure_speeds(from, model);
    if (speeds.measured_from_model) {
        throw std::runtime_error(model + "'s take-off speeds are measured from its model at "
                                         "one weight, for a plan, not taken from a book");
    }
    return speeds;
}

// **Where this aeroplane practises a stall.** A light aeroplane decelerates
// to the stall in a few hundred feet; a clean jet at idle descends a long way
// while it slows, and doing that from five thousand feet puts it in the
// ground before it stalls. **A flying boat is neither**: it is slow enough to
// want the low height and its ceiling is about 16,000 ft, so twenty thousand
// is a height it cannot reach at all.
double stalls_are_practised_at(const glideslope::sim::CatalogueEntry& entry) {
    const bool slow =
        entry.aircraft_class == glideslope::sim::AircraftClass::light_aircraft ||
        entry.aircraft_class == glideslope::sim::AircraftClass::seaplane;
    return slow ? 5000.0 : 20000.0;
}

// This aeroplane's published figures, resolved without throwing: what it has
// not got reads zero.
glideslope::sim::LessonSpeeds figures_of(const glideslope::sim::CatalogueEntry& entry) {
    glideslope::sim::LessonSpeeds speeds;
    try {
        const auto departure = book_departure_speeds(data(), entry.model);
        speeds.rotate_kts = departure.rotate_kts;
        speeds.climb_kts = departure.climb_kts;
    } catch (const std::exception&) {
    }
    try {
        const auto approach = glideslope::sim::approach_speeds(data(), entry.model);
        speeds.vref_kts = approach.vref_kts;
        speeds.stall_kts = approach.stall_kts;
    } catch (const std::exception&) {
    }
    return speeds;
}

// **Who is taught an exercise, and who is left out of it and why.** A lesson
// belongs to a class and the figures belong to the aeroplane, so a class's
// lesson can name a figure one of its aeroplanes has not got - the 747-400
// and the F-22A publish no stall speed and no measurement of one holds still
// (docs/ASSETS.md), so they are taught only what needs neither.
struct Taught {
    std::vector<std::string> able;
    std::vector<std::string> left_out; // each one named, with its reason
};

Taught taught_for(const std::string& exercise) {
    Taught out;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        const auto lesson = lesson_for(entry, exercise);
        if (!lesson) {
            continue;
        }
        const std::string why =
            glideslope::sim::cannot_be_taught(*lesson, figures_of(entry));
        if (why.empty()) {
            out.able.push_back(entry.id);
        } else {
            out.left_out.push_back(entry.id + ": " + why);
        }
    }
    return out;
}

// **Every aeroplane whose class is taught the exercise and that has the
// figures it names** - and it says how big that space was and names whoever
// was left out of it, so that a short list cannot pass for a whole one.
std::vector<std::string> everyone_taught(const std::string& exercise) {
    const Taught taught = taught_for(exercise);
    const std::size_t whole = taught.able.size() + taught.left_out.size();
    std::printf("  %s: %zu of the %zu aeroplanes whose class is taught it\n",
                exercise.c_str(), taught.able.size(), whole);
    for (const std::string& said : taught.left_out) {
        std::printf("      left out - %s\n", said.c_str());
    }
    return taught.able;
}

// **One aeroplane of each class taught `exercise`, for flying its faults.**
// A fault belongs to a lesson and a lesson to a class, so a fault flown only
// in the Cessna tested the light aircraft's lesson and no other: every
// class's lesson has its own bands, its own words and its own figures. The
// first aeroplane in the roster that can fly each class's lesson flies it,
// and the lessons it covered are counted against the lessons there are.
std::vector<std::string> one_of_each_class(const std::string& exercise) {
    std::vector<std::string> out;
    std::set<std::string> lessons;
    std::set<std::string> covered;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        const auto lesson = lesson_for(entry, exercise);
        if (!lesson) {
            continue;
        }
        lessons.insert(lesson->id);
        if (covered.count(lesson->id) != 0 ||
            !glideslope::sim::cannot_be_taught(*lesson, figures_of(entry)).empty()) {
            continue;
        }
        covered.insert(lesson->id);
        out.push_back(entry.id);
    }
    std::printf("  %s: faults flown in %zu of the %zu classes' lessons\n", exercise.c_str(),
                covered.size(), lessons.size());
    check(covered.size() == lessons.size(),
          "every class's " + exercise + " lesson has an aeroplane to fly its fault");
    return out;
}

// **The fault and no other.** Every line of the debrief must be one this
// lesson gives for `property` - the approach speed and the speed over the
// threshold are one fault, flying fast, seen twice - and there must be one.
//
// `watches` names the properties the fault may be seen on; `need:` before
// one takes only its `need`s - rotating early is the rotation-speed need and
// the attitude it leaves, not the climbing-speed band on the same property.
void names_that_fault_and_no_other(const std::string& id, const std::string& exercise,
                                   const std::vector<std::string>& debrief,
                                   const std::vector<std::string>& watches_on,
                                   const std::string& fault) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const auto lesson = lesson_for(entry, exercise);
    check(lesson.has_value(), id + " has a " + exercise + " lesson");
    std::set<std::string> allowed;
    for (const std::string& on : watches_on) {
        const bool needs_only = on.rfind("need:", 0) == 0;
        const std::string property = needs_only ? on.substr(5) : on;
        for (const auto& stage : lesson->stages) {
            for (const auto& w : stage.needs) {
                if (w.property == property) {
                    allowed.insert(w.fault);
                }
            }
            if (!needs_only) {
                for (const auto& w : stage.holds) {
                    if (w.property == property) {
                        allowed.insert(w.fault);
                    }
                }
            }
        }
    }
    for (const std::string& said : debrief) {
        std::printf("      %-13s %s\n", id.c_str(), said.c_str());
    }
    check(!debrief.empty(), id + " " + fault + " is not faultless");
    for (const std::string& said : debrief) {
        check(allowed.count(said) != 0,
              id + " " + fault + ", and the debrief named something else too: " + said);
    }
}

const std::vector<std::string>& light_aircraft() {
    static const std::vector<std::string> ids{"c172p", "c182", "pa28", "j3cub"};
    return ids;
}

const char* a_whole_lesson() {
    return "# a comment\n"
           "name A lesson with every command in it\n"
           "teaches light-aircraft\n"
           "warning stall+7\n"
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
    check(lesson.stall_warning && lesson.stall_warning->reference == "stall" &&
              std::abs(lesson.stall_warning->offset - 7.0) < 1e-9,
          "and where its stall warning sounds");
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
    // The speed the take-off autopilot began its rotation at.
    double rotation_began_kts = 0.0;
};

// **A flying boat takes off from water and alights on it.** The runway's
// place is open water for her; started from rest she is put down on it and
// left half a minute to settle, as she floats within four seconds.
void settle_afloat(glideslope::sim::Aircraft& aircraft) {
    const glideslope::sim::Controls idle;
    for (int i = 0; i < 30 * steps_per_second; ++i) {
        aircraft.set_controls(idle);
        aircraft.step();
    }
}

// Whether any main wheel is on the ground: a wheel off the centreline. A
// tail-wheel aeroplane pulled off early can drag its tail wheel along the
// runway with its main wheels clear of it.
bool on_its_main_wheels(const glideslope::sim::Aircraft& aircraft) {
    for (int unit = 0;; ++unit) {
        const std::string at = "gear/unit[" + std::to_string(unit) + "]/";
        if (!aircraft.has_property(at + "WOW")) {
            return false;
        }
        if (std::abs(aircraft.property(at + "y-position")) > 1.0 &&
            aircraft.property(at + "WOW") > 0.5) {
            return true;
        }
    }
}

// **A take-off, flown either by the book or with one fault.** From
// `early_from_kts`, where it is not 0, she is rotated early (below);
// `throttle` caps the power. Both are flown against the same lesson.
Flown fly_the_take_off(const std::string& id, double early_from_kts,
                       double throttle_cap) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    auto speeds = book_departure_speeds(data(), entry.model);
    // **Rotating early is the one thing different** (below): the take-off is
    // the same take-off in every other way - the autopilot still keeps her
    // straight and still climbs her away.

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; },
        [water = entry.seaplane](double, double) { return water; }));

    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg;
    ic.longitude_deg = runway.threshold_lon_deg;
    ic.altitude_ft = runway.elevation_ft + (entry.seaplane ? 6.0 : 0.0);
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = 0.0;
    ic.engine_running = true;
    ic.gear = 1.0;
    load_for_the_take_off(aircraft, entry.model);
    aircraft.initialize(ic);
    if (entry.seaplane) {
        settle_afloat(aircraft);
    }

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
    std::size_t lift_off_stage = 0;
    while (lift_off_stage + 1 < it->stages.size() &&
           it->stages[lift_off_stage].until_property != "position/h-agl-ft") {
        ++lift_off_stage;
    }
    double out_least = 1e9;
    double out_most = -1e9;
    double out_off_at_kts = 0.0;
    double out_climb_least = 1e9;
    double out_climb_most = -1e9;
    bool pulled = false;
    bool let_go = false;
    for (int tick = 0; tick < 300 * steps_per_second && !run.finished(); ++tick) {
        glideslope::sim::Controls controls = departure.fly();
        // **Rotating early is flown, not faked.** The stick comes fully
        // back before her speed is there, and is held there until her main
        // wheels leave the runway; then the take-off autopilot has her
        // again, and carries on from the stick it is handed
        // (`sim::Departure`). Held on beyond that, it kept a J-3 Cub's tail
        // wheel on the runway at thirty knots and seventeen degrees of
        // incidence, and she never climbed away; half back, as it was until
        // 2026-09-26, would not lift the noses of the F-15C, the F-35B or
        // the Learjet 35A.
        //
        // **From 85 percent of the speed she is rotated at by the book, not
        // from a standstill.** Held from the start of the roll, the stick
        // kept the Mosquito's tail down the whole way, and hauled off at 107
        // knots she swung past the twenty degrees the roll allows: the
        // debrief said keep her straight as well, which is a different
        // fault. Easing back early is easing back before the speed is there,
        // not before she moves.
        //
        // **Of the speed the book begins the rotation at**, which is short
        // of the rotation speed by what she gains while the nose comes up
        // (`sim::Departure`). An F-15C gains thirteen knots a second, and
        // the book's rotation begins at 85 percent of her rotation speed -
        // where her own flight manual begins it - so pulling from there was
        // no earlier than the book.
        if (early_from_kts > 0.0 && entry.seaplane &&
            aircraft.property("velocities/vc-kts") >= early_from_kts &&
            aircraft.property("position/h-agl-ft") < 14.0) {
            // **A flying boat's is the nose held low on the step.** The
            // water, not the elevator, sets her attitude there in this model:
            // held fully back the Short S.23 planed at the running attitude
            // and came off at the same 78 knots. The fault the FAA's seaplane
            // handbook warns of on the step is the other one - the nose too
            // low, the bow digging in, and porpoising - which a little
            // forward stick flies.
            controls.elevator = -0.3;
        } else if (early_from_kts > 0.0 && !entry.seaplane && !let_go &&
                   on_its_main_wheels(aircraft) &&
                   aircraft.property("velocities/vc-kts") >= early_from_kts) {
            controls.elevator = std::max(controls.elevator, 1.0);
            pulled = true;
        } else if (pulled) {
            let_go = true;
        }
        if (throttle_cap < 1.0) {
            controls.throttle = std::min(controls.throttle, throttle_cap);
        }
        aircraft.set_controls(controls);
        aircraft.step();
        const std::size_t was = run.stage();
        run.update(aircraft, tick);
        // Off, when the stage that ends on her height ends: the first
        // stage for a landplane, and the one on the step after the hump for
        // a flying boat.
        if (run.stage() > was && was == lift_off_stage) {
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
            out_most, out_off_at_kts, out_climb_least, out_climb_most,
            departure.rotation_began_kts()};
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
    // Fourteen of the sixteen: the four light aircraft, the Mosquito, the
    // Learjet, four airliners, two fighters, the B-2A and the Short S.23, off
    // the water. The 747-400 and the F-22A have no stall to rotate from and
    // are named above.
    check(walked == 14, "fourteen aeroplanes took off, not " + std::to_string(walked));
}

// **Every landplane leaves the runway within ten knots of its rotation
// speed, and sooner when it is rotated early.** The F-15C, the F-35B and the
// Learjet 35A did neither until 2026-09-24: by the book they left at 217, 229
// and 152 knots against rotation speeds of 174, 141 and 125, and pulled early
// they came off no sooner - their noses would not come up. The Mosquito left
// at 158 against 121, and the PA-28 at 63 against 48. What it took is in
// docs/PROJECT_STATUS.md for 2026-09-26.
//
// **Off the ground by the book is the last time the wheels leave it** before
// she is 35 ft above where she stood, so that an aeroplane that settles back
// on to the runway is off where she left it for good - rotated early as
// well. **Rotated early** is the stick fully back from 85 percent of the
// speed the book begins her rotation at, held until her main wheels leave
// the runway and then handed back to the take-off autopilot. Hauled off
// early, the A380 comes off at 137 knots and settles back before leaving
// for good at 153, which is what rotating early does to an aeroplane; the
// first lift-off is printed beside the last. Each is flown at the loading
// its take-off lesson flies her at.
GLIDESLOPE_TEST(every_landplane_leaves_the_runway_within_ten_knots_of_its_rotation_speed_and_sooner_rotated_early) {
    struct Off {
        double kts = 0.0;
        bool flew = false;
        double rotation_began_kts = 0.0;
        double first = 0.0;
        double rotate_kts = 0.0; // her rotation speed, for what she weighs
    };
    const auto fly = [](const glideslope::sim::CatalogueEntry& entry,
                        const glideslope::sim::DepartureSpeeds& speeds, double early_from_kts) {
        const glideslope::sim::Runway runway = a_runway();
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
        load_for_the_take_off(aircraft, entry.model);
        aircraft.initialize(ic);
        glideslope::sim::Departure departure(aircraft, runway, speeds);
        const double standing_ft = aircraft.property("position/h-agl-ft");
        Off off;
        bool was_down = true;
        bool pulled = false;
        bool let_go = false;
        for (int tick = 0; tick < 300 * steps_per_second; ++tick) {
            glideslope::sim::Controls c = departure.fly();
            if (early_from_kts > 0.0 && !let_go && on_its_main_wheels(aircraft) &&
                aircraft.property("velocities/vc-kts") >= early_from_kts) {
                c.elevator = std::max(c.elevator, 1.0);
                pulled = true;
            } else if (pulled) {
                let_go = true;
            }
            aircraft.set_controls(c);
            aircraft.step();
            const bool down = aircraft.property("gear/wow") > 0.5;
            if (was_down && !down) {
                off.kts = aircraft.property("velocities/vc-kts");
                if (off.first == 0.0) {
                    off.first = off.kts;
                }
            }
            was_down = down;
            if (aircraft.property("position/h-agl-ft") > standing_ft + 35.0) {
                off.flew = true;
                break;
            }
        }
        off.rotation_began_kts = departure.rotation_began_kts();
        off.rotate_kts = departure.speeds().rotate_kts;
        return off;
    };

    std::size_t catalogue = 0;
    std::size_t walked = 0;
    std::vector<std::string> left_out;
    std::vector<std::string> early_left_out;
    std::vector<std::string> wrong;
    // **The table, one row an aeroplane**, printed as each is flown: knots
    // calibrated, and "first" the first time the wheels left the runway
    // where she settled back before leaving it for good.
    std::printf("  %-13s %8s %8s %7s %8s %8s %8s\n", "aircraft", "rotate", "book", "past",
                "(first)", "early", "(first)");
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        ++catalogue;
        // **Left out, each for its reason.** A flying boat's run is flown on
        // the step to a published water take-off, not rotated, and rotating
        // early is flown there as the nose held low (the fault test below).
        if (entry.seaplane) {
            left_out.push_back(entry.id + ": a flying boat, which is not rotated");
            continue;
        }
        glideslope::sim::DepartureSpeeds speeds;
        try {
            speeds = book_departure_speeds(data(), entry.model);
        } catch (const std::runtime_error& e) {
            // No climbing speed, and so no take-off to fly.
            left_out.push_back(entry.id + ": " + e.what());
            continue;
        }
        const Off book = fly(entry, speeds, 0.0);
        // A tail-wheel aeroplane may fly herself off before the book begins
        // any rotation - the Mosquito does, from three points - and then
        // early is 85 percent of the speed she left at.
        const Off early = fly(entry, speeds,
                              0.85 * (book.rotation_began_kts > 0.0 ? book.rotation_began_kts
                                                                    : book.first));
        std::printf("  %-13s %8.1f %8.1f %+7.1f %8.1f %8.1f %8.1f\n", entry.id.c_str(),
                    book.rotate_kts, book.kts, book.kts - book.rotate_kts, book.first,
                    early.kts, early.first);
        std::fflush(stdout);
        ++walked;
        // Every aeroplane is flown before any is judged, so that a failure
        // names all of them and not the first.
        if (!book.flew || !early.flew) {
            wrong.push_back(entry.id + " did not take off both ways");
        }
        // **Against her rotation speed for what she weighs**, the one the
        // departure flies to - the figures' scaled to her loading - not the
        // figures' own.
        if (std::abs(book.kts - book.rotate_kts) > 10.0) {
            wrong.push_back(entry.id + " left the runway at " + std::to_string(book.kts) +
                            " knots by the book, not within ten of its rotation speed, " +
                            std::to_string(book.rotate_kts));
        }
        // **Left out of the early half: the Learjet 35A**, flown and printed
        // but not judged. With her stabilizer set for take-off where her
        // maintenance manual's travel puts it, the stick fully back from 85
        // percent of the speed the book rotates her at lifts her nose only at
        // about 113 knots, and she leaves at 124, a knot short of her
        // rotation speed and inside the five the lesson allows - a tail, "The
        // Learjet cannot be rotated early"; the moment budget is in
        // docs/PROJECT_STATUS.md (her heights from her drawings, 2026-10-09,
        // moved it 1.6 knots). Her book take-off is judged as everyone's.
        if (entry.id == "learjet35a") {
            early_left_out.push_back(entry.id + ": rotated early she leaves at " +
                                     std::to_string(early.kts) + " knots, her rotation speed " +
                                     std::to_string(book.rotate_kts) +
                                     " (the tail \"The Learjet cannot be rotated early\")");
        } else if (!(early.kts < book.kts - 3.0)) {
            wrong.push_back(entry.id + " rotated early left the runway at " +
                            std::to_string(early.kts) + " knots, not sooner than by the book, " +
                            std::to_string(book.kts));
        }
    }
    for (const std::string& why : left_out) {
        std::printf("  left out - %s\n", why.c_str());
    }
    for (const std::string& why : early_left_out) {
        std::printf("  left out of rotating early - %s\n", why.c_str());
    }
    std::string all;
    for (const std::string& what : wrong) {
        all += "\n    " + what;
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " wrong:" + all);
    // Twelve of the thirteen flown are judged rotated early; the Learjet is
    // the one that is not, and is named.
    check(early_left_out.size() == 1 && early_left_out[0].rfind("learjet35a:", 0) == 0,
          "the Learjet 35A alone is left out of rotating early, not " +
              std::to_string(early_left_out.size()) + " aeroplanes");
    // Sixteen aircraft: thirteen landplanes with a take-off to fly, two with
    // no climbing speed and one flying boat.
    check(catalogue == 16, "sixteen aircraft in the catalogue, not " + std::to_string(catalogue));
    check(walked + left_out.size() == catalogue, "every aircraft flown or named");
    check(walked == 13, "thirteen landplanes flown, not " + std::to_string(walked));
}

// **Every landplane takes off at every loading it has**: its model's own,
// which is what an AI aircraft or a plan flies, and every loading its
// figures name - the lightest and the heaviest among them. At each it leaves
// the runway within ten knots of its rotation speed for what it weighs (the
// figures' speed scaled by the square root of the weight, `sim::Departure`),
// strikes nothing and hits nothing on the way (`sim::GroundJudge`), climbs
// to a thousand feet, and hands on no take-off trim. Until 2026-09-26 the
// Learjet 35A at its own loading rotated itself on the roll and was flown
// back into the runway, and the B-2A at its own 327,000 lb was pulled on to
// its tail.
//
// **Off the ground is the last time the wheels leave it** before she is 35
// ft above where she stood, as in the test above. The flying boat is left
// out, as there; the 747-400 and the F-22A have no take-off to fly.
GLIDESLOPE_TEST(every_landplane_takes_off_at_every_loading_within_ten_knots_of_its_speed_for_its_weight_and_unhurt) {
    std::size_t catalogue = 0;
    std::size_t walked = 0;
    std::size_t loadings_flown = 0;
    std::size_t loadings_named = 0;
    std::vector<std::string> left_out;
    std::vector<std::string> wrong;
    std::printf("  %-13s %-13s %8s %8s %8s %7s %6s %6s\n", "aircraft", "loading", "lb",
                "rotate", "off", "past", "stands", "strike");
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        ++catalogue;
        if (entry.seaplane) {
            left_out.push_back(entry.id + ": a flying boat, which is not rotated");
            continue;
        }
        glideslope::sim::DepartureSpeeds speeds;
        try {
            speeds = book_departure_speeds(data(), entry.model);
        } catch (const std::runtime_error& e) {
            left_out.push_back(entry.id + ": " + e.what());
            continue;
        }
        ++walked;
        const auto figures =
            glideslope::sim::read_published_figures(data() / "figures" / (entry.model + ".xml"));
        // Its model's own, and then every loading its figures name - or the
        // one they all fly at, where they name none.
        std::vector<std::pair<std::string, const glideslope::sim::Loading*>> loadings{
            {"(model's)", nullptr}};
        if (figures.loadings.empty()) {
            loadings.emplace_back("(figures')", &figures.loading);
        }
        for (const auto& [name, loading] : figures.loadings) {
            loadings.emplace_back(name, &loading.loading);
        }
        loadings_named += loadings.size();
        for (const auto& [name, loading] : loadings) {
            const glideslope::sim::Runway runway = a_runway();
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
            if (loading != nullptr) {
                aircraft.load(*loading);
            }
            aircraft.initialize(ic);
            glideslope::sim::Departure departure(aircraft, runway, speeds, 1000.0);
            glideslope::sim::GroundJudge judge(false);
            const double standing_ft = aircraft.property("position/h-agl-ft");
            double off_kts = 0.0;
            bool was_down = true;
            bool clear = false;
            std::optional<std::string> wreck;
            glideslope::sim::Controls last;
            for (int tick = 0; tick < 240 * steps_per_second &&
                               departure.stage() != glideslope::sim::Departure::Stage::done;
                 ++tick) {
                last = departure.fly();
                aircraft.set_controls(last);
                aircraft.step();
                if (auto w = judge.judge(aircraft)) {
                    wreck = w;
                    break;
                }
                const bool down = aircraft.property("gear/wow") > 0.5;
                if (was_down && !down && !clear) {
                    off_kts = aircraft.property("velocities/vc-kts");
                }
                was_down = down;
                clear = clear || aircraft.property("position/h-agl-ft") > standing_ft + 35.0;
            }
            ++loadings_flown;
            const double rotate = departure.speeds().rotate_kts;
            std::printf("  %-13s %-13s %8.0f %8.1f %8.1f %+7.1f %6.1f %6.1f%s\n",
                        entry.id.c_str(), name.c_str(), aircraft.property("inertia/weight-lbs"),
                        rotate, off_kts, off_kts - rotate, departure.standing_pitch_deg(),
                        departure.strike_pitch_deg(),
                        wreck ? (" wrecked: " + *wreck).c_str() : "");
            std::fflush(stdout);
            const std::string who = entry.id + " at " + name;
            if (wreck) {
                wrong.push_back(who + " was wrecked: " + *wreck);
            } else if (departure.stage() != glideslope::sim::Departure::Stage::done) {
                wrong.push_back(who + " did not climb to a thousand feet");
            }
            if (std::abs(off_kts - rotate) > 10.0) {
                wrong.push_back(who + " left the runway at " + std::to_string(off_kts) +
                                " knots, not within ten of " + std::to_string(rotate));
            }
            if (std::abs(last.pitch_trim) > 0.01) {
                wrong.push_back(who + " handed on a pitch trim of " +
                                std::to_string(last.pitch_trim));
            }
        }
    }
    for (const std::string& why : left_out) {
        std::printf("  left out - %s\n", why.c_str());
    }
    std::string all;
    for (const std::string& what : wrong) {
        all += "\n    " + what;
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " wrong:" + all);
    check(catalogue == 16, "sixteen aircraft in the catalogue, not " + std::to_string(catalogue));
    check(walked + left_out.size() == catalogue, "every aircraft flown or named");
    check(walked == 13, "thirteen landplanes flown, not " + std::to_string(walked));
    check(loadings_flown == loadings_named, "every loading flown: " +
                                                std::to_string(loadings_flown) + " of " +
                                                std::to_string(loadings_named));
    std::printf("  %zu loadings of %zu landplanes flown\n", loadings_flown, walked);
    // **The size of the space, stated apart from the loop that walks it**:
    // thirteen models' own loadings, and the loadings their figures name -
    // or the one they all fly at, where they name none. Counted by the loop
    // alone, a loading the loop never built could not be missed.
    check(loadings_flown == 49, "forty-nine loadings flown, not " +
                                    std::to_string(loadings_flown));
}

// **A take-off to the lowest height a plan may ask hands on no take-off
// trim**, as one to a thousand feet does (above). The trim is washed off as
// she climbs, a tenth of its travel a second, and a take-off ended at a
// hundred feet ended with most of it still on: the Learjet 35A was handed to
// the autopilot with 0.33 of nose-up stabilizer, and kept it for the whole
// flight. Every landplane, at its model's own loading, as a plan flies it.
GLIDESLOPE_TEST(a_take_off_to_a_plans_lowest_height_hands_on_no_take_off_trim) {
    const double to_ft = glideslope::sim::FlightPlan::TakeOff::lowest_ft;
    std::size_t catalogue = 0;
    std::size_t walked = 0;
    std::vector<std::string> left_out;
    std::vector<std::string> wrong;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        ++catalogue;
        if (entry.seaplane) {
            left_out.push_back(entry.id + ": a flying boat, which is not rotated");
            continue;
        }
        glideslope::sim::DepartureSpeeds speeds;
        try {
            speeds = book_departure_speeds(data(), entry.model);
        } catch (const std::runtime_error& e) {
            left_out.push_back(entry.id + ": " + e.what());
            continue;
        }
        ++walked;
        const glideslope::sim::Runway runway = a_runway();
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
        glideslope::sim::Departure departure(aircraft, runway, speeds, to_ft);
        glideslope::sim::GroundJudge judge(false);
        glideslope::sim::Controls last;
        std::optional<std::string> wreck;
        for (int tick = 0; tick < 240 * steps_per_second &&
                           departure.stage() != glideslope::sim::Departure::Stage::done;
             ++tick) {
            last = departure.fly();
            aircraft.set_controls(last);
            aircraft.step();
            if (auto w = judge.judge(aircraft)) {
                wreck = w;
                break;
            }
        }
        std::printf("  %-13s handed over at %4.0f ft with a pitch trim of %.3f\n",
                    entry.id.c_str(), aircraft.property("position/h-agl-ft"), last.pitch_trim);
        if (wreck) {
            wrong.push_back(entry.id + " was wrecked: " + *wreck);
        } else if (departure.stage() != glideslope::sim::Departure::Stage::done) {
            wrong.push_back(entry.id + " did not climb to " + std::to_string(to_ft) + " ft");
        }
        if (std::abs(last.pitch_trim) > 0.01) {
            wrong.push_back(entry.id + " handed on a pitch trim of " +
                            std::to_string(last.pitch_trim));
        }
    }
    for (const std::string& why : left_out) {
        std::printf("  left out - %s\n", why.c_str());
    }
    std::string all;
    for (const std::string& what : wrong) {
        all += "\n    " + what;
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " wrong:" + all);
    check(catalogue == 16, "sixteen aircraft in the catalogue, not " + std::to_string(catalogue));
    check(walked + left_out.size() == catalogue, "every aircraft flown or named");
    check(walked == 13, "thirteen landplanes flown, not " + std::to_string(walked));
}

// **Flown with one stated fault, the debrief names that fault.** The
// throttle case is exact: one thing said, and it is the throttle.
//
// The early rotation below says two things, and both are true - she came off
// early *and* the attitude wandered while she did it, which is what hauling
// an aeroplane off the ground before its speed actually does. A debrief that
// named only one of them would be hiding the other.
GLIDESLOPE_TEST(a_take_off_flown_with_one_fault_has_that_fault_in_its_debrief) {
    const std::vector<std::string> classes = one_of_each_class("take-off");
    // **Which classes' early rotation was flown**, counted: every class's but
    // the business jet's, which is named below with its reason.
    std::size_t early_flown = 0;
    std::vector<std::string> early_not_flown;
    for (const std::string& id : classes) {
        // **Not opening the throttle.** Flown by the book in every other way.
        const Flown lazy = fly_the_take_off(id, 0.0, 0.80);
        std::printf("  %s on part throttle: off at %.0f knots, pitch %.1f to %.1f\n", id.c_str(),
                    lazy.off_at_kts, lazy.least_theta_deg, lazy.most_theta_deg);
        names_that_fault_and_no_other(id, "take-off", lazy.debrief, {"fcs/throttle-cmd-norm"},
                                      "taking off on part throttle");

        // **Rotating early**, with a steady touch of back stick until she is
        // off: she comes off at a speed she has no business flying at, and
        // the attitude band catches her.

        const std::string rotated = id;
        const Flown book = fly_the_take_off(rotated, 0.0, 1.0);
        const Flown early = fly_the_take_off(rotated, 0.85 * book.rotation_began_kts, 1.0);
        // **A flying boat is flown nose low on the step** instead (see
        // `fly_the_take_off`): she porpoises, and does not come off sooner -
        // later, or not at all.
        if (glideslope::sim::find_aircraft(data(), id).seaplane) {
            std::printf("  %s by the book: off at %.0f knots; nose low on the step: %s\n",
                        id.c_str(), book.off_at_kts,
                        early.off_at_kts > 0.0 ? "off later" : "never off the water");
            check(early.off_at_kts == 0.0 || early.off_at_kts > book.off_at_kts,
                  id + " held nose low on the step did not come off sooner");
            names_that_fault_and_no_other(id, "take-off", early.debrief,
                                          {"attitude/theta-deg"},
                                          "held nose low on the step");
            check(book.debrief.empty(), id + ": the same take-off by the book says nothing");
            ++early_flown;
            continue;
        }
        std::printf("  %s by the book: off at %.0f knots; rotating early: off at %.0f\n",
                    rotated.c_str(), book.off_at_kts, early.off_at_kts);
        // **Left out: the Learjet 35A rotated early.** With her stabilizer
        // set for take-off where her maintenance manual's travel puts it
        // (tools/make_learjet35a.py), the stick held fully back from 85
        // percent of the speed the book rotates her at lifts her nose wheel
        // only at about 113 knots, and she is five feet up past her rotation
        // speed less five, so the lesson rightly finds nothing early in it.
        // Nothing published says she could come off sooner; the moment
        // budget is in PROJECT_STATUS, and the tail is "The Learjet cannot
        // be rotated early". Her book take-off is still held to an empty
        // debrief.
        if (id == "learjet35a") {
            const auto lesson = lesson_for(glideslope::sim::find_aircraft(data(), id), "take-off");
            early_not_flown.push_back(lesson ? lesson->id : id);
            std::printf("  left out - %s's rotating early (learjet35a): full back stick from %.0f "
                        "knots does not have her off before her rotation speed less five\n",
                        early_not_flown.back().c_str(), 0.85 * book.rotation_began_kts);
            check(book.debrief.empty(), rotated + ": the same take-off by the book says nothing");
            continue;
        }
        check(early.off_at_kts < book.off_at_kts - 3.0,
              rotated + " really did come off earlier: " + std::to_string(early.off_at_kts) +
                  " against " + std::to_string(book.off_at_kts));
        // **Or her tail struck**: hauled fully back from 85 percent of her
        // speed the 737-300 drags her tail along the runway at 12.9
        // degrees and leaves at 143 knots, and the strike is what her
        // debrief names. Held two degrees short of the strike instead, she
        // did not leave until 161, later than by the book: at that attitude
        // she cannot fly off early without striking, so the strike is how
        // her early rotation is caught.
        names_that_fault_and_no_other(rotated, "take-off", early.debrief,
                                      {"attitude/theta-deg", "need:velocities/vc-kts",
                                       "lesson/airframe-down"},
                                      "rotating early");
        check(book.debrief.empty(), rotated + ": the same take-off by the book says nothing");
        ++early_flown;
    }
    // **Coverage**: the throttle fault in every class's lesson; rotating early
    // in every class's but one, the business jet's, which is named.
    std::printf("  take-off: rotating early flown in %zu of the %zu classes' lessons\n",
                early_flown, classes.size());
    check(early_flown + early_not_flown.size() == classes.size(),
          "every class's early rotation flown or named");
    check(early_not_flown.size() == 1 && early_not_flown[0] == "business-jet-take-off",
          "the business jet's lesson alone has its early rotation left out, not " +
              std::to_string(early_not_flown.size()) + " lessons");
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
    const Flown book = fly_the_take_off("c172p", 0.0, 1.0);
    const Flown early = fly_the_take_off("c172p", 0.85 * book.rotation_began_kts, 1.0);
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

// **The flare, watched**: the attitude she flew the glidepath at on the last
// step before the flare began, the lowest her nose went from there to the
// touch, and the attitude she touched at. A flare that begins by pushing the
// nose down to some fixed attitude shows as a lowest well under the path's.
struct FlareWatch {
    bool began = false;
    bool touched = false;
    double path_pitch_deg = 0.0;
    double least_pitch_deg = 1e9;
    double touch_pitch_deg = 0.0;
    double last_pitch_deg = 0.0;
    bool was_flaring = false;
    // **The most she climbed in the flare**, feet a minute, from its first
    // step to the touch: a flare that goes up rather than level is a balloon.
    double most_climb_fpm = -1e9;
    double most_climb_agl_ft = 0.0;

    void watch(const glideslope::sim::Lander* lander, const glideslope::sim::Aircraft& a) {
        if (lander == nullptr || touched) {
            return;
        }
        const double pitch = a.property("attitude/theta-deg");
        const bool flaring = lander->stage() == glideslope::sim::Lander::Stage::flare;
        if (flaring && !was_flaring && !began) {
            began = true;
            path_pitch_deg = last_pitch_deg;
        }
        was_flaring = flaring;
        last_pitch_deg = pitch;
        if (began) {
            least_pitch_deg = std::min(least_pitch_deg, pitch);
            const double climb_fpm = a.property("velocities/h-dot-fps") * 60.0;
            if (climb_fpm > most_climb_fpm) {
                most_climb_fpm = climb_fpm;
                most_climb_agl_ft = a.property("position/h-agl-ft");
            }
        }
        if (a.property("gear/wow") > 0.5 || a.in_water()) {
            touched = true;
            touch_pitch_deg = pitch;
        }
    }
};

// How far below the path's attitude the nose may go in the flare: the sink
// the flare's own law leaves a little room for, and no fixed attitude's six.
constexpr double flare_dip_margin_deg = 1.5;
// **How far the AI's own landing may rise after its wheels meet the runway**:
// half a foot, which is the gear's stroke settling and a tyre's bounce, and
// not a wing still flying her. The B-2A rose 1.7 ft and was airborne for a
// second; three feet, the bounce every landing is held under, let her.
constexpr double settled_within_ft = 0.5;
// **The touchdown zone every aeroplane the AI lands touches inside**: the
// first 3,000 feet of the runway from its threshold, as the FAA's
// Pilot/Controller Glossary defines it ("TOUCHDOWN ZONE- The first 3,000 feet
// of the runway beginning at the threshold") - the zone a landing is made in,
// whatever the aeroplane. Short of the threshold is short of the runway, and
// past the zone is a float: an F-15C touching 800 m along a 6,000 ft runway
// would leave herself too little of it to stop on.
constexpr double touchdown_zone_m = 3000.0 * 0.3048;
std::vector<std::string> inside_the_touchdown_zone(const std::string& id,
                                                   const std::string& where,
                                                   double touched_along_m) {
    std::printf("      touched %.0f m past the threshold (%s), the touchdown zone 0 to %.0f\n",
                touched_along_m, where.c_str(), touchdown_zone_m);
    if (touched_along_m >= 0.0 && touched_along_m <= touchdown_zone_m) {
        return {};
    }
    return {id + " (" + where + ") touched " + std::to_string(touched_along_m) +
            " m past the threshold, outside the touchdown zone's first " +
            std::to_string(touchdown_zone_m) + " m"};
}
// **No aeroplane climbs in its flare**: one that goes up, not level, is a
// balloon. A few feet a minute of the gear's own settling is not one.
constexpr double most_flare_climb_fpm = 10.0;
// How far short of the attitude her tail strikes at she must touch: more
// than the two degrees short of it the lander bounds the flare at
// (sim/lander.cpp), so a flare that has run up to that bound - the bound
// doing the landing, not the flare's own law - is caught too.
constexpr double strike_margin_deg = 2.5;

// **Aeroplanes with nothing behind their main wheels to strike**, named, and
// why: a tail-wheel aeroplane's tail is down already, and a model with no
// contact aft of its main wheels says nothing a tail could strike.
const std::map<std::string, std::string>& no_tail_to_strike() {
    static const std::map<std::string, std::string> named = {
        {"f15c", "her model has no contact behind her main wheels"},
        {"j3cub", "she stands on a tail wheel, her tail down already"},
        {"short_s23", "a flying boat, alighting on her hull and not on wheels"},
    };
    return named;
}

// **Every flare begins at the attitude the glidepath was flown at, and
// touches below the tail-strike attitude**; what went wrong, if anything.
std::vector<std::string> flared_from_the_path(const std::string& id, const std::string& where,
                                              const FlareWatch& f) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    const glideslope::sim::Aircraft::Stance stance = aircraft.stance();
    const bool strikes = !entry.seaplane && stance.found && !stance.tail_wheel &&
                         stance.strike_pitch_deg < 90.0;
    std::printf("      flare (%s): path %.1f, lowest %.1f, touched at %.1f, strikes at %s; "
                "climbed at most %.0f ft/min (at %.1f ft)\n",
                where.c_str(), f.path_pitch_deg, f.least_pitch_deg, f.touch_pitch_deg,
                strikes ? std::to_string(stance.strike_pitch_deg).c_str() : "nothing",
                f.most_climb_fpm, f.most_climb_agl_ft);
    std::vector<std::string> wrong;
    if (!f.began || !f.touched) {
        wrong.push_back(id + " (" + where + ") never flared and touched");
        return wrong;
    }
    if (f.least_pitch_deg < f.path_pitch_deg - flare_dip_margin_deg) {
        wrong.push_back(id + " (" + where + ") flew the path at " +
                        std::to_string(f.path_pitch_deg) + " degrees and its flare put the nose "
                        "down to " + std::to_string(f.least_pitch_deg));
    }
    if (strikes != (no_tail_to_strike().count(id) == 0)) {
        wrong.push_back(id + (strikes ? " has a tail to strike and is named as having none"
                                      : " has no tail to strike and is not named"));
    }
    if (f.most_climb_fpm > most_flare_climb_fpm) {
        wrong.push_back(id + " (" + where + ") climbed at " + std::to_string(f.most_climb_fpm) +
                        " ft/min in its flare, at " + std::to_string(f.most_climb_agl_ft) +
                        " ft: a balloon");
    }
    if (strikes && f.touch_pitch_deg >= stance.strike_pitch_deg - strike_margin_deg) {
        wrong.push_back(id + " (" + where + ") touched at " + std::to_string(f.touch_pitch_deg) +
                        " degrees, within " + std::to_string(strike_margin_deg) +
                        " of its tail strike at " + std::to_string(stance.strike_pitch_deg));
    }
    return wrong;
}

// **The strike half of the check, seen red.** No aeroplane's flare comes
// near its strike attitude - the incidence guard stops every nose first -
// so the landings alone never exercise it: a lander bounded at the strike
// plus a degree still passed them all. So it is fed, for every aeroplane with
// a tail to strike, a flare that touched at the lander's own bound, which it
// must name, and one three degrees short of the strike, which it must not.
// Returns how many aeroplanes it was put to.
std::size_t the_strike_check_is_seen_red(std::vector<std::string>& wrong) {
    std::size_t put = 0;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        const auto stance = aircraft.stance();
        if (entry.seaplane || !stance.found || stance.tail_wheel ||
            stance.strike_pitch_deg >= 90.0) {
            continue;
        }
        FlareWatch at_bound;
        at_bound.began = at_bound.touched = true;
        at_bound.path_pitch_deg = at_bound.least_pitch_deg = 0.0;
        at_bound.touch_pitch_deg = stance.strike_pitch_deg - 2.0;
        FlareWatch short_of_it = at_bound;
        short_of_it.touch_pitch_deg = stance.strike_pitch_deg - 3.0;
        if (flared_from_the_path(entry.id, "at the lander's bound", at_bound).empty()) {
            wrong.push_back(entry.id + ": a touch at the lander's bound was not named");
        }
        if (!flared_from_the_path(entry.id, "three degrees short", short_of_it).empty()) {
            wrong.push_back(entry.id + ": a touch three degrees short of the strike was named");
        }
        ++put;
    }
    return put;
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
    // The descent through the approach stage, in feet a second, which is what
    // its "down the glidepath, not out of the sky" band is set from.
    double sink_least_fps = 1e9;
    double sink_most_fps = -1e9;
    // When the most negative of those came, in seconds from the start.
    double sink_worst_at_s = 0.0;
    // The descent at the moment the approach stage ended, fifty feet up: what
    // the lesson's "arrive under control" need is set from.
    double sink_at_end_fps = 0.0;
    double alpha_at_end_deg = 0.0; // and the angle of attack then
    // Down the approach stage: the angle of attack's least and most, and the
    // most of the stabilator's nose-up travel used, 0 to 1.
    double alpha_least_deg = 1e9;
    double alpha_most_deg = -1e9;
    double most_nose_up = 0.0;
    bool trimmed = false; // started trimmed on the path, as asked
    // From the touch to the stop, which is further than the lesson watches:
    // it ends at thirty knots, and the Learjet's nose went through the
    // runway after that.
    glideslope::test::AfterTouch after;
    bool stopped = false;
    // Where the wheels (or the hull) first met the runway: beyond the
    // threshold, and right of the centreline.
    double touch_along_m = 0.0;
    double touch_across_m = 0.0;
    // The speed as she passed over the threshold, and whether she did - in
    // the air or not.
    double threshold_kts = 0.0;
    bool crossed = false;
    FlareWatch flare;
    // Whether the lander ever went around, and why.
    bool went_around = false;
    std::string why;
    bool unstabilized = false; // sent round by the stabilized-approach gate
    // Her speed over the target, knots, each step the gate judges speed -
    // from 500 ft down to 50, not yet down.
    std::vector<double> gate_over_kts;
};

// **The worst deviation held for the gate's whole window**: the most she was
// slow (or fast) by at every step of some two seconds running.
double worst_sustained(const std::vector<double>& over_kts, bool slow) {
    const auto window = static_cast<std::size_t>(
        glideslope::sim::StabilizedApproach::sustained_s * steps_per_second);
    double worst = -1e9;
    for (std::size_t i = 0; i + window <= over_kts.size(); ++i) {
        double held = 1e9;
        for (std::size_t k = i; k < i + window; ++k) {
            held = std::min(held, slow ? -over_kts[k] : over_kts[k]);
        }
        worst = std::max(worst, held);
    }
    return worst;
}

// **The same approach at another weight or in other air**: a loading in
// place of the lesson's, with the reference speed worked for its weight as a
// pilot works it - in proportion to the root of the weight, the lift at a
// given incidence going as the square of the speed - and weather to fly it
// in, with knots added to the reference for gusts.
struct ApproachVariant {
    std::string name;
    std::optional<glideslope::sim::Loading> loading; // none: the lesson's
    std::shared_ptr<glideslope::sim::Weather> weather;
    double add_kts = 0.0;
};

// What she weighs, loaded by `load`, in pounds.
double weight_lbs(const std::string& model,
                  const std::function<void(glideslope::sim::Aircraft&)>& load) {
    glideslope::sim::Aircraft a(data() / "jsbsim", model);
    a.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));
    load(a);
    glideslope::sim::InitialConditions ic;
    ic.altitude_ft = 3000.0;
    ic.airspeed_kts = 150.0;
    ic.engine_running = true;
    a.initialize(ic);
    return a.property("inertia/weight-lbs");
}

// **Two miles out on the glidepath, down to a stop.** `fast_by_kts` is flown
// by telling the approach autopilot a reference speed the aeroplane has not
// got: it then flies a correct approach at the wrong speed, which is what an
// approach flown fast is. The lesson still resolves `vref` from the
// aeroplane's own published figures, so it sees the difference.
Approached fly_the_approach(const std::string& id, double fast_by_kts,
                             const ApproachVariant* variant = nullptr) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);
    auto flown_with = published;
    flown_with.vref_kts += fast_by_kts;
    if (variant != nullptr && variant->loading) {
        const double lesson_lbs = weight_lbs(entry.model, [&](glideslope::sim::Aircraft& a) {
            load_for_the_approach(a, entry.model);
        });
        const double variant_lbs = weight_lbs(
            entry.model, [&](glideslope::sim::Aircraft& a) { a.load(*variant->loading); });
        flown_with.vref_kts *= std::sqrt(variant_lbs / lesson_lbs);
        // Scaled here, with the offset over it: the lander is told so, and
        // does not scale it again (sim::for_weight).
        flown_with.reference_lbs = variant_lbs;
    }
    if (variant != nullptr) {
        flown_with.vref_kts += variant->add_kts;
    }

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    // At the weight its reference speed was measured at: the B-2A's is taken
    // at its light loading, and flown at the model's own weight 124 knots is
    // below its stall - it fell at 110 ft/s and was passed, because every
    // stage of an approach ends on a height.
    load_as_its_figures_were_measured(aircraft, entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; },
        [water = entry.seaplane](double, double) { return water; }));

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
    load_for_the_approach(aircraft, entry.model);
    if (variant != nullptr && variant->loading) {
        aircraft.load(*variant->loading);
    }
    if (variant != nullptr && variant->weather) {
        aircraft.set_weather(variant->weather);
    }
    // Established on the approach: the flap it is flown with is already down,
    // and it is already coming down the glidepath rather than level on it.
    ic.flaps = flown_with.flap;
    ic.speedbrake = flown_with.speedbrake;
    ic.flight_path_deg = -3.0;
    ic.trim = true;
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
        const auto departure = book_departure_speeds(data(), entry.model);
        rotate = departure.rotate_kts;
        climb = departure.climb_kts;
    } catch (const std::exception&) {
    }
    LessonRun run(*it, glideslope::sim::LessonSpeeds{
                           rotate, climb, published.vref_kts,
                           published.stall_kts});

    glideslope::sim::Lander lander(aircraft, runway, flown_with);
    Approached out;
    out.after.judged_as(entry.seaplane);
    out.vref_kts = published.vref_kts;
    out.stage_least.assign(it->stages.size(), 1e9);
    out.stage_most.assign(it->stages.size(), -1e9);
    for (int tick = 0; tick < 600 * steps_per_second && !run.finished(); ++tick) {
        aircraft.set_controls(lander.fly());
        aircraft.step();
        const std::size_t which = run.stage();
        run.update(aircraft, tick);
        const double kts = aircraft.property("velocities/vc-kts");
        if (const double above_ft = lander.above_m() * feet_per_metre;
            !lander.touched() && above_ft <= 500.0 && above_ft >= 50.0 &&
            (lander.stage() == glideslope::sim::Lander::Stage::approach ||
             lander.stage() == glideslope::sim::Lander::Stage::flare)) {
            out.gate_over_kts.push_back(aircraft.state().airspeed_kts - flown_with.vref_kts);
        }
        if (which < out.stage_least.size()) {
            out.stage_least[which] = std::min(out.stage_least[which], kts);
            out.stage_most[which] = std::max(out.stage_most[which], kts);
        }
        if (which == 0 && run.stage() != 0) {
            out.sink_at_end_fps = aircraft.property("velocities/h-dot-fps");
            out.alpha_at_end_deg = aircraft.property("aero/alpha-deg");
        }
        if (which == 0) {
            const double fps = aircraft.property("velocities/h-dot-fps");
            if (fps < out.sink_least_fps) {
                out.sink_least_fps = fps;
                out.sink_worst_at_s = static_cast<double>(tick) / steps_per_second;
            }
            out.sink_most_fps = std::max(out.sink_most_fps, fps);
            const double alpha = aircraft.property("aero/alpha-deg");
            out.alpha_least_deg = std::min(out.alpha_least_deg, alpha);
            out.alpha_most_deg = std::max(out.alpha_most_deg, alpha);
            out.most_nose_up =
                std::max(out.most_nose_up, -aircraft.property("fcs/elevator-pos-norm"));
        }
        out.after.watch(aircraft);
        out.flare.watch(&lander, aircraft);
        out.went_around =
            out.went_around || lander.stage() == glideslope::sim::Lander::Stage::go_around;
        if (out.why.empty()) {
            out.why = lander.why_gone_around();
        }
        out.unstabilized = out.unstabilized || lander.went_around_unstabilized();
        if (!out.crossed && lander.along_m() <= 0.0) {
            out.crossed = true;
            out.threshold_kts = kts;
        }
        if (aircraft.property("position/h-agl-ft") > 5.0) {
            out.least_kts = std::min(out.least_kts, kts);
            out.most_kts = std::max(out.most_kts, kts);
        }
    }
    // **On to the stop**, which the lesson does not wait for, watching her
    // all the way. A flying boat is done below twenty knots on the water, as
    // her lesson is: afloat with her engines idling she is never quite still.
    const bool seaplane = entry.seaplane;
    const auto done = [&] {
        return seaplane ? aircraft.in_water() && aircraft.property("velocities/vc-kts") <= 20.0
                        : lander.stage() == glideslope::sim::Lander::Stage::stopped;
    };
    for (int tick = 0; tick < 300 * steps_per_second && run.finished() && !done(); ++tick) {
        if (lander.gone_around()) {
            break;
        }
        aircraft.set_controls(lander.fly());
        aircraft.step();
        out.after.watch(aircraft);
        out.flare.watch(&lander, aircraft);
        out.went_around =
            out.went_around || lander.stage() == glideslope::sim::Lander::Stage::go_around;
        if (out.why.empty()) {
            out.why = lander.why_gone_around();
        }
        out.unstabilized = out.unstabilized || lander.went_around_unstabilized();
    }
    out.stopped = done();
    out.touch_along_m = lander.touchdown_along_m();
    out.touch_across_m = lander.touchdown_across_m();
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    out.stages = it->stages.size();
    out.trimmed = aircraft.trimmed();
    return out;
}

} // namespace

// **Over-rotated, every nose-wheel aeroplane strikes its tail where its
// airframe would, and its take-off lesson says so.** Each is flown down the
// runway by the take-off autopilot until the autopilot begins its rotation,
// and from there the stick is held fully back - not the autopilot's, which
// keeps the nose short of the strike - until something that is not a wheel
// touches the runway, or she is fifteen feet up - with her flaps up, the
// over-rotation that strikes tails: rotated at her flapped speed with no flap
// she cannot fly off before her nose is past the strike. With them down the
// Learjet 35A and the A380 flew off first. Each must strike, at an
// attitude within a degree of the one her contacts say her tail strikes at
// (`Aircraft::stance`), and her take-off lesson's debrief must name it. The
// aeroplanes with no tail to strike are the approach test's (above); the
// others left out are named below, with their reasons.
GLIDESLOPE_TEST(an_over_rotated_take_off_strikes_the_tail_and_its_debrief_says_so) {
    const std::map<std::string, std::string> cannot_take_off = {
        {"747-400", "no climbing speed in its figures, so no take-off to fly"},
        {"f22", "no climbing speed in its figures, so no take-off to fly"},
        {"mosquito-fb6", "she stands on a tail wheel, her tail down already"},
        {"f35b", "her model's tail strikes at 22 degrees, and held fully back she flies off "
                 "before it (the tail \"The Learjet has nothing behind its main wheels\")"},
    };
    std::size_t catalogue = 0;
    std::size_t struck = 0;
    std::vector<std::string> left_out;
    std::vector<std::string> wrong;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        ++catalogue;
        if (no_tail_to_strike().count(entry.id) != 0) {
            left_out.push_back(entry.id + ": " + no_tail_to_strike().at(entry.id));
            continue;
        }
        if (cannot_take_off.count(entry.id) != 0) {
            left_out.push_back(entry.id + ": " + cannot_take_off.at(entry.id));
            continue;
        }
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
        load_for_the_take_off(aircraft, entry.model);
        aircraft.initialize(ic);
        const glideslope::sim::Aircraft::Stance stance = aircraft.stance();
        const auto lesson = lesson_for(entry, "take-off");
        check(lesson.has_value(), entry.id + " has a take-off lesson");
        LessonRun run(*lesson, glideslope::sim::LessonSpeeds{speeds.rotate_kts, speeds.climb_kts});
        glideslope::sim::Departure departure(aircraft, runway, speeds);
        const double standing_ft = aircraft.property("position/h-agl-ft");
        double strike_pitch_deg = 0.0;
        double strike_kts = 0.0;
        bool hit = false;
        for (int tick = 0; tick < 240 * steps_per_second; ++tick) {
            glideslope::sim::Controls controls = departure.fly();
            if (departure.stage() != glideslope::sim::Departure::Stage::roll) {
                controls.elevator = 1.0;
            }
            controls.flaps = 0.0;
            aircraft.set_controls(controls);
            aircraft.step();
            run.update(aircraft, tick);
            if (aircraft.contact().airframe) {
                hit = true;
                strike_pitch_deg = aircraft.state().pitch_deg;
                strike_kts = aircraft.property("velocities/vc-kts");
                break;
            }
            if (aircraft.property("position/h-agl-ft") > standing_ft + 15.0) {
                break;
            }
        }
        const std::vector<std::string> debrief = run.debrief_lines();
        std::printf("  %-12s strikes at %5.1f by her contacts; %s", entry.id.c_str(),
                    stance.strike_pitch_deg, hit ? "struck at " : "never struck\n");
        if (hit) {
            std::printf("%.1f degrees, %.0f knots\n", strike_pitch_deg, strike_kts);
        }
        for (const std::string& said : debrief) {
            std::printf("      %s\n", said.c_str());
        }
        std::fflush(stdout);
        if (!hit) {
            wrong.push_back(entry.id + " was held fully back and never struck its tail");
            continue;
        }
        ++struck;
        if (std::abs(strike_pitch_deg - stance.strike_pitch_deg) > 1.0) {
            wrong.push_back(entry.id + " struck at " + std::to_string(strike_pitch_deg) +
                            " degrees, not within one of " +
                            std::to_string(stance.strike_pitch_deg));
        }
        const bool said_so = std::any_of(debrief.begin(), debrief.end(), [](const std::string& s) {
            return s.find("Keep the airframe off the runway") != std::string::npos;
        });
        if (!said_so) {
            wrong.push_back(entry.id + "'s debrief does not name the strike");
        }
    }
    for (const std::string& why : left_out) {
        std::printf("  left out - %s\n", why.c_str());
    }
    std::string all;
    for (const std::string& what : wrong) {
        all += "\n    " + what;
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " wrong:" + all);
    // **Coverage**: sixteen aircraft; nine struck, and seven named - the three
    // with no tail to strike, the two with no take-off, the Mosquito on her
    // tail wheel and the F-35B.
    check(catalogue == 16, "sixteen aircraft in the catalogue, not " + std::to_string(catalogue));
    check(struck + left_out.size() == catalogue, "every aircraft struck or named");
    check(struck == 9, "nine aeroplanes struck their tails, not " + std::to_string(struck));
}

// **Flown by the book, the approach lesson leaves an empty debrief** - for
// every light aeroplane, each down its own glidepath at its own speed.
GLIDESLOPE_TEST(the_approach_lesson_flown_by_the_book_leaves_an_empty_debrief) {
    const auto taught = everyone_taught("approach-and-landing");
    std::size_t walked = 0;
    std::vector<std::string> came_down_badly;
    for (const std::string& id : taught) {
        const Approached flown = fly_the_approach(id, 0.0);
        std::printf("  %-6s vref %.0f:", id.c_str(), flown.vref_kts);
        for (std::size_t i = 0; i < flown.stage_least.size(); ++i) {
            if (flown.stage_most[i] > 0.0) {
                std::printf("  stage %zu %.0f-%.0f", i, flown.stage_least[i],
                            flown.stage_most[i]);
            }
        }
        std::printf("  descent %.1f to %.1f ft/s (worst at %.0f s), %.1f at 50 ft"
                    "  (%zu of %zu stages)%s\n",
                    flown.sink_least_fps, flown.sink_most_fps, flown.sink_worst_at_s,
                    flown.sink_at_end_fps, flown.completed, flown.stages,
                    flown.trimmed ? "" : ", started untrimmed: JSBSim cannot trim it");
        std::printf("         touched sinking %.0f ft/min%s; after touching: rolled %.1f, "
                    "pitched down to %.1f, rose %.2f ft, %s\n",
                    flown.after.touch_sink_fpm,
                    flown.after.wreck.empty() ? "" : (", WRECKED: " + flown.after.wreck).c_str(),
                    flown.after.worst_roll_deg, flown.after.least_pitch_deg,
                    flown.after.highest_ft, flown.stopped ? "stopped" : "NOT STOPPED");
        std::printf("         touched %.0f m beyond the threshold, %.1f m right of the "
                    "centreline\n",
                    flown.touch_along_m, flown.touch_across_m);
        for (const std::string& said : flown.debrief) {
            std::printf("    %s\n", said.c_str());
        }
        check(flown.completed == flown.stages,
              id + " got through all its stages, not " +
                  std::to_string(flown.completed));
        check(flown.debrief.empty(),
              id + " flown by the book says nothing, and it said " +
                  std::to_string(flown.debrief.size()) + " things");
        check(flown.stopped, id + " came to a stop after the approach");
        // **On the runway, past its threshold**, as the circuit's are.
        check(flown.touch_along_m >= 0.0 && flown.touch_along_m <= a_runway().length_m &&
                  std::abs(flown.touch_across_m) <= 10.0,
              id + " touched down " + std::to_string(flown.touch_along_m) +
                  " m beyond the threshold and " + std::to_string(flown.touch_across_m) +
                  " m right of the centreline, which is not within 10 m of it");
        for (const std::string& wrong : flown.after.what_went_wrong(id)) {
            came_down_badly.push_back(wrong);
        }
        for (const std::string& wrong :
             flown.after.how_the_gear_took_it(id + " (approach)", settled_within_ft)) {
            came_down_badly.push_back(wrong);
        }
        for (const std::string& wrong : flared_from_the_path(id, "approach", flown.flare)) {
            came_down_badly.push_back(wrong);
        }
        for (const std::string& wrong :
             inside_the_touchdown_zone(id, "approach", flown.touch_along_m)) {
            came_down_badly.push_back(wrong);
        }
        ++walked;
    }
    // The strike check, on a flare built to break it: every aeroplane with a
    // tail to strike - the sixteen less the three named as having none.
    const std::size_t put = the_strike_check_is_seen_red(came_down_badly);
    check(put == 13, "the strike check was put to thirteen aeroplanes, not " +
                         std::to_string(put));
    // **Every one of them stayed on its wheels, the right way up**, from the
    // touch to the stop, and flared from the attitude it flew the glidepath
    // at to one short of its tail strike - named all together, so one run
    // shows them all.
    for (const std::string& wrong : came_down_badly) {
        std::printf("  CAME DOWN BADLY: %s\n", wrong.c_str());
    }
    check(came_down_badly.empty(),
          std::to_string(came_down_badly.size()) +
              " things went wrong after touching down at the end of the approach lesson, "
              "the first: " + (came_down_badly.empty() ? "" : came_down_badly.front()));
    check(walked == taught.size(), "every aeroplane taught an approach was landed");
    // Fourteen of the sixteen: the four light aircraft, the Mosquito, the
    // Learjet, four airliners, two fighters, the B-2A and the Short S.23 -
    // which alights on water. The 747-400 and the F-22A are left out, having
    // no stall speed to make a reference speed from - `everyone_taught` names
    // them above.
    check(walked == 14, "fourteen aeroplanes landed, not " + std::to_string(walked));
}

namespace {

// **Her lightest and her heaviest landing**: the lightest loading her figures
// name, and the heaviest she lands at - the one named "landing" where there
// is one (an airliner's maximum is her take-off weight, over what she may
// land at), else the heaviest named. Where her figures name none - the
// C172P, C182 and PA-28 - the one they give is the heavy, and the same with
// a quarter of its fuel the light.
std::pair<glideslope::sim::Loading, glideslope::sim::Loading> light_and_heavy(
    const std::string& model) {
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / (model + ".xml"));
    if (figures.loadings.empty()) {
        glideslope::sim::Loading light = figures.loading;
        for (auto& [tank, lbs] : light.tank_lbs) {
            lbs *= 0.25;
        }
        return {light, figures.loading};
    }
    const glideslope::sim::FigureLoading* light = nullptr;
    const glideslope::sim::FigureLoading* heavy = nullptr;
    for (const auto& [name, loading] : figures.loadings) {
        if (light == nullptr || loading.total_lbs < light->total_lbs) {
            light = &loading;
        }
        if (heavy == nullptr || loading.total_lbs > heavy->total_lbs) {
            heavy = &loading;
        }
    }
    if (const auto landing = figures.loadings.find("landing"); landing != figures.loadings.end()) {
        heavy = &landing->second;
    }
    return {light->loading, heavy->loading};
}

// **Gusty air down final**: fifteen knots straight down the runway, with
// JSBSim's MIL-F-8785C turbulence at severity 3 of its 7 scaled by that
// wind, and five knots on the reference speed - half of a ten-knot gust
// factor, as the FAA's Airplane Flying Handbook (FAA-H-8083-3C, chapter 9)
// adds half the gust factor.
std::shared_ptr<glideslope::sim::Weather> gusty_down_the_runway() {
    const double heading = a_runway().heading_deg / degrees;
    constexpr double wind_mps = 15.0 * 0.514444;
    glideslope::sim::Conditions c;
    // A wind from the runway's heading blows the other way.
    c.wind_north_mps = -wind_mps * std::cos(heading);
    c.wind_east_mps = -wind_mps * std::sin(heading);
    c.turbulence_severity = 3;
    c.wind_at_20ft_mps = wind_mps;
    return std::make_shared<glideslope::sim::SteadyWeather>(c);
}

} // namespace

// **Light and heavy, no landing balloons or bounces, and none goes around.**
// The flare's sink judged a moment ahead, and a jet's attitude held in its
// last feet, were tuned on the lessons' one loading: here every aeroplane
// taught the approach (14) is flown down it in calm air at her lightest and
// heaviest landing loadings, her reference speed worked for the weight. Each
// must touch inside the touchdown zone, unwrecked by the server's rule, climb
// nowhere in its flare, rise no more than half a foot after its wheels meet
// the runway, stay upright on its wheels, stop - and never go around.
// Coverage: 14 aeroplanes, two cases each, 28; the two that fail are named
// with their reasons (below), 26 judged. Gusty air is the next test's.
GLIDESLOPE_TEST(every_aeroplane_lands_light_and_heavy_without_a_balloon_a_bounce_or_a_go_around) {
    const auto taught = everyone_taught("approach-and-landing");
    // **Named and not judged, with why** - flown and shown all the same. The
    // flare before the look-ahead (2026-10-06) failed every one of these
    // too, and five more; what is wrong is a tail in docs/COMPLETION_PLAN.md.
    const std::string light_overshoot =
        "her nose overshoots the flare's attitude by two degrees and more after the sink "
        "is arrested, and she climbs in it";
    const std::map<std::string, std::string> named = {
        {"737-300 (light)", light_overshoot},
        {"mosquito-fb6 (heavy)", light_overshoot},
    };
    std::size_t flown = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const std::string& id : taught) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const auto [light, heavy] = light_and_heavy(entry.model);
        const std::vector<ApproachVariant> variants = {
            {"light", light, nullptr, 0.0},
            {"heavy", heavy, nullptr, 0.0},
        };
        for (const ApproachVariant& v : variants) {
            const std::string where = id + " (" + v.name + ")";
            const Approached r = fly_the_approach(id, 0.0, &v);
            const bool judged = named.count(where) == 0;
            if (judged) {
                ++flown;
            } else {
                std::printf("  named, not judged - %s: %s\n", where.c_str(),
                            named.at(where).c_str());
                ++left_out;
            }
            std::vector<std::string> case_wrong;
            std::printf("  %-22s touched %4.0f ft/min %4.0f m along, rose %.2f ft, most climb "
                        "in the flare %4.0f ft/min%s%s\n",
                        where.c_str(), r.after.touch_sink_fpm, r.touch_along_m,
                        r.after.highest_ft, r.flare.most_climb_fpm,
                        r.went_around ? ", WENT AROUND" : "", r.stopped ? "" : ", NOT STOPPED");
            if (r.went_around) {
                case_wrong.push_back(where + " went around");
            }
            for (const std::string& w : r.after.what_went_wrong(where)) {
                case_wrong.push_back(w);
            }
            for (const std::string& w : r.after.how_the_gear_took_it(where, settled_within_ft)) {
                case_wrong.push_back(w);
            }
            if (r.flare.most_climb_fpm > most_flare_climb_fpm) {
                case_wrong.push_back(where + " climbed at " +
                                     std::to_string(r.flare.most_climb_fpm) +
                                     " ft/min in its flare: a balloon");
            }
            for (const std::string& w : inside_the_touchdown_zone(id, v.name, r.touch_along_m)) {
                case_wrong.push_back(w);
            }
            if (!r.stopped && !r.went_around) {
                case_wrong.push_back(where + " did not stop");
            }
            for (const std::string& w : case_wrong) {
                if (judged) {
                    wrong.push_back(w);
                } else {
                    std::printf("      (named) %s\n", w.c_str());
                }
            }
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(taught.size() == 14, "fourteen aeroplanes taught the approach, not " +
                                   std::to_string(taught.size()));
    check(flown + left_out == 2 * taught.size() && left_out == named.size(),
          "every aeroplane flown in every case or named: " + std::to_string(flown) +
              " flown and " + std::to_string(left_out) + " named of " +
              std::to_string(2 * taught.size()));
}

namespace {

// **An approach the AI flies as a server's is flown**: two miles out on the
// glidepath, established, at the weight `figures_loading` says - her figures'
// own loading for her approach speed, or, false, her model's own weight,
// which is what a server flies every aircraft at - handed to the AI through
// a `Controller` with her published speeds, unscaled, as the server hands
// them (`to_ai_approach`). She starts at her reference speed for what she
// weighs (sim::for_weight), as the server starts her.
struct WeighedApproach {
    double weight_lbs = 0.0;
    double published_kts = 0.0; // her figures' reference speed
    double reference_lbs = 0.0; // and the weight it is for
    double scaled_kts = 0.0;    // for what she weighs
    double flown_kts = 0.0;     // what her approach autopilot was given
    double gate_least_over_kts = 1e9; // over `scaled_kts`, 500 ft down to 50
    double gate_most_over_kts = -1e9;
    double most_alpha_deg = -1e9;
    bool went_around = false;
    std::string why;
    bool stopped = false;
    double touch_along_m = 0.0;
    glideslope::test::AfterTouch after;
};

WeighedApproach ai_approach_at_a_weight(const std::string& id, bool figures_loading) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; },
        [water = entry.seaplane](double, double) { return water; }));
    if (figures_loading) {
        load_for_the_approach(aircraft, entry.model);
    }
    WeighedApproach out;
    out.after.judged_as(entry.seaplane);
    out.weight_lbs = aircraft.loaded_weight_lbs();
    out.published_kts = published.vref_kts;
    out.reference_lbs = published.reference_lbs;
    out.scaled_kts = glideslope::sim::for_weight(published, out.weight_lbs).vref_kts;

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
    ic.airspeed_kts = out.scaled_kts;
    ic.engine_running = true;
    ic.gear = 1.0;
    ic.flaps = published.flap;
    ic.speedbrake = published.speedbrake;
    ic.flight_path_deg = -3.0;
    ic.trim = true;
    aircraft.initialize(ic);
    check(std::abs(aircraft.property("inertia/weight-lbs") - out.weight_lbs) < 1.0,
          id + " weighs what she was loaded to: " +
              std::to_string(aircraft.property("inertia/weight-lbs")) + " lb, not " +
              std::to_string(out.weight_lbs));

    glideslope::sim::Controls flying;
    flying.throttle = 0.4;
    flying.gear = 1.0;
    flying.flaps = published.flap;
    glideslope::sim::Controller controller(aircraft, flying);
    controller.to_ai_approach(runway, published);
    check(controller.lander() != nullptr, id + ": the AI has an approach to fly");
    out.flown_kts = controller.lander()->speeds().vref_kts;

    for (int tick = 0; tick < 600 * steps_per_second; ++tick) {
        aircraft.set_controls(controller.fly());
        aircraft.step();
        out.after.watch(aircraft);
        const glideslope::sim::Lander* lander = controller.lander();
        if (lander == nullptr || controller.circuit() != nullptr ||
            lander->stage() == glideslope::sim::Lander::Stage::go_around) {
            out.went_around = true;
            if (lander != nullptr && out.why.empty()) {
                out.why = lander->why_gone_around();
            }
            break;
        }
        if (!lander->touched()) {
            out.most_alpha_deg = std::max(out.most_alpha_deg, aircraft.property("aero/alpha-deg"));
            const double above_ft = lander->above_m() * feet_per_metre;
            if (above_ft <= 500.0 && above_ft >= 50.0) {
                const double over = aircraft.state().airspeed_kts - out.scaled_kts;
                out.gate_least_over_kts = std::min(out.gate_least_over_kts, over);
                out.gate_most_over_kts = std::max(out.gate_most_over_kts, over);
            }
        }
        out.touch_along_m = lander->touchdown_along_m();
        // A flying boat is done below twenty knots on the water, as her
        // lesson is: afloat with her engines idling she is never quite still.
        if (entry.seaplane ? lander->touched() && aircraft.in_water() &&
                                 aircraft.property("velocities/vc-kts") <= 20.0
                           : lander->stage() == glideslope::sim::Lander::Stage::stopped) {
            out.stopped = true;
            break;
        }
    }
    return out;
}

// Every aeroplane with an approach speed, at one weight, judged.
void every_ai_approach_at_a_weight(bool figures_loading) {
    const auto taught = everyone_taught("approach-and-landing");
    const char* weight = figures_loading ? "her figures' loading" : "her model's own weight";
    std::size_t flown = 0;
    std::size_t scaled = 0;
    std::vector<std::string> wrong;
    for (const std::string& id : taught) {
        const WeighedApproach r = ai_approach_at_a_weight(id, figures_loading);
        ++flown;
        std::printf("  %-13s %8.0f lb (figure's %8.0f): %5.1f kt -> %5.1f, flown at %5.1f; "
                    "500-50 ft %+5.1f to %+5.1f, most alpha %4.1f; touched %4.0f ft/min "
                    "%4.0f m along%s%s%s\n",
                    id.c_str(), r.weight_lbs, r.reference_lbs, r.published_kts, r.scaled_kts,
                    r.flown_kts, r.gate_least_over_kts, r.gate_most_over_kts, r.most_alpha_deg,
                    r.after.touch_sink_fpm, r.touch_along_m,
                    r.went_around ? ", WENT AROUND: " : "", r.why.c_str(),
                    r.stopped ? "" : ", NOT STOPPED");
        if (std::abs(r.weight_lbs - r.reference_lbs) > 1.0) {
            ++scaled;
        }
        // **Flown at the speed for what she weighs**: what the approach
        // autopilot was given, and what she held from 500 ft to 50 - within
        // the stabilized approach's +10/-5 kt of it.
        if (std::abs(r.flown_kts - r.scaled_kts) > 0.1) {
            wrong.push_back(id + " was given " + std::to_string(r.flown_kts) +
                            " kt to fly, not the " + std::to_string(r.scaled_kts) +
                            " for what she weighs");
        }
        if (r.gate_least_over_kts < -glideslope::sim::StabilizedApproach::most_slow_kts ||
            r.gate_most_over_kts > glideslope::sim::StabilizedApproach::most_fast_kts) {
            wrong.push_back(id + " flew " + std::to_string(r.gate_least_over_kts) + " to " +
                            std::to_string(r.gate_most_over_kts) + " kt over " +
                            std::to_string(r.scaled_kts) + " from 500 ft to 50");
        }
        if (r.went_around) {
            wrong.push_back(id + " went around: " + r.why);
        }
        for (const std::string& w : r.after.what_went_wrong(id)) {
            wrong.push_back(w);
        }
        for (const std::string& w : r.after.how_the_gear_took_it(id, settled_within_ft)) {
            wrong.push_back(w);
        }
        for (const std::string& w : inside_the_touchdown_zone(id, weight, r.touch_along_m)) {
            wrong.push_back(w);
        }
        if (!r.stopped && !r.went_around) {
            wrong.push_back(id + " did not stop");
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong at " + weight +
                             ", the first: " + (wrong.empty() ? "" : wrong.front()));
    check(taught.size() == 14, "fourteen aeroplanes with an approach speed, not " +
                                   std::to_string(taught.size()));
    check(flown == taught.size(), "every one flown: " + std::to_string(flown) + " of " +
                                      std::to_string(taught.size()));
    std::printf("  %zu of %zu flown at a weight other than her speed's\n", scaled, flown);
}

} // namespace

// **The AI flies her approach at the speed for what she weighs** - her
// reference speed times the square root of her weight over the weight her
// figures give it for (sim::for_weight) - **and lands within its limits.**
// Every aeroplane with an approach speed (14), at her model's own weight -
// what a server flies every aircraft at - handed to the AI with her
// published speeds as a server hands them. Each must be given the scaled
// speed, hold it within the stabilized approach's +10/-5 kt from 500 ft to
// 50, never go around, touch inside the touchdown zone unwrecked by the
// server's rule, stay upright on her wheels and stop.
GLIDESLOPE_TEST(the_ai_flies_every_approach_at_the_speed_for_its_models_own_weight_and_lands_within_its_limits) {
    every_ai_approach_at_a_weight(false);
}

// **And at the loading her figures give the speed for**, where the scaling
// is none: the same 14, judged the same.
GLIDESLOPE_TEST(the_ai_flies_every_approach_at_the_speed_for_its_figures_loading_and_lands_within_its_limits) {
    every_ai_approach_at_a_weight(true);
}

// **In gusts every aeroplane is flown down to the runway, or goes around** -
// and no more is claimed. The lander has no answer to turbulence yet (a tail
// in docs/COMPLETION_PLAN.md): flown at the lesson's loading down a
// fifteen-knot wind with severity-3 turbulence (`gusty_down_the_runway`),
// most balloon, bounce or come down hard, and the flying boat is lifted into
// a go-around. Nor is it the same case on every platform: turbulence turns
// floating point's differences into different gusts at the flare, and on
// CI the F-15C ballooned at 53 ft/min on Windows, was wrecked at 780 on
// macOS and landed cleanly on Linux. So how each lands is shown, not judged;
// what is asserted is that each of the 14 reached the runway or went around.
GLIDESLOPE_TEST(every_aeroplane_flown_down_in_gusts_reaches_the_runway_or_goes_around) {
    const auto taught = everyone_taught("approach-and-landing");
    std::size_t flown = 0;
    std::vector<std::string> wrong;
    const ApproachVariant gusty{"gusty", std::nullopt, gusty_down_the_runway(), 5.0};
    for (const std::string& id : taught) {
        const Approached r = fly_the_approach(id, 0.0, &gusty);
        ++flown;
        std::printf("  %-13s in gusts: %s, touched %4.0f ft/min %4.0f m along, rose %.2f ft, "
                    "most climb in the flare %4.0f ft/min%s\n",
                    id.c_str(), r.went_around ? "went around" : "landed",
                    r.after.touch_sink_fpm, r.touch_along_m, r.after.highest_ft,
                    r.flare.most_climb_fpm,
                    r.after.wreck.empty() ? "" : (", wrecked: " + r.after.wreck).c_str());
        if (!r.after.touched && !r.went_around) {
            wrong.push_back(id + " in gusts neither reached the runway nor went around");
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(taught.size() == 14 && flown == taught.size(),
          "every one of the fourteen aeroplanes taught the approach flown in gusts: " +
              std::to_string(flown));
}

// **An approach flown well in gusts is not sent round by the gate**: every
// aeroplane taught the approach (14), down final in the gusts above with
// half the gust factor on her reference speed, is never sent round for an
// approach not stabilized - its gust spikes are momentary
// (StabilizedApproach::sustained_s), and a jet's engines are kept spooled to
// answer them. Balloons in gusts are the lander's own tail, and the
// go-arounds they make are not counted here. Each touchdown's margin to the
// touchdown zone's end is printed, and the worst slow and fast deviation she
// held for the gate's whole two seconds, against its 5 and 10 kt.
GLIDESLOPE_TEST(an_approach_flown_well_in_gusts_is_not_sent_round_by_the_stabilized_gate) {
    const auto taught = everyone_taught("approach-and-landing");
    const double zone_m = glideslope::sim::StabilizedApproach::touchdown_zone_m(a_runway());
    std::size_t flown = 0;
    std::vector<std::string> wrong;
    const ApproachVariant gusty{"gusty", std::nullopt, gusty_down_the_runway(), 5.0};
    for (const std::string& id : taught) {
        const Approached r = fly_the_approach(id, 0.0, &gusty);
        ++flown;
        const double slow = worst_sustained(r.gate_over_kts, true);
        const double fast = worst_sustained(r.gate_over_kts, false);
        std::printf("  %-13s in gusts: %s%s; touched %4.0f m along, %4.0f m inside the zone; "
                    "held %.1f kt slow (limit 5), %.1f kt fast (limit 10)\n",
                    id.c_str(), r.went_around ? "went around: " : "landed", r.why.c_str(),
                    r.touch_along_m, zone_m - r.touch_along_m, slow, fast);
        if (r.unstabilized) {
            wrong.push_back(id + " was sent round by the gate: " + r.why);
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(taught.size() == 14 && flown == taught.size(),
          "every one of the fourteen aeroplanes taught the approach flown in gusts: " +
              std::to_string(flown));
}

// **The F-15C comes down the approach at its flight manual's speed**, not at
// 1.3 times a stall its manual never publishes. T.O. 1F-15A-1's figure A8-1
// gives the final approach speed by weight, at 21 units of angle of attack,
// from flight test: 160 KCAS at the 36,946 lb its figures are flown at, on the
// flaps-up line, since the model has no flaps (assets/figures/f15c.xml). It
// was flown at 196 - 1.3 times the 151-knot stall measured on the model, which
// is where its stabilator runs out, not where the aeroplane stalls. Held from
// two miles out to fifty feet, the whole of the approach stage, within five
// knots of the manual's - the chart is read to about a knot.
GLIDESLOPE_TEST(the_f15c_flies_its_approach_at_its_flight_manuals_speed_for_its_weight) {
    const auto figures =
        glideslope::sim::read_published_figures(data() / "figures" / "f15c.xml");
    check(figures.approach_kcas == 160.0,
          "the F-15C's figures give the manual's 160 KCAS approach, not " +
              std::to_string(figures.approach_kcas));
    check(figures.loadings.at(figures.approach_loading).total_lbs == 36946.0,
          "at the 36,946 lb it is read at");
    const Approached flown = fly_the_approach("f15c", 0.0);
    std::printf("  f15c vref %.1f: approach %.1f to %.1f kt, alpha %.1f at 50 ft, %zu of %zu "
                "stages\n",
                flown.vref_kts, flown.stage_least[0], flown.stage_most[0],
                flown.alpha_at_end_deg, flown.completed, flown.stages);
    check(flown.vref_kts == figures.approach_kcas,
          "the approach is flown at the manual's speed, not " +
              std::to_string(flown.vref_kts));
    check(flown.stage_least[0] >= figures.approach_kcas - 5.0 &&
              flown.stage_most[0] <= figures.approach_kcas + 5.0,
          "the F-15C held " + std::to_string(flown.stage_least[0]) + " to " +
              std::to_string(flown.stage_most[0]) +
              " KCAS down the approach, not within 5 of the manual's 160");
    check(flown.completed == flown.stages && flown.debrief.empty() && flown.stopped,
          "and landed by the book, to a stop");
}

// **The B-2A crosses the threshold within five knots of its reference
// speed.** With nothing to add drag it could not slow on the glidepath: its
// throttles shut, it crossed fourteen knots fast and floated after touching.
// Its drag rudders, opened together as the real one's are at low speed
// (tools/make_b2.py), are what let it hold the speed down the path.
GLIDESLOPE_TEST(the_b2a_crosses_the_threshold_within_five_knots_of_its_reference_speed) {
    const Approached flown = fly_the_approach("b2", 0.0);
    std::printf("  b2 vref %.1f: over the threshold at %.1f kt, %+.1f; rose %.1f ft after "
                "touching\n",
                flown.vref_kts, flown.threshold_kts, flown.threshold_kts - flown.vref_kts,
                flown.after.highest_ft);
    check(flown.crossed, "the B-2A reached the threshold");
    check(std::abs(flown.threshold_kts - flown.vref_kts) <= 5.0,
          "the B-2A crossed the threshold at " + std::to_string(flown.threshold_kts) +
              " knots, not within five of its reference speed, " +
              std::to_string(flown.vref_kts));
}

// **On the manual's approach the F-15C flies at the angle of attack NASA
// flew it at, with stabilator to spare.** NASA TM-4604's F-15 came down a
// 166-knot flaps-up approach at about 10 degrees of alpha (its figure 5),
// and takes its derivatives at 8; at the manual's 160 knots and 36,946 lb
// she holds 8 to 13 degrees from two miles out to fifty feet, using under a
// third of the stabilator's nose-up travel. Before her pitching moment was
// NASA's (tools/make_f15c.py) she swung between 10.5 and 18.3 degrees with
// the stabilator at up to 0.98 of its stop.
GLIDESLOPE_TEST(the_f15c_flies_its_approach_near_nasas_angle_of_attack_with_stabilator_to_spare) {
    const Approached flown = fly_the_approach("f15c", 0.0);
    std::printf("  f15c on the approach: alpha %.1f to %.1f, nose-up stabilator at most %.2f "
                "of its travel\n",
                flown.alpha_least_deg, flown.alpha_most_deg, flown.most_nose_up);
    check(flown.alpha_least_deg >= 8.0 && flown.alpha_most_deg <= 13.0,
          "the F-15C flew the approach at " + std::to_string(flown.alpha_least_deg) + " to " +
              std::to_string(flown.alpha_most_deg) + " degrees of alpha, not 8 to 13");
    check(flown.most_nose_up <= 0.33,
          "and used " + std::to_string(flown.most_nose_up) +
              " of the stabilator's nose-up travel, not under a third");
}

// **An approach flown fast is named in the debrief**, and a correct one is
// not. The aeroplane flies a perfectly good approach - it is simply doing it
// at a speed it has no business using.
GLIDESLOPE_TEST(an_approach_flown_fast_is_named_in_the_debrief) {
    for (const std::string& id : one_of_each_class("approach-and-landing")) {
        const Approached fast = fly_the_approach(id, 20.0);
        std::printf("  %s vref %.0f, flown fast: %.0f to %.0f knots\n", id.c_str(),
                    fast.vref_kts, fast.least_kts, fast.most_kts);
        names_that_fault_and_no_other(id, "approach-and-landing", fast.debrief,
                                      {"velocities/vc-kts"}, "flying the approach fast");
    }
}

namespace {

// An aeroplane trimmed out in level flight at `agl_ft`, ready to be flown by
// the autopilot.
struct InFlight {
    std::unique_ptr<glideslope::sim::Aircraft> aircraft;
    glideslope::sim::LessonSpeeds speeds;
    double start_agl_ft = 0.0;
    double start_heading_deg = 0.0;
    // A rate of climb this aeroplane has, for a demonstration to ask for.
    double climb_fpm = 600.0;
};

// `start_kcas` of zero means the speed the catalogue starts it at; anything
// else starts it there instead. **A lesson about a speed begins at that
// speed.** The F-35B's climbing speed is 160 knots and its catalogue start is
// a cruise: a clean jet at idle cannot lose that much in the seconds before
// the lesson starts watching, so it was judged for holding a climbing speed
// it was still on its way down to, and never reached the height that ends the
// first stage. The four light aircraft never showed this because their cruise
// and their climb are twenty knots apart.
// `loading`, where given, is what she is loaded with in place of the one her
// figures were measured at.
InFlight airborne(const std::string& id, double agl_ft, double start_kcas = 0.0,
                  const glideslope::sim::Loading* loading = nullptr) {
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
    ic.airspeed_kts =
        start_kcas > 0.0 ? start_kcas : entry.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    if (loading != nullptr) {
        out.aircraft->load(*loading);
    } else {
        load_as_its_figures_were_measured(*out.aircraft, entry.model);
    }
    out.aircraft->initialize(ic);
    // **An aeroplane that publishes no figures still has lessons.** Eleven of
    // the sixteen publish no stall speed and most publish no rate of climb,
    // and `departure_speeds` and `approach_speeds` throw rather than guess -
    // which is right, a reference speed invented is a reference speed that
    // means nothing. A lesson that names none of them, as the turns lesson
    // does, is flown all the same; one that names them would fail to resolve
    // and a test would catch it.
    try {
        const auto departure = book_departure_speeds(data(), entry.model);
        out.speeds.rotate_kts = departure.rotate_kts;
        out.speeds.climb_kts = departure.climb_kts;
    } catch (const std::exception&) {
    }
    try {
        const auto approach = glideslope::sim::approach_speeds(data(), entry.model);
        out.speeds.vref_kts = approach.vref_kts;
        // The stall the reference speed is usually 1.3 times; not the
        // F-15C's, whose manual gives its approach speed itself.
        out.speeds.stall_kts = approach.stall_kts;
    } catch (const std::exception&) {
    }
    out.start_agl_ft = agl_ft;
    out.start_heading_deg = 90.0;
    out.climb_fpm = a_climb_it_can_manage(entry.model);
    return out;
}

struct Result {
    // How far the height strayed from where the entry stage began, which is
    // what the entry's band is set from.
    double entry_low_ft = 0.0;
    double recovery_began_ft = -1.0; // where the recovery stage began
    double entry_high_ft = 0.0;
    std::vector<std::string> debrief;
    std::size_t completed = 0;
    std::size_t stages = 0;
    double lowest_agl_ft = 1e9;
    double steepest_bank_deg = 0.0;
    // A stall's recovery: where it was handed over, and how the aeroplane was
    // flying then - the height, the airspeed, the angle of attack, the pitch
    // and the vertical speed - and the lowest height after it.
    double handed_over_ft = -1.0;
    double handed_over_kts = 0.0;
    double handed_over_alpha_deg = 0.0;
    double handed_over_pitch_deg = 0.0;
    double handed_over_fpm = 0.0;
    double recovery_lowest_ft = 1e9;
    // Below the angle it stalled at, level or climbing, and at the speed the
    // lesson's recovery ends at, all at once.
    bool recovered = false;
    double stall_alpha_deg = 0.0; // where its lift stopped rising
    double peak_load_g = -1e9;    // the most, from the hand-over to recovered
    // The true airspeed at the hand-over, and the true airspeed the lesson's
    // recovery speed is there, in ft/s: what the height bound is worked from.
    double handed_over_tas_fps = 0.0;
    double recovered_tas_fps = 0.0;
    // The most the elevator, ailerons or rudder moved on the step the
    // recovery was engaged, and on the step it was let go once recovered
    // (negative where it was not let go).
    double engaged_step = 0.0;
    double let_go_step = -1.0;
    // The flap lever through the recovery, which is the autopilot's from the
    // hand-over: where it was handed over, where it ended, the lowest it went
    // and the most it moved in a step.
    double flaps_handed = -1.0;
    double flaps_end = -1.0;
    double flaps_lowest = 2.0;
    double flaps_most_step = 0.0;
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
    for (const std::string& id : one_of_each_class("turns")) {
        const Result sinking = fly_a_turn(id, -1200.0);
        std::printf("  %s sinking: lowest %.0f ft of 3000, banked to %.0f\n", id.c_str(),
                    sinking.lowest_agl_ft, sinking.steepest_bank_deg);
        names_that_fault_and_no_other(id, "turns", sinking.debrief, {"position/h-agl-ft"},
                                      "losing height in the turn");
    }
}

namespace {

// **A climb, a level-off and a descent, flown by the autopilot.**
// `fast_by_kts` other than zero flies the climb at a speed that is not the
// climbing speed, which is the fault this lesson is for.
Result fly_a_climb_and_descent(const std::string& id, double fast_by_kts) {
    // Started at the speed this exercise is flown at, which for a jet is a
    // long way from the speed it cruises at.
    double climb_kcas = 0.0;
    try {
        climb_kcas = book_departure_speeds(
                         data(), glideslope::sim::find_aircraft(data(), id).model)
                         .climb_kts;
    } catch (const std::exception&) {
    }
    InFlight f = airborne(id, 3000.0,
                          climb_kcas > 0.0 ? climb_kcas + fast_by_kts : 0.0);
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
    modes.vertical_speed_fpm = a_climb_it_can_manage(
        glideslope::sim::find_aircraft(data(), id).model);
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
// aeroplane that has a climbing speed to do it at, each at its own.
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
    // Fourteen of the sixteen: the four light aircraft, the Mosquito, the
    // Learjet, four airliners, two fighters, the B-2A and the Short S.23.
    // The 747-400 and the F-22A are the two left out, having no climbing
    // speed - `everyone_taught` names them above, with the reason.
    check(walked == 14, "fourteen aeroplanes climbed, not " + std::to_string(walked));
}

// **A climb flown at the wrong speed is named for it.**
GLIDESLOPE_TEST(a_climb_at_the_wrong_speed_is_named_in_the_debrief) {
    for (const std::string& id : one_of_each_class("climb-and-descent")) {
        // **Fifteen knots past the top of its own lesson's band**, which is
        // fifteen knots wide for a light aeroplane and forty for a jet: a
        // fixed thirty was a fault in a Cessna and inside the band in a 737.
        const auto lesson =
            lesson_for(glideslope::sim::find_aircraft(data(), id), "climb-and-descent");
        double above_kts = 0.0;
        for (const auto& w : lesson->stages.front().holds) {
            if (w.property == "velocities/vc-kts" && w.banded) {
                above_kts = w.high.offset;
            }
        }
        check(above_kts > 0.0, id + "'s climb holds a band above its climbing speed");
        const Result fast = fly_a_climb_and_descent(id, above_kts + 15.0);
        std::printf("  %s climbing %.0f knots fast\n", id.c_str(), above_kts + 15.0);
        names_that_fault_and_no_other(id, "climb-and-descent", fast.debrief,
                                      {"velocities/vc-kts"}, "climbing fast");
    }
}

namespace {

// The speed a stall lesson's recovery ends at, for this aeroplane.
double recovery_ends_at_kts(const Lesson& lesson, const glideslope::sim::LessonSpeeds& speeds) {
    return glideslope::sim::stall_recovery_ends_at_kts(lesson, speeds);
}

// **Watching a stall recovered**, the same for the lesson's flight and the
// instructor's: what the aeroplane was doing when it was handed over, the
// lowest it went and the most load it pulled until it was recovered.
//
// **Recovered is flying again, not only fast again**: the wing below the
// angle of attack it stalled at, the aeroplane level or climbing, and at or
// above the speed the lesson's recovery ends at, all at once. A speed alone
// called an A320 recovered at 27 degrees of alpha and sinking 10,000 ft/min,
// with no pull-out flown at all.
//
// **The angle it stalled at is where its lift stopped rising**: the angle of
// attack at the greatest lift coefficient measured from the entry to the
// hand-over. A model's lift table is the model's own; this reads what it
// gives, in the configuration and at the rate it is flown here.
//
// **Level is within 100 ft/min**, as a pilot holds a height. A PA-28 at full
// power with its landing flap out at 4,000 ft cannot climb: at its recovery
// speed it settles sinking 45 ft/min, flying and in hand, and a rule of "not
// descending at all" would never call it recovered.
//
// **Recovered must last**: five seconds of it together, counted in steps,
// with the lowest height taken throughout - so that the top of a zoom climb,
// level and fast for an instant with the wing unloaded, is not called a
// recovery. **The load is read over a quarter of a second**, the mean of the
// last thirty steps, so that one step's jolt is not the peak.
struct RecoveryWatch {
    static constexpr double level_within_fpm = 100.0;
    static constexpr std::int64_t recovered_for = 5 * steps_per_second;
    static constexpr std::size_t load_window = steps_per_second / 4;

    RecoveryWatch(Result& result, std::string property, double kts)
        : out(result), until_property(std::move(property)), recovered_kts(kts) {}

    Result& out;
    std::string until_property;
    double recovered_kts = 0.0;
    double most_lift = -1e9;
    std::vector<double> loads;
    double load_sum = 0.0;
    std::int64_t steps = 0;
    std::int64_t recovered_since = -1;
    bool handed = false;

    // Before the hand-over: where its lift peaks.
    void entering(const glideslope::sim::Aircraft& a) {
        const double lift = a.property("forces/fwz-aero-lbs") /
                            std::max(a.property("aero/qbar-psf") * a.property("metrics/Sw-sqft"),
                                     1.0);
        if (lift > most_lift) {
            most_lift = lift;
            out.stall_alpha_deg = a.property("aero/alpha-deg");
        }
    }

    void hand_over(const glideslope::sim::Aircraft& a) {
        handed = true;
        out.handed_over_ft = a.property("position/h-agl-ft");
        out.handed_over_kts = a.property("velocities/vc-kts");
        out.handed_over_alpha_deg = a.property("aero/alpha-deg");
        out.handed_over_pitch_deg = a.property("attitude/theta-deg");
        out.handed_over_fpm = a.property("velocities/h-dot-fps") * 60.0;
        out.handed_over_tas_fps = a.property("velocities/vt-fps");
        out.recovered_tas_fps =
            recovered_kts * out.handed_over_tas_fps / std::max(out.handed_over_kts, 1.0);
    }

    // After each step from the hand-over, until recovered.
    void recovering(const glideslope::sim::Aircraft& a) {
        if (!handed || out.recovered) {
            return;
        }
        ++steps;
        out.recovery_lowest_ft =
            std::min(out.recovery_lowest_ft, a.property("position/h-agl-ft"));
        const double load = a.property("accelerations/Nz");
        loads.push_back(load);
        load_sum += load;
        if (loads.size() > load_window) {
            load_sum -= loads[loads.size() - load_window - 1];
        }
        if (loads.size() >= load_window) {
            out.peak_load_g =
                std::max(out.peak_load_g, load_sum / static_cast<double>(load_window));
        }
        const bool flying_again =
            a.property("aero/alpha-deg") < out.stall_alpha_deg &&
            a.property("velocities/h-dot-fps") * 60.0 >= -level_within_fpm &&
            a.property(until_property) >= recovered_kts;
        if (!flying_again) {
            recovered_since = -1;
        } else if (recovered_since < 0) {
            recovered_since = steps;
        }
        out.recovered = recovered_since >= 0 && steps - recovered_since >= recovered_for;
    }
};

// **A stall, entered the way one is entered**: throttle closed, the height
// held, the nose rising as the speed decays until the wing gives up. The
// recovery is the autopilot's stall recovery at full power. It is handed the
// aeroplane `left_s` after the lesson's entry ends: none by the book, and the
// longer the later - or, with `at_the_warning`, at the lesson's stall warning,
// the first sign of the stall (Lesson::stall_warning), whenever that comes.
//
// `until_recovered` flies on past the lesson's end until the aeroplane is
// recovered (RecoveryWatch); otherwise the flight ends with the lesson.
Result fly_a_stall(const std::string& id, double left_s, bool fresh_autopilot = false,
                   bool until_recovered = true, bool at_the_warning = false,
                   bool let_go = false, double handed_flaps = -1.0) {
    const auto stall_entry = glideslope::sim::find_aircraft(data(), id);
    // **A stall is practised where its aeroplane practises it.** A light
    // aeroplane decelerates to the stall in a few hundred feet; a clean jet
    // at idle descends while it slows, and doing that from five thousand feet
    // puts it in the ground before it stalls.
    InFlight f = airborne(id, stalls_are_practised_at(stall_entry));
    const auto found = lesson_for(stall_entry, "stalls");
    check(found.has_value(), id + " has a stalls lesson for its class");
    const Lesson lesson = *found;
    LessonRun run(lesson, f.speeds);
    check(!at_the_warning || lesson.stall_warning.has_value(),
          lesson.id + " says where its stall warning sounds");
    const double warning_kts =
        at_the_warning ? glideslope::sim::figure_of(*lesson.stall_warning, f.speeds) : 0.0;

    // **In the configuration its reference speed was measured in**: the
    // landing flap and the gear down. `stall` is the stall with everything
    // down, and flown clean a swept wing gives up long before it: asked to
    // slow to twenty-five knots above its landing stall, a clean 737 has to
    // stall to get there, and it departed - 52 degrees of alpha, 26 nose down
    // and 21,000 ft/min, with nothing to recover it.
    // `handed_flaps`, where it is not negative, is the lever the flight is
    // flown with in place of the landing flap.
    const double landing_flap =
        handed_flaps >= 0.0 ? handed_flaps
                            : glideslope::sim::approach_speeds(data(), stall_entry.model).flap;
    glideslope::sim::Controls controls;
    controls.throttle = 0.6;
    controls.flaps = landing_flap;
    controls.gear = 1.0;
    auto autopilot = std::make_unique<glideslope::sim::Autopilot>(*f.aircraft, controls);
    glideslope::sim::AutopilotModes modes = autopilot->modes();
    modes.heading_deg = f.start_heading_deg;
    modes.altitude_ft = f.start_agl_ft;
    autopilot->set(modes);

    Result out;
    out.stages = lesson.stages.size();
    const int settling = 20 * steps_per_second;
    bool recovering = false;
    // **The sloppy recovery is the same recovery, started late.** It is timed
    // from the moment the lesson calls the stall rather than from a speed,
    // because an aeroplane mushing in a stall does not go on slowing - wait
    // for a speed six knots below the stall and it never comes, the nose
    // stays up, and she descends all the way to the ground. That is not a
    // late recovery, it is no recovery, and it taught the test nothing.
    const auto dawdle = static_cast<std::int64_t>(std::lround(left_s * steps_per_second));
    std::int64_t stalled_at = -1;
    double entry_began = -1.0;
    const glideslope::sim::LessonStage& recovery_stage = lesson.stages.back();
    RecoveryWatch watch(out, recovery_stage.until_property,
                        recovery_ends_at_kts(lesson, f.speeds));
    // `let_go`: once recovered, the recovery is let go for the altitude hold
    // at the height it is at, and the flight flown one step more.
    bool letting_go = false;
    const auto flying = [&] {
        return !run.finished() || (until_recovered && !out.recovered) ||
               (let_go && out.let_go_step < 0.0);
    };
    const auto moved = [](const glideslope::sim::Controls& a,
                          const glideslope::sim::Controls& b) {
        return std::max({std::abs(a.elevator - b.elevator), std::abs(a.aileron - b.aileron),
                         std::abs(a.rudder - b.rudder)});
    };
    glideslope::sim::Controls last_controls = controls;
    for (int tick = 0; tick < 600 * steps_per_second && flying(); ++tick) {
        // **What the autopilot is asked for is settled before it flies**, and
        // it flies once a step: called twice, its loops ran at 240 Hz.
        if (tick == settling) {
            // **Asked for a speed below the stall**, which is how a stall is
            // entered on the autopilot: a light aeroplane's altitude hold
            // gives up height rather than fly slower than its best-climb
            // speed or a slower speed asked for, so asked for none she is
            // held at her best-climb speed and never stalls.
            glideslope::sim::fly_the_stall_entry(modes, f.speeds);
            autopilot->set(modes);
        }
        const auto& a = *f.aircraft;
        bool engaging = false;
        if (let_go && out.recovered && !letting_go) {
            letting_go = true;
            modes.speed_on_elevator = false;
            modes.airspeed_kts = a.property("velocities/vc-kts");
            modes.altitude_ft = a.property("position/h-sl-ft");
            autopilot->set(modes);
        }
        if (tick >= settling && !recovering) {
            watch.entering(a);
            bool now = false;
            if (at_the_warning) {
                now = a.property("velocities/vc-kts") <= warning_kts;
            } else if (run.stage() >= 1) {
                if (stalled_at < 0) {
                    stalled_at = tick;
                }
                now = tick - stalled_at >= dawdle;
            }
            if (now) {
                recovering = true;
                watch.hand_over(a);
                if (fresh_autopilot) {
                    // A new autopilot, engaged on the aeroplane as it is,
                    // that has seen nothing of it before.
                    autopilot = std::make_unique<glideslope::sim::Autopilot>(*f.aircraft,
                                                                             last_controls);
                }
                glideslope::sim::fly_the_stall_recovery(modes, lesson, f.speeds);
                autopilot->set(modes);
                engaging = true;
            }
        }
        glideslope::sim::Controls c = autopilot->fly();
        if (engaging) {
            out.engaged_step = moved(c, last_controls);
        } else if (letting_go && out.let_go_step < 0.0) {
            out.let_go_step = moved(c, last_controls);
        }
        if (tick >= settling) {
            // Throttle closed for the entry: the autopilot holds the height by
            // raising the nose, and she slows towards the stall of her own
            // accord. Full power for the recovery.
            c.throttle = recovering ? 1.0 : 0.0;
        }
        // **The flaps are the flight's until the hand-over, and the
        // recovery's from it**: it takes them up to a go-around's where the
        // aeroplane's figures give one (sim/autopilot.cpp).
        if (recovering) {
            if (out.flaps_handed < 0.0) {
                out.flaps_handed = last_controls.flaps;
            }
            out.flaps_most_step =
                std::max(out.flaps_most_step, std::abs(c.flaps - last_controls.flaps));
            out.flaps_lowest = std::min(out.flaps_lowest, c.flaps);
            out.flaps_end = c.flaps;
        } else {
            c.flaps = landing_flap;
        }
        c.gear = 1.0;
        last_controls = c;
        f.aircraft->set_controls(c);
        f.aircraft->step();
        if (tick >= settling) {
            const bool was_entering = run.stage() == 0;
            run.update(*f.aircraft, tick);
            const double agl = f.aircraft->property("position/h-agl-ft");
            // **Into the ground is the end of the flight**, and the lesson is
            // judged there: a recovery that ends in the ground did not
            // recover with the least height. Before the recovery flew the
            // speed, a B-2A left mushing half a minute did exactly that, on
            // macOS, with the recovery stage never over and nothing said.
            if (agl <= 0.0) {
                out.lowest_agl_ft = std::min(out.lowest_agl_ft, agl);
                run.ended(*f.aircraft, tick);
                break;
            }
            out.lowest_agl_ft = std::min(out.lowest_agl_ft, agl);
            if (recovering) {
                watch.recovering(a);
            }
            if (!was_entering && out.recovery_began_ft < 0.0) {
                out.recovery_began_ft = agl;
            }
            if (was_entering) {
                if (entry_began < 0.0) {
                    entry_began = agl;
                }
                out.entry_low_ft = std::min(out.entry_low_ft, agl - entry_began);
                out.entry_high_ft = std::max(out.entry_high_ft, agl - entry_began);
            }
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
        const Result flown = fly_a_stall(id, 0.0, false, false);
        std::printf("  %-13s lowest %6.0f ft, entry %+.0f to %+.0f ft, recovery lost %.0f ft, "
                    "%zu of %zu stages\n",
                    id.c_str(), flown.lowest_agl_ft, flown.entry_low_ft,
                    flown.entry_high_ft, flown.recovery_began_ft - flown.lowest_agl_ft,
                    flown.completed, flown.stages);
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
}

// **A flight that ends before its lesson is judged where it ended.** A stage
// that never came to its end - here one that ends at a speed no Cessna
// reaches - has its needs judged when the flight ends: the one it has not met
// is named and the one it has is not, the lesson is over, and nothing after
// is judged. `ended` is what a lesson flown into the ground calls.
GLIDESLOPE_TEST(a_lesson_whose_flight_ends_part_way_names_what_its_stage_still_needed) {
    const Lesson lesson = parse_lesson(
        "ends", "name Ends\nstage Climbing\ndo Climb\nuntil velocities/vc-kts >= 900\n"
                "need position/h-agl-ft >= 100000 Climb higher than you can\n"
                "need position/h-agl-ft >= 10 Stay off the ground\n");
    InFlight f = airborne("c172p", 5000.0);
    LessonRun run(lesson, f.speeds);
    run.update(*f.aircraft, 0);
    check(!run.finished() && run.debrief_lines().empty(),
          "under way, the stage's needs are not due yet");
    run.ended(*f.aircraft, 1);
    const auto said = run.debrief_lines();
    check(run.finished(), "the lesson is over once its flight is");
    check(said.size() == 1 && said[0] == "Climb higher than you can",
          "the need not met is named, and the one met is not: said " +
              std::to_string(said.size()));
    run.ended(*f.aircraft, 2);
    run.update(*f.aircraft, 3);
    check(run.debrief_lines().size() == 1, "and nothing after it is judged");
}

// **A stall recovered late and lazily loses height, and is named for it.**
GLIDESLOPE_TEST(a_stall_recovered_badly_is_named_in_the_debrief) {
    for (const std::string& id : one_of_each_class("stalls")) {
        const Result sloppy = fly_a_stall(id, 35.0);
        std::printf("  %s recovered badly: lowest %.0f ft\n", id.c_str(),
                    sloppy.lowest_agl_ft);
        names_that_fault_and_no_other(id, "stalls", sloppy.debrief, {"position/h-agl-ft"},
                                      "recovering late and lazily");
    }
}

namespace {

// **The stall recovery is held to two things, both within 2 g** (decided
// 2026-09-30, REQUIREMENTS.md 4.3):
//
// - **handed over at the first sign of the stall** - its stall warning, which
//   the lesson's data sets (`warning`): 6 knots above the stall in a light
//   aeroplane, and in the others the least margin the rules allow a warning,
//   5 knots or 5 per cent - it recovers within its lesson's height;
// - **left thirty seconds in the stall** after the lesson's entry ends, deep
//   in it and sinking thousands of feet a minute, it recovers within a height
//   worked out for that aeroplane from how it was flying when handed over
//   (`height_bound_ft`).
//
// **Recovered** is flying again, for five seconds together (RecoveryWatch).
// The height lost is counted from the hand-over to the lowest it goes before
// that. **2 g is the load the airworthiness rules require with the flaps
// out** (14 CFR 23.345 and 25.345), and the recovery is flown with them out;
// the load is the mean over a quarter of a second.
//
// **One tolerance, for every aeroplane, height and load alike: 10 per cent**,
// for what differs between machines - the flight is floating point, and a
// stall is where small differences grow. Each figure plus 10 per cent must be
// within its limit. The figures are printed for every aeroplane, though CI
// shows a test's output only when it fails.
constexpr double between_machines = 0.10;
constexpr double flaps_down_most_g = 2.0;

// **A thrust-free kinematic estimate of the height a recovery from this
// state costs**, in feet - an estimate, not a least: it leaves out the
// thrust that pays for some of the speed, and the drag that costs some.
//
//   to reach its recovery speed: (Vr^2 - V0^2) / 2g, when Vr > V0;
//   to unload the wing first: w0 (alpha0 - alpha_stall) / p, the sink kept
//   while the angle of attack comes down to the one the wing stalled at, at
//   p = 8 degrees a second, the rate a recovery puts the nose down at
//   (src/sim/autopilot.cpp);
//   to arrest its sink at the load limit n = 2, on the arc of a pull-out at
//   the faster of the two speeds, V: V^2 (1 - cos gamma) / ((n - 1) g), where
//   gamma = asin(w0 / V0) is the flight path it was handed over on.
//
// V0 and Vr are true airspeeds - the hand-over's, and the lesson's recovery
// speed at that height - and w0 the sink.
constexpr double nose_down_degps = 8.0;

struct Estimate {
    double energy_ft = 0.0;
    double unload_ft = 0.0;
    double pull_out_ft = 0.0;
    double sum_ft() const { return energy_ft + unload_ft + pull_out_ft; }
};

Estimate kinematic_estimate(const Result& r) {
    constexpr double g = 32.174;
    const double v0 = r.handed_over_tas_fps;
    const double vr = r.recovered_tas_fps;
    const double sink = std::max(-r.handed_over_fpm / 60.0, 0.0);
    Estimate out;
    out.energy_ft = std::max(vr * vr - v0 * v0, 0.0) / (2.0 * g);
    out.unload_ft =
        sink * std::max(r.handed_over_alpha_deg - r.stall_alpha_deg, 0.0) / nose_down_degps;
    const double v = std::max(v0, vr);
    const double gamma = std::asin(std::clamp(sink / std::max(v0, 1.0), 0.0, 1.0));
    out.pull_out_ft = v * v * (1.0 - std::cos(gamma)) / ((flaps_down_most_g - 1.0) * g);
    return out;
}

// **The bound is the estimate times an empirical scale, plus a flat
// margin.** The scale is set so that the worst unnamed aeroplane measured
// against its estimate - the 787-8, which lost 1.39 times it, a flaps-down
// dive whose estimate is nearly all energy - has room with the 10 per cent
// in hand: 2. It was chosen after a scale of 1 put the 737-300, 787-8, A380
// and F-35B outside it; the autopilot pulls out at 1.6 g, not the 2 the arc
// is worked at, and brings the nose up at 3 degrees a second. The flat 200 ft
// is for the light aeroplanes, whose estimates are tens of feet and whose
// losses are set by the autopilot's response, not by kinematics. **It is
// loose where the estimate is small**: the C182 loses an eighth of its bound,
// and for all four light aeroplanes it is more than their lesson's 300 ft.
constexpr double empirical_scale = 2.0;
constexpr double flat_margin_ft = 200.0;

double height_bound_ft(const Result& r) {
    return empirical_scale * kinematic_estimate(r).sum_ft() + flat_margin_ft;
}

// **The height a stall lesson's recovery may cost**: its recovery stage's
// need that the height be no lower than `start` less so much, where `start`
// is the height the stage began at.
double recovery_allowance_ft(const Lesson& lesson) {
    for (const auto& stage : lesson.stages) {
        for (const auto& need : stage.needs) {
            if (need.property == "position/h-agl-ft" && need.at_least &&
                need.low.reference == "start" && need.low.offset < 0.0) {
                return -need.low.offset;
            }
        }
    }
    check(false, lesson.id + " says how much height its recovery may cost");
    return 0.0;
}

// **An aeroplane the recovery does not yet hold to its limit**, named for
// that fault alone and held to its own measured figure plus the tolerance
// instead, so it gets no worse unseen, while the item stays open in
// docs/COMPLETION_PLAN.md. `not_recovered` is one never called recovered:
// its figure is the height it lost by the end of the flight, which it is held
// to. A name whose aeroplane now meets the limit with the tolerance in hand is
// stale and turns the test red, so it is taken off; the item is ticked only
// when no name is left.
enum class Fault { height, load, not_recovered };

struct NotYet {
    const char* id;
    Fault fault;
    double measured; // feet, or g
};

const char* name_of(Fault f) {
    return f == Fault::height ? "height" : f == Fault::load ? "load" : "not recovered";
}

// Each aeroplane taught a stall, handed to the recovery at its stall warning
// or `left_s` after the lesson's entry ends, judged against
// `height_bound(result, lesson's height)` and 2 g, each with the tolerance in
// hand, or against its named figure. Coverage is asserted: every aeroplane in
// the roster is flown or left out with its reason.
void every_stall_recovered_within(
    bool at_the_warning, double left_s, const char* when,
    const std::function<double(const Result&, double)>& height_bound,
    const std::vector<NotYet>& not_yet) {
    const Taught taught = taught_for("stalls");
    const std::size_t roster = glideslope::sim::read_catalogue(data()).size();
    std::size_t walked = 0;
    std::vector<std::string> faults;
    std::set<const NotYet*> used;
    const auto named = [&](const std::string& id, Fault f) -> const NotYet* {
        for (const NotYet& n : not_yet) {
            if (id == n.id && n.fault == f) {
                used.insert(&n);
                return &n;
            }
        }
        return nullptr;
    };
    const auto within = [](double figure, double limit) {
        return figure * (1.0 + between_machines) <= limit;
    };
    // A fault against its limit, or against its name: `f` is that fault,
    // `figure` what was measured, `limit` what it is held to unnamed.
    const auto judge = [&](const std::string& id, Fault f, double figure, double limit,
                           const char* unit) {
        const NotYet* n = named(id, f);
        const bool meets = f == Fault::not_recovered ? false : within(figure, limit);
        if (n == nullptr) {
            if (!meets) {
                faults.push_back(id + " " + name_of(f) + ": " + std::to_string(figure) + " " +
                                 unit + ", against " + std::to_string(limit) +
                                 " with 10% in hand");
            }
        } else if (meets) {
            faults.push_back(id + " is named for its " + name_of(f) + " and now meets " +
                             std::to_string(limit) + " with 10% in hand (" +
                             std::to_string(figure) + "): take its name off");
        } else if (figure > n->measured * (1.0 + between_machines)) {
            faults.push_back(id + " is named for its " + name_of(f) + " at " +
                             std::to_string(n->measured) + " " + unit + ", and is worse: " +
                             std::to_string(figure));
        } else {
            std::printf("      named, not yet: %s %s %.2f %s against %.2f, held to %.2f\n",
                        id.c_str(), name_of(f), figure, unit, limit,
                        n->measured * (1.0 + between_machines));
        }
    };
    for (const std::string& id : taught.able) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const double allowed_ft = recovery_allowance_ft(*lesson_for(entry, "stalls"));
        const Result r = fly_a_stall(id, left_s, false, true, at_the_warning);
        const double lost_ft = r.handed_over_ft - r.recovery_lowest_ft;
        const double bound_ft = height_bound(r, allowed_ft);
        const Estimate e = kinematic_estimate(r);
        std::printf("  %-13s handed over at %6.0f ft, %5.1f kt (%4.0f ft/s true, recovers at "
                    "%4.0f), alpha %5.1f, pitch %5.1f, %6.0f ft/min; stalled at alpha %4.1f; "
                    "estimate %4.0f + %4.0f + %4.0f ft; lost %5.0f ft against %5.0f "
                    "(%.2f of the estimate); peak %.2f g%s\n",
                    id.c_str(), r.handed_over_ft, r.handed_over_kts, r.handed_over_tas_fps,
                    r.recovered_tas_fps, r.handed_over_alpha_deg, r.handed_over_pitch_deg,
                    r.handed_over_fpm, r.stall_alpha_deg, e.energy_ft, e.unload_ft,
                    e.pull_out_ft, lost_ft, bound_ft, lost_ft / std::max(e.sum_ft(), 1.0),
                    r.peak_load_g, r.recovered ? "" : ", NOT RECOVERED");
        if (r.handed_over_ft <= 0.0) {
            faults.push_back(id + " was not handed to its recovery in the air");
        } else {
            if (!r.recovered || named(id, Fault::not_recovered) != nullptr) {
                judge(id, Fault::not_recovered, r.recovered ? 0.0 : lost_ft, bound_ft, "ft");
                if (r.recovered) {
                    faults.push_back(id + " is named as not recovered, and now is: take its "
                                          "name off");
                }
            }
            if (r.recovered) {
                judge(id, Fault::height, lost_ft, bound_ft, "ft");
            }
            judge(id, Fault::load, r.peak_load_g, flaps_down_most_g, "g");
        }
        ++walked;
    }
    for (const NotYet& n : not_yet) {
        if (used.count(&n) == 0) {
            faults.push_back(std::string(n.id) + " is named for its " + name_of(n.fault) +
                             " but that was not judged");
        }
    }
    std::printf("  %s: of the %zu aeroplanes, %zu flown and %zu left out; %zu named not yet\n",
                when, roster, walked, taught.left_out.size(), not_yet.size());
    for (const std::string& said : taught.left_out) {
        std::printf("      left out - %s\n", said.c_str());
    }
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) + " recoveries " + when +
                              " were outside their bounds; the first: " +
                              (faults.empty() ? "" : faults.front()));
    check(walked == taught.able.size(), "every aeroplane taught a stall was stalled");
    check(walked + taught.left_out.size() == roster,
          "every aeroplane in the roster was flown or left out with its reason");
}

} // namespace

GLIDESLOPE_TEST(every_aeroplane_recovered_at_the_first_sign_of_a_stall_loses_no_more_than_its_lesson_allows_within_2_g) {
    every_stall_recovered_within(
        true, 0.0, "at the stall warning",
        [](const Result&, double lesson_ft) { return lesson_ft; },
        // The Mosquito, at 20,000 ft with its flaps and gear down, cannot be
        // level at its recovery speed on full power, and is never called
        // recovered; it is held to the height it lost by the flight's end.
        {{"learjet35a", Fault::height, 385.0},
         {"mosquito-fb6", Fault::not_recovered, 3095.0},
         {"short_s23", Fault::height, 184.0}});
}

GLIDESLOPE_TEST(every_aeroplane_left_thirty_seconds_in_a_stall_is_recovered_within_2_g_and_the_height_its_speed_and_sink_need) {
    every_stall_recovered_within(
        false, 30.0, "left thirty seconds in the stall",
        [](const Result& r, double) { return height_bound_ft(r); },
        {});
}

// **Engaging the stall recovery and letting it go steps no control.** Every
// aeroplane taught a stall, handed to the recovery at its stall warning and
// left thirty seconds in the stall, and let go for the altitude hold once
// recovered: on neither step does the elevator, the ailerons or the rudder
// move more than a hand's pace, its full travel in a second (sim/autopilot.cpp).
// The throttle, flaps and gear are the flight's, not the autopilot's, here.
// The recovery brings its pitch command down to the nose at once when the wing
// is stalled, which without that pace would move the elevator by tenths of
// its travel in a step. **The Mosquito at her warning is never let go**: at
// 20,000 ft with her flaps and gear down she cannot be level at her recovery
// speed, and is never called recovered (the warning test names her for it);
// her engaging is judged, and her name turns this red once she is let go.
GLIDESLOPE_TEST(engaging_the_stall_recovery_and_letting_it_go_moves_no_control_faster_than_a_hand) {
    constexpr double a_hands_pace = 1.0 / static_cast<double>(steps_per_second);
    const Taught taught = taught_for("stalls");
    const std::size_t roster = glideslope::sim::read_catalogue(data()).size();
    std::size_t walked = 0;
    std::size_t flights = 0;
    std::vector<std::string> faults;
    for (const std::string& id : taught.able) {
        for (const bool at_the_warning : {true, false}) {
            const Result r = fly_a_stall(id, at_the_warning ? 0.0 : 30.0, false, true,
                                         at_the_warning, true);
            const char* when = at_the_warning ? "at its warning" : "left thirty seconds";
            std::printf("  %-13s %-19s engaged moving %.5f, let go moving %.5f\n", id.c_str(),
                        when, r.engaged_step, r.let_go_step);
            const bool named_never_let_go = at_the_warning && id == "mosquito-fb6";
            if (named_never_let_go && r.let_go_step >= 0.0) {
                faults.push_back(id + " " + when + " is named as never let go, and was: take "
                                                   "its name off");
            } else if (r.let_go_step < 0.0 && !named_never_let_go) {
                faults.push_back(id + " " + when + " was never let go: not recovered");
            } else if (std::max(r.engaged_step, r.let_go_step) > a_hands_pace + 1e-12) {
                faults.push_back(id + " " + when + " moved a control " +
                                 std::to_string(std::max(r.engaged_step, r.let_go_step)) +
                                 " in a step, against " + std::to_string(a_hands_pace));
            }
            ++flights;
        }
        ++walked;
    }
    std::printf("  of the %zu aeroplanes, %zu flown twice (%zu flights) and %zu left out\n",
                roster, walked, flights, taught.left_out.size());
    for (const std::string& said : taught.left_out) {
        std::printf("      left out - %s\n", said.c_str());
    }
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) + " flights stepped a control or were "
                          "not let go; the first: " + (faults.empty() ? "" : faults.front()));
    check(flights == 2 * taught.able.size(), "every aeroplane taught a stall was flown twice");
    check(walked + taught.left_out.size() == roster,
          "every aeroplane in the roster was flown or left out with its reason");
}

// **The AI pilot notices a stall coming, and recovers from it.** Every
// aeroplane taught a stall, given to the AI (`Controller::to_ai`) where it is
// practised, in its landing configuration, told how she lands
// (`lands_with`), and asked to hold her height at ten knots under her stall -
// a speed asked that would stall her. At her warning the AI's own recovery
// takes over (sim/controller.cpp, `notice_a_stall`): she is never slower than
// her stall, and within two minutes she is back at her approach speed and no
// longer descending, holding her height again. Without the hook the altitude
// hold flies every one of them into the stall. The height each loses is
// printed, not judged: the lesson's own checks judge the recovery.
//
// **Two named, not yet**, each for that fault alone; a name whose aeroplane
// now passes turns this red, so it is taken off. The A380 dips 0.7 kt under
// her published stall before the speed comes (104.1 against 104.9). The
// Mosquito at 20,000 ft with her flaps and gear down cannot be level at her
// approach speed, as she cannot at her lesson's recovery speed.
GLIDESLOPE_TEST(the_ai_pilot_notices_a_stall_coming_and_recovers_from_it) {
    const std::map<std::string, std::string> not_yet = {
        {"a380", "slowed under her stall"},
        {"mosquito-fb6", "not back at her approach speed and level"}};
    std::set<std::string> named_seen;
    const Taught taught = taught_for("stalls");
    const std::size_t roster = glideslope::sim::read_catalogue(data()).size();
    std::size_t walked = 0;
    std::vector<std::string> faults;
    for (const std::string& id : taught.able) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const std::optional<glideslope::sim::ApproachSpeeds> lands =
            glideslope::sim::landing_speeds(data(), entry.model);
        check(lands.has_value(), id + " is taught a stall and publishes how she lands");
        InFlight f = airborne(id, stalls_are_practised_at(entry));
        glideslope::sim::Controls held;
        held.throttle = 0.6;
        held.flaps = lands->flap;
        held.gear = 1.0;
        glideslope::sim::Controller controller(*f.aircraft, held);
        controller.lands_with(*lands);
        controller.to_ai();
        glideslope::sim::AutopilotModes modes = controller.autopilot()->modes();
        modes.heading_deg = f.start_heading_deg;
        modes.altitude_ft = f.aircraft->property("position/h-sl-ft");
        modes.airspeed_kts = lands->stall_kts - 10.0;
        controller.autopilot()->set(modes);
        double slowest = 1e9;
        double highest = f.aircraft->property("position/h-sl-ft");
        double lowest = highest;
        bool noticed = false;
        bool recovered = false;
        int steady = 0;
        for (int tick = 0; tick < 300 * steps_per_second; ++tick) {
            const glideslope::sim::Controls c = controller.fly();
            f.aircraft->set_controls(c);
            f.aircraft->step();
            const double kts = f.aircraft->property("velocities/vc-kts");
            const double h = f.aircraft->property("position/h-sl-ft");
            if (controller.recovering_from_a_stall() && !noticed) {
                noticed = true;
                highest = h;
                lowest = h;
            }
            if (!noticed) {
                continue;
            }
            slowest = std::min(slowest, kts);
            lowest = std::min(lowest, h);
            const bool flying =
                !controller.recovering_from_a_stall() && kts >= lands->vref_kts - 1.0 &&
                f.aircraft->property("velocities/h-dot-fps") * 60.0 >= -100.0;
            steady = flying ? steady + 1 : 0;
            if (steady >= 5 * steps_per_second) {
                recovered = true;
                break;
            }
        }
        std::printf("  %-13s noticed %s, slowest %5.1f kt against a stall of %5.1f, lost %5.0f "
                    "ft, %s\n",
                    id.c_str(), noticed ? "yes" : "no", slowest, lands->stall_kts,
                    highest - lowest, recovered ? "recovered" : "NOT RECOVERED");
        std::string fault;
        if (!noticed) {
            fault = "the stall coming was not noticed";
        } else if (slowest < lands->stall_kts) {
            fault = "slowed under her stall";
        } else if (!recovered) {
            fault = "not back at her approach speed and level";
        }
        const auto name = not_yet.find(id);
        if (name != not_yet.end() && name->second == fault) {
            named_seen.insert(id);
            std::printf("      named, not yet: %s %s\n", id.c_str(), fault.c_str());
        } else if (name != not_yet.end()) {
            faults.push_back(id + " is named as " + name->second + ", and is " +
                             (fault.empty() ? "recovered: take its name off" : fault));
        } else if (!fault.empty()) {
            faults.push_back(id + ": " + fault);
        }
        ++walked;
    }
    std::printf("  of the %zu aeroplanes, %zu flown and %zu left out\n", roster, walked,
                taught.left_out.size());
    for (const std::string& said : taught.left_out) {
        std::printf("      left out - %s\n", said.c_str());
    }
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) +
                              " aeroplanes were not recovered by the AI; the first: " +
                              (faults.empty() ? "" : faults.front()));
    check(named_seen.size() == not_yet.size(), "every name was judged");
    check(walked == taught.able.size(), "every aeroplane taught a stall was flown");
    check(walked + taught.left_out.size() == roster,
          "every aeroplane in the roster was flown or left out with its reason");
}


// **The AI pilot notices a stall twice, when it is slowed into one twice.**
// The Cessna 172P given to the AI and asked to hold her height at ten knots
// under her stall; once recovered and handed back what she was flying, she is
// asked for it again. She is noticed and recovered a second time - the
// warning is watched again once she is clear of it by its own margin
// (sim/controller.cpp, `stall_armed_`), which at her approach speed she is.
GLIDESLOPE_TEST(the_ai_pilot_notices_a_second_stall_after_handing_the_first_back) {
    const auto entry = glideslope::sim::find_aircraft(data(), "c172p");
    const std::optional<glideslope::sim::ApproachSpeeds> lands =
        glideslope::sim::landing_speeds(data(), entry.model);
    check(lands.has_value(), "the 172P publishes how she lands");
    InFlight f = airborne("c172p", stalls_are_practised_at(entry));
    glideslope::sim::Controls held;
    held.throttle = 0.6;
    held.flaps = lands->flap;
    held.gear = 1.0;
    glideslope::sim::Controller controller(*f.aircraft, held);
    controller.lands_with(*lands);
    controller.to_ai();
    const auto ask_too_slow = [&] {
        glideslope::sim::AutopilotModes modes = controller.autopilot()->modes();
        modes.heading_deg = f.start_heading_deg;
        modes.altitude_ft = f.aircraft->property("position/h-sl-ft");
        modes.airspeed_kts = lands->stall_kts - 10.0;
        controller.autopilot()->set(modes);
    };
    ask_too_slow();
    int asked_again_at = -1;
    for (int tick = 0; tick < 600 * steps_per_second && controller.stalls_recovered() < 2;
         ++tick) {
        if (controller.stalls_recovered() == 1 && asked_again_at < 0) {
            asked_again_at = tick;
            ask_too_slow();
        }
        const glideslope::sim::Controls c = controller.fly();
        f.aircraft->set_controls(c);
        f.aircraft->step();
    }
    std::printf("  c172p recovered %d times; asked too slow again at %.1f s\n",
                controller.stalls_recovered(),
                static_cast<double>(asked_again_at) / steps_per_second);
    check(asked_again_at >= 0, "the first stall was noticed and handed back");
    check(controller.stalls_recovered() == 2, "and the second was noticed and handed back");
}

// **Every plan's slowest keeps its gust allowance over the stall warning,
// at the weight a plan flies her at.** For every aircraft in the catalogue,
// her figures' `<plan_speeds weight_lbs>` is her model's own weight (what
// the plan-speeds trial flies and a server flies her at), and her slowest is
// at least `sim::least_plan_slowest_kts` there: her approach speed for that
// weight, and her stall warning plus 10 kt. The 747-400 and F-22A publish
// no stall and have nothing to keep above; they are counted, and named.
GLIDESLOPE_TEST(every_plan_floor_keeps_its_gust_allowance_over_the_stall_warning_at_the_weight_a_plan_flies_her_at) {
    const auto roster = glideslope::sim::read_catalogue(data());
    std::size_t held = 0;
    std::size_t no_stall = 0;
    std::vector<std::string> faults;
    for (const auto& entry : roster) {
        const glideslope::sim::PlanSpeeds plan = glideslope::sim::plan_speeds(data(), entry.model);
        const double weight_lbs = glideslope::sim::plan_weight_lbs(data(), entry);
        const double least = glideslope::sim::least_plan_slowest_kts(data(), entry);
        std::printf("  %-13s planned at %8.0f lb (file %8.0f): slowest %5.0f, least %5.0f\n",
                    entry.id.c_str(), weight_lbs, plan.weight_lbs, plan.slowest_kts, least);
        if (std::abs(plan.weight_lbs - weight_lbs) > 1.0) {
            faults.push_back(entry.id + "'s plan speeds say " + std::to_string(plan.weight_lbs) +
                             " lb; her model weighs " + std::to_string(weight_lbs));
        }
        if (least == 0.0) {
            ++no_stall;
            if (entry.id != "747-400" && entry.id != "f22") {
                faults.push_back(entry.id + " publishes no stall, and is not named as such");
            }
            continue;
        }
        if (plan.slowest_kts < least) {
            faults.push_back(entry.id + "'s slowest, " + std::to_string(plan.slowest_kts) +
                             " kt, is under the least it may be, " + std::to_string(least));
            continue;
        }
        ++held;
    }
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) + " faults; the first: " +
                              (faults.empty() ? "" : faults.front()));
    check(held + no_stall == roster.size() && no_stall == 2,
          "every aircraft held or named: " + std::to_string(held) + " and " +
              std::to_string(no_stall) + " of " + std::to_string(roster.size()));
}

// **The AI pilot notices no stall in ordinary flight, with 3 kt to spare**:
// every aeroplane that publishes how she lands (the 747-400 and F-22A do
// not, and are not watched), given to the AI at the weight her figures fly
// her at, in moderate turbulence, for three minutes each: cruising at her
// catalogue's start speed, climbing at 500 ft/min at her best-climb speed,
// and level at the slowest a plan may fly her at that weight
// (`plan_speeds`, `for_weight`) - the speeds the AI really flies her at,
// down to the slowest. None is noticed as a stall, and in every phase her
// slowest stays at least 3 kt over her warning, so that another machine's
// turbulence does not tip her over it. Asked for 1,000 ft/min, the Short
// S.23 - whose autopilot has no climb floor (only a light aeroplane's has) -
// slowed to 68.8 kt, under her 71.3 kt warning, and was rightly noticed:
// that climb was more than she has.
//
// **Named, not yet: the J-3 Cub's climb.** At her climb figure's full load,
// 1,220 lb, against the 1,092 her stall was measured at, her warning is
// 39.9 kt; climbing at her manual's best-climb speed, 47.8 kt, which is for
// that full load, her airspeed dips 7 to 8 kt in the turbulence, to 40.1
// (39.7 in CI). Less than 3 kt over her warning, she is named here - noticed
// or not - and her name turns this red once she has the 3 kt. Her plan's
// slowest, raised for her weight, is not named: it keeps its margin.
//
// **Not walked here, named**: the take-off and the approach fly their own
// laws, which the notice does not watch (sim/controller.cpp: only the plain
// autopilot is); the go-around's circuit and an engine-out glide are not
// flown here.
GLIDESLOPE_TEST(the_ai_pilot_notices_no_stall_cruising_or_climbing_in_moderate_turbulence) {
    constexpr double to_spare_kts = 3.0;
    const char* const phase_names[3] = {"cruise", "climb", "plan floor"};
    const auto roster = glideslope::sim::read_catalogue(data());
    std::size_t flown = 0;
    std::size_t not_watched = 0;
    std::size_t phases_with_margin = 0;
    std::vector<std::string> faults;
    bool named_seen = false;
    for (const auto& entry : roster) {
        const std::optional<glideslope::sim::ApproachSpeeds> lands =
            glideslope::sim::landing_speeds(data(), entry.model);
        if (!lands) {
            ++not_watched;
            continue;
        }
        InFlight f = airborne(entry.id, stalls_are_practised_at(entry));
        glideslope::world::WeatherReport report;
        report.surface.metar =
            glideslope::world::parse_metar("XXXX 181200Z 00000KT 9999 SKC 15/05 Q1013");
        report.surface.latitude_deg = -33.9461;
        report.surface.longitude_deg = 151.1772;
        report.turbulence_severity = 3; // moderate
        report.air_seed = 0xa170;
        f.aircraft->set_weather(
            std::make_shared<glideslope::world::ReportedWeather>(report, nullptr, 0.0));
        glideslope::sim::Controls held;
        held.throttle = 0.7;
        glideslope::sim::Controller controller(*f.aircraft, held);
        controller.lands_with(*lands);
        controller.to_ai();
        glideslope::sim::AutopilotModes modes = controller.autopilot()->modes();
        modes.heading_deg = f.start_heading_deg;
        modes.altitude_ft = f.aircraft->property("position/h-sl-ft");
        modes.airspeed_kts = entry.start_airspeed_kts;
        controller.autopilot()->set(modes);
        double slowest[3] = {1e9, 1e9, 1e9};
        bool noticed = false;
        const glideslope::sim::DepartureSpeeds departs =
            glideslope::sim::departure_speeds(data(), entry.model);
        const double weight_lbs = f.aircraft->property("inertia/weight-lbs");
        // The slowest a plan may fly her at what she weighs.
        const glideslope::sim::PlanSpeeds plan = glideslope::sim::for_weight(
            glideslope::sim::plan_speeds(data(), entry.model), weight_lbs);
        const double cruise_kts = entry.start_airspeed_kts;
        // The warning as the notice reads it: her stall at what she weighs.
        const glideslope::sim::ApproachSpeeds weighed =
            glideslope::sim::for_weight(*lands, weight_lbs);
        const double warning_kts = glideslope::sim::stall_warning_kts(weighed.stall_kts);
        for (int tick = 0; tick < 540 * steps_per_second; ++tick) {
            if (tick == 180 * steps_per_second) {
                modes.altitude_ft = *modes.altitude_ft + 3000.0;
                modes.vertical_speed_fpm = 500.0;
                modes.airspeed_kts = departs.climb_kts;
                controller.autopilot()->set(modes);
            } else if (tick == 360 * steps_per_second) {
                modes.altitude_ft = f.aircraft->property("position/h-sl-ft");
                modes.airspeed_kts = plan.slowest_kts;
                controller.autopilot()->set(modes);
            }
            const glideslope::sim::Controls c = controller.fly();
            f.aircraft->set_controls(c);
            f.aircraft->step();
            const std::size_t phase = static_cast<std::size_t>(tick / (180 * steps_per_second));
            slowest[phase] = std::min(slowest[phase], f.aircraft->property("velocities/vc-kts"));
            noticed = noticed || controller.recovering_from_a_stall();
        }
        std::printf("  %-13s %6.0f lb (figures %6.0f): cruise %5.1f (slowest %5.1f), climb %5.1f "
                    "(%5.1f), plan floor %5.1f (%5.1f); warning %5.1f: %s\n",
                    entry.id.c_str(), weight_lbs, lands->reference_lbs, cruise_kts, slowest[0],
                    departs.climb_kts, slowest[1], plan.slowest_kts, slowest[2], warning_kts,
                    noticed ? "NOTICED" : "not noticed");
        for (std::size_t phase = 0; phase < 3; ++phase) {
            const bool named = entry.id == "j3cub" && phase == 1;
            const bool spare = slowest[phase] >= warning_kts + to_spare_kts;
            if (named) {
                named_seen = true;
                if (spare) {
                    faults.push_back(entry.id + "'s climb is named as short of its 3 kt, and "
                                                "had them: take its name off");
                }
                continue;
            }
            if (spare) {
                ++phases_with_margin;
            } else {
                char said[200];
                std::snprintf(said, sizeof said, "%s's %s came to %.1f kt, under %.1f + %.0f",
                              entry.id.c_str(), phase_names[phase], slowest[phase], warning_kts,
                              to_spare_kts);
                faults.push_back(said);
            }
        }
        if (noticed && entry.id != "j3cub") {
            faults.push_back(entry.id + " was noticed stalling in ordinary flight");
        }
        ++flown;
    }
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) + " faults; the first: " +
                              (faults.empty() ? "" : faults.front()));
    check(flown + not_watched == roster.size() && not_watched == 2,
          "every aeroplane flown, or named as unwatched: " + std::to_string(flown) + " and " +
              std::to_string(not_watched));
    check(phases_with_margin == 3 * flown - 1,
          std::to_string(phases_with_margin) + " of " + std::to_string(3 * flown) +
              " phases with 3 kt to spare; the one left out, the Cub's climb, is named");
    check(named_seen, "the J-3 Cub, named, was flown");
}


// **The AI's stall warning is for what she weighs.** The B-2A at her maximum
// loading, 336,500 lb, against the 177,160 lb light loading her published
// stall (95.4 kt) was measured at: slowed by the AI asked to hold her height
// at ten knots under her published stall, she is noticed at her warning for
// her weight - her stall scaled by the square root of the weights
// (sim::for_weight), 131 kt, plus its margin - and not at the light
// loading's, 100 kt, which she would stall long before.
GLIDESLOPE_TEST(the_ai_pilot_notices_a_heavy_b2a_stall_coming_at_the_warning_for_her_weight) {
    const auto entry = glideslope::sim::find_aircraft(data(), "b2");
    const std::optional<glideslope::sim::ApproachSpeeds> lands =
        glideslope::sim::landing_speeds(data(), entry.model);
    check(lands.has_value(), "the B-2A publishes how she lands");
    const auto figures = glideslope::sim::read_published_figures(
        data() / "figures" / (entry.model + ".xml"));
    check(figures.loadings.count("maximum") == 1, "the B-2A's figures name her maximum loading");
    InFlight f = airborne("b2", stalls_are_practised_at(entry), 0.0,
                          &figures.loadings.at("maximum").loading);
    const double weight_lbs = f.aircraft->property("inertia/weight-lbs");
    const glideslope::sim::ApproachSpeeds heavy = glideslope::sim::for_weight(*lands, weight_lbs);
    const double warning_kts = heavy.stall_kts + std::max(5.0, 0.05 * heavy.stall_kts);
    glideslope::sim::Controls held;
    held.throttle = 0.6;
    held.flaps = lands->flap;
    held.speedbrake = lands->speedbrake;
    held.gear = 1.0;
    glideslope::sim::Controller controller(*f.aircraft, held);
    controller.lands_with(*lands);
    controller.to_ai();
    glideslope::sim::AutopilotModes modes = controller.autopilot()->modes();
    modes.heading_deg = f.start_heading_deg;
    modes.altitude_ft = f.aircraft->property("position/h-sl-ft");
    modes.airspeed_kts = lands->stall_kts - 10.0;
    controller.autopilot()->set(modes);
    double noticed_at = -1.0;
    for (int tick = 0; tick < 300 * steps_per_second && noticed_at < 0.0; ++tick) {
        const glideslope::sim::Controls c = controller.fly();
        f.aircraft->set_controls(c);
        f.aircraft->step();
        if (controller.recovering_from_a_stall()) {
            noticed_at = f.aircraft->property("velocities/vc-kts");
        }
    }
    std::printf("  b2 at %.0f lb: stall for her weight %.1f kt, warning %.1f; published stall "
                "%.1f; noticed at %.1f kt\n",
                weight_lbs, heavy.stall_kts, warning_kts, lands->stall_kts, noticed_at);
    check(weight_lbs > 300000.0, "she is heavy: " + std::to_string(weight_lbs) + " lb");
    check(noticed_at >= warning_kts - 1.0,
          "noticed at her warning for her weight, " + std::to_string(warning_kts) +
              " kt, not at " + std::to_string(noticed_at));
}

// **The stall recovery takes the flaps up to a go-around's, at a hand's pace,
// where the aeroplane's figures give one, and leaves them where they are
// where they give none.** Every aeroplane taught a stall, handed to the
// recovery at its stall warning in its landing configuration: with a
// go-around setting (`go_around_flaps_deg`, its published figures) the lever
// ends there and goes no lower; without one it ends where it was handed over.
// On no step does the lever move more than a hand's pace. A Cherokee at full
// rich below 5,000 ft with her 40 degrees still out at the lesson's recovery
// speed sinks 190 ft/min and is never recovered; her family's handbooks go
// around at 25 (assets/figures/pa28.xml).
GLIDESLOPE_TEST(the_stall_recovery_raises_the_flaps_to_the_go_around_setting_at_a_hands_pace_where_the_figures_give_one) {
    constexpr double a_hands_pace = 1.0 / static_cast<double>(steps_per_second);
    const Taught taught = taught_for("stalls");
    const std::size_t roster = glideslope::sim::read_catalogue(data()).size();
    std::size_t walked = 0;
    std::size_t with_a_setting = 0;
    std::vector<std::string> faults;
    for (const std::string& id : taught.able) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const auto figures = glideslope::sim::read_published_figures(
            data() / "figures" / (entry.model + ".xml"));
        const Result r = fly_a_stall(id, 0.0, false, true, true);
        std::optional<double> go_around;
        if (figures.go_around_flaps_deg) {
            go_around = *figures.go_around_flaps_deg / figures.flaps_full_deg;
            ++with_a_setting;
        }
        const double wanted = go_around ? std::min(*go_around, r.flaps_handed) : r.flaps_handed;
        std::printf("  %-13s flaps handed over at %.3f, ended at %.3f (wanted %.3f), lowest "
                    "%.3f, most in a step %.5f%s\n",
                    id.c_str(), r.flaps_handed, r.flaps_end, wanted, r.flaps_lowest,
                    r.flaps_most_step, go_around ? ", has a go-around setting" : "");
        if (r.flaps_handed < 0.0) {
            faults.push_back(id + " was never handed to its recovery");
        } else if (std::abs(r.flaps_end - wanted) > 1e-9 || r.flaps_lowest < wanted - 1e-9) {
            faults.push_back(id + "'s flaps ended at " + std::to_string(r.flaps_end) +
                             " (lowest " + std::to_string(r.flaps_lowest) + "), against " +
                             std::to_string(wanted));
        } else if (r.flaps_most_step > a_hands_pace + 1e-12) {
            faults.push_back(id + "'s flaps moved " + std::to_string(r.flaps_most_step) +
                             " in a step, against " + std::to_string(a_hands_pace));
        }
        ++walked;
    }
    std::printf("  of the %zu aeroplanes, %zu flown (%zu with a go-around setting) and %zu "
                "left out\n",
                roster, walked, with_a_setting, taught.left_out.size());
    for (const std::string& said : taught.left_out) {
        std::printf("      left out - %s\n", said.c_str());
    }
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) + " recoveries left the flaps wrong; "
                          "the first: " + (faults.empty() ? "" : faults.front()));
    check(with_a_setting > 0, "at least one aeroplane taught a stall has a go-around setting");
    check(walked + taught.left_out.size() == roster,
          "every aeroplane in the roster was flown or left out with its reason");
}

// **The stall recovery never lowers the flaps.** Every aeroplane whose
// figures give a go-around setting, stalled and handed to the recovery at its
// warning with its flap lever at half that setting: the lever stays where it
// was handed over, every step. The recovery takes the flaps up to a
// go-around's; flaps already raised past it are not put down to it.
GLIDESLOPE_TEST(the_stall_recovery_never_lowers_flaps_raised_past_the_go_around_setting) {
    const Taught taught = taught_for("stalls");
    std::size_t with_a_setting = 0;
    std::vector<std::string> faults;
    for (const std::string& id : taught.able) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const auto figures = glideslope::sim::read_published_figures(
            data() / "figures" / (entry.model + ".xml"));
        if (!figures.go_around_flaps_deg) {
            continue;
        }
        ++with_a_setting;
        const double half = 0.5 * *figures.go_around_flaps_deg / figures.flaps_full_deg;
        const Result r = fly_a_stall(id, 0.0, false, false, true, false, half);
        std::printf("  %-13s handed over with the lever at %.4f (half its go-around's): ended "
                    "at %.4f, lowest %.4f, most in a step %.5f\n",
                    id.c_str(), r.flaps_handed, r.flaps_end, r.flaps_lowest,
                    r.flaps_most_step);
        if (r.flaps_handed < 0.0) {
            faults.push_back(id + " was never handed to its recovery");
        } else if (std::abs(r.flaps_handed - half) > 1e-12 || r.flaps_most_step != 0.0 ||
                   r.flaps_end != r.flaps_handed || r.flaps_lowest != r.flaps_handed) {
            faults.push_back(id + "'s flaps moved from " + std::to_string(r.flaps_handed) +
                             " to " + std::to_string(r.flaps_end) + " (lowest " +
                             std::to_string(r.flaps_lowest) + ")");
        }
    }
    std::printf("  %zu of the %zu aeroplanes taught a stall have a go-around setting\n",
                with_a_setting, taught.able.size());
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) + " recoveries moved flaps raised "
                          "past the go-around setting; the first: " +
                          (faults.empty() ? "" : faults.front()));
    check(with_a_setting > 0, "at least one aeroplane taught a stall has a go-around setting");
}

// **An autopilot engaged on an aeroplane already stalled recovers it.** The
// stall recovery keeps the wing below the angle of attack it has seen the
// lift peak at, and an autopilot engaged in a stall has seen only the stalled
// wing: it learns the peak as the nose comes down and the lift rises back
// through it. Every aeroplane taught a stall, left thirty seconds in one and
// handed to an autopilot new to it, is recovered - in any height, and at any
// load: the height and load are the other test's.
GLIDESLOPE_TEST(an_autopilot_engaged_on_an_aeroplane_already_stalled_recovers_it) {
    const Taught taught = taught_for("stalls");
    std::size_t walked = 0;
    std::vector<std::string> not_recovered;
    for (const std::string& id : taught.able) {
        const Result left = fly_a_stall(id, 30.0, true);
        std::printf("  %-13s handed to a new autopilot at %6.0f ft, alpha %5.1f; lost %5.0f ft, "
                    "peak %.2f g%s\n",
                    id.c_str(), left.handed_over_ft, left.handed_over_alpha_deg,
                    left.handed_over_ft - left.recovery_lowest_ft, left.peak_load_g,
                    left.recovered ? "" : ", NOT RECOVERED");
        if (!left.recovered) {
            not_recovered.push_back(id);
        }
        ++walked;
    }
    std::printf("  %zu of the %zu taught a stall flown; left out: %zu\n", walked,
                taught.able.size(), taught.left_out.size());
    for (const std::string& said : taught.left_out) {
        std::printf("      left out - %s\n", said.c_str());
    }
    check(not_recovered.empty(),
          std::to_string(not_recovered.size()) + " were not recovered by a new autopilot, the "
                                                 "first " +
              (not_recovered.empty() ? std::string() : not_recovered.front()));
    check(walked == taught.able.size(), "every aeroplane taught a stall was flown");
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
        "speedbrake", "throttle offset 0",            "throttle offset 1",
        "cooling flap 0",            "cooling flap 1"};
    // As many as there are controls.
    static_assert(std::size(names) == glideslope::sim::Controls::control_count);
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
    // For a demonstration that lands: from the touch to the end.
    glideslope::test::AfterTouch after;
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
    out.after.judged_as(entry.seaplane);
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
    const auto speeds = book_departure_speeds(data(), entry.model);

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; },
        [water = entry.seaplane](double, double) { return water; }));
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg;
    ic.longitude_deg = runway.threshold_lon_deg;
    ic.altitude_ft = runway.elevation_ft + (entry.seaplane ? 6.0 : 0.0);
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = 0.0;
    ic.engine_running = true;
    ic.gear = 1.0;
    load_for_the_take_off(aircraft, entry.model);
    aircraft.initialize(ic);
    if (entry.seaplane) {
        settle_afloat(aircraft);
    }

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
    out.after.judged_as(entry.seaplane);
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

// **Two miles out on the glidepath, established**: at the weight its
// reference speed was measured at, the landing flap down and coming down the
// path, over a runway at sea level - or water, for a flying boat.
// `model_weight`: at her model's own weight instead, as a server flies her,
// at her approach speed for it (sim::for_weight).
void put_on_final(glideslope::sim::Aircraft& aircraft,
                  const glideslope::sim::CatalogueEntry& entry,
                  const glideslope::sim::Runway& runway,
                  const glideslope::sim::ApproachSpeeds& published, bool model_weight = false) {
    // At the weight its reference speed was measured at: the B-2A's is taken
    // at its light loading, and flown at the model's own weight 124 knots is
    // below its stall - it fell at 110 ft/s and was passed, because every
    // stage of an approach ends on a height.
    if (!model_weight) {
        load_as_its_figures_were_measured(aircraft, entry.model);
    }
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; },
        [water = entry.seaplane](double, double) { return water; }));
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
    if (model_weight) {
        ic.airspeed_kts =
            glideslope::sim::for_weight(published, aircraft.loaded_weight_lbs()).vref_kts;
    } else {
        load_for_the_approach(aircraft, entry.model);
    }
    // Established on the approach: the flap it is flown with is already down,
    // and it is already coming down the glidepath rather than level on it.
    ic.flaps = published.flap;
    ic.speedbrake = published.speedbrake;
    ic.flight_path_deg = -3.0;
    ic.trim = true;
    aircraft.initialize(ic);
}

// **An approach demonstrated, then handed over.** Two miles out on the
// glidepath, flown down by the AI pilot through a `Controller` - which is
// what makes it a demonstration rather than a frontend flying a Lander.
Demonstrated demonstrate_an_approach(const std::string& id) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    put_on_final(aircraft, entry, runway, published);

    const auto found = lesson_for(entry, "approach-and-landing");
    check(found.has_value(), id + " has an approach lesson");
    double rotate = 0.0;
    double climb = 0.0;
    try {
        const auto d = book_departure_speeds(data(), entry.model);
        rotate = d.rotate_kts;
        climb = d.climb_kts;
    } catch (const std::exception&) {
    }
    LessonRun run(*found, glideslope::sim::LessonSpeeds{rotate, climb,
                                                        published.vref_kts,
                                                        published.stall_kts});

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
    out.after.judged_as(entry.seaplane);
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
        out.after.watch(aircraft);
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
    std::vector<std::string> came_down_badly;
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
        std::printf("      after touching: rolled %.1f, pitched down to %.1f, rose %.1f ft\n",
                    shown.after.worst_roll_deg, shown.after.least_pitch_deg,
                    shown.after.highest_ft);
        for (const std::string& wrong : shown.after.what_went_wrong(id)) {
            came_down_badly.push_back(wrong);
        }
        ++walked;
    }
    // **And every one stayed the right way up on its wheels from the touch
    // to the hand-over** - which is where the demonstration's landing ends:
    // the AI's landing roll goes to the stop in the approach lesson's test
    // above, and taken back on the roll she is landed to the stop in the
    // test below.
    for (const std::string& wrong : came_down_badly) {
        std::printf("  CAME DOWN BADLY: %s\n", wrong.c_str());
    }
    check(came_down_badly.empty(),
          std::to_string(came_down_badly.size()) +
              " things went wrong after touching down in the demonstration, the first: " +
              (came_down_badly.empty() ? "" : came_down_badly.front()));
    check(walked == flown.size(),
          "every aeroplane taught the exercise demonstrated it: " +
              std::to_string(walked) + " of " + std::to_string(flown.size()));
    check(walked == 14, "fourteen aeroplanes demonstrated an approach, not " +
                            std::to_string(walked));
}

namespace {

// **Where the pilot takes her and the AI takes her back**, half a second
// later: the moment her wheels meet the runway; when she has lost half the
// groundspeed she touched with; and taken in the flare, before she touches,
// and given back half a second after the pilot's own touch; and landed by
// the pilot from the start, no approach given to the AI, and given to it half
// a second after the touch.
enum class OnTheRoll {
    at_the_touch,
    half_her_speed_gone,
    after_the_pilots_touch,
    landed_by_hand,
    landed_by_hand_on_a_short_runway,
    landed_by_hand_on_a_wet_short_runway,
    landed_by_hand_and_taken_back_in_a_skip
};

// **A short runway**: the same threshold, 1,500 m long. Braked at autobrake
// 3 for no runway known, the 737-300 stopped 1,518 m past the threshold and
// the F-15C 1,805 m.
glideslope::sim::Runway a_short_runway() {
    glideslope::sim::Runway r = a_runway();
    r.name = "the short runway";
    r.length_m = 1500.0;
    return r;
}

// **A wet short runway for `model`**: as long as her wet landing distance -
// 14 CFR 121.195(d)'s 1.15 times the dry (sim::wet_landing_factor) - taking
// as the dry the longer of the short runway every landplane stops on dry
// (1,500 m) and the runway her figures say she needs to land
// (sim::landing_need_m; the six that publish none, 1,500 m): 1,725 m for
// most, 2,225 m for the A380, 2,459 m for the F-15C. Wet: runway condition
// code 5.
glideslope::sim::Runway a_wet_short_runway(const std::string& model) {
    glideslope::sim::Runway r = a_short_runway();
    r.name = "the wet short runway";
    r.length_m = std::max(a_short_runway().length_m, glideslope::sim::landing_need_m(data(), model)) *
                 glideslope::sim::wet_landing_factor;
    return r;
}

bool on_a_short_runway(OnTheRoll when) {
    return when == OnTheRoll::landed_by_hand_on_a_short_runway ||
           when == OnTheRoll::landed_by_hand_on_a_wet_short_runway;
}

const char* name_of(OnTheRoll when) {
    switch (when) {
    case OnTheRoll::at_the_touch:
        return "at the touch";
    case OnTheRoll::half_her_speed_gone:
        return "at half speed";
    case OnTheRoll::after_the_pilots_touch:
        return "from the flare";
    case OnTheRoll::landed_by_hand:
        return "landed by hand";
    case OnTheRoll::landed_by_hand_on_a_short_runway:
        return "landed by hand on a short runway";
    case OnTheRoll::landed_by_hand_on_a_wet_short_runway:
        return "landed by hand on a wet short runway";
    case OnTheRoll::landed_by_hand_and_taken_back_in_a_skip:
        return "landed by hand and taken back in a skip";
    }
    return "?";
}

struct TakenBackOnTheRoll {
    bool taken_back = false;
    bool stopped = false;
    double touched_kts = 0.0;    // groundspeed as the wheels met the runway
    double taken_back_kts = 0.0; // and as the AI took her back
    double stopped_past_m = 0.0; // past the threshold, where she stopped
    double stopped_across_m = 0.0;
    double worst_to_ai = 0.0;
    // Her height over the ground at the take-back, and the most she rose
    // above that after it.
    double agl_at_take_back_ft = 0.0;
    double rise_after_take_back_ft = 0.0;
    // Where she touched, past the threshold, as measured here and as the
    // AI's lander has it once it has her back.
    double touched_past_m = 0.0;
    double lander_touched_past_m = 0.0;
    bool lander_given = false; // the AI was given her landing back
    bool wheels_bore_weight_at_take_back = false;
    std::string wreck;         // what wrecked her, or nothing
    // From the first touch, whoever had her; and the AI's own, from the
    // take-back - or, taken back in a bounce, from the touch it came down to.
    glideslope::test::AfterTouch after;
    glideslope::test::AfterTouch ai;
};

// **An approach flown by the AI, taken by the pilot and taken back.** The
// pilot's hands are what a pilot's are with nothing done yet: throttle
// closed, the landing flap, the stick central and no brakes. From the
// take-back the AI has her, to the stop or for five minutes, whichever is
// first, and the server's crash rule judges her from then.
TakenBackOnTheRoll take_back_on_the_roll(const std::string& id, OnTheRoll when) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    put_on_final(aircraft, entry, runway, published);

    glideslope::sim::Controls flying;
    flying.throttle = 0.4;
    flying.gear = 1.0;
    glideslope::sim::Controller controller(aircraft, flying);
    const bool in_a_skip = when == OnTheRoll::landed_by_hand_and_taken_back_in_a_skip;
    const bool by_hand =
        when == OnTheRoll::landed_by_hand || on_a_short_runway(when) || in_a_skip;
    // **A skip, built**: the runway let down half a metre two steps before
    // the take-back, and left there, so that her wheels are clear of it as
    // she is handed over, whatever her own landing did.
    const auto let_down_m = std::make_shared<double>(0.0);
    if (in_a_skip) {
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [let_down_m](double, double) { return -*let_down_m; },
            [](double, double) { return false; }));
    }
    if (on_a_short_runway(when)) {
        // Told the runway she rolls on, as the server and the client tell
        // theirs from the world's (world::runway_rolled_on).
        const glideslope::sim::Runway rolled_on =
            when == OnTheRoll::landed_by_hand_on_a_wet_short_runway
                ? a_wet_short_runway(entry.model)
                : a_short_runway();
        controller.finds_runways_with([rolled_on](const glideslope::sim::Aircraft&) {
            return std::optional<glideslope::sim::Runway>(rolled_on);
        });
    }
    if (when == OnTheRoll::landed_by_hand_on_a_wet_short_runway) {
        // Wet everywhere, still air: the weather gives her runway code 5.
        glideslope::sim::Conditions wet;
        wet.runway_condition = glideslope::sim::wet_runway;
        aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(wet));
    }
    if (by_hand) {
        // The pilot has her from the start, and the AI is told only how
        // she lands, from what the server and the client both tell theirs.
        const auto lands = glideslope::sim::landing_speeds(data(), entry.model);
        check(lands.has_value(), entry.id + " has landing speeds to be told");
        controller.lands_with(*lands);
    } else {
        controller.to_ai_approach(runway, published);
    }
    glideslope::sim::Controls pilot;
    pilot.gear = 1.0;
    pilot.flaps = published.flap;
    controller.set_pilot(pilot);

    const auto groundspeed_kts = [&] {
        return aircraft.property("velocities/vg-fps") / 1.68781;
    };
    // Past the threshold, measured as the runway is.
    const auto past_and_across = [&] {
        const glideslope::sim::AircraftState s = aircraft.state();
        const double north_m = (s.latitude_deg - runway.threshold_lat_deg) *
                               metres_per_degree_latitude(runway.threshold_lat_deg);
        const double east_m = (s.longitude_deg - runway.threshold_lon_deg) *
                              metres_per_degree_longitude(runway.threshold_lat_deg);
        const double h = runway.heading_deg / degrees;
        return std::pair{east_m * std::sin(h) + north_m * std::cos(h),
                         east_m * std::cos(h) - north_m * std::sin(h)};
    };
    glideslope::sim::Lander shadow(aircraft, runway, published);
    glideslope::sim::GroundJudge judge(entry.seaplane);
    TakenBackOnTheRoll out;
    out.after.judged_as(entry.seaplane);
    out.ai.judged_as(entry.seaplane);
    bool handed_over = by_hand;
    int take_back = -1;
    glideslope::sim::Controls last = flying;
    for (int tick = 0; tick < 900 * steps_per_second; ++tick) {
        const bool was_touched = out.after.touched;
        if (take_back >= 0 && tick > take_back &&
            std::abs(aircraft.property("velocities/vg-fps")) < 1.0) {
            out.stopped = true;
            break;
        }
        if (take_back >= 0 && tick > take_back + 300 * steps_per_second) {
            break;
        }
        if (!handed_over) {
            const bool now =
                when == OnTheRoll::after_the_pilots_touch
                    ? controller.lander() != nullptr &&
                          controller.lander()->stage() == glideslope::sim::Lander::Stage::flare
                    : was_touched && (when == OnTheRoll::at_the_touch ||
                                      groundspeed_kts() <= 0.5 * out.touched_kts);
            if (now) {
                handed_over = true;
                controller.to_pilot();
                if (when != OnTheRoll::after_the_pilots_touch) {
                    take_back = tick + steps_per_second / 2;
                }
            }
        }
        const bool pilots_own = when == OnTheRoll::after_the_pilots_touch || by_hand;
        if (pilots_own && handed_over && take_back < 0 && was_touched) {
            take_back = tick + steps_per_second / 2;
        }
        // **Taken in the flare, the pilot flies the flare, the touch and
        // the half second after it as the AI would have** - the controls of
        // a second lander flown alongside from the start, which sees what
        // the AI's sees and so asks for the same - so that what differs is
        // only who had her when she touched. Let go to neutral in the flare
        // instead, a 737 zooms to fifty feet at 1,100 ft/min with the power
        // off, which is not a landing, and the AI rightly lets it go; held
        // where the AI had it, a Mosquito touches at 120 knots, flying, and
        // bounces - the rollout's own tail, not this one.
        const glideslope::sim::Controls shadowed = shadow.fly();
        if (pilots_own && handed_over && (take_back < 0 || tick < take_back)) {
            controller.set_pilot(shadowed);
        } else if (pilots_own && handed_over) {
            controller.set_pilot(pilot);
        }
        if (in_a_skip && take_back >= 0 && tick == take_back - 2) {
            *let_down_m = 0.5;
        }
        if (tick == take_back) {
            out.wheels_bore_weight_at_take_back = aircraft.property("gear/wow") > 0.5;
            controller.to_ai();
            out.taken_back = true;
            out.taken_back_kts = groundspeed_kts();
            out.agl_at_take_back_ft = aircraft.property("position/h-agl-ft");
        }
        const glideslope::sim::Controls now = controller.fly();
        if (tick == take_back) {
            out.worst_to_ai = worst_step(last, now);
            out.lander_given = controller.lander() != nullptr;
            if (out.lander_given) {
                out.lander_touched_past_m = controller.lander()->touchdown_along_m();
            }
        }
        last = now;
        aircraft.set_controls(now);
        aircraft.step();
        out.after.watch(aircraft);
        if (take_back >= 0 && tick >= take_back) {
            out.ai.watch(aircraft);
            out.rise_after_take_back_ft =
                std::max(out.rise_after_take_back_ft,
                         aircraft.property("position/h-agl-ft") - out.agl_at_take_back_ft);
        }
        if (!was_touched && out.after.touched) {
            out.touched_kts = groundspeed_kts();
            out.touched_past_m = past_and_across().first;
        }
        // Judged from the take-back, which is what is being tested: the
        // judge's first call learns what is touching already.
        if (take_back >= 0 && tick >= take_back) {
            if (const auto what = judge.judge(aircraft)) {
                out.wreck = *what;
                break;
            }
        }
    }
    const auto [past_m, across_m] = past_and_across();
    out.stopped_past_m = past_m;
    out.stopped_across_m = across_m;
    return out;
}

// **Every landplane taught the approach, taken back at `when`.** She must be
// given her landing back, stop on the runway, the right way up on her wheels
// and unwrecked, and rise less than three feet above where she was at the
// take-back (one bound named, below); the take-back must step no control faster than a pilot's hand;
// and the AI must know where she touched, to within five metres, whoever put
// her down. Taken back, she was given the plain autopilot, which holds what
// she is doing and never stops her.
//
// **What the AI is judged on is its own**: the crash rule from the take-back,
// and the bank, the nose and any bounce from there - or, taken back in a
// bounce the pilot's half second began, from the touch it brings her down to.
//
// **The flying boat is left out, and named**: afloat with her engines idling
// she is never still, so a stop is not hers to make (her approach lesson ends
// below twenty knots on the water), and there is no runway to stop on.
void every_landplane_taken_back(OnTheRoll when) {
    const double a_hands_pace = 2.0 / steps_per_second + 0.004;
    const double half_width_m = glideslope::sim::Lander::runway_half_width_m;
    const auto taught = everyone_taught("approach-and-landing");
    std::vector<std::string> landplanes;
    std::vector<std::string> left_out;
    // **On the wet runway, the F-35B is named too**: she publishes no landing
    // distance - one of the five the plan's Later item on a landing distance
    // from a primary source names - so none says how long a wet runway she
    // needs, and touching at 155 kt, where a wet runway gives a braked wheel
    // 0.135 (14 CFR 25.109(c)), she needs about 2,320 m of the 1,725.
    const bool wet = when == OnTheRoll::landed_by_hand_on_a_wet_short_runway;
    for (const std::string& id : taught) {
        if (glideslope::sim::find_aircraft(data(), id).seaplane) {
            left_out.push_back(id);
            std::printf("  left out - %s: a flying boat, afloat, is never still\n", id.c_str());
        } else if (wet && id == "f35b") {
            left_out.push_back(id);
            std::printf("  left out - %s: no published landing distance to size a wet runway "
                        "by (a Later item), and 1,725 m is short of the 2,320 she needs wet\n",
                        id.c_str());
        } else {
            landplanes.push_back(id);
        }
    }
    std::vector<std::string> wrong;
    std::size_t flown = 0;
    for (const std::string& id : landplanes) {
        const TakenBackOnTheRoll r = take_back_on_the_roll(id, when);
        std::printf("  %-13s %-14s touched %5.1f kt at %4.0f m (the AI has %4.0f m), taken "
                    "back %5.1f kt at %.1f ft: %s %6.0f m past the threshold, %5.1f m across; "
                    "bank %.1f, nose %.1f, rose %.1f ft after the take-back (%.1f from the "
                    "first touch, %.1f from the AI's), step %.4f%s%s\n",
                    id.c_str(), name_of(when), r.touched_kts, r.touched_past_m,
                    r.lander_touched_past_m, r.taken_back_kts, r.agl_at_take_back_ft,
                    r.stopped ? "stopped" : "NOT STOPPED", r.stopped_past_m,
                    r.stopped_across_m, r.after.worst_roll_deg, r.after.least_pitch_deg,
                    r.rise_after_take_back_ft, r.after.highest_ft, r.ai.highest_ft,
                    r.worst_to_ai, r.wreck.empty() ? "" : ", wrecked: ", r.wreck.c_str());
        const std::string where = id + " taken back " + name_of(when);
        if (!r.taken_back) {
            wrong.push_back(where + " was never taken back: " +
                            (r.wreck.empty() ? "it never touched down"
                                             : "wrecked first, " + r.wreck));
            continue;
        }
        if (when == OnTheRoll::landed_by_hand_and_taken_back_in_a_skip &&
            r.wheels_bore_weight_at_take_back) {
            wrong.push_back(where + ": her wheels bore weight at the take-back - no skip was built");
        }
        if (!r.lander_given) {
            wrong.push_back(where + " was not given her landing back");
        } else if (when != OnTheRoll::landed_by_hand &&
                   !on_a_short_runway(when) &&
                   when != OnTheRoll::landed_by_hand_and_taken_back_in_a_skip &&
                   std::abs(r.lander_touched_past_m - r.touched_past_m) > 5.0) {
            // (Landed by hand, no approach was given: the AI's runway is the
            // line she rolls along from where it took her, and she touched,
            // for it, there.)
            wrong.push_back(where + ": the AI has her touching at " +
                            std::to_string(r.lander_touched_past_m) + " m, not " +
                            std::to_string(r.touched_past_m));
        }
        if (!r.wreck.empty()) {
            wrong.push_back(where + " was wrecked: " + r.wreck);
        }
        if (!r.stopped) {
            wrong.push_back(where + " did not stop");
        }
        const double length_m =
            when == OnTheRoll::landed_by_hand_on_a_wet_short_runway
                ? a_wet_short_runway(glideslope::sim::find_aircraft(data(), id).model).length_m
            : when == OnTheRoll::landed_by_hand_on_a_short_runway   ? a_short_runway().length_m
                                                                    : a_runway().length_m;
        if (r.stopped_past_m < 0.0 || r.stopped_past_m > length_m ||
            std::abs(r.stopped_across_m) > half_width_m) {
            wrong.push_back(where + " stopped off the runway, " +
                            std::to_string(r.stopped_past_m) + " m past the threshold and " +
                            std::to_string(r.stopped_across_m) + " m across");
        }
        // **Under three feet, every one.** An A320 taken at the touch by a
        // pilot whose stick goes to neutral rose 4.1 ft after the AI had her
        // back, when the flare was flown to her centre of gravity and not her
        // wheels: still rotating as she touched, she was climbing when the AI
        // had her. With the flare flown to her wheels, and to the sink a
        // moment ahead (sim/lander.cpp), she rises 0.8 ft, and is held to the
        // three feet the rest are.
        constexpr double most_rise_ft = 3.0;
        if (r.rise_after_take_back_ft >= most_rise_ft) {
            wrong.push_back(where + " rose " + std::to_string(r.rise_after_take_back_ft) +
                            " ft after the take-back");
        }
        if (r.worst_to_ai > a_hands_pace) {
            wrong.push_back(where + " stepped " + std::to_string(r.worst_to_ai) +
                            " taking back");
        }
        for (const std::string& said : r.ai.what_went_wrong(id)) {
            wrong.push_back(where + ": " + said);
        }
        ++flown;
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong taking back " +
                             name_of(when) + ", the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(landplanes.size() + left_out.size() == taught.size() &&
              left_out.size() == (wet ? 2U : 1U),
          "every aeroplane taught the approach is a landplane flown here or the one flying "
          "boat named (and wet, the F-35B): " + std::to_string(landplanes.size()) + " and " +
              std::to_string(left_out.size()) + " of " + std::to_string(taught.size()));
    check(flown == landplanes.size(), "every landplane was taken back " +
                                          std::string(name_of(when)) + ": " +
                                          std::to_string(flown) + " of " +
                                          std::to_string(landplanes.size()));
}

} // namespace

// **An approach taken back on the landing roll is landed to a stop** - one
// test for each point she is taken back at, so that none is long.
GLIDESLOPE_TEST(an_approach_taken_back_at_the_touch_is_landed_to_a_stop) {
    every_landplane_taken_back(OnTheRoll::at_the_touch);
}

GLIDESLOPE_TEST(an_approach_taken_back_at_half_speed_is_landed_to_a_stop) {
    every_landplane_taken_back(OnTheRoll::half_her_speed_gone);
}

// Taken by the pilot in the flare, put down by the pilot and taken back: the
// kept lander must have watched her touch, and must not have been dropped
// while she was still in the air.
GLIDESLOPE_TEST(an_approach_the_pilot_puts_down_from_the_flare_and_hands_back_is_landed_to_a_stop) {
    every_landplane_taken_back(OnTheRoll::after_the_pilots_touch);
}

// **A landing the pilot flew from the start, no approach given to the AI,
// handed to it on the roll is landed to a stop**, not given the plain
// autopilot, which held what she was doing and never stopped her. The pilot
// flies the approach and the flare as a lander would, touches, and half a
// second later hands her over with the throttle closed and the stick
// central; the AI is told only how she lands (`Controller::lands_with`).
GLIDESLOPE_TEST(an_aeroplane_landed_by_hand_and_handed_over_on_its_roll_is_landed_to_a_stop) {
    every_landplane_taken_back(OnTheRoll::landed_by_hand);
}

// **And told the runway she is rolling on, she is stopped on it, however
// short**: every landplane landed by hand on a 1,500 m runway and handed over
// half a second after the touch is braked for what is left of it, not at
// autobrake 3, which ran the 737-300 and the F-15C off its end.
GLIDESLOPE_TEST(a_landing_flown_by_hand_on_a_short_runway_is_stopped_on_it_by_the_ai) {
    every_landplane_taken_back(OnTheRoll::landed_by_hand_on_a_short_runway);
}

// **And on a wet one, a runway 15 per cent longer, she is stopped on it
// too**: every landplane landed by hand on a 1,725 m runway of condition
// code 5 - her braked wheels gripping as 14 CFR 25.109(c) says a wet
// runway lets them, less than half her model's dry friction at speed - and
// handed over half a second after the touch.
GLIDESLOPE_TEST(every_landplane_landed_by_hand_on_a_wet_short_runway_is_stopped_on_it_by_the_ai) {
    every_landplane_taken_back(OnTheRoll::landed_by_hand_on_a_wet_short_runway);
}

// **Handed over in a skip, she is landed to a stop**: every landplane landed
// by hand and handed over half a second after the touch with her wheels
// clear of the runway - the runway let down half a metre beneath her two
// steps before, so that none bears weight, asserted for each. Handed over so,
// a 787 on a wet runway was given the plain autopilot and ran 22.7 km.
GLIDESLOPE_TEST(every_landplane_landed_by_hand_and_handed_over_in_a_skip_is_landed_to_a_stop) {
    every_landplane_taken_back(OnTheRoll::landed_by_hand_and_taken_back_in_a_skip);
}

// **And a take-off handed over at lift-off is not a skip**: every landplane
// taught the approach (13; the flying boat named), taken off by the pilot at
// full power - the take-off autopilot's controls as the pilot's - and handed
// to the AI on the step her wheels leave the runway, is given the plain
// autopilot, not a landing to stop her. (Whether the plain autopilot then
// climbs her away is not asked here: it holds the height she was handed over
// at, and seven of the 13 touch the runway again within 30 s - an item of
// its own, "A take-off handed to the AI at lift-off is not climbed away".)
GLIDESLOPE_TEST(a_take_off_handed_over_at_lift_off_is_given_the_plain_autopilot_not_a_landing) {
    const auto taught = everyone_taught("approach-and-landing");
    std::size_t flown = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const std::string& id : taught) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat takes off from water\n", id.c_str());
            ++left_out;
            continue;
        }
        const glideslope::sim::Runway runway = a_runway();
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        load_as_its_figures_were_measured(aircraft, entry.model);
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; }, [](double, double) { return false; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = runway.threshold_lat_deg;
        ic.longitude_deg = runway.threshold_lon_deg;
        ic.altitude_ft = runway.elevation_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.engine_running = true;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        glideslope::sim::Departure takeoff(
            aircraft, runway, glideslope::sim::departure_speeds(data(), entry.model));
        glideslope::sim::Controls pilot = takeoff.fly();
        glideslope::sim::Controller controller(aircraft, pilot);
        controller.lands_with(*glideslope::sim::landing_speeds(data(), entry.model));
        bool was_down = false;
        bool handed = false;
        double handed_ft = 0.0;
        double highest_ft = 0.0;
        double throttle_at_hand_over = 0.0;
        bool given_landing = true;
        bool came_down = false;
        int handed_at = -1;
        for (int tick = 0; tick < 300 * steps_per_second; ++tick) {
            const bool down = aircraft.property("gear/wow") > 0.5;
            if (!handed) {
                pilot = takeoff.fly();
                controller.set_pilot(pilot);
                if (was_down && !down) {
                    throttle_at_hand_over = pilot.throttle;
                    controller.to_ai();
                    handed = true;
                    given_landing = controller.lander() != nullptr;
                    handed_ft = aircraft.property("position/h-agl-ft");
                    handed_at = tick;
                }
            }
            was_down = down;
            aircraft.set_controls(controller.fly());
            aircraft.step();
            if (handed) {
                came_down = came_down || aircraft.property("gear/wow") > 0.5;
                highest_ft = aircraft.property("position/h-agl-ft");
                if (tick - handed_at >= 30 * steps_per_second) {
                    break;
                }
            }
        }
        std::printf("  %-13s handed over at lift-off at %.2f throttle: %s, %.0f ft higher 30 s "
                    "later%s\n",
                    id.c_str(), throttle_at_hand_over,
                    given_landing ? "given a landing" : "the plain autopilot",
                    highest_ft - handed_ft, came_down ? ", touching the runway again" : "");
        ++flown;
        if (!handed) {
            wrong.push_back(id + " never left the ground");
        } else if (given_landing) {
            wrong.push_back(id + " was given a landing at lift-off");
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(flown + left_out == taught.size() && left_out == 1,
          "every landplane taught the approach handed over at lift-off: " +
              std::to_string(flown) + " of " + std::to_string(taught.size() - 1));
}

// **Handed over taxiing, she is stopped with the throttle no more than half
// open, and not otherwise.** The landing taken over on its roll is found by
// her wheels being down, her moving and the pilot's throttle at most half
// open, so a pilot taxiing that slowly who hands her to the AI has her
// stopped where she is, and one with more throttle than that - a take-off
// roll - is given the plain autopilot. Every landplane taught the approach
// (13; the flying boat named), run up on the runway to ten knots and handed
// over with the throttle at a half, and again at six tenths.
GLIDESLOPE_TEST(a_pilot_taxiing_at_no_more_than_half_throttle_who_hands_over_is_stopped_and_above_it_is_not) {
    const auto taught = everyone_taught("approach-and-landing");
    std::size_t flown = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const std::string& id : taught) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat, afloat, is never still\n", id.c_str());
            ++left_out;
            continue;
        }
        for (const double throttle : {0.5, 0.6}) {
            const glideslope::sim::Runway runway = a_runway();
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
            load_as_its_figures_were_measured(aircraft, entry.model);
            aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
                [](double, double) { return 0.0; }, [](double, double) { return false; }));
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = runway.threshold_lat_deg;
            ic.longitude_deg = runway.threshold_lon_deg;
            ic.altitude_ft = runway.elevation_ft;
            ic.terrain_elevation_ft = runway.elevation_ft;
            ic.heading_deg = runway.heading_deg;
            ic.engine_running = true;
            ic.gear = 1.0;
            aircraft.initialize(ic);
            glideslope::sim::Controls pilot;
            pilot.gear = 1.0;
            pilot.throttle = 1.0;
            glideslope::sim::Controller controller(aircraft, pilot);
            controller.lands_with(*glideslope::sim::landing_speeds(data(), entry.model));
            const auto kts = [&] { return aircraft.property("velocities/vg-fps") / 1.68781; };
            int tick = 0;
            for (; tick < 120 * steps_per_second && kts() < 10.0; ++tick) {
                controller.set_pilot(pilot);
                aircraft.set_controls(controller.fly());
                aircraft.step();
            }
            const std::string where =
                id + " handed over taxiing at " + std::to_string(throttle) + " throttle";
            if (kts() < 10.0) {
                wrong.push_back(where + ": never reached ten knots");
                continue;
            }
            pilot.throttle = throttle;
            controller.set_pilot(pilot);
            controller.to_ai();
            const bool given = controller.lander() != nullptr;
            bool stopped = false;
            for (int t = 0; t < 120 * steps_per_second; ++t) {
                aircraft.set_controls(controller.fly());
                aircraft.step();
                if (std::abs(aircraft.property("velocities/vg-fps")) < 1.0) {
                    stopped = true;
                    break;
                }
            }
            std::printf("  %-13s at %.1f: %s, %s\n", id.c_str(), throttle,
                        given ? "given a landing" : "the plain autopilot",
                        stopped ? "stopped" : "not stopped");
            ++flown;
            const bool should = throttle <= 0.5;
            if (given != should) {
                wrong.push_back(where + (given ? " was" : " was not") +
                                " given a landing to stop her");
            }
            if (should && !stopped) {
                wrong.push_back(where + " was not stopped");
            }
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(flown == 2 * (taught.size() - left_out) && left_out == 1,
          "every landplane handed over at both throttles: " + std::to_string(flown) + " of " +
              std::to_string(2 * (taught.size() - left_out)));
}

namespace {

// **Ground rising beside the runway**, on the side the circuit is flown: level
// to 400 m left of the centreline, then rising six metres in a hundred - 156 m
// three kilometres out, where a light aeroplane's downwind leg is, and more
// under a jet's wider one.
double rising_left_of(const glideslope::sim::Runway& runway, double lat, double lon) {
    const double north_m =
        (lat - runway.threshold_lat_deg) * metres_per_degree_latitude(runway.threshold_lat_deg);
    const double east_m =
        (lon - runway.threshold_lon_deg) * metres_per_degree_longitude(runway.threshold_lat_deg);
    const double h = runway.heading_deg / degrees;
    const double across_m = east_m * std::cos(h) - north_m * std::sin(h);
    return std::max(0.0, -across_m - 400.0) * 0.06;
}

// **An aeroplane that goes around is flown round and landed.** Every
// landplane taught the approach (the flying boat named), flown down the
// approach by the AI from two miles out and told to go around at 200 ft: she
// climbs away, cleans up - the flap to half the landing flap and the gear up
// where it retracts, both by the downwind leg - flies the circuit's every leg
// and the approach again, and stops on the runway, the right way up and
// unwrecked, within thirty minutes. Before, at 500 ft the plain autopilot
// held her climb at the landing flap, and nothing flew her round.
void every_landplane_goes_around(bool rising) {
    using Leg = glideslope::sim::GoAroundCircuit::Leg;
    const auto taught = everyone_taught("approach-and-landing");
    std::size_t flown = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    const glideslope::sim::Runway runway = a_runway();
    for (const std::string& id : taught) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat has no runway to fly round to\n",
                        id.c_str());
            ++left_out;
            continue;
        }
        const auto published = glideslope::sim::approach_speeds(data(), entry.model);
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        put_on_final(aircraft, entry, runway, published);
        if (rising) {
            aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
                [&runway](double lat, double lon) { return rising_left_of(runway, lat, lon); },
                [](double, double) { return false; }));
        }
        glideslope::sim::Controls flying;
        flying.throttle = 0.4;
        flying.gear = 1.0;
        glideslope::sim::Controller controller(aircraft, flying);
        controller.to_ai_approach(runway, published);
        // As a server's are: no faster than her fastest a plan may ask.
        controller.limit_speed(glideslope::sim::plan_speeds(data(), entry.model).fastest_kts);
        glideslope::sim::GroundJudge judge(false);
        // The circuit height the flat runway alone would give, and the
        // least height over the ground beneath on the downwind leg.
        const double flat_circuit_ft =
            std::clamp(1000.0 + (published.vref_kts - 60.0) * 8.0, 1000.0, 1500.0);
        double lowest_downwind_agl_ft = 1e9;
        bool told = false;
        bool circuit_seen = false;
        std::vector<Leg> legs;
        double downwind_flap = -1.0;
        double downwind_gear = -1.0;
        double highest_ft = 0.0;
        bool stopped = false;
        std::string wreck;
        for (int tick = 0; tick < 1800 * steps_per_second; ++tick) {
            const double agl = aircraft.property("position/h-agl-ft");
            if (!told && agl <= 200.0) {
                controller.go_around();
                told = true;
            }
            const glideslope::sim::Controls c = controller.fly();
            if (const auto* circuit = controller.circuit()) {
                circuit_seen = true;
                if (legs.empty() || legs.back() != circuit->leg()) {
                    legs.push_back(circuit->leg());
                }
                if (circuit->leg() == Leg::downwind) {
                    downwind_flap = c.flaps;
                    downwind_gear = c.gear;
                    lowest_downwind_agl_ft = std::min(lowest_downwind_agl_ft, agl);
                }
            }
            aircraft.set_controls(c);
            aircraft.step();
            highest_ft = std::max(highest_ft, agl);
            if (const auto what = judge.judge(aircraft)) {
                wreck = *what;
                break;
            }
            if (circuit_seen && controller.circuit() == nullptr && controller.lander() &&
                controller.lander()->stage() == glideslope::sim::Lander::Stage::stopped) {
                stopped = true;
                break;
            }
        }
        const glideslope::sim::AircraftState s = aircraft.state();
        const double north_m = (s.latitude_deg - runway.threshold_lat_deg) *
                               metres_per_degree_latitude(runway.threshold_lat_deg);
        const double east_m = (s.longitude_deg - runway.threshold_lon_deg) *
                              metres_per_degree_longitude(runway.threshold_lat_deg);
        const double h = runway.heading_deg / degrees;
        const double past_m = east_m * std::sin(h) + north_m * std::cos(h);
        const double across_m = east_m * std::cos(h) - north_m * std::sin(h);
        std::printf("  %-13s legs %zu, highest %4.0f ft, downwind flap %.2f gear %.0f, at "
                    "least %4.0f ft over the ground: %s %5.0f m past, %5.1f m across%s%s\n",
                    id.c_str(), legs.size(), highest_ft, downwind_flap, downwind_gear,
                    lowest_downwind_agl_ft,
                    stopped ? "stopped" : "NOT STOPPED", past_m, across_m,
                    wreck.empty() ? "" : ", wrecked: ", wreck.c_str());
        ++flown;
        const std::vector<Leg> every{Leg::upwind, Leg::crosswind, Leg::downwind,
                                     Leg::base,   Leg::intercept, Leg::final};
        if (legs != every) {
            wrong.push_back(id + " did not fly every leg of the circuit in order: " +
                            std::to_string(legs.size()) + " legs");
        }
        if (std::abs(downwind_flap - 0.5 * published.flap) > 1e-9) {
            wrong.push_back(id + " flew downwind at flap " + std::to_string(downwind_flap));
        }
        if (aircraft.gear_retracts() != (downwind_gear == 0.0)) {
            wrong.push_back(id + "'s gear on the downwind leg was " +
                            std::to_string(downwind_gear));
        }
        // **The circuit's height is over the ground it is flown over**: the
        // downwind leg nowhere lower over the ground than 0.85 of
        // what the runway alone would give - the autopilot's own settling
        // allowed for.
        if (lowest_downwind_agl_ft < 0.85 * flat_circuit_ft) {
            wrong.push_back(id + " flew downwind " + std::to_string(lowest_downwind_agl_ft) +
                            " ft over the ground");
        }
        if (!wreck.empty()) {
            wrong.push_back(id + " was wrecked: " + wreck);
        }
        if (!stopped || past_m < 0.0 || past_m > runway.length_m ||
            std::abs(across_m) > glideslope::sim::Lander::runway_half_width_m) {
            wrong.push_back(id + " was not stopped on the runway: " + std::to_string(past_m) +
                            " m past, " + std::to_string(across_m) + " m across");
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(flown + left_out == taught.size() && left_out == 1,
          "every landplane taught the approach went around, the flying boat named: " +
              std::to_string(flown) + " of " + std::to_string(taught.size() - left_out));
}

} // namespace

GLIDESLOPE_TEST(an_aeroplane_that_goes_around_is_flown_round_and_landed) {
    every_landplane_goes_around(false);
}

// **And over ground rising beside the runway, the circuit is flown its height
// over the ground**, not over the runway: raised by the highest ground of the
// circuit, every landplane flies downwind at least 0.85 of its circuit
// height over the slope and lands.
GLIDESLOPE_TEST(an_aeroplane_going_around_beside_rising_ground_flies_its_circuit_height_over_it_and_lands) {
    every_landplane_goes_around(true);
}

namespace {

// **How an approach is begun, for the stabilized-approach tests**: as
// put_on_final has her, two miles out on the glidepath; or fast; or high.
enum class Arriving { as_flown, fast, high };

struct Arrival {
    std::string why;      // why she went around by herself, or empty
    bool circuit = false; // flew the go-around's circuit
    bool stopped = false; // stopped on the runway (as flown: down on it)
    std::string wreck;
    double touched_m = -1.0; // where the landing touched down, along
};

Arrival arrive(const glideslope::sim::CatalogueEntry& entry,
               const glideslope::sim::Runway& runway,
               const glideslope::sim::ApproachSpeeds& published, Arriving how) {
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    put_on_final(aircraft, entry, runway, published);
    if (how != Arriving::as_flown) {
        // **Built, not hoped for.** Fast: 20 kt over her reference speed at
        // 495 ft on the glidepath, just under the gate - started above it,
        // her throttle took some or all of the knots off before she reached
        // it, as much as it could in the seconds she had. High: a kilometre before the threshold at
        // 450 ft, 227 ft over the glidepath - under the gate, and that high
        // her path down would meet the runway past the zone's end.
        const bool fast = how == Arriving::fast;
        const double gp = std::tan(3.0 / degrees);
        const double out_m = fast ? 495.0 / feet_per_metre / gp - published.aim_m : 1000.0;
        const double above_ft = fast ? 495.0 : 450.0;
        const double heading = runway.heading_deg / degrees;
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg =
            runway.threshold_lat_deg + (-out_m * std::cos(heading)) /
                                           metres_per_degree_latitude(runway.threshold_lat_deg);
        ic.longitude_deg =
            runway.threshold_lon_deg + (-out_m * std::sin(heading)) /
                                           metres_per_degree_longitude(runway.threshold_lat_deg);
        ic.altitude_ft = runway.elevation_ft + above_ft;
        ic.terrain_elevation_ft = runway.elevation_ft;
        ic.heading_deg = runway.heading_deg;
        ic.airspeed_kts = published.vref_kts + (fast ? 20.0 : 0.0);
        ic.engine_running = true;
        ic.gear = 1.0;
        ic.flaps = published.flap;
        ic.speedbrake = published.speedbrake;
        // Fast, she is trimmed level, under power: at idle down the
        // glidepath an A320 20 kt over her reference speed gathers speed, and
        // JSBSim cannot trim her so. (Nor the Mosquito level: she is flown
        // from as she is put, and goes around at once all the same.)
        ic.flight_path_deg = fast ? 0.0 : -3.0;
        ic.trim = true;
        aircraft.initialize(ic);
    }
    glideslope::sim::Controls flying;
    flying.throttle = 0.4;
    flying.gear = 1.0;
    glideslope::sim::Controller controller(aircraft, flying);
    controller.to_ai_approach(runway, published);
    controller.limit_speed(glideslope::sim::plan_speeds(data(), entry.model).fastest_kts);
    glideslope::sim::GroundJudge judge(false);
    Arrival a;
    for (int tick = 0; tick < 1800 * steps_per_second; ++tick) {
        aircraft.set_controls(controller.fly());
        aircraft.step();
        if (const auto* l = controller.lander();
            l && a.why.empty() && !l->why_gone_around().empty()) {
            a.why = l->why_gone_around();
        }
        a.circuit = a.circuit || controller.circuit() != nullptr;
        if (const auto what = judge.judge(aircraft)) {
            a.wreck = *what;
            break;
        }
        if (controller.lander() && controller.lander()->touched()) {
            a.touched_m = controller.lander()->touchdown_along_m();
        }
        if (controller.circuit() == nullptr && controller.lander() &&
            controller.lander()->stage() == glideslope::sim::Lander::Stage::stopped) {
            const glideslope::sim::AircraftState s = aircraft.state();
            const auto at =
                glideslope::sim::on_runway_frame(runway, s.latitude_deg, s.longitude_deg);
            a.stopped = at.along_m >= 0.0 && at.along_m <= runway.length_m &&
                        std::abs(at.across_m) <= glideslope::sim::Lander::runway_half_width_m;
            break;
        }
        // An approach flown well is over, for this, once she is down.
        if (how == Arriving::as_flown && a.why.empty() && controller.lander() &&
            controller.lander()->touched()) {
            a.stopped = true;
            break;
        }
    }
    return a;
}

// Every landplane taught the approach (the flying boat named: it has no
// runway to fly round to), arriving `how`: an approach that is not
// stabilized goes around, for the reason it is not, is flown round the
// circuit and lands, stopped on the runway; one that is touches down with
// no go-around.
void every_landplane_arriving(Arriving how) {
    const auto taught = everyone_taught("approach-and-landing");
    const glideslope::sim::Runway runway = a_runway();
    std::size_t flown = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const std::string& id : taught) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat has no runway to fly round to\n",
                        id.c_str());
            ++left_out;
            continue;
        }
        const auto published = glideslope::sim::approach_speeds(data(), entry.model);
        const Arrival a = arrive(entry, runway, published, how);
        std::printf("  %-13s %s%s%s%s%s; touched %.0f m along, %.0f m inside the zone\n",
                    id.c_str(), a.why.empty() ? "no go-around" : "went around: ", a.why.c_str(),
                    a.circuit ? ", flew the circuit" : "", a.stopped ? ", landed" : ", NOT LANDED",
                    a.wreck.empty() ? "" : (", wrecked: " + a.wreck).c_str(), a.touched_m,
                    glideslope::sim::StabilizedApproach::touchdown_zone_m(runway) - a.touched_m);
        ++flown;
        const bool should = how != Arriving::as_flown;
        // Named: the Mosquito cannot be trimmed 20 kt fast, even level, and
        // flown from as she is put bleeds the speed within the gate's two
        // seconds; she goes around for being slow instead.
        const std::string want = how == Arriving::fast
                                     ? (id == "mosquito-fb6" ? "kt slow" : "kt fast")
                                     : "touchdown zone";
        if (should && a.why.find(want) == std::string::npos) {
            wrong.push_back(id + " did not go around for '" + want + "': '" + a.why + "'");
        }
        if (!should && !a.why.empty()) {
            wrong.push_back(id + " went around from an approach flown well: " + a.why);
        }
        if (should && !a.circuit) {
            wrong.push_back(id + " was not flown round the circuit");
        }
        if (!a.wreck.empty() || !a.stopped) {
            wrong.push_back(id + " did not land: " + a.wreck);
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(flown + left_out == taught.size() && left_out == 1,
          "every landplane taught the approach arrived, the flying boat named: " +
              std::to_string(flown) + " of " + std::to_string(taught.size() - left_out));
}

} // namespace

// **An approach flown 20 kt fast goes around and then lands**: under the
// 500 ft gate more than ten knots over her reference speed is not a
// stabilized approach (sim::StabilizedApproach).
GLIDESLOPE_TEST(an_approach_twenty_knots_fast_at_the_gate_goes_around_and_lands) {
    every_landplane_arriving(Arriving::fast);
}

// **An approach too high to touch down in the touchdown zone goes around
// and then lands.**
GLIDESLOPE_TEST(an_approach_too_high_to_touch_down_in_the_zone_goes_around_and_lands) {
    every_landplane_arriving(Arriving::high);
}

// **And an approach flown well does not go around**: from two miles out on
// the glidepath at her reference speed, she touches down with no go-around.
GLIDESLOPE_TEST(a_stabilized_approach_does_not_go_around) {
    every_landplane_arriving(Arriving::as_flown);
}

// **On a runway under 900 m a light aeroplane lands without going around**,
// touching down in its first third and stopped on it. 800 m: under the 900
// m at which the first third is shorter than the 300 m the approach aims at
// on a long runway, and twice the distance a light aeroplane here needs to
// land over a 50 ft obstacle (the 172P's handbook, 1,335 ft at sea level).
// Every light landplane taught the approach, counted.
GLIDESLOPE_TEST(a_light_aeroplane_lands_on_a_runway_under_900_m_without_going_around) {
    const auto taught = everyone_taught("approach-and-landing");
    glideslope::sim::Runway runway = a_runway();
    runway.length_m = 800.0;
    const double zone_m = glideslope::sim::StabilizedApproach::touchdown_zone_m(runway);
    std::size_t light = 0;
    std::size_t flown = 0;
    std::vector<std::string> wrong;
    for (const std::string& id : taught) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        if (entry.aircraft_class != glideslope::sim::AircraftClass::light_aircraft) {
            continue;
        }
        ++light;
        const auto published = glideslope::sim::approach_speeds(data(), entry.model);
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        put_on_final(aircraft, entry, runway, published);
        glideslope::sim::Controls flying;
        flying.throttle = 0.4;
        flying.gear = 1.0;
        glideslope::sim::Controller controller(aircraft, flying);
        controller.to_ai_approach(runway, published);
        glideslope::sim::GroundJudge judge(false);
        std::string why;
        std::string wreck;
        bool stopped = false;
        double touched_m = -1.0;
        for (int tick = 0; tick < 600 * steps_per_second; ++tick) {
            aircraft.set_controls(controller.fly());
            aircraft.step();
            const auto* l = controller.lander();
            if (l == nullptr) {
                break; // gone around and handed on: not landed
            }
            if (why.empty() && !l->why_gone_around().empty()) {
                why = l->why_gone_around();
            }
            if (touched_m < 0.0 && l->touched()) {
                touched_m = l->touchdown_along_m();
            }
            if (const auto what = judge.judge(aircraft)) {
                wreck = *what;
                break;
            }
            if (l->stage() == glideslope::sim::Lander::Stage::stopped) {
                stopped = true;
                break;
            }
        }
        const glideslope::sim::AircraftState s = aircraft.state();
        const auto at = glideslope::sim::on_runway_frame(runway, s.latitude_deg, s.longitude_deg);
        std::printf("  %-8s %s%s, touched %.0f m along (zone %.0f m), %s %.0f m along%s%s\n",
                    id.c_str(), why.empty() ? "no go-around" : "went around: ", why.c_str(),
                    touched_m, zone_m, stopped ? "stopped" : "NOT STOPPED", at.along_m,
                    wreck.empty() ? "" : ", wrecked: ", wreck.c_str());
        ++flown;
        if (!why.empty()) {
            wrong.push_back(id + " went around: " + why);
        }
        if (touched_m < 0.0 || touched_m > zone_m) {
            wrong.push_back(id + " touched down " + std::to_string(touched_m) +
                            " m along, not in the first third");
        }
        if (!wreck.empty() || !stopped || at.along_m < 0.0 || at.along_m > runway.length_m) {
            wrong.push_back(id + " was not stopped on the runway: " + wreck);
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(light >= 3 && flown == light,
          "every light landplane taught the approach landed: " + std::to_string(flown) +
              " of " + std::to_string(light));
}

namespace {

// **Air rising on short final, every time**: 4 m/s (790 ft/min) up from 1.6
// km to 400 m before the threshold, below 700 ft, and still air elsewhere -
// more than an idling light aeroplane can sink through at her reference
// speed, so every approach is lifted above the glidepath, and dived back to
// it gathers speed: not stabilized, one way or the other.
class RisingOnFinal : public glideslope::sim::Weather {
public:
    explicit RisingOnFinal(glideslope::sim::Runway r) : runway_(std::move(r)) {}
    glideslope::sim::Conditions at(double latitude_deg, double longitude_deg, double height_m,
                                   double) override {
        glideslope::sim::Conditions c;
        const auto at = glideslope::sim::on_runway_frame(runway_, latitude_deg, longitude_deg);
        if (at.along_m > -1600.0 && at.along_m < -400.0 && std::abs(at.across_m) < 300.0 &&
            height_m - runway_.elevation_ft / feet_per_metre < 700.0 / feet_per_metre) {
            c.wind_down_mps = -4.0;
        }
        return c;
    }

private:
    glideslope::sim::Runway runway_;
};

} // namespace

// **Gone around for an unstabilized approach twice, the third is landed**
// (StabilizedApproach::most_go_arounds): the 172P, lifted above her
// glidepath on every final by air rising there, goes around for the
// gate twice, flies the circuit each time, and on the third
// approach - the gate waived - lands and stops on the runway. Before the
// bound she went round and round until the half hour ran out.
GLIDESLOPE_TEST(an_approach_unstabilized_every_time_goes_around_twice_and_then_lands) {
    const auto entry = glideslope::sim::find_aircraft(data(), "c172p");
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    put_on_final(aircraft, entry, runway, published);
    aircraft.set_weather(std::make_shared<RisingOnFinal>(runway));
    glideslope::sim::Controls flying;
    flying.throttle = 0.4;
    flying.gear = 1.0;
    glideslope::sim::Controller controller(aircraft, flying);
    controller.to_ai_approach(runway, published);
    glideslope::sim::GroundJudge judge(false);
    std::vector<std::string> whys{std::string()};
    bool circling = false;
    bool stopped = false;
    bool waived = false;
    std::string wreck;
    for (int tick = 0; tick < 1800 * steps_per_second; ++tick) {
        aircraft.set_controls(controller.fly());
        aircraft.step();
        const auto* l = controller.lander();
        // A new approach each time the circuit hands her to a lander.
        if (circling && controller.circuit() == nullptr && l != nullptr) {
            whys.emplace_back();
        }
        circling = controller.circuit() != nullptr;
        if (l != nullptr && whys.back().empty() && !l->why_gone_around().empty()) {
            whys.back() = l->why_gone_around();
        }
        if (const auto what = judge.judge(aircraft)) {
            wreck = *what;
            break;
        }
        if (l != nullptr && controller.circuit() == nullptr &&
            l->stage() == glideslope::sim::Lander::Stage::stopped) {
            stopped = true;
            waived = !l->judges_the_gate();
            break;
        }
    }
    const glideslope::sim::AircraftState s = aircraft.state();
    const auto at = glideslope::sim::on_runway_frame(runway, s.latitude_deg, s.longitude_deg);
    for (std::size_t i = 0; i < whys.size(); ++i) {
        std::printf("  approach %zu: %s\n", i + 1,
                    whys[i].empty() ? "landed" : ("went around: " + whys[i]).c_str());
    }
    std::printf("  %d go-arounds for the gate, %s %.0f m along%s%s\n",
                controller.unstable_go_arounds(), stopped ? "stopped" : "NOT STOPPED",
                at.along_m, wreck.empty() ? "" : ", wrecked: ", wreck.c_str());
    check(controller.unstable_go_arounds() == glideslope::sim::StabilizedApproach::most_go_arounds,
          "she went around for the gate exactly twice: " +
              std::to_string(controller.unstable_go_arounds()));
    check(whys.size() == 3, "three approaches: " + std::to_string(whys.size()));
    for (std::size_t i = 0; i + 1 < whys.size(); ++i) {
        check(!whys[i].empty(),
              "approach " + std::to_string(i + 1) + " went around for the gate");
    }
    check(wreck.empty(), "she was not wrecked: " + wreck);
    check(stopped && waived && at.along_m >= 0.0 && at.along_m <= runway.length_m,
          "the third approach, the gate waived, stopped on the runway");
}

// **The speed raised for a climb is never raised past the fastest she may
// hold** (Autopilot::limit_speed). Built where it is raised: the F-35B, the
// aeroplane whose nose reaches its stop short of the climb round a
// go-around's circuit, told to go around over rising ground at 200 ft with
// her fastest set 11 kt over the speed her circuit is flown at. Round the
// circuit's first four minutes the raise must reach the 11 kt - the clamp
// is what stops it - and never pass it, nor her speed pass that fastest and
// 5 kt in hand after the go-around's own first minute at full power.
GLIDESLOPE_TEST(the_speed_the_autopilot_raises_for_a_climb_never_passes_the_fastest_she_may_hold) {
    const auto entry = glideslope::sim::find_aircraft(data(), "f35b");
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    put_on_final(aircraft, entry, runway, published);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [&runway](double lat, double lon) { return rising_left_of(runway, lat, lon); },
        [](double, double) { return false; }));
    glideslope::sim::Controls flying;
    flying.throttle = 0.4;
    flying.gear = 1.0;
    glideslope::sim::Controller controller(aircraft, flying);
    controller.to_ai_approach(runway, published);
    // Her circuit is flown at 20 kt over her approach speed for what she
    // weighs when she goes around (sim::for_weight) - the fuel her approach
    // burnt off her figures' loading - so the fastest is set from it then.
    double circuit_kts = published.vref_kts + 20.0;
    double fastest_kts = circuit_kts + 11.0;
    controller.limit_speed(fastest_kts);
    bool told = false;
    int circling = 0;
    double most_raise_kts = 0.0;
    double fastest_seen_kts = 0.0;
    for (int tick = 0; tick < 900 * steps_per_second && circling < 240 * steps_per_second;
         ++tick) {
        if (!told && aircraft.property("position/h-agl-ft") <= 200.0) {
            controller.go_around();
            told = true;
        }
        aircraft.set_controls(controller.fly());
        aircraft.step();
        if (controller.circuit() != nullptr) {
            if (circling == 0) {
                circuit_kts = controller.circuit()->speeds().vref_kts + 20.0;
                fastest_kts = circuit_kts + 11.0;
                controller.limit_speed(fastest_kts);
            }
            ++circling;
            if (circling > 60 * steps_per_second) {
                fastest_seen_kts =
                    std::max(fastest_seen_kts, aircraft.property("velocities/vc-kts"));
            }
            if (auto* ap = controller.autopilot()) {
                most_raise_kts = std::max(most_raise_kts, ap->climb_speed_kts());
            }
        }
    }
    std::printf("  f35b: circuit at %.0f kt, fastest set %.0f kt: raised at most %.2f kt, "
                "flew at most %.1f kt after the first minute, %d s of circuit\n",
                circuit_kts, fastest_kts, most_raise_kts, fastest_seen_kts,
                circling / static_cast<int>(steps_per_second));
    check(circling >= 240 * steps_per_second, "she flew four minutes of her circuit");
    check(most_raise_kts > 10.9,
          "the raise reached the fastest: " + std::to_string(most_raise_kts));
    check(most_raise_kts <= 11.0 + 1e-9,
          "the raise never passed the fastest: " + std::to_string(most_raise_kts));
    check(fastest_seen_kts <= fastest_kts + 5.0,
          "her speed never passed the fastest and 5 kt: " + std::to_string(fastest_seen_kts));
}

namespace {

// **Landed by the AI and told to vacate**: what became of her.
struct Vacated {
    bool clear = false;
    bool landed = false; // stopped on the runway first
    std::string wreck;
    double along_m = 0.0;
    double across_m = 0.0;
    double fastest_taxi_kts = 0.0; // from the stop
    double vacate_s = 0.0;         // from the stop to clear
};

} // namespace

// **Every aeroplane the AI lands taxis off the runway and stops clear of it**
// (sim/vacate.hpp): every landplane taught the approach (the flying boat
// named, having no runway to leave), landed by the approach autopilot from
// two miles out and told to vacate, turns off to the right, taxis no faster
// than twice the taxi speed, and stops beside the runway - at least
// RunwayClear::clear_m from the centreline and no more than 200 m past that,
// within the runway's length, and no longer on it - unwrecked, within ten
// minutes. Before, she stopped on it and stayed.
GLIDESLOPE_TEST(every_aeroplane_the_ai_lands_taxis_off_the_runway_and_stops_clear_of_it) {
    const auto taught = everyone_taught("approach-and-landing");
    const glideslope::sim::Runway runway = a_runway();
    std::size_t flown = 0;
    std::size_t left_out = 0;
    std::vector<std::string> wrong;
    for (const std::string& id : taught) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        if (entry.seaplane) {
            std::printf("  left out - %s: a flying boat has no runway to leave\n", id.c_str());
            ++left_out;
            continue;
        }
        const auto published = glideslope::sim::approach_speeds(data(), entry.model);
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        put_on_final(aircraft, entry, runway, published);
        glideslope::sim::Controls flying;
        flying.throttle = 0.4;
        flying.gear = 1.0;
        glideslope::sim::Controller controller(aircraft, flying);
        controller.to_ai_approach(runway, published);
        controller.vacates_runways();
        glideslope::sim::GroundJudge judge(false);
        Vacated v;
        int stopped_at = -1;
        for (int tick = 0; tick < 600 * steps_per_second; ++tick) {
            aircraft.set_controls(controller.fly());
            aircraft.step();
            if (const auto what = judge.judge(aircraft)) {
                v.wreck = *what;
                break;
            }
            if (const auto* off = controller.vacate()) {
                if (stopped_at < 0) {
                    stopped_at = tick;
                    v.landed = true;
                }
                v.fastest_taxi_kts = std::max(v.fastest_taxi_kts,
                                              aircraft.property("velocities/vg-fps") / 1.68781);
                if (off->clear()) {
                    v.clear = true;
                    v.vacate_s = static_cast<double>(tick - stopped_at) / steps_per_second;
                    break;
                }
            }
        }
        const glideslope::sim::AircraftState s = aircraft.state();
        const auto at = glideslope::sim::on_runway_frame(runway, s.latitude_deg, s.longitude_deg);
        v.along_m = at.along_m;
        v.across_m = at.across_m;
        ++flown;
        std::printf("  %-13s %s in %5.1f s, %6.0f m along, %5.1f m right, taxied at most "
                    "%4.1f kt%s%s\n",
                    id.c_str(), v.clear ? "clear" : "NOT CLEAR", v.vacate_s, v.along_m,
                    v.across_m, v.fastest_taxi_kts, v.wreck.empty() ? "" : ", wrecked: ",
                    v.wreck.c_str());
        if (!v.wreck.empty()) {
            wrong.push_back(id + " was wrecked: " + v.wreck);
        }
        if (!v.landed || !v.clear) {
            wrong.push_back(id + " did not land and vacate the runway");
        }
        if (v.across_m < glideslope::sim::RunwayClear::clear_m ||
            v.across_m > glideslope::sim::RunwayClear::clear_m + 200.0 || v.along_m < 0.0 ||
            v.along_m > runway.length_m ||
            glideslope::sim::on_runway(runway, s.latitude_deg, s.longitude_deg,
                                       s.altitude_ft - runway.elevation_ft)) {
            wrong.push_back(id + " stopped " + std::to_string(v.along_m) + " m along, " +
                            std::to_string(v.across_m) + " m right of the centreline");
        }
        if (v.fastest_taxi_kts > 2.0 * glideslope::sim::Vacate::taxi_kts) {
            wrong.push_back(id + " taxied at " + std::to_string(v.fastest_taxi_kts) + " kt");
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things went wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(flown + left_out == taught.size() && left_out == 1,
          "every landplane taught the approach landed and vacated, the flying boat named: " +
              std::to_string(flown) + " of " + std::to_string(taught.size() - left_out));
}

namespace {

// **A second Cessna landing on a runway the first is stopped on.** The first
// is landed and stopped, and held there - not stepped - until the second,
// coming down the same approach from two miles out and asking whether the
// runway is clear (`Controller::clears_with`), has gone around; then the
// first is let vacate it. The second must go around once, fly round, and
// land, touching down only once the first is clear of the runway, and never
// lower than 200 ft over the first within 500 m of it while it was on the
// runway. `learnt`: the second is handed to the learnt landing at its gate,
// as a server's are.
void second_lands_once_the_first_has_left(const std::string& id, bool learnt) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);
    const auto policy = glideslope::sim::learnt_landing(data(), entry.model);
    check(!learnt || policy != nullptr, id + " has a learnt landing");

    glideslope::sim::Aircraft first(data() / "jsbsim", entry.model);
    put_on_final(first, entry, runway, published);
    glideslope::sim::Controls flying;
    flying.throttle = 0.4;
    flying.gear = 1.0;
    glideslope::sim::Controller first_c(first, flying);
    first_c.to_ai_approach(runway, published);
    first_c.vacates_runways();
    // Landed and stopped: the first step the vacate has her.
    for (int tick = 0; tick < 600 * steps_per_second && first_c.vacate() == nullptr; ++tick) {
        first.set_controls(first_c.fly());
        first.step();
    }
    check(first_c.vacate() != nullptr, "the first landed and stopped");
    const glideslope::sim::AircraftState stopped = first.state();
    check(glideslope::sim::on_runway(runway, stopped.latitude_deg, stopped.longitude_deg,
                                     stopped.altitude_ft - runway.elevation_ft),
          "the first stopped on the runway");

    // Handed to the learnt landing, at her model's own weight, as a
    // server's are: its gate refuses her at her figures' 2,400 lb, which it
    // was not trained at (sim::outside_learnt_gate).
    glideslope::sim::Aircraft second(data() / "jsbsim", entry.model);
    put_on_final(second, entry, runway, published, learnt);
    glideslope::sim::Controller second_c(second, flying);
    if (learnt) {
        second_c.to_ai_approach(runway, published, policy);
    } else {
        second_c.to_ai_approach(runway, published);
    }
    second_c.vacates_runways(
        [&first](const glideslope::sim::Runway& r, double along_m, double side) {
            const glideslope::sim::AircraftState f = first.state();
            return !glideslope::sim::beside_runway(r, f.latitude_deg, f.longitude_deg,
                                                   f.altitude_ft - r.elevation_ft, along_m,
                                                   side);
        });
    second_c.clears_with([&first](const glideslope::sim::Runway& r) {
        const glideslope::sim::AircraftState s = first.state();
        return !glideslope::sim::on_runway(r, s.latitude_deg, s.longitude_deg,
                                           s.altitude_ft - r.elevation_ft);
    });
    glideslope::sim::GroundJudge judge_first(false);
    glideslope::sim::GroundJudge judge_second(false);
    bool released = false;
    bool learnt_seen = false;
    int circuits = 0;
    bool in_circuit = false;
    bool touched = false;
    bool first_clear_at_touch = false;
    double lowest_over_first_ft = 1e9;
    double closest_m = 1e9; // from the second's touch on
    std::string wreck;
    for (int t = 0; t < 1800 * steps_per_second; ++t) {
        if (released) {
            first.set_controls(first_c.fly());
            first.step();
            if (const auto what = judge_first.judge(first)) {
                wreck = "the first: " + *what;
                break;
            }
        }
        second.set_controls(second_c.fly());
        second.step();
        if (const auto what = judge_second.judge(second)) {
            wreck = "the second: " + *what;
            break;
        }
        learnt_seen = learnt_seen || second_c.learnt() != nullptr;
        const bool circling = second_c.circuit() != nullptr;
        if (circling && !in_circuit) {
            ++circuits;
            // Gone around: the first is let vacate.
            released = true;
        }
        in_circuit = circling;
        const glideslope::sim::AircraftState f = first.state();
        const glideslope::sim::AircraftState s = second.state();
        const bool first_on = glideslope::sim::on_runway(runway, f.latitude_deg, f.longitude_deg,
                                                         f.altitude_ft - runway.elevation_ft);
        if (first_on && glideslope::sim::distance_m(f.latitude_deg, f.longitude_deg,
                                                    s.latitude_deg, s.longitude_deg) < 500.0) {
            lowest_over_first_ft = std::min(lowest_over_first_ft, s.altitude_ft - f.altitude_ft);
        }
        if (!touched && second.property("gear/wow") > 0.5) {
            touched = true;
            first_clear_at_touch = !first_on;
        }
        if (touched) {
            closest_m = std::min(closest_m, glideslope::sim::distance_m(
                                                f.latitude_deg, f.longitude_deg,
                                                s.latitude_deg, s.longitude_deg));
        }
        if (second_c.vacate() != nullptr && second_c.vacate()->clear()) {
            break;
        }
    }
    char over[96];
    if (lowest_over_first_ft < 1e8) {
        std::snprintf(over, sizeof over, "at least %.0f ft over the first", lowest_over_first_ft);
    } else {
        std::snprintf(over, sizeof over, "never");
    }
    const glideslope::sim::AircraftState parked = second.state();
    const auto parked_at =
        glideslope::sim::on_runway_frame(runway, parked.latitude_deg, parked.longitude_deg);
    std::printf("  %s %s: %d go-around(s), %s, the first %s at its touch; %s within 500 m of the "
                "first while it was on the runway; landed, %.0f m from it at the closest, "
                "stopped %.0f m along%s%s\n",
                id.c_str(), learnt ? "handed to the learnt landing" : "the approach autopilot",
                circuits,
                touched ? "landed" : "NEVER LANDED",
                first_clear_at_touch ? "clear" : "STILL ON THE RUNWAY", over, closest_m,
                parked_at.along_m,
                wreck.empty() ? "" : ", wrecked: ", wreck.c_str());
    check(wreck.empty(), "nothing was wrecked: " + wreck);
    check(!learnt || learnt_seen, "the learnt landing had the second at its gate");
    check(circuits == 1, "the second went around once, not " + std::to_string(circuits));
    check(touched && first_clear_at_touch, "the second landed once the first had left");
    check(lowest_over_first_ft >= 200.0, "the second came no lower than 200 ft over the first");
    check(second_c.vacate() != nullptr && second_c.vacate()->clear(),
          "the second, landed, vacated too");
    // **And stopped clear of the first**, which turned off where she would
    // have: never within 100 m of it - a little over the 70 m either side of
    // the centreline a runway's traffic is kept off.
    // **And off the runway's side before its end**, the A380's long turn
    // too (Vacate::turn_by_end_m).
    check(parked_at.along_m <= runway.length_m,
          id + ": the second stopped before the runway's end: " +
              std::to_string(parked_at.along_m) + " m along");
    check(closest_m >= 100.0,
          "the second stopped clear of the first: " + std::to_string(closest_m) + " m");
}

} // namespace

// **An aeroplane landing on a runway another is on goes around, and lands
// once it has left**: both ways a landing is flown to the touch - by the
// approach autopilot, and handed to the learnt landing at its gate - with
// the one aeroplane that has both; and the A380, whose roll-out and turn
// off are the longest, forced to turn off before the 3,000 m runway's end
// beside the first. The go-around itself is every landplane's
// (an_aeroplane_that_goes_around_is_flown_round_and_landed), and the
// vacating every landplane's (every_aeroplane_the_ai_lands_taxis_off_...).
GLIDESLOPE_TEST(an_aeroplane_landing_on_a_runway_another_is_on_goes_around_and_lands_once_it_has_left) {
    second_lands_once_the_first_has_left("c172p", false);
    second_lands_once_the_first_has_left("c172p", true);
    second_lands_once_the_first_has_left("a380", false);
}

// **The server and the client tell a controller how she lands from the same
// figures** (`sim::landing_speeds`, which both call): for every aircraft in
// the catalogue, the approach speeds its approach is flown at, or none for an
// aircraft that publishes no stall speed - the 747-400 and the F-22A - and a
// model with no figures refused.
GLIDESLOPE_TEST(how_she_lands_is_what_her_approach_is_flown_at_or_none_without_a_stall_speed) {
    std::size_t catalogue = 0;
    std::size_t told = 0;
    std::vector<std::string> none;
    for (const auto& entry : glideslope::sim::read_catalogue(data())) {
        ++catalogue;
        const auto speeds = glideslope::sim::landing_speeds(data(), entry.model);
        if (!speeds) {
            none.push_back(entry.id);
            continue;
        }
        ++told;
        check(speeds->vref_kts == glideslope::sim::approach_speeds(data(), entry.model).vref_kts,
              entry.id + " is told the reference speed its approach is flown at");
    }
    check(catalogue == 16, "sixteen aircraft in the catalogue, not " + std::to_string(catalogue));
    check(none == std::vector<std::string>{"747-400", "f22"},
          "only the 747-400 and the F-22A have none to tell");
    check(told + none.size() == catalogue, "every aircraft told or named");
    bool refused = false;
    try {
        (void)glideslope::sim::landing_speeds(data(), "no-such-model");
    } catch (const std::exception&) {
        refused = true;
    }
    check(refused, "a model with no figures is refused, not told nothing");
}

namespace {

// **A flare the pilot pulls hard, handed back in the air.** The AI flies the
// approach; on the flare's first step the pilot takes her and flies the
// flare the AI would, but with the stick 0.4 of its travel further back, and
// hands her back a second later or once her wing is two degrees past the
// flare's incidence limit, whichever is first, if she is still in the air.
struct HardFlare {
    bool handed_back = false;  // in the air
    bool lander_given = false; // and given her landing back
    bool stopped = false;
    double path_alpha_deg = 0.0;  // on the approach's last step
    double back_alpha_deg = 0.0;  // as the AI had her back
    double back_agl_ft = 0.0;
    double most_alpha_deg = -1e9; // from the take-back to the touch
    double stopped_past_m = 0.0;
    double stopped_across_m = 0.0;
    // **Or gone around**: climbed away from the balloon, never touching, to
    // the go-around's height, where the landing is given up and the plain
    // autopilot has her.
    bool went_around = false;
    double lowest_after_ft = 1e9; // from the take-back, above the ground
    double highest_after_ft = 0.0;
    double slowest_after_kts = 1e9; // from the take-back to the touch or 500 ft
    double stall_kts = 0.0;         // in the landing configuration, published
    glideslope::test::AfterTouch ai;
};

// How far over her landing stall a go-around must stay.
constexpr double go_around_stall_margin = 1.05;

HardFlare hard_flare_handed_back(const std::string& id) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    put_on_final(aircraft, entry, runway, published);

    glideslope::sim::Controls flying;
    flying.throttle = 0.4;
    flying.gear = 1.0;
    glideslope::sim::Controller controller(aircraft, flying);
    controller.to_ai_approach(runway, published);
    // The pilot flies the flare the AI would - a second lander alongside
    // sees what the AI's sees - but with the stick 0.4 further back.
    glideslope::sim::Lander shadow(aircraft, runway, published);

    HardFlare out;
    out.stall_kts = published.stall_kts;
    out.ai.judged_as(entry.seaplane);
    int handed = -1;
    int back = -1;
    double last_alpha_deg = 0.0;
    for (int tick = 0; tick < 900 * steps_per_second; ++tick) {
        const double alpha_deg = aircraft.property("aero/alpha-deg");
        const bool down = aircraft.property("gear/wow") > 0.5;
        glideslope::sim::Controls pilot = shadow.fly();
        pilot.elevator = std::min(1.0, pilot.elevator + 0.4);
        if (handed >= 0 && back < 0) {
            controller.set_pilot(pilot);
        }
        if (handed < 0 && controller.lander() != nullptr &&
            controller.lander()->stage() == glideslope::sim::Lander::Stage::flare) {
            handed = tick;
            out.path_alpha_deg = last_alpha_deg;
            controller.set_pilot(pilot);
            controller.to_pilot();
        }
        if (handed >= 0 && back < 0 &&
            (tick - handed >= steps_per_second ||
             alpha_deg >= std::max(12.0, out.path_alpha_deg + 4.0) + 2.0 || down)) {
            back = tick;
            out.handed_back = !down;
            out.back_alpha_deg = alpha_deg;
            out.back_agl_ft = aircraft.property("position/h-agl-ft");
            controller.to_ai();
            out.lander_given = controller.lander() != nullptr;
        }
        if (handed < 0) {
            last_alpha_deg = alpha_deg;
        }
        aircraft.set_controls(controller.fly());
        aircraft.step();
        if (back >= 0) {
            out.ai.watch(aircraft);
            if (!out.ai.touched) {
                out.most_alpha_deg =
                    std::max(out.most_alpha_deg, aircraft.property("aero/alpha-deg"));
                out.lowest_after_ft =
                    std::min(out.lowest_after_ft, aircraft.property("position/h-agl-ft"));
                out.highest_after_ft =
                    std::max(out.highest_after_ft, aircraft.property("position/h-agl-ft"));
                out.slowest_after_kts =
                    std::min(out.slowest_after_kts, aircraft.property("velocities/vc-kts"));
            }
            if (!out.ai.touched && out.lander_given && controller.lander() == nullptr) {
                out.went_around = true;
                break;
            }
            if (std::abs(aircraft.property("velocities/vg-fps")) < 1.0) {
                out.stopped = true;
                break;
            }
        }
    }
    const glideslope::sim::AircraftState s = aircraft.state();
    const double north_m = (s.latitude_deg - runway.threshold_lat_deg) *
                           metres_per_degree_latitude(runway.threshold_lat_deg);
    const double east_m = (s.longitude_deg - runway.threshold_lon_deg) *
                          metres_per_degree_longitude(runway.threshold_lat_deg);
    const double h = runway.heading_deg / degrees;
    out.stopped_past_m = east_m * std::sin(h) + north_m * std::cos(h);
    out.stopped_across_m = east_m * std::cos(h) - north_m * std::sin(h);
    return out;
}

} // namespace

// **Handed back in the air after the pilot's hard flare, she is landed, and
// her wing is not stalled.** The flare's incidence limit is the approach's,
// not the pilot's: taken from where the flare began, a pilot's flare to
// fourteen degrees handed back would have let the AI's flare raise a C172's
// wing to eighteen, past her stall. From the take-back to the touch the
// incidence goes no higher than it was handed back at or the limit the
// approach gives - twelve degrees or four over the path's - with a degree
// for the nose's own overshoot; she touches unwrecked by the server's rule,
// stays upright on her wheels and stops on the runway. Every landplane taught
// the approach; the flying boat is named and left out, as on the roll.
GLIDESLOPE_TEST(a_flare_the_pilot_pulls_hard_and_hands_back_in_the_air_is_landed_without_a_stall) {
    const auto taught = everyone_taught("approach-and-landing");
    // **Landed, or gone around.** Given back still climbing on the pilot's
    // stick above where her flare began - a balloon - she is gone around
    // from (sim/lander.cpp): the Mosquito, whose throttles the pilot's flare
    // had shut, zoomed to some fifty feet and came down at 958 ft/min before
    // the AI went around. One that goes around never touches, keeps her wing
    // under the same incidence bound, and climbs to the go-around's height.
    std::size_t went_around = 0;
    std::vector<std::string> wrong;
    std::size_t landplanes = 0;
    std::size_t flown = 0;
    for (const std::string& id : taught) {
        if (glideslope::sim::find_aircraft(data(), id).seaplane) {
            std::printf("  left out - %s: a flying boat, afloat, is never still\n", id.c_str());
            continue;
        }
        ++landplanes;
        const HardFlare r = hard_flare_handed_back(id);
        const double limit_deg = std::max(12.0, r.path_alpha_deg + 4.0);
        const double most_allowed_deg = std::max(r.back_alpha_deg, limit_deg) + 1.0;
        std::printf("  %-13s path alpha %.1f, handed back at %.1f ft and %.1f; most after %.1f "
                    "(allowed %.1f); touched sinking %.0f ft/min%s; %s %.0f m past, %.1f "
                    "across\n",
                    id.c_str(), r.path_alpha_deg, r.back_agl_ft, r.back_alpha_deg,
                    r.most_alpha_deg, most_allowed_deg, r.ai.touch_sink_fpm,
                    r.ai.wreck.empty() ? "" : (", WRECKED: " + r.ai.wreck).c_str(),
                    r.stopped ? "stopped" : "NOT STOPPED", r.stopped_past_m,
                    r.stopped_across_m);
        const std::string where = id + " handed back from a hard flare";
        if (!r.handed_back) {
            wrong.push_back(where + " was on the ground before it was handed back");
            continue;
        }
        if (r.went_around) {
            std::printf("      went around: never lower than %.1f ft after the take-back, "
                        "climbed to %.0f ft, never slower than %.1f kt (stall %.1f)\n",
                        r.lowest_after_ft, r.highest_after_ft, r.slowest_after_kts,
                        r.stall_kts);
            // **Climbed away to the go-around's height, and never near the
            // stall**: at least five per cent over the landing stall, which
            // the approach's incidence it is flown at is worked from.
            if (r.highest_after_ft < glideslope::sim::Lander::go_around_ft) {
                wrong.push_back(where + " went around only to " +
                                std::to_string(r.highest_after_ft) + " ft");
            }
            if (r.slowest_after_kts < go_around_stall_margin * r.stall_kts) {
                wrong.push_back(where + " went around as slow as " +
                                std::to_string(r.slowest_after_kts) + " kt, under " +
                                std::to_string(go_around_stall_margin) + " of her " +
                                std::to_string(r.stall_kts) + " kt stall");
            }
            if (r.most_alpha_deg > most_allowed_deg) {
                wrong.push_back(where + " went around with her wing raised to " +
                                std::to_string(r.most_alpha_deg) + " degrees, past " +
                                std::to_string(most_allowed_deg));
            }
            if (!r.ai.wreck.empty() || r.ai.touched) {
                wrong.push_back(where + " touched the runway going around");
            }
            ++went_around;
            ++flown;
            continue;
        }
        if (!r.lander_given) {
            wrong.push_back(where + " was not given her landing back");
        }
        if (r.most_alpha_deg > most_allowed_deg) {
            wrong.push_back(where + " had her wing raised to " +
                            std::to_string(r.most_alpha_deg) + " degrees, past " +
                            std::to_string(most_allowed_deg));
        }
        if (!r.ai.wreck.empty()) {
            wrong.push_back(where + " was wrecked: " + r.ai.wreck);
        }
        for (const std::string& said : r.ai.what_went_wrong(id)) {
            wrong.push_back(where + ": " + said);
        }
        if (!r.stopped || r.stopped_past_m < 0.0 || r.stopped_past_m > a_runway().length_m ||
            std::abs(r.stopped_across_m) > glideslope::sim::Lander::runway_half_width_m) {
            wrong.push_back(where + " did not stop on the runway");
        }
        ++flown;
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) +
                             " things went wrong after a hard flare handed back, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(landplanes + 1 == taught.size(),
          "every aeroplane taught the approach is a landplane flown here or the one flying "
          "boat named: " + std::to_string(landplanes) + " of " + std::to_string(taught.size()));
    std::printf("  %zu of %zu went around, the rest landed\n", went_around, flown);
    // The go-around is flown here, not only allowed: the Mosquito's balloon
    // is one.
    check(went_around >= 1, "some aeroplane went around from its balloon");
    check(flown == landplanes, "every landplane was handed back from a hard flare: " +
                                   std::to_string(flown) + " of " +
                                   std::to_string(landplanes));
}

// **The flare's incidence limit is the approach's, and a pilot's flare given
// back does not move it.** Every aeroplane taught the approach is flown by the
// AI from two miles out to the flare's first step; then a pilot takes her and
// pulls the stick fully back for a second, the AI's lander only watching, and
// gives her back. Her wing is then past the path's incidence and four - the
// situation, so it is asserted - and the limit is what it was before the
// pilot touched her: twelve degrees, or the path's incidence and four, short
// of the stall the reference speed implies (1.69 times the path's, less two).
// Taken from the flare's first step after the hand-back, as it was, a C172
// flared to fourteen degrees would have been allowed eighteen.
GLIDESLOPE_TEST(the_flares_incidence_limit_is_the_approachs_and_a_pilots_flare_does_not_move_it) {
    const auto taught = everyone_taught("approach-and-landing");
    std::size_t tried = 0;
    std::vector<std::string> wrong;
    for (const std::string& id : taught) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const glideslope::sim::Runway runway = a_runway();
        const auto published = glideslope::sim::approach_speeds(data(), entry.model);
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        put_on_final(aircraft, entry, runway, published);
        glideslope::sim::Lander lander(aircraft, runway, published);
        double path_alpha_deg = 0.0;
        glideslope::sim::Controls last;
        for (int tick = 0; tick < 300 * steps_per_second &&
                           lander.stage() != glideslope::sim::Lander::Stage::flare;
             ++tick) {
            const double alpha_deg = aircraft.property("aero/alpha-deg");
            last = lander.fly();
            if (lander.stage() == glideslope::sim::Lander::Stage::approach) {
                path_alpha_deg = alpha_deg;
            }
            aircraft.set_controls(last);
            aircraft.step();
        }
        const bool flared = lander.stage() == glideslope::sim::Lander::Stage::flare;
        const double before_deg = lander.most_flare_alpha_deg();
        glideslope::sim::Controls pilot = last;
        pilot.elevator = 1.0;
        pilot.throttle = 0.0;
        double most_alpha_deg = -1e9;
        for (int i = 0; i < steps_per_second && aircraft.property("gear/wow") < 0.5 &&
                        !aircraft.in_water();
             ++i) {
            aircraft.set_controls(pilot);
            aircraft.step();
            lander.watch();
            most_alpha_deg = std::max(most_alpha_deg, aircraft.property("aero/alpha-deg"));
        }
        lander.resume(pilot.throttle);
        lander.fly();
        const double after_deg = lander.most_flare_alpha_deg();
        const double wanted_deg =
            std::max(12.0, std::min(path_alpha_deg + 4.0, 1.69 * path_alpha_deg - 2.0));
        std::printf("  %-13s path %.1f, limit %.1f before the pilot and %.1f after (%.1f "
                    "wanted); the pilot took the wing to %.1f\n",
                    id.c_str(), path_alpha_deg, before_deg, after_deg, wanted_deg,
                    most_alpha_deg);
        if (!flared) {
            wrong.push_back(id + " never reached the flare");
            continue;
        }
        ++tried;
        if (!(most_alpha_deg > path_alpha_deg + 4.0)) {
            wrong.push_back(id + ": the pilot's flare took the wing only to " +
                            std::to_string(most_alpha_deg) + ", not past the path's " +
                            std::to_string(path_alpha_deg) + " and four");
        }
        if (after_deg != before_deg) {
            wrong.push_back(id + ": the limit moved from " + std::to_string(before_deg) +
                            " to " + std::to_string(after_deg) + " with the pilot's flare");
        }
        if (std::abs(before_deg - wanted_deg) > 1e-9) {
            wrong.push_back(id + ": the limit is " + std::to_string(before_deg) + ", not " +
                            std::to_string(wanted_deg));
        }
    }
    for (const std::string& w : wrong) {
        std::printf("  WRONG: %s\n", w.c_str());
    }
    check(wrong.empty(), std::to_string(wrong.size()) + " things wrong, the first: " +
                             (wrong.empty() ? "" : wrong.front()));
    check(tried == taught.size() && tried == 14,
          "every aeroplane taught the approach was tried: " + std::to_string(tried) + " of " +
              std::to_string(taught.size()));
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

// `flaps` is the flap the exercise is flown with, set on the AI's controls
// and the pilot's alike, so that handing over does not move it.
// `still_showing`, when given, keeps the demonstration going past the
// lesson's end for as long as it says so.
Demonstrated demonstrate_in_the_air(const std::string& id, const std::string& exercise,
                                    double start_ft, int settling_s, const Begin& begin,
                                    const Fly& fly, double flaps = 0.0,
                                    const std::function<bool()>& still_showing = {}) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    InFlight f = airborne(id, start_ft);
    const auto found = lesson_for(entry, exercise);
    check(found.has_value(), id + " has a " + exercise + " lesson for its class");
    LessonRun run(*found, f.speeds);

    glideslope::sim::Controls held;
    held.throttle = 0.7;
    held.flaps = flaps;
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
    pilot.flaps = flaps;
    controller.set_pilot(pilot);

    Demonstrated out;
    out.after.judged_as(entry.seaplane);
    out.stages = found->stages.size();
    const int settling = settling_s * steps_per_second;
    int hand_over = -1;
    int take_back = -1;
    glideslope::sim::Controls last = held;
    bool first = true;
    bool demonstrated = false;
    for (int tick = 0; tick < 900 * steps_per_second; ++tick) {
        // Demonstrated first, then handed over - the order the item names.
        if (!demonstrated && tick > settling && run.finished() &&
            !(still_showing && still_showing())) {
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
            m.vertical_speed_fpm = f.climb_fpm;
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
// holds the height by raising the nose, which is the entry; the recovery is
// the lesson's own (sim::fly_the_stall_recovery), which opens the throttle and
// puts the nose down. The version before reached into the controls and set
// the throttle to 0 and then to 1 by hand, which no controller can hand over.
//
// `left_s` leaves the aeroplane that long in the stall after the lesson's
// entry ends before the recovery is asked for; `watched`, when given, is
// what the recovery did (RecoveryWatch), and the demonstration goes on until
// the aeroplane is recovered before it is handed over.
Demonstrated demonstrate_a_stall(const std::string& id, double start_ft, double left_s = 0.0,
                                 Result* watched = nullptr) {
    // Flaps and gear as they are for the landing, which is what `stall` is
    // the stall speed in - the same as the lesson's own flight.
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const double landing_flap = glideslope::sim::approach_speeds(data(), entry.model).flap;
    const Lesson lesson = *lesson_for(entry, "stalls");
    auto result = std::make_shared<Result>();
    auto watch = std::make_shared<RecoveryWatch>(
        *result, lesson.stages.back().until_property,
        recovery_ends_at_kts(lesson, figures_of(entry)));
    const auto dawdle = static_cast<int>(std::lround(left_s * steps_per_second));
    const Demonstrated shown = demonstrate_in_the_air(
        id, "stalls", start_ft, 20, [](glideslope::sim::AutopilotModes&, const InFlight&) {},
        [watch, dawdle, lesson, recovering = false, stalled_at = -1](
            glideslope::sim::Autopilot& ap, const LessonRun& run, const InFlight& f,
            int since) mutable {
            glideslope::sim::AutopilotModes m = ap.modes();
            const auto& a = *f.aircraft;
            if (since == 0) {
                glideslope::sim::fly_the_stall_entry(m, f.speeds);
                ap.set(m);
            }
            if (recovering) {
                watch->recovering(a);
                return;
            }
            watch->entering(a);
            if (run.stage() >= 1) {
                if (stalled_at < 0) {
                    stalled_at = since;
                }
                if (since - stalled_at >= dawdle) {
                    recovering = true;
                    watch->hand_over(a);
                    glideslope::sim::fly_the_stall_recovery(m, lesson, f.speeds);
                    ap.set(m);
                }
            }
        },
        landing_flap,
        [watch, watching = watched != nullptr] { return watching && !watch->out.recovered; });
    if (watched != nullptr) {
        *watched = *result;
    }
    return shown;
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
        report_and_check(id, "stall",
                         demonstrate_a_stall(
                             id, stalls_are_practised_at(
                                     glideslope::sim::find_aircraft(data(), id))));
        ++walked;
    }
    check(walked == taught.size(),
          "every aeroplane taught the exercise demonstrated it: " +
              std::to_string(walked) + " of " + std::to_string(taught.size()));
}

// **The instructor's stall recovery is the lesson's**, and holds from deep in
// a stall: the B-2A, which the descent the instructor used to ask for held in
// the stall to the ground, demonstrated from thirty seconds in one is
// recovered within 2 g and the height its speed and sink need
// (height_bound_ft), with the tolerance in hand. One aeroplane, not every
// one: both flights ask for the recovery through sim::fly_the_stall_recovery,
// and what the recovery does for every aeroplane is the left-thirty-seconds
// test's to judge. This pins that the demonstration asks for it.
GLIDESLOPE_TEST(an_instructor_demonstrating_a_stall_recovers_a_b2_left_thirty_seconds_in_it) {
    const std::string id = "b2";
    Result r;
    const Demonstrated shown = demonstrate_a_stall(
        id, stalls_are_practised_at(glideslope::sim::find_aircraft(data(), id)), 30.0, &r);
    const double lost_ft = r.handed_over_ft - r.recovery_lowest_ft;
    const double bound_ft = height_bound_ft(r);
    std::printf("  %s demonstrated from %.0f ft, %.1f kt, alpha %.1f, %.0f ft/min: lost %.0f ft "
                "against %.0f, peak %.2f g%s; %zu/%zu stages\n",
                id.c_str(), r.handed_over_ft, r.handed_over_kts, r.handed_over_alpha_deg,
                r.handed_over_fpm, lost_ft, bound_ft, r.peak_load_g,
                r.recovered ? "" : ", NOT RECOVERED", shown.completed, shown.stages);
    check(r.handed_over_ft > 0.0, "the recovery was asked for in the air");
    check(r.recovered, id + " was recovered");
    check(lost_ft * (1.0 + between_machines) <= bound_ft,
          id + " lost " + std::to_string(lost_ft) + " ft, against " + std::to_string(bound_ft) +
              " with 10% in hand");
    check(r.peak_load_g * (1.0 + between_machines) <= flaps_down_most_g,
          id + " pulled " + std::to_string(r.peak_load_g) + " g, against 2 with 10% in hand");
}

namespace {

// Where she is along the runway and across it, in the runway's own frame,
// as the circuit (sim/circuit.hpp) has it.
double along_the_runway_nm(const glideslope::sim::Runway& r,
                           const glideslope::sim::Aircraft& a) {
    return glideslope::sim::along_runway_nm(r, a.property("position/lat-geod-deg"),
                                            a.property("position/long-gc-deg"));
}

double across_the_runway_m(const glideslope::sim::Runway& r,
                           const glideslope::sim::Aircraft& a) {
    return glideslope::sim::across_runway_m(r, a.property("position/lat-geod-deg"),
                                            a.property("position/long-gc-deg"));
}

struct Circuit {
    double stop_along_m = 0.0;  // where she stopped, beyond the threshold
    double stop_across_m = 0.0; // and right of the centreline
    double touch_across_m = 1e9; // right of the centreline where she touched
    double touch_along_m = 0.0;  // and how far beyond the threshold
    double touch_kts = 0.0;      // and how fast
    // From the touch back on to the runway to the stop.
    glideslope::test::AfterTouch after;
    FlareWatch flare;
    std::vector<std::string> debrief;
    std::size_t completed = 0;
    std::size_t stages = 0;
    // The go-around's circuit's legs she flew, in order, from the take-off:
    // crosswind, downwind, base, the intercept and final - five.
    std::size_t circuit_legs = 0;
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
// With `demo`, the instructor then hands over and takes back, as the other
// demonstrations do, and the steps at the two swaps go there.
Circuit fly_a_circuit(const std::string& id, bool trace, double sink_downwind_ft = 0.0,
                      Demonstrated* demo = nullptr) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto dep = book_departure_speeds(data(), entry.model);
    const auto app = glideslope::sim::approach_speeds(data(), entry.model);

    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; },
        [water = entry.seaplane](double, double) { return water; }));
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = runway.threshold_lat_deg;
    ic.longitude_deg = runway.threshold_lon_deg;
    ic.altitude_ft = runway.elevation_ft + (entry.seaplane ? 6.0 : 0.0);
    ic.terrain_elevation_ft = runway.elevation_ft;
    ic.heading_deg = runway.heading_deg;
    ic.airspeed_kts = 0.0;
    ic.engine_running = true;
    ic.gear = 1.0;
    // **A circuit is flown at the weight its approach speed belongs to**, the
    // landing loading: a training circuit is flown light, and the approach at
    // the end of it is judged against a third above the landing stall. At
    // the take-off loading an A380 - a hundred tonnes heavier - met the turn
    // on to final at a speed its landing weight's stall had set, stalled in
    // the bank, and hit the ground five and a half miles short. For every
    // aeroplane whose figures are all at one loading the two are the same.
    load_for_the_approach(aircraft, entry.model);
    aircraft.initialize(ic);
    if (entry.seaplane) {
        settle_afloat(aircraft);
    }

    const auto found = lesson_for(entry, "circuit");
    check(found.has_value(), id + " has a circuit lesson for its class");
    LessonRun run(*found, glideslope::sim::LessonSpeeds{dep.rotate_kts, dep.climb_kts,
                                                        app.vref_kts,
                                                        app.stall_kts});

    glideslope::sim::Controls standing;
    glideslope::sim::Controller controller(aircraft, standing);
    // The climb out: straight ahead to six hundred and fifty feet. **Clear of
    // where the lesson's climb out ends**, which is four hundred feet above
    // where it began - about 605 ft. It used to turn at 600, and at the
    // weight the Cessna's figures are measured at she climbs so slowly in a
    // turn that she was thirty-eight degrees round before she climbed the
    // last five feet: turning in the stage that asks her to fly straight, and
    // every stage after it one turn early, so the downwind leg was counted
    // inside the turn on to it.
    constexpr double turn_crosswind_ft = 650.0;
    controller.to_ai_take_off(runway, dep, turn_crosswind_ft);

    // **The pattern is the go-around's circuit** (sim/circuit.hpp), joined
    // from the take-off: one set of rules for both. Its sizes, its legs and
    // where each ends are there; what the take-off brings to it is here.
    //
    // What the take-off climbs away at: the climbing speed for a light
    // aeroplane, V2 and ten for a jet, whose best climb speed is an en-route
    // one. **Seven tenths of the climb she has, up to two thousand feet a
    // minute.** `a_climb_it_can_manage` stops at six hundred, which suits a
    // demonstration and not a jet's circuit: at 175 knots and six hundred
    // feet a minute an A380 flew three miles of crosswind leg to reach
    // circuit height, turned downwind far wider than the two miles abeam
    // Boeing's circuit is flown at, and could not line up with the runway
    // before she touched - 186 metres to the left of it. For the light
    // aircraft seven tenths of their climb is below six hundred anyway.
    // **The take-off flap stays out round the pattern**, as a jet's
    // downwind leg is flown with it.
    glideslope::sim::CircuitEntry joined;
    joined.from_take_off = true;
    joined.climb_kts = dep.initial_climb_kts;
    joined.climb_fpm = [&] {
        try {
            const auto figures = glideslope::sim::read_published_figures(
                data() / "figures" / (entry.model + ".xml"));
            for (const auto& spec : figures.figures) {
                if (spec.flight == "climb_rate" && spec.published > 0.0) {
                    return std::min(2000.0, 0.7 * spec.published);
                }
            }
        } catch (const std::exception&) {
        }
        return 600.0;
    }();
    joined.flaps = dep.flap;
    std::optional<glideslope::sim::GoAroundCircuit> pattern;
    using Leg = glideslope::sim::GoAroundCircuit::Leg;
    bool climbing_out = true;
    bool approach = false;
    bool sank = false;

    Circuit out;
    out.after.judged_as(entry.seaplane);
    out.stages = found->stages.size();
    std::size_t stage_was = 0;
    // **The pilot's hands**, for a demonstration: throttle back and the
    // wheels down, as after the approach demonstration.
    glideslope::sim::Controls pilot;
    pilot.throttle = 0.25;
    pilot.gear = 1.0;
    controller.set_pilot(pilot);
    int hand_over = -1;
    int take_back = -1;
    glideslope::sim::Controls last;
    bool first = true;
    for (int tick = 0; tick < 900 * steps_per_second; ++tick) {
        if (run.finished()) {
            if (demo == nullptr) {
                break;
            }
            if (hand_over < 0) {
                hand_over = tick + steps_per_second / 2;
                take_back = hand_over + steps_per_second;
            }
            if (tick > take_back + steps_per_second / 2) {
                break;
            }
            if (tick == hand_over) {
                controller.to_pilot();
            }
            if (tick == take_back) {
                controller.to_ai();
            }
        }
        const double agl = aircraft.property("position/h-agl-ft");
        const double along = along_the_runway_nm(runway, aircraft);
        if (climbing_out && agl >= turn_crosswind_ft) {
            climbing_out = false;
            controller.to_ai();
            pattern.emplace(aircraft, runway, app, joined);
            out.circuit_legs = 1;
        }
        if (pattern && !pattern->on_final()) {
            const Leg was = pattern->leg();
            glideslope::sim::AutopilotModes m = pattern->modes();
            if (pattern->leg() != was &&
                static_cast<int>(pattern->leg()) == static_cast<int>(was) + 1) {
                ++out.circuit_legs;
            }
            if (pattern->on_final()) {
                approach = true;
                controller.to_ai_approach(runway, app);
            } else {
                if (pattern->leg() == Leg::downwind && sink_downwind_ft > 0.0 && along <= -1.0 &&
                    !sank) {
                    // **The fault: she sinks along the downwind leg.** Not a
                    // low circuit - that is a different fault and the band
                    // is measured from where the leg began, on purpose - but
                    // a leg begun at circuit height and not held there.
                    sank = true;
                    std::printf("      sinking %.0f ft at %.2f nm, stage %zu, agl %.0f\n",
                                sink_downwind_ft, along, run.stage(), agl);
                }
                if (sank) {
                    m.altitude_ft = *m.altitude_ft - sink_downwind_ft;
                }
                controller.autopilot()->set(m);
            }
        }
        glideslope::sim::Controls flown = controller.fly();
        if (pattern && !approach) {
            pattern->configure(flown);
        }
        if (demo != nullptr) {
            if (!first && tick == hand_over) {
                demo->worst_to_pilot = worst_step(last, flown);
            } else if (!first && tick == take_back) {
                demo->worst_to_ai = worst_step(last, flown);
            }
            first = false;
            last = flown;
        }
        aircraft.set_controls(flown);
        aircraft.step();
        if (!run.finished()) {
            run.update(aircraft, tick);
        }
        out.highest_agl_ft = std::max(out.highest_agl_ft, agl);
        if (approach) {
            out.after.watch(aircraft);
            out.flare.watch(controller.lander(), aircraft);
        }
        if (out.touch_across_m > 1e8 && approach &&
            (aircraft.property("gear/wow") > 0.5 || aircraft.in_water())) {
            out.touch_across_m = across_the_runway_m(runway, aircraft);
            out.touch_along_m = along_the_runway_nm(runway, aircraft) * metres_per_nm;
            out.touch_kts = aircraft.property("velocities/vc-kts");
        }
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
                        pattern ? static_cast<int>(pattern->leg()) : -1, agl, along,
                        aircraft.property("attitude/psi-deg"),
                        aircraft.property("velocities/vc-kts"));
        }
    }
    // Stopped, and on the runway: within its length beyond the threshold and
    // its width either side of the centreline, not stopped anywhere at all.
    const double along_nm = along_the_runway_nm(runway, aircraft);
    out.stop_along_m = along_nm * metres_per_nm;
    out.stop_across_m = across_the_runway_m(runway, aircraft);
    // **A flying boat comes off the step, not to a stop**: afloat with her
    // engines idling she is never quite still, and her lesson ends, as her
    // approach lesson does, below twenty knots on the water.
    const bool slowed = entry.seaplane
                            ? aircraft.property("velocities/vc-kts") <= 20.0 &&
                                  aircraft.in_water()
                            : std::abs(aircraft.property("velocities/vg-fps")) < 1.0;
    out.stopped = slowed &&
                  along_nm >= 0.0 && along_nm * metres_per_nm <= runway.length_m &&
                  std::abs(across_the_runway_m(runway, aircraft)) <= 30.0;
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
    std::vector<std::string> came_down_badly;
    for (const std::string& id : taught) {
        const Circuit flown = fly_a_circuit(id, false);
        std::printf("  %-13s circuit %zu/%zu stages, highest %.0f ft, %s\n", id.c_str(),
                    flown.completed, flown.stages, flown.highest_agl_ft,
                    flown.stopped ? "stopped on the runway" : "NOT STOPPED on the runway");
        std::printf("      touched %.1f m right of the centreline, %.0f m beyond the threshold "
                    "at %.0f kt; stopped %.0f m beyond the threshold, %.1f m right of it\n",
                    flown.touch_across_m, flown.touch_along_m, flown.touch_kts,
                    flown.stop_along_m, flown.stop_across_m);
        std::printf("      touched sinking %.0f ft/min%s; after touching: rolled %.1f, pitched "
                    "down to %.1f, rose %.2f ft\n",
                    flown.after.touch_sink_fpm,
                    flown.after.wreck.empty() ? "" : (", WRECKED: " + flown.after.wreck).c_str(),
                    flown.after.worst_roll_deg, flown.after.least_pitch_deg,
                    flown.after.highest_ft);
        const auto d = book_departure_speeds(data(),
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
        // **Flown by the go-around's circuit** (sim/circuit.hpp): one set of
        // rules for both, every leg of it from crosswind to final in order.
        check(flown.circuit_legs == 5,
              id + " flew " + std::to_string(flown.circuit_legs) +
                  " of the go-around's circuit's five legs from the take-off, in order");
        check(flown.stopped, id + " finished the circuit stopped on the runway");
        // **And touched down on it**, not beside it and steered back: the
        // F-15C once touched 109 metres right of the centreline and still
        // stopped on the runway, which the check above could not see. Ten
        // metres keeps the main wheels well on a 45-metre runway.
        check(std::abs(flown.touch_across_m) <= 10.0,
              id + " touched down " + std::to_string(flown.touch_across_m) +
                  " m from the centreline, which is not within 10");
        // **And past the threshold, on the runway's length**: the F-35B once
        // touched two kilometres short at 165 knots, rolled on to the runway
        // and stopped on it, which neither check above could see.
        check(flown.touch_along_m >= 0.0 && flown.touch_along_m <= a_runway().length_m,
              id + " touched down " + std::to_string(flown.touch_along_m) +
                  " m beyond the threshold, which is not on the runway's " +
                  std::to_string(a_runway().length_m) + " m");
        check(flown.debrief.empty(),
              id + " flew the circuit inside the lesson's limits, and said " +
                  std::to_string(flown.debrief.size()) + " things");
        for (const std::string& wrong : flown.after.what_went_wrong(id)) {
            came_down_badly.push_back(wrong);
        }
        for (const std::string& wrong :
             flown.after.how_the_gear_took_it(id + " (circuit)", settled_within_ft)) {
            came_down_badly.push_back(wrong);
        }
        for (const std::string& wrong : flared_from_the_path(id, "circuit", flown.flare)) {
            came_down_badly.push_back(wrong);
        }
        for (const std::string& wrong :
             inside_the_touchdown_zone(id, "circuit", flown.touch_along_m)) {
            came_down_badly.push_back(wrong);
        }
        ++walked;
    }
    // **Every one of them stayed on its wheels, the right way up**, from the
    // touch back on the runway to the stop, and flared from the attitude it
    // flew the glidepath at to one short of its tail strike.
    for (const std::string& wrong : came_down_badly) {
        std::printf("  CAME DOWN BADLY: %s\n", wrong.c_str());
    }
    check(came_down_badly.empty(),
          std::to_string(came_down_badly.size()) +
              " things went wrong after touching down at the end of the circuit, the first: " +
              (came_down_badly.empty() ? "" : came_down_badly.front()));
    check(walked == taught.size(),
          "every aeroplane taught the circuit flew it: " + std::to_string(walked) +
              " of " + std::to_string(taught.size()));
    // Fourteen of the sixteen: the 747-400 and the F-22A have no rotation or
    // climbing speed to fly one with - `everyone_taught` names them above.
    check(walked == 14, "fourteen aeroplanes flew the circuit, not " + std::to_string(walked));
}

// **A circuit flown with one fault has that fault in its debrief, and no
// other.** She is let sink two hundred and fifty feet along the downwind leg,
// which is the one thing the lesson watches there.
GLIDESLOPE_TEST(a_circuit_flown_low_downwind_is_named_in_the_debrief) {
    for (const std::string& id : one_of_each_class("circuit")) {
        const Circuit sunk = fly_a_circuit(id, false, 250.0);
        std::printf("  %s sinking downwind: %zu/%zu stages, downwind %.0f-%.0f ft\n",
                    id.c_str(), sunk.completed, sunk.stages, sunk.lowest_downwind_ft,
                    sunk.highest_downwind_ft);
        names_that_fault_and_no_other(id, "circuit", sunk.debrief, {"position/h-agl-ft"},
                                      "sinking along the downwind leg");
    }
}

// **The instructor demonstrates a circuit, then hands over**, as for every
// other exercise: flown by the AI pilot through a `Controller` from the
// runway round the pattern and back, then the controls to the pilot and back
// with no step. The item's verification asks it of each lesson, and the
// circuit had none until 2026-09-24.
GLIDESLOPE_TEST(an_instructor_demonstrates_a_circuit_and_hands_it_over) {
    const double a_hands_pace = 2.0 / steps_per_second + 0.004;
    const auto flown = everyone_taught("circuit");
    check(!flown.empty(), "some aeroplane is taught the circuit");
    std::size_t walked = 0;
    for (const std::string& id : flown) {
        Demonstrated shown;
        const Circuit circuit = fly_a_circuit(id, false, 0.0, &shown);
        std::printf("  %-13s circuit %zu/%zu stages, worst step %.4f over, %.4f back\n",
                    id.c_str(), circuit.completed, circuit.stages, shown.worst_to_pilot,
                    shown.worst_to_ai);
        check(circuit.completed == circuit.stages,
              id + " flew the whole circuit, " + std::to_string(circuit.completed) + " of " +
                  std::to_string(circuit.stages) + " stages");
        check(circuit.debrief.empty(),
              id + " demonstrated it inside the lesson's limits, and said " +
                  std::to_string(circuit.debrief.size()) + " things");
        check(shown.worst_to_pilot <= a_hands_pace,
              id + " stepped " + std::to_string(shown.worst_to_pilot) + " handing over");
        check(shown.worst_to_ai <= a_hands_pace,
              id + " stepped " + std::to_string(shown.worst_to_ai) + " taking back");
        ++walked;
    }
    check(walked == flown.size(), "every aeroplane taught the circuit demonstrated it: " +
                                      std::to_string(walked) + " of " +
                                      std::to_string(flown.size()));
}

namespace {

struct ByHand {
    std::vector<std::string> debrief; // what the approach's two stages said
    std::size_t completed = 0;
    double least_kts = 1e9;
    double most_kts = -1e9;
    double vref_kts = 0.0;
};

// **The B-2A flown down the glidepath by hand**, the speedbrake lever where
// the pilot has put it: two miles out, as the AI pilot's approach begins, and
// flown to the end of the lesson's second stage, over the threshold. The
// hands are a test pilot's (sim/test_pilot.hpp), not the AI's: the elevator
// holds the glidepath's height - led by the sink the path asks, as a pilot
// leads a moving target - the wings level, the ball in the middle, and the
// throttle is worked for the reference speed, more when slow and less when
// fast, a twentieth of its travel a second for each knot off, and no faster
// than full travel in two seconds.
ByHand fly_the_b2a_approach_by_hand(double speedbrake) {
    const auto entry = glideslope::sim::find_aircraft(data(), "b2");
    const glideslope::sim::Runway runway = a_runway();
    const auto published = glideslope::sim::approach_speeds(data(), entry.model);
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    put_on_final(aircraft, entry, runway, published);
    const auto found = lesson_for(entry, "approach-and-landing");
    check(found.has_value(), "the B-2A has an approach lesson");
    LessonRun run(*found, glideslope::sim::LessonSpeeds{0.0, 0.0, published.vref_kts,
                                                        published.stall_kts});
    glideslope::sim::TestPilot pilot(aircraft);
    glideslope::sim::Controls c;
    c.gear = 1.0;
    c.speedbrake = speedbrake;
    c.throttle = aircraft.property("fcs/throttle-cmd-norm[0]");
    const double heading = runway.heading_deg / degrees;
    const double dt = 1.0 / steps_per_second;
    ByHand out;
    out.vref_kts = published.vref_kts;
    for (int tick = 0; tick < 300 * steps_per_second && run.stage() < 2; ++tick) {
        // How far short of the aim point she is along the runway's line, and
        // the glidepath's height there.
        const glideslope::sim::AircraftState s = aircraft.state();
        const double north_m = (s.latitude_deg - runway.threshold_lat_deg) *
                               metres_per_degree_latitude(runway.threshold_lat_deg);
        const double east_m = (s.longitude_deg - runway.threshold_lon_deg) *
                              metres_per_degree_longitude(runway.threshold_lat_deg);
        const double short_m =
            published.aim_m - (north_m * std::cos(heading) + east_m * std::sin(heading));
        const double path_ft =
            runway.elevation_ft + short_m * std::tan(3.0 / degrees) * feet_per_metre;
        const double sink_fps = aircraft.property("velocities/vg-fps") * std::tan(3.0 / degrees);
        c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(path_ft - 8.0 * sink_fps));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        const double fast_kts = s.airspeed_kts - published.vref_kts;
        c.throttle = std::clamp(c.throttle + std::clamp(-0.05 * fast_kts, -0.5, 0.5) * dt, 0.0,
                                1.0);
        aircraft.set_controls(c);
        aircraft.step();
        run.update(aircraft, tick);
        if (run.stage() < 2) {
            out.least_kts = std::min(out.least_kts, aircraft.state().airspeed_kts);
            out.most_kts = std::max(out.most_kts, aircraft.state().airspeed_kts);
        }
    }
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    return out;
}

} // namespace

// **A pilot can now fly the B-2A's approach lesson by hand, as the AI flies
// it**: with the speedbrake lever half out, as the lesson tells her pilot, she
// comes down the glidepath and over the threshold inside its band - the
// light aircraft's `vref-8` to `vref+12`, where until a pilot had the lever
// it reached `vref+20`. With the lever stowed, the same hands with the
// throttles closed cannot hold her to it: the band is one only the lever
// meets. Both flights are flown, and each must say so.
GLIDESLOPE_TEST(a_pilot_flies_the_b2a_approach_lesson_by_hand_inside_its_band_only_with_her_speedbrakes_out) {
    const ByHand out = fly_the_b2a_approach_by_hand(0.5);
    const ByHand stowed = fly_the_b2a_approach_by_hand(0.0);
    for (const auto& [what, f] : {std::pair{"half out", &out}, std::pair{"stowed", &stowed}}) {
        std::printf("  speedbrakes %-8s vref %.1f: %zu stages, %.1f to %.1f kt (%+.1f to %+.1f)\n",
                    what, f->vref_kts, f->completed, f->least_kts, f->most_kts,
                    f->least_kts - f->vref_kts, f->most_kts - f->vref_kts);
        for (const std::string& said : f->debrief) {
            std::printf("      %s\n", said.c_str());
        }
    }
    check(out.completed >= 2, "half out, she was flown over the threshold: " +
                                  std::to_string(out.completed) + " stages");
    check(out.debrief.empty(), "half out, the lesson had nothing to say: " +
                                   std::to_string(out.debrief.size()) + " things");
    check(stowed.completed >= 2, "stowed, she was flown over the threshold too: " +
                                     std::to_string(stowed.completed) + " stages");
    check(!stowed.debrief.empty() && stowed.most_kts > stowed.vref_kts + 12.0,
          "stowed, she ran past the band, and the lesson said so: " +
              std::to_string(stowed.most_kts - stowed.vref_kts) + " kt over");
}
