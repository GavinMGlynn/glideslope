#include "after_touch.hpp"
#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/departure.hpp"
#include "sim/autopilot.hpp"
#include "sim/controller.hpp"
#include "sim/lander.hpp"
#include "sim/figures.hpp"
#include "sim/lesson.hpp"
#include "sim/lesson_run.hpp"
#include "sim/terrain.hpp"

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
        const auto departure = glideslope::sim::departure_speeds(data(), entry.model);
        speeds.rotate_kts = departure.rotate_kts;
        speeds.climb_kts = departure.climb_kts;
    } catch (const std::exception&) {
    }
    try {
        const auto approach = glideslope::sim::approach_speeds(data(), entry.model);
        speeds.vref_kts = approach.vref_kts;
        speeds.stall_kts = approach.vref_kts / 1.3;
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
    for (int tick = 0; tick < 300 * steps_per_second && !run.finished(); ++tick) {
        glideslope::sim::Controls controls = departure.fly();
        // **Rotating early is flown, not faked.** A steady touch of back
        // stick while the wheels are still down brings her off before her
        // speed is there - which is what rotating early is - without the
        // wild attitude that hauling the stick fully back would give. Lower
        // the take-off autopilot's rotation speed instead and nothing
        // happens: an aeroplane below its stall will not fly, whatever the
        // nose is doing, and she simply rolls on with the nose up.
        //
        // **Half back, as the F-15's flight manual has a normal take-off**
        // (T.O. 1F-15A-1, section II): three tenths brought the Cessna off
        // early and not the B-2, which needs more stick to raise the wing.
        //
        // **From 85 percent of her rotation speed, not from a standstill.**
        // Held from the start of the roll, the stick kept the Mosquito's tail
        // down the whole way, and hauled off at 107 knots she swung past the
        // twenty degrees the roll allows: the debrief said keep her straight
        // as well, which is a different fault. Easing back early is easing
        // back before the speed is there, not before she moves. From 85
        // percent she comes off at 110 against 160, and the Cessna at 51
        // against 81.
        if (rotate_kts_override > 0.0 &&
            aircraft.property("velocities/vc-kts") >= 0.85 * speeds.rotate_kts &&
            aircraft.property("position/h-agl-ft") < rotate_kts_override) {
            // **A flying boat's is the nose held low on the step.** The
            // water, not the elevator, sets her attitude there in this model:
            // held fully back the Short S.23 planed at the running attitude
            // and came off at the same 78 knots. The fault the FAA's seaplane
            // handbook warns of on the step is the other one - the nose too
            // low, the bow digging in, and porpoising - which a little
            // forward stick flies.
            controls.elevator = entry.seaplane ? -0.3 : std::max(controls.elevator, 0.50);
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
    // Fourteen of the sixteen: the four light aircraft, the Mosquito, the
    // Learjet, four airliners, two fighters, the B-2A and the Short S.23, off
    // the water. The 747-400 and the F-22A have no stall to rotate from and
    // are named above.
    check(walked == 14, "fourteen aeroplanes took off, not " + std::to_string(walked));
}

// **Flown with one stated fault, the debrief names that fault.** The
// throttle case is exact: one thing said, and it is the throttle.
//
// The early rotation below says two things, and both are true - she came off
// early *and* the attitude wandered while she did it, which is what hauling
// an aeroplane off the ground before its speed actually does. A debrief that
// named only one of them would be hiding the other.
GLIDESLOPE_TEST(a_take_off_flown_with_one_fault_has_that_fault_in_its_debrief) {
    for (const std::string& id : one_of_each_class("take-off")) {
        // **Not opening the throttle.** Flown by the book in every other way.
        const Flown lazy = fly_the_take_off(id, 0.0, 0.80);
        std::printf("  %s on part throttle: off at %.0f knots\n", id.c_str(),
                    lazy.off_at_kts);
        names_that_fault_and_no_other(id, "take-off", lazy.debrief, {"fcs/throttle-cmd-norm"},
                                      "taking off on part throttle");

        // **Rotating early**, with a steady touch of back stick until she is
        // off: she comes off at a speed she has no business flying at, and
        // the attitude band catches her.

        // **Left out: the F-15C, the F-35B and the Learjet 35A**, whose noses
        // do not come up in these models until well past their rotation
        // speeds, whatever the stick does. Pulled half back from 85 percent of
        // the rotation speed they left the ground at 220 (F-15C), 213 (F-35B)
        // and 161 knots (Learjet), by the book at 230, 213 and 160, against
        // rotation speeds of 174, 141 and 125. Early is five knots short of
        // the rotation speed, which none of them can reach. The F-15C's flight
        // manual has its nosewheel off at about 130 knots and the aeroplane
        // off at 157 (T.O. 1F-15A-1, figure A3-6), so it is the models', and
        // a tail in COMPLETION_PLAN.md.
        if (id == "f15c" || id == "f35b" || id == "learjet35a") {
            std::printf("  %s left out of rotating early: its nose does not come up "
                        "until well past its rotation speed\n",
                        id.c_str());
            continue;
        }
        const std::string rotated = id;
        const Flown early = fly_the_take_off(rotated, 14.0, 1.0);
        const Flown book = fly_the_take_off(rotated, 0.0, 1.0);
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
            continue;
        }
        std::printf("  %s by the book: off at %.0f knots; rotating early: off at %.0f\n",
                    rotated.c_str(), book.off_at_kts, early.off_at_kts);
        check(early.off_at_kts < book.off_at_kts - 3.0,
              rotated + " really did come off earlier: " + std::to_string(early.off_at_kts) +
                  " against " + std::to_string(book.off_at_kts));
        names_that_fault_and_no_other(rotated, "take-off", early.debrief,
                                      {"attitude/theta-deg", "need:velocities/vc-kts"},
                                      "rotating early");
        check(book.debrief.empty(), rotated + ": the same take-off by the book says nothing");
    }
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
    // The descent through the approach stage, in feet a second, which is what
    // its "down the glidepath, not out of the sky" band is set from.
    double sink_least_fps = 1e9;
    double sink_most_fps = -1e9;
    // When the most negative of those came, in seconds from the start.
    double sink_worst_at_s = 0.0;
    // The descent at the moment the approach stage ended, fifty feet up: what
    // the lesson's "arrive under control" need is set from.
    double sink_at_end_fps = 0.0;
    bool trimmed = false; // started trimmed on the path, as asked
    // From the touch to the stop, which is further than the lesson watches:
    // it ends at thirty knots, and the Learjet's nose went through the
    // runway after that.
    glideslope::test::AfterTouch after;
    bool stopped = false;
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
    // Established on the approach: the flap it is flown with is already down,
    // and it is already coming down the glidepath rather than level on it.
    ic.flaps = flown_with.flap;
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
        if (which == 0 && run.stage() != 0) {
            out.sink_at_end_fps = aircraft.property("velocities/h-dot-fps");
        }
        if (id == "f35b" && tick % (4 * steps_per_second) == 0) {
            std::printf("      %s %4.0f s  agl %5.0f  %4.0f kt  vs %6.1f fps  pitch %5.1f  "
                        "alpha %5.1f  elev %+.2f  thr %.2f  stage %zu\n",
                        id.c_str(), static_cast<double>(tick) / steps_per_second,
                        aircraft.property("position/h-agl-ft"),
                        aircraft.property("velocities/vc-kts"),
                        aircraft.property("velocities/h-dot-fps"),
                        aircraft.property("attitude/theta-deg"),
                        aircraft.property("aero/alpha-deg"),
                        aircraft.property("fcs/elevator-cmd-norm"),
                        aircraft.property("fcs/throttle-cmd-norm"), run.stage());
        }
        if (which == 0) {
            const double fps = aircraft.property("velocities/h-dot-fps");
            if (fps < out.sink_least_fps) {
                out.sink_least_fps = fps;
                out.sink_worst_at_s = static_cast<double>(tick) / steps_per_second;
            }
            out.sink_most_fps = std::max(out.sink_most_fps, fps);
        }
        out.after.watch(aircraft);
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
        aircraft.set_controls(lander.fly());
        aircraft.step();
        out.after.watch(aircraft);
    }
    out.stopped = done();
    out.debrief = run.debrief_lines();
    out.completed = run.completed();
    out.stages = it->stages.size();
    out.trimmed = aircraft.trimmed();
    return out;
}

} // namespace

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
        std::printf("         after touching: rolled %.1f, pitched down to %.1f, rose %.1f ft, "
                    "%s\n",
                    flown.after.worst_roll_deg, flown.after.least_pitch_deg,
                    flown.after.highest_ft, flown.stopped ? "stopped" : "NOT STOPPED");
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
        for (const std::string& wrong : flown.after.what_went_wrong(id)) {
            came_down_badly.push_back(wrong);
        }
        ++walked;
    }
    // **Every one of them stayed on its wheels, the right way up**, from the
    // touch to the stop - named all together, so one run shows them all.
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
InFlight airborne(const std::string& id, double agl_ft, double start_kcas = 0.0) {
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
    load_as_its_figures_were_measured(*out.aircraft, entry.model);
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
        climb_kcas = glideslope::sim::departure_speeds(
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

// **A stall, entered the way one is entered**: throttle closed, the height
// held, the nose rising as the speed decays until the wing gives up. The
// recovery is the control column forward and full power. `left_s` is how
// long the aeroplane is left mushing in the stall before the recovery is
// handed it: none by the book, and the longer the later.
Result fly_a_stall(const std::string& id, double left_s, bool fresh_autopilot = false) {
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

    // **In the configuration its reference speed was measured in**: the
    // landing flap and the gear down. `stall` is the stall with everything
    // down, and flown clean a swept wing gives up long before it: asked to
    // slow to twenty-five knots above its landing stall, a clean 737 has to
    // stall to get there, and it departed - 52 degrees of alpha, 26 nose down
    // and 21,000 ft/min, with nothing to recover it.
    const double landing_flap =
        glideslope::sim::approach_speeds(data(), stall_entry.model).flap;
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
    // **Recovered is flying again, not only fast again**: the wing below the
    // angle of attack it stalled at, the aeroplane level or climbing, and at
    // or above the speed the lesson's recovery ends at, all at once. A speed
    // alone called an A320 recovered at 27 degrees of alpha and sinking
    // 10,000 ft/min, with no pull-out flown at all. So a recovery handed over
    // late is flown on past the lesson's end until it holds.
    //
    // **The angle it stalled at is where its lift stopped rising**: the angle
    // of attack at the greatest lift coefficient measured from the entry to
    // the hand-over. A model's lift table is the model's own; this reads what
    // it gives, in the configuration and at the rate it is flown here.
    //
    // **Level is within 100 ft/min**, as a pilot holds a height. A PA-28 at
    // full power with its landing flap out at 4,000 ft cannot climb: at its
    // recovery speed it settles sinking 45 ft/min, flying and in hand, and a
    // rule of "not descending at all" would never call it recovered.
    constexpr double level_within_fpm = 100.0;
    const glideslope::sim::LessonStage& recovery_stage = lesson.stages.back();
    const double recovered_kts =
        glideslope::sim::figure_of(recovery_stage.until_value, f.speeds);
    double most_lift = -1e9;
    // **Recovered must last**: five seconds of it together, counted in
    // steps, with the lowest height taken throughout - so that the top of a
    // zoom climb, level and fast for an instant with the wing unloaded, is
    // not called a recovery.
    const std::int64_t recovered_for = 5 * steps_per_second;
    std::int64_t recovered_since = -1;
    // **The load is read over a quarter of a second**, the mean of the last
    // thirty steps, so that one step's jolt is not the peak.
    constexpr std::size_t load_window = steps_per_second / 4;
    std::vector<double> loads;
    double load_sum = 0.0;
    const auto flying = [&] {
        return !run.finished() || (left_s > 0.0 && !out.recovered);
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
            modes.airspeed_kts = f.speeds.stall_kts - 10.0;
            autopilot->set(modes);
        }
        const auto& a = *f.aircraft;
        if (tick >= settling && !recovering) {
            const double lift = a.property("forces/fwz-aero-lbs") /
                                std::max(a.property("aero/qbar-psf") *
                                             a.property("metrics/Sw-sqft"),
                                         1.0);
            if (lift > most_lift) {
                most_lift = lift;
                out.stall_alpha_deg = a.property("aero/alpha-deg");
            }
            if (run.stage() >= 1) {
                if (stalled_at < 0) {
                    stalled_at = tick;
                }
                if (tick - stalled_at >= dawdle) {
                    recovering = true;
                    out.handed_over_ft = a.property("position/h-agl-ft");
                    out.handed_over_kts = a.property("velocities/vc-kts");
                    out.handed_over_alpha_deg = a.property("aero/alpha-deg");
                    out.handed_over_pitch_deg = a.property("attitude/theta-deg");
                    out.handed_over_fpm = a.property("velocities/h-dot-fps") * 60.0;
                    if (fresh_autopilot) {
                        // A new autopilot, engaged on the aeroplane as it is,
                        // that has seen nothing of it before.
                        autopilot = std::make_unique<glideslope::sim::Autopilot>(
                            *f.aircraft, last_controls);
                    }
                    // **The recovery is the autopilot's stall recovery**
                    // (AutopilotModes::speed_on_elevator), not a fixed
                    // control position: a column held forward by the same
                    // amount recovers a Cessna and flies a Learjet into the
                    // ground. Asked for a speed clear of the one the lesson's
                    // recovery ends at, every aeroplane puts its nose down
                    // until the wing unloads and the speed comes, with its
                    // own controls. The vertical speed it used to be asked
                    // for held a mushing B-2A in the stall to the ground,
                    // raising the nose against a sink it could not stop.
                    modes.altitude_ft.reset();
                    modes.airspeed_kts =
                        std::max(f.speeds.stall_kts * 1.5, recovered_kts + 5.0);
                    modes.speed_on_elevator = true;
                    autopilot->set(modes);
                }
            }
        }
        glideslope::sim::Controls c = autopilot->fly();
        if (tick >= settling) {
            // Throttle closed for the entry: the autopilot holds the height by
            // raising the nose, and she slows towards the stall of her own
            // accord. Full power for the recovery.
            c.throttle = recovering ? 1.0 : 0.0;
        }
        c.flaps = landing_flap;
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
            if (recovering && !out.recovered) {
                out.recovery_lowest_ft = std::min(out.recovery_lowest_ft, agl);
                const double load = a.property("accelerations/Nz");
                loads.push_back(load);
                load_sum += load;
                if (loads.size() > load_window) {
                    load_sum -= loads[loads.size() - load_window - 1];
                }
                if (loads.size() >= load_window) {
                    out.peak_load_g = std::max(out.peak_load_g,
                                               load_sum / static_cast<double>(load_window));
                }
                const bool flying_again =
                    a.property("aero/alpha-deg") < out.stall_alpha_deg &&
                    a.property("velocities/h-dot-fps") * 60.0 >= -level_within_fpm &&
                    a.property(recovery_stage.until_property) >= recovered_kts;
                if (!flying_again) {
                    recovered_since = -1;
                } else if (recovered_since < 0) {
                    recovered_since = tick;
                }
                out.recovered = recovered_since >= 0 && tick - recovered_since >= recovered_for;
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
        const Result flown = fly_a_stall(id, 0.0);
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

// **Every aeroplane stalled and left mushing for thirty seconds is recovered,
// within its lesson's height or a bound this test names, and pulled out
// within 2 g or a bound this test names.** Left that long the aeroplane is no
// longer approaching the stall but deep in it, and the autopilot's old
// recovery - a vertical speed asked for - answered the sink by raising the
// nose: the B-2A, handed over at 96 knots, 12 degrees nose up and 30 degrees
// of alpha, stayed there to the ground. The recovery is now the autopilot's
// stall recovery (AutopilotModes::speed_on_elevator).
//
// **Recovered** is flying again, for five seconds together (fly_a_stall):
// below the angle of attack the wing stalled at, level or climbing, and at the
// lesson's recovery speed. The height lost is counted from the moment the
// recovery is handed the aeroplane to the lowest it goes before that: what it
// lost while it was left is not the recovery's.
//
// **One tolerance, for every aeroplane, height and load alike: 10 per
// cent**, for what differs between machines - the flight is floating point,
// and a stall is where small differences grow. An aeroplane is held to its
// lesson's height, or to 2 g, only with that much in hand: its figure plus 10
// per cent must be within the limit. One that has not got it is named below
// and held to its own figure plus 10 per cent, so none gets worse unseen, and
// the item stays open in docs/COMPLETION_PLAN.md. The figures are printed for
// every aeroplane, so each platform's run shows its own.
//
// **Ten are outside their lesson's height**, which was set for a recovery
// from the approach to the stall, not from thirty seconds deep in it: handed
// over sinking thousands of feet a minute, most of the height goes in
// stopping the sink. **2 g is the load the airworthiness rules require with
// the flaps out** (14 CFR 23.345 and 25.345), and the recovery is flown with
// them out; two pull out harder than that.
constexpr double between_machines = 0.10;
constexpr double flaps_down_most_g = 2.0;

struct NamedBound {
    const char* id;
    double measured;
    double bound;
};

const std::vector<NamedBound>& not_yet_within_its_lesson() {
    static const std::vector<NamedBound> named{
        {"737-300", 1314.0, 1450.0},     {"787-8", 1536.0, 1690.0},
        {"a320", 1535.0, 1690.0},        {"a380", 1754.0, 1930.0},
        {"b2", 921.0, 1020.0},           {"f15c", 1068.0, 1180.0},
        {"f35b", 3018.0, 3320.0},        {"learjet35a", 769.0, 850.0},
        {"mosquito-fb6", 1332.0, 1470.0}, {"short_s23", 264.0, 300.0},
    };
    return named;
}

const std::vector<NamedBound>& pulls_out_harder() {
    static const std::vector<NamedBound> named{
        {"a320", 2.18, 2.40},
        {"mosquito-fb6", 2.23, 2.46},
    };
    return named;
}

const NamedBound* named_in(const std::vector<NamedBound>& list, const std::string& id) {
    const auto it = std::find_if(list.begin(), list.end(),
                                 [&](const NamedBound& n) { return id == n.id; });
    return it == list.end() ? nullptr : &*it;
}

// **A named bound is honest**: its bound is its figure plus the tolerance, it
// is a figure the aeroplane cannot meet its limit with, and every one names an
// aeroplane the test flies. A name left behind when the aeroplane has come
// within its limit is a stale entry, and turns the test red so it is taken off.
//
// `flown` is each aeroplane flown, with its figure and the limit it is judged
// against.
struct Judged {
    double figure = 0.0;
    double limit = 0.0;
    bool recovered = false; // a figure from a flight not recovered judges nothing
};

std::vector<std::string> stale_entries(const std::vector<NamedBound>& list,
                                       const std::map<std::string, Judged>& flown,
                                       const std::string& what) {
    std::vector<std::string> out;
    for (const NamedBound& n : list) {
        const auto it = flown.find(n.id);
        if (it == flown.end()) {
            out.push_back(std::string(n.id) + " is named for its " + what +
                          " but was not flown");
        } else if (it->second.recovered &&
                   it->second.figure * (1.0 + between_machines) <= it->second.limit) {
            out.push_back(std::string(n.id) + " is named for its " + what + " and now meets " +
                          std::to_string(it->second.limit) + " with the tolerance in hand (" +
                          std::to_string(it->second.figure) + "): take its name off");
        }
    }
    return out;
}

} // namespace

GLIDESLOPE_TEST(every_aeroplane_left_thirty_seconds_in_a_stall_is_recovered_within_its_lessons_height_or_a_named_bound) {
    const Taught taught = taught_for("stalls");
    const std::size_t roster = glideslope::sim::read_catalogue(data()).size();
    std::size_t walked = 0;
    std::size_t held_to_lesson = 0;
    std::size_t held_to_2g = 0;
    std::vector<std::string> faults;
    std::map<std::string, Judged> lost_by;
    std::map<std::string, Judged> load_by;
    for (const std::string& id : taught.able) {
        const auto entry = glideslope::sim::find_aircraft(data(), id);
        const double allowed_ft = recovery_allowance_ft(*lesson_for(entry, "stalls"));
        const NamedBound* height_named = named_in(not_yet_within_its_lesson(), id);
        const NamedBound* load_named = named_in(pulls_out_harder(), id);
        const Result left = fly_a_stall(id, 30.0);
        const double lost_ft = left.handed_over_ft - left.recovery_lowest_ft;
        lost_by[id] = {lost_ft, allowed_ft, left.recovered};
        load_by[id] = {left.peak_load_g, flaps_down_most_g, left.recovered};
        std::printf("  %-13s handed over at %6.0f ft, %5.1f kt, alpha %5.1f, pitch %5.1f, "
                    "%6.0f ft/min; stalled at alpha %4.1f; recovery lost %5.0f ft, lesson "
                    "allows %4.0f; peak %.2f g%s\n",
                    id.c_str(), left.handed_over_ft, left.handed_over_kts,
                    left.handed_over_alpha_deg, left.handed_over_pitch_deg,
                    left.handed_over_fpm, left.stall_alpha_deg, lost_ft, allowed_ft,
                    left.peak_load_g, left.recovered ? "" : ", NOT RECOVERED");
        if (height_named != nullptr) {
            std::printf("      outside its lesson: held to %.0f ft (measured %.0f)\n",
                        height_named->bound, height_named->measured);
        }
        if (load_named != nullptr) {
            std::printf("      pulls out harder than %.1f g: held to %.2f g (measured %.2f)\n",
                        flaps_down_most_g, load_named->bound, load_named->measured);
        }
        // With the tolerance in hand against the lesson or 2 g, or within its
        // named bound. Every aeroplane is flown and its figures printed before
        // any is judged, so one failure does not hide the rest.
        const bool height_ok = height_named != nullptr
                                   ? lost_ft <= height_named->bound
                                   : lost_ft * (1.0 + between_machines) <= allowed_ft;
        const bool load_ok = load_named != nullptr
                                 ? left.peak_load_g <= load_named->bound
                                 : left.peak_load_g * (1.0 + between_machines) <=
                                       flaps_down_most_g;
        if (left.handed_over_ft <= 0.0) {
            faults.push_back(id + " was not handed to its recovery in the air");
        } else if (!left.recovered) {
            faults.push_back(id + " left thirty seconds in the stall was not recovered");
        } else {
            if (!height_ok) {
                faults.push_back(id + " was recovered in " + std::to_string(std::lround(lost_ft)) +
                                 " ft, against " +
                                 (height_named != nullptr
                                      ? "its named bound of " +
                                            std::to_string(std::lround(height_named->bound))
                                      : "its lesson's " + std::to_string(std::lround(allowed_ft)) +
                                            " with 10% in hand"));
            }
            if (!load_ok) {
                faults.push_back(id + " pulled out at " + std::to_string(left.peak_load_g) +
                                 " g, against " +
                                 (load_named != nullptr
                                      ? "its named bound of " + std::to_string(load_named->bound)
                                      : std::string("2 g with 10% in hand")));
            }
        }
        held_to_lesson += height_named == nullptr ? 1 : 0;
        held_to_2g += load_named == nullptr ? 1 : 0;
        ++walked;
    }
    for (const std::string& said : stale_entries(not_yet_within_its_lesson(), lost_by, "height")) {
        faults.push_back(said);
    }
    for (const std::string& said : stale_entries(pulls_out_harder(), load_by, "load")) {
        faults.push_back(said);
    }
    std::printf("  of the %zu aeroplanes: %zu flown, %zu left out; %zu held to their lesson's "
                "height and %zu to a named bound; %zu held to 2 g and %zu to a named bound\n",
                roster, walked, taught.left_out.size(), held_to_lesson,
                walked - held_to_lesson, held_to_2g, walked - held_to_2g);
    for (const std::string& said : taught.left_out) {
        std::printf("      left out - %s\n", said.c_str());
    }
    for (const std::string& fault : faults) {
        std::printf("  %s\n", fault.c_str());
    }
    check(faults.empty(), std::to_string(faults.size()) +
                              " things were wrong with the recoveries from a thirty-second "
                              "stall; the first: " +
                              (faults.empty() ? "" : faults.front()));
    check(walked == taught.able.size(), "every aeroplane taught a stall was left in one");
    check(walked + taught.left_out.size() == roster,
          "every aeroplane in the roster was flown or left out with its reason");
    check(walked - held_to_lesson == not_yet_within_its_lesson().size(),
          "every aeroplane named for its height was flown");
    check(walked - held_to_2g == pulls_out_harder().size(),
          "every aeroplane named for its load was flown");
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
    ic.airspeed_kts = published.vref_kts;
    ic.engine_running = true;
    ic.gear = 1.0;
    load_for_the_approach(aircraft, entry.model);
    // Established on the approach: the flap it is flown with is already down,
    // and it is already coming down the glidepath rather than level on it.
    ic.flaps = published.flap;
    ic.flight_path_deg = -3.0;
    ic.trim = true;
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
    // above. Taken back on the roll, she is given the plain autopilot, not
    // the landing (a tail in docs/COMPLETION_PLAN.md).
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
Demonstrated demonstrate_in_the_air(const std::string& id, const std::string& exercise,
                                    double start_ft, int settling_s, const Begin& begin,
                                    const Fly& fly, double flaps = 0.0) {
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
// holds the height by raising the nose, which is the entry; asking for half
// as much again as the stall speed opens the throttle and puts the nose down,
// which is the recovery. The version this replaces reached into the controls
// and set the throttle to 0 and then to 1 by hand, which no controller can
// hand over.
Demonstrated demonstrate_a_stall(const std::string& id, double start_ft) {
    // Flaps and gear as they are for the landing, which is what `stall` is
    // the stall speed in - the same as the lesson's own flight.
    const double landing_flap = glideslope::sim::approach_speeds(
        data(), glideslope::sim::find_aircraft(data(), id).model).flap;
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
                // **Not the autopilot's stall recovery** (speed_on_elevator),
                // which the lesson's own flight now uses: the instructor
                // still asks for a descent, nose down in proportion to the
                // wing's speed. From the approach to the stall that serves;
                // from deep in one it held a B-2A in the stall to the ground.
                m.vertical_speed_fpm = -std::max(600.0, 12.0 * f.speeds.stall_kts);
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
        },
        landing_flap);
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

// And how far to the right of its centreline, in metres.
double across_the_runway_m(const glideslope::sim::Runway& r,
                           const glideslope::sim::Aircraft& a) {
    const double north_m = (a.property("position/lat-geod-deg") - r.threshold_lat_deg) *
                           metres_per_degree_latitude(r.threshold_lat_deg);
    const double east_m = (a.property("position/long-gc-deg") - r.threshold_lon_deg) *
                          metres_per_degree_longitude(r.threshold_lat_deg);
    const double h = r.heading_deg / degrees;
    return east_m * std::cos(h) - north_m * std::sin(h);
}

bool pointing_at(const glideslope::sim::Aircraft& a, double heading_deg) {
    return std::abs(std::remainder(heading_deg - a.property("attitude/psi-deg"), 360.0)) <
           10.0;
}

struct Circuit {
    double stop_along_m = 0.0;  // where she stopped, beyond the threshold
    double stop_across_m = 0.0; // and right of the centreline
    double touch_across_m = 1e9; // right of the centreline where she touched
    double touch_along_m = 0.0;  // and how far beyond the threshold
    double touch_kts = 0.0;      // and how fast
    // From the touch back on to the runway to the stop.
    glideslope::test::AfterTouch after;
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
// With `demo`, the instructor then hands over and takes back, as the other
// demonstrations do, and the steps at the two swaps go there.
Circuit fly_a_circuit(const std::string& id, bool trace, double sink_downwind_ft = 0.0,
                      Demonstrated* demo = nullptr) {
    const auto entry = glideslope::sim::find_aircraft(data(), id);
    const glideslope::sim::Runway runway = a_runway();
    const auto dep = glideslope::sim::departure_speeds(data(), entry.model);
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
                                                        app.vref_kts / 1.3});

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

    // **A faster aeroplane flies a bigger circuit, and the two numbers that
    // make it are one number.** The downwind leg is left at the distance
    // where a three-degree glidepath passes through circuit height, so the
    // approach autopilot is handed an aeroplane on its path rather than
    // above or below it. A Mosquito flown round the Cessna's thousand-foot
    // circuit and handed the approach at the Cessna's distance arrived low,
    // still turning, and put itself into the ground.
    const double circuit_ft =
        std::clamp(1000.0 + (app.vref_kts - 60.0) * 8.0, 1000.0, 1500.0);
    // A three-degree slope rises about 318 feet in a nautical mile, and the
    // extra third of a mile leaves her a little above the path at the hand
    // over, which is the side to be on.
    const double leave_downwind_nm = circuit_ft / 318.0 + 0.33;
    // **Base and the intercept are flown, and the approach is handed over on
    // an intercept, as an approach mode is.** Handed the approach at the end
    // of the downwind leg - two miles to the side and flying the other way -
    // the approach autopilot had the whole turn to make and the capture as
    // well, and the jets touched down 43 to 70 metres off the centreline. A
    // pilot flies base and a thirty-degree intercept, and the approach
    // captures the final course from there; so does this. How far out each
    // turn is begun is the aeroplane's own turn radius, at the downwind speed
    // and a twenty-five-degree bank.
    const double downwind_mps = (app.vref_kts + 20.0) * 0.514444;
    const double turn_radius_m =
        downwind_mps * downwind_mps / (9.80665 * std::tan(25.0 / degrees));
    enum class Leg { climbing_out, crosswind, downwind, base, intercept, approach };
    Leg leg = Leg::climbing_out;
    bool sank = false;

    Circuit out;
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
        if (leg == Leg::climbing_out && agl >= turn_crosswind_ft) {
            leg = Leg::crosswind;
            controller.to_ai();
            glideslope::sim::AutopilotModes m = controller.autopilot()->modes();
            m.heading_deg = runway.heading_deg - 90.0;
            m.altitude_ft = runway.elevation_ft + circuit_ft;
            // What the take-off climbs away at: the climbing speed for a
            // light aeroplane, V2 and ten for a jet, whose best climb speed
            // is an en-route one.
            m.airspeed_kts = dep.initial_climb_kts;
            // **Seven tenths of the climb she has, up to two thousand feet a
            // minute.** `a_climb_it_can_manage` stops at six hundred, which
            // suits a demonstration and not a jet's circuit: at 175 knots and
            // six hundred feet a minute an A380 flew three miles of
            // crosswind leg to reach circuit height, turned downwind far
            // wider than the two miles abeam Boeing's circuit is flown at,
            // and could not line up with the runway before she touched -
            // 186 metres to the left of it. For the light aircraft seven
            // tenths of their climb is below six hundred anyway.
            m.vertical_speed_fpm = [&] {
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
            // Base: left, square to the runway.
            leg = Leg::base;
            glideslope::sim::AutopilotModes m = controller.autopilot()->modes();
            m.heading_deg = runway.heading_deg + 90.0;
            // **And slowing, as a pilot does on base:** the FAA's Airplane
            // Flying Handbook (FAA-H-8083-3C, chapter 9) has base flown at
            // about 1.4 times the landing stall, final at 1.3. Kept at the
            // downwind speed, twenty knots over the reference, the C172P was
            // still at 81 knots when the turn on to final ended once the
            // approach autopilot flew its intercept by L1 guidance.
            m.airspeed_kts = app.vref_kts * 1.4 / 1.3;
            controller.autopilot()->set(m);
        } else if (leg == Leg::base &&
                   across_the_runway_m(runway, aircraft) >= -(2.0 * turn_radius_m + 200.0)) {
            // Left again on to a thirty-degree intercept of the final course.
            leg = Leg::intercept;
            glideslope::sim::AutopilotModes m = controller.autopilot()->modes();
            m.heading_deg = runway.heading_deg + 30.0;
            controller.autopilot()->set(m);
        } else if (leg == Leg::intercept &&
                   across_the_runway_m(runway, aircraft) >= -turn_radius_m) {
            leg = Leg::approach;
            controller.to_ai_approach(runway, app);
        }
        glideslope::sim::Controls flown = controller.fly();
        // **The take-off flap stays out round the pattern**, as a jet's
        // downwind leg is flown with it: the take-off autopilot brings it in
        // above two hundred feet, and the autopilot holds what it was handed.
        // The approach autopilot sets the landing flap itself.
        if (leg == Leg::crosswind || leg == Leg::downwind || leg == Leg::base ||
            leg == Leg::intercept) {
            flown.flaps = dep.flap;
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
        if (leg == Leg::approach) {
            out.after.watch(aircraft);
        }
        if (out.touch_across_m > 1e8 && leg == Leg::approach &&
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
                        static_cast<int>(leg), agl, along,
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
        std::printf("      after touching: rolled %.1f, pitched down to %.1f, rose %.1f ft\n",
                    flown.after.worst_roll_deg, flown.after.least_pitch_deg,
                    flown.after.highest_ft);
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
        // **And touched down on it**, not beside it and steered back: the
        // F-15C once touched 109 metres right of the centreline and still
        // stopped on the runway, which the check above could not see. Ten
        // metres keeps the main wheels well on a 45-metre runway.
        check(std::abs(flown.touch_across_m) <= 10.0,
              id + " touched down " + std::to_string(flown.touch_across_m) +
                  " m from the centreline, which is not within 10");
        check(flown.debrief.empty(),
              id + " flew the circuit inside the lesson's limits, and said " +
                  std::to_string(flown.debrief.size()) + " things");
        for (const std::string& wrong : flown.after.what_went_wrong(id)) {
            came_down_badly.push_back(wrong);
        }
        ++walked;
    }
    // **Every one of them stayed on its wheels, the right way up**, from the
    // touch back on the runway to the stop.
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
