#pragma once

// A lesson being flown, on one aircraft, and the debrief it leaves.
//
// **The stages are flown in order.** One stage is current at a time; it ends
// when its `until` is met, and the next begins on the same tick.
//
// **A `hold` is judged on every tick of its stage**, because an approach
// flown too fast is wrong while it is happening. It is recorded once however
// long it goes on: a debrief that said "too fast" four hundred times would
// name the fault and bury it.
//
// **A `need` is judged once, when the stage ends**, because flap down by the
// time you turn final is not a fault until you have turned final. A lesson
// that is abandoned part-way leaves the current stage's needs unjudged: they
// have not come due.
//
// **The debrief is what to do differently, in the order it happened, and
// nothing else.** There is no score and no mark (`FEATURES.md`). A lesson
// flown to the book leaves it empty.
//
// This is the simulation's own: it reads the aircraft and keeps a state, and
// draws nothing.

#include "sim/aircraft.hpp"
#include "sim/lesson.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace glideslope::sim {

// One thing to do differently, and where it happened.
struct Fault {
    std::string what;  // the lesson's own words
    std::string stage; // the stage it happened in
    std::int64_t at_tick = -1;
};

class LessonRun {
public:
    // `speeds` are this aeroplane own published figures, which is what a
    // `rotate` or `climb` in the lesson resolves against. A lesson with no
    // named figures in it does not care what they are.
    LessonRun(Lesson lesson, LessonSpeeds speeds);

    // Read the aircraft, judge the current stage, and move on if it is over.
    // `tick` is the simulation's step count.
    //
    // **A property this aircraft has not got is left alone** rather than
    // throwing: a lesson is data, and data that names a state the aeroplane
    // has not got is a fault for a test to catch, not a reason to end a
    // flight.
    void update(const Aircraft& aircraft, std::int64_t tick);

    const Lesson& lesson() const { return lesson_; }
    // Which stage is being flown. Equal to the number of stages once the
    // lesson is over.
    std::size_t stage() const { return stage_; }
    bool finished() const { return stage_ >= lesson_.stages.size(); }
    // How far through: the stages completed.
    std::size_t completed() const { return stage_; }

    // What to do differently, in the order it happened.
    const std::vector<Fault>& debrief() const { return debrief_; }
    // The same as the lesson's own words alone, which is what a screen shows.
    std::vector<std::string> debrief_lines() const;

private:
    void judge_needs(const Aircraft& aircraft, std::int64_t tick);

    Lesson lesson_;
    LessonSpeeds speeds_;
    std::size_t stage_ = 0;
    // One flag per hold of the current stage, so a broken band is recorded
    // once rather than on every tick.
    std::vector<bool> already_;
    std::vector<Fault> debrief_;
};

} // namespace glideslope::sim
