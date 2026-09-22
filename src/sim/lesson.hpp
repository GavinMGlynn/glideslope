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
//                                 `<=` or `>=`, and the figure
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

// What must hold, or must have become true. `low`/`high` are the band for a
// `hold`; for a `need`, `low` is the figure and `at_least` says which way.
struct LessonWatch {
    std::string property;
    double low = 0.0;
    double high = 0.0;
    bool banded = false;   // a `hold`: both ends count
    bool at_least = false; // a `need`: `>=` when true, `<=` when false
    std::string fault;     // what the debrief says when it is not met
};

struct LessonStage {
    std::string name;
    std::vector<std::string> doing; // what to do, in the order given
    // What ends the stage.
    std::string until_property;
    double until_value = 0.0;
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

} // namespace glideslope::sim
