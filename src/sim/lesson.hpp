#pragma once

// A lesson, as data (assets/lessons): a sequence of stages, each with what to
// do and what to watch, so a lesson is changed by changing its file and no
// code (`REQUIREMENTS.md` 4.3).
//
//   name TEXT...                  what the lesson is called
//   teaches CLASS                 which class of aircraft it is for, as the
//                                 `.aircraft` files spell it: light-aircraft,
//                                 seaplane, second-world-war, business-jet,
//                                 airliner, fighter, bomber
//   stage TEXT...                 a stage of the lesson, in the order flown
//   do TEXT...                    what to do at this stage: what the pilot is
//                                 told, and what the instructor demonstrates
//   until PROPERTY OP VALUE       what ends the stage: a JSBSim property,
//                                 `<=` or `>=`, and the figure. A figure is a
//                                 number, or one the aeroplane publishes -
//                                 `rotate`, `climb`, `vref`, or `start` -
//                                 what this property read when the stage
//                                 began - with an optional offset:
//                                 `rotate-3`, `vref+10`, `start-150`
//   hold PROPERTY LOW HIGH TEXT...   a band that must hold for the whole
//                                 stage; TEXT is what the debrief says if it
//                                 is broken
//   need PROPERTY OP VALUE TEXT...   something that must be true by the time
//                                 the stage ends; TEXT is what the debrief
//                                 says if it is not
//
// with `#` beginning a comment.
//
// **`hold` and `need` are the two ways a lesson can be flown badly**, and
// they are not the same. A `hold` is broken the moment the aeroplane leaves
// the band - an approach flown too fast is wrong while it is happening. A
// `need` is only judged when the stage ends - flap down by the time you turn
// final is not a fault until you have turned final.
//
// **There is no score.** A debrief is a list of what to do differently, in
// the order it happened, and nothing else (`FEATURES.md`).
//
// **A property is not checked against the model here**, as with checklists:
// whether an aircraft has `fcs/flap-pos-deg` is a question for the aircraft,
// and a test loads each model and asks.

#include "sim/catalogue.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::sim {

struct LessonError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// **A figure in a lesson, which may be the aeroplane own published one.** A
// lesson teaches a class, and the aeroplanes in a class do not share their
// speeds: the four light aircraft rotate between about 34 knots and about 55.
// A literal number can only be the slowest of them, which catches nothing on
// the fastest. So a figure may instead name one the aeroplane publishes -
// `rotate`, `climb`, `vref`, `stall` - with an optional offset: `rotate-3`,
// `climb+10`.
//
// **`start` is the fourth, and it is not a speed.** It is whatever this
// watch own property read when the stage began, which is how a lesson says
// "hold the height you are at" or "hold this heading" without knowing where
// the aeroplane is: `hold position/h-agl-ft start-150 start+150`. It is
// resolved by the runner, which is the only thing that knows when a stage
// began and what the aeroplane read then.
//
// The names are the ones `sim::departure_speeds` works out from
// `assets/figures/<id>.xml`, because those are the two a lesson needs and
// they are already read from the aeroplane published figures.
struct LessonNumber {
    double literal = 0.0;
    std::string reference; // empty: `literal` is the figure
    double offset = 0.0;

    bool named() const { return !reference.empty(); }
};

// The aeroplane own figures, for resolving the references above.
struct LessonSpeeds {
    double rotate_kts = 0.0;
    double climb_kts = 0.0;
    // The speed over the threshold, 1.3 times the landing stall, which
    // `sim::approach_speeds` works out from the same published figures.
    double vref_kts = 0.0;
    // The published stall speed with everything down, which is `vref` divided
    // by the 1.3 that made it.
    double stall_kts = 0.0;
};

// The figure `number` means for an aeroplane with these speeds.
double figure_of(const LessonNumber& number, const LessonSpeeds& speeds);

// A figure as a lesson writes it: "55", "rotate", "rotate-3", "vref+10",
// "start-150".
// False for anything else, including a name there is none of.
bool read_number(std::string_view text, LessonNumber& out);

// What must hold, or must have become true. `low`/`high` are the band for a
// `hold`; for a `need`, `low` is the figure and `at_least` says which way.
struct LessonWatch {
    std::string property;
    LessonNumber low;
    LessonNumber high;
    bool banded = false;   // a `hold`: both ends count
    bool at_least = false; // a `need`: `>=` when true, `<=` when false
    std::string fault;     // what the debrief says when it is not met
};

struct LessonStage {
    std::string name;
    std::vector<std::string> doing; // what to do, in the order given
    // What ends the stage.
    std::string until_property;
    LessonNumber until_value;
    bool until_at_least = false;
    std::vector<LessonWatch> holds;
    std::vector<LessonWatch> needs;
};

struct Lesson {
    std::string id; // the file's name, less .lesson
    std::string name;
    AircraftClass teaches = AircraftClass::light_aircraft;
    std::vector<LessonStage> stages;
};

// One lesson's file. Throws LessonError naming the line of anything it cannot
// read, and for a lesson with no name, no class or no stages.
Lesson parse_lesson(const std::string& id, std::string_view text);

// Every lesson in `data`/lessons, by id. Throws LessonError for a file it
// cannot read.
std::vector<Lesson> read_lessons(const std::filesystem::path& data);

// The figures a lesson names - any of "rotate", "climb", "vref" and "stall"
// that appear anywhere in it, in that order, without repeats. `start` is not
// among them: it is read off the aeroplane when a stage begins rather than
// published about it.
std::vector<std::string> figures_named(const Lesson& lesson);

// **Why this aeroplane cannot be taught this lesson**, or an empty string if
// it can. A lesson belongs to a class, but the figures belong to the
// aeroplane, and the two do not always meet: eleven of the sixteen in the
// roster publish no stall speed and most publish no rate of climb, so a
// class's lesson can name a figure that one of its aeroplanes has not got.
// `departure_speeds` and `approach_speeds` throw rather than guess, which is
// right - a reference speed invented means nothing - and this says the same
// thing without throwing, so that a caller can leave an aeroplane out and say
// which figure it was missing.
std::string cannot_be_taught(const Lesson& lesson, const LessonSpeeds& speeds);

} // namespace glideslope::sim
