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

bool met(const LessonWatch& watch, double value, const LessonSpeeds& speeds) {
    const double low = figure_of(watch.low, speeds);
    if (watch.banded) {
        return value >= low && value <= figure_of(watch.high, speeds);
    }
    return watch.at_least ? value >= low : value <= low;
}

} // namespace

LessonRun::LessonRun(Lesson lesson, LessonSpeeds speeds)
    : lesson_(std::move(lesson)), speeds_(speeds) {
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
        if (!met(need, value, speeds_)) {
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
            if (!met(stage.holds[i], value, speeds_)) {
                debrief_.push_back({stage.holds[i].fault, stage.name, tick});
                already_[i] = true;
            }
        }

        // Is the stage over?
        double ending = 0.0;
        if (!value_of(aircraft, stage.until_property, ending)) {
            return; // nothing here can end it; a test catches that
        }
        const double ends_at = figure_of(stage.until_value, speeds_);
        const bool over =
            stage.until_at_least ? ending >= ends_at : ending <= ends_at;
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
