#include "frontend/lessons_screen.hpp"

#include "sim/catalogue.hpp"

#include <algorithm>
#include <utility>

namespace glideslope::frontend {

const char* LessonsScreen::name_of(State state) {
    switch (state) {
    case State::closed:
        return "closed";
    case State::choosing:
        return "choosing";
    case State::refused:
        return "refused";
    case State::settling:
        return "settling";
    case State::demonstrating:
        return "demonstrating";
    case State::handed_over:
        return "handed-over";
    case State::ended:
        return "ended";
    }
    return "?";
}

LessonsScreen::LessonsScreen(const std::vector<sim::Lesson>& lessons,
                             sim::AircraftClass aircraft_class, std::string aircraft)
    : aircraft_(std::move(aircraft)), class_name_(sim::name_of(aircraft_class)) {
    for (const sim::Lesson& lesson : lessons) {
        if (lesson.teaches == aircraft_class) {
            lessons_.push_back(lesson);
        }
    }
}

const sim::Lesson* LessonsScreen::chosen() const {
    return lessons_.empty() ? nullptr : &lessons_[chosen_];
}

const sim::Lesson* LessonsScreen::find(const std::string& id) const {
    const auto it = std::find_if(lessons_.begin(), lessons_.end(),
                                 [&](const sim::Lesson& l) { return l.id == id; });
    return it == lessons_.end() ? nullptr : &*it;
}

void LessonsScreen::toggle() {
    state_ = state_ == State::closed ? State::choosing : State::closed;
}

void LessonsScreen::next() {
    if (state_ != State::choosing) {
        state_ = State::choosing;
        return;
    }
    if (!lessons_.empty() && !asked_) {
        chosen_ = (chosen_ + 1) % lessons_.size();
    }
}

bool LessonsScreen::choose(const std::string& id) {
    const auto it = std::find_if(lessons_.begin(), lessons_.end(),
                                 [&](const sim::Lesson& l) { return l.id == id; });
    if (it == lessons_.end()) {
        return false;
    }
    chosen_ = static_cast<std::size_t>(it - lessons_.begin());
    state_ = State::choosing;
    return true;
}

std::optional<std::string> LessonsScreen::ask() {
    if (state_ != State::choosing || lessons_.empty() || asked_) {
        return std::nullopt;
    }
    asked_ = lessons_[chosen_].id;
    return asked_;
}

void LessonsScreen::told(const net::LessonTold& told) {
    last_ = told;
    asked_.reset();
    switch (told.part) {
    case net::LessonPart::refused:
        state_ = State::refused;
        break;
    case net::LessonPart::settling:
        state_ = State::settling;
        break;
    case net::LessonPart::demonstrating:
        state_ = State::demonstrating;
        break;
    case net::LessonPart::handed_over:
        state_ = State::handed_over;
        break;
    case net::LessonPart::ended:
        state_ = State::ended;
        break;
    }
}

std::vector<std::string> LessonsScreen::lines() const {
    std::vector<std::string> out;
    const sim::Lesson* lesson = find(last_.lesson);
    const std::string name = lesson != nullptr ? lesson->name : last_.lesson;
    const std::string of = std::to_string(last_.stages);
    switch (state_) {
    case State::closed:
        return out;
    case State::choosing:
        out.push_back("Lessons for the " + aircraft_ + ", " + class_name_);
        if (lessons_.empty()) {
            out.push_back("  none teaches her class");
        }
        for (std::size_t i = 0; i < lessons_.size(); ++i) {
            out.push_back((i == chosen_ ? "+ " : "  ") + lessons_[i].name);
        }
        out.push_back("F3 next, Return fly it, F2 close");
        return out;
    case State::refused:
        out.push_back("Lesson refused: " + name);
        out.insert(out.end(), last_.lines.begin(), last_.lines.end());
        out.push_back("F2 close");
        return out;
    case State::settling:
        out.push_back(name);
        out.push_back("The instructor has control");
        out.push_back("Settling her before the lesson");
        return out;
    case State::demonstrating: {
        out.push_back(name);
        out.push_back("The instructor has control");
        const sim::LessonStage* stage =
            lesson != nullptr && last_.stage < lesson->stages.size() ? &lesson->stages[last_.stage]
                                                                      : nullptr;
        out.push_back("Stage " + std::to_string(last_.stage + 1) + " of " + of +
                      (stage != nullptr ? ": " + stage->name : std::string()));
        if (stage != nullptr) {
            for (const std::string& doing : stage->doing) {
                out.push_back("  " + doing);
            }
        }
        return out;
    }
    case State::handed_over:
        out.push_back(name);
        out.push_back("Handed over: you have control");
        out.push_back("Debrief: " + std::to_string(last_.stage) + " of " + of + " stages flown");
        if (last_.lines.empty()) {
            out.push_back("  Flown to the book: nothing to do differently");
        }
        for (const std::string& line : last_.lines) {
            out.push_back("  " + line);
        }
        out.push_back("F2 close");
        return out;
    case State::ended:
        out.push_back("Lesson ended: " + name);
        out.insert(out.end(), last_.lines.begin(), last_.lines.end());
        out.push_back("F2 close");
        return out;
    }
    return out;
}

} // namespace glideslope::frontend
