#include "sim/lesson_run.hpp"

#include <map>
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

// The shortest way round from `from` to `to`, in degrees: negative to the
// left, positive to the right, always between -180 and 180. This is what
// makes a turn through north a turn rather than a jump of 350 degrees.
double the_short_way(double from, double to) {
    double turned = to - from;
    while (turned > 180.0) {
        turned -= 360.0;
    }
    while (turned < -180.0) {
        turned += 360.0;
    }
    return turned;
}

// A figure, with `start` resolved against what the property read when the
// stage began.
double figure(const LessonNumber& number, const LessonSpeeds& speeds,
              const std::map<std::string, double>& began, const std::string& property) {
    if (number.reference == "start") {
        const auto it = began.find(property);
        return (it == began.end() ? 0.0 : it->second) + number.offset;
    }
    return figure_of(number, speeds);
}

bool met(const LessonWatch& watch, double value, const LessonSpeeds& speeds,
         const std::map<std::string, double>& began) {
    const double low = figure(watch.low, speeds, began, watch.property);
    if (watch.banded) {
        return value >= low && value <= figure(watch.high, speeds, began,
                                               watch.property);
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
        if (!read(aircraft, need.property, value)) {
            continue;
        }
        if (!met(need, value, speeds_, began_)) {
            debrief_.push_back({need.fault, stage.name, tick});
        }
    }
}

// What every property this stage watches reads now, so that a `start` in it
// has something to mean.
bool LessonRun::read(const Aircraft& aircraft, const std::string& property,
                     double& out) const {
    if (property == "lesson/turned-deg") {
        double heading = 0.0;
        if (!value_of(aircraft, "attitude/psi-deg", heading)) {
            return false;
        }
        out = the_short_way(heading_at_start_deg_, heading);
        return true;
    }
    return value_of(aircraft, property, out);
}

void LessonRun::remember_the_start(const Aircraft& aircraft) {
    began_.clear();
    (void)value_of(aircraft, "attitude/psi-deg", heading_at_start_deg_);
    if (finished()) {
        return;
    }
    const LessonStage& stage = lesson_.stages[stage_];
    const auto note = [&](const std::string& property) {
        double value = 0.0;
        if (read(aircraft, property, value)) {
            began_[property] = value;
        }
    };
    note(stage.until_property);
    for (const LessonWatch& watch : stage.holds) {
        note(watch.property);
    }
    for (const LessonWatch& watch : stage.needs) {
        note(watch.property);
    }
}

void LessonRun::update(const Aircraft& aircraft, std::int64_t tick) {
    // The first tick of the lesson is the first stage own beginning.
    if (!noted_) {
        remember_the_start(aircraft);
        noted_ = true;
    }
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
            if (!read(aircraft, stage.holds[i].property, value)) {
                continue;
            }
            if (!met(stage.holds[i], value, speeds_, began_)) {
                debrief_.push_back({stage.holds[i].fault, stage.name, tick});
                already_[i] = true;
            }
        }

        // Is the stage over?
        double ending = 0.0;
        if (!read(aircraft, stage.until_property, ending)) {
            return; // nothing here can end it; a test catches that
        }
        const double ends_at =
            figure(stage.until_value, speeds_, began_, stage.until_property);
        const bool over =
            stage.until_at_least ? ending >= ends_at : ending <= ends_at;
        if (!over) {
            return;
        }
        judge_needs(aircraft, tick);
        ++stage_;
        already_.assign(finished() ? 0 : lesson_.stages[stage_].holds.size(), false);
        remember_the_start(aircraft);
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
