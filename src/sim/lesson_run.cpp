#include "sim/lesson_run.hpp"

#include <stdexcept>

namespace glideslope::sim {
namespace {

// A property, or nothing if this aircraft has not got it.
bool value_of(const Aircraft& aircraft, const std::string& property, double& out) {
    try {
        out = aircraft.property(property);
        return true;
    } catch (const std::out_of_range&) {
        return false;
    }
}

bool met(const LessonWatch& watch, double value) {
    if (watch.banded) {
        return value >= watch.low && value <= watch.high;
    }
    return watch.at_least ? value >= watch.low : value <= watch.low;
}

} // namespace

LessonRun::LessonRun(Lesson lesson) : lesson_(std::move(lesson)) {
    if (!lesson_.stages.empty()) {
        already_.assign(lesson_.stages.front().holds.size(), false);
    }
}

void LessonRun::judge_needs(const Aircraft& aircraft, std::int64_t tick) {
    const LessonStage& stage = lesson_.stages[stage_];
    for (const LessonWatch& need : stage.needs) {
        double value = 0.0;
        if (!value_of(aircraft, need.property, value)) {
            continue;
        }
        if (!met(need, value)) {
            debrief_.push_back({need.fault, stage.name, tick});
        }
    }
}

void LessonRun::update(const Aircraft& aircraft, std::int64_t tick) {
    // **More than one stage may end on a tick**, when a stage's `until` is
    // already true as it begins. The loop is what lets the lesson move on
    // rather than sitting on a stage that was over before it started.
    while (!finished()) {
        const LessonStage& stage = lesson_.stages[stage_];

        // Every band that must hold, judged now, and recorded once.
        for (std::size_t i = 0; i < stage.holds.size() && i < already_.size(); ++i) {
            if (already_[i]) {
                continue;
            }
            double value = 0.0;
            if (!value_of(aircraft, stage.holds[i].property, value)) {
                continue;
            }
            if (!met(stage.holds[i], value)) {
                debrief_.push_back({stage.holds[i].fault, stage.name, tick});
                already_[i] = true;
            }
        }

        // Is the stage over?
        double ending = 0.0;
        if (!value_of(aircraft, stage.until_property, ending)) {
            return; // nothing here can end it; a test catches that
        }
        const bool over = stage.until_at_least ? ending >= stage.until_value
                                               : ending <= stage.until_value;
        if (!over) {
            return;
        }
        judge_needs(aircraft, tick);
        ++stage_;
        already_.assign(finished() ? 0 : lesson_.stages[stage_].holds.size(), false);
    }
}

std::vector<std::string> LessonRun::debrief_lines() const {
    std::vector<std::string> out;
    out.reserve(debrief_.size());
    for (const Fault& fault : debrief_) {
        out.push_back(fault.what);
    }
    return out;
}

} // namespace glideslope::sim
