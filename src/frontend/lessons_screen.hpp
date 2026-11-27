#pragma once

// **The window client's lessons screen**: the lessons for her aeroplane's
// class, one chosen, and once it is asked for, what the server's instructor
// says of it - refused, settling her, each stage it flies, the hand-over with
// the debrief, or ended before it (`LESSON`, docs/TRANSPORT.md).
//
// It draws nothing: it is the screen's state and the words it shows, a line
// each, which gfx::lesson_lines spells and wraps and the HUD draws where the
// checklist is. So every state it can be in is held by a unit test without a
// window, and each is shot by a test that draws it.
//
// **The words of a stage are the client's own copy of the lesson's**: the
// server says which stage, and the debrief - which is the server's alone,
// because only the server flew it.

#include "net/messages.hpp"
#include "sim/lesson.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::frontend {

class LessonsScreen {
public:
    enum class State {
        closed,        // nothing shown
        choosing,      // the list, one chosen; asked for or not
        refused,       // the server would not begin it, and why
        settling,      // the instructor settling her in the air first
        demonstrating, // the instructor flying a stage
        handed_over,   // hers, and the debrief
        ended,         // ended before the hand-over, and why
    };
    // Every state, in that order, for a test that walks them.
    static constexpr std::size_t states = 7;
    static const char* name_of(State state);

    // `lessons` are every lesson there is; the screen keeps those for
    // `aircraft_class`, in the order given. `aircraft` names her, as the
    // catalogue does.
    LessonsScreen(const std::vector<sim::Lesson>& lessons, sim::AircraftClass aircraft_class,
                  std::string aircraft);

    State state() const { return state_; }
    std::size_t count() const { return lessons_.size(); }
    const std::vector<sim::Lesson>& lessons() const { return lessons_; }
    // The one chosen, while there are any.
    const sim::Lesson* chosen() const;

    // **F2**: shown as the list, or hidden whatever it shows.
    void toggle();
    // **F3**: the next lesson in the list, round to the first; the list
    // shown if it was not.
    void next();
    // **Chosen by its id** (`--lesson`): the list shown, with it chosen.
    // False, and nothing changed, for an id not in the list.
    bool choose(const std::string& id);
    // **Return**: the chosen lesson's id, to ask the server for, once; then
    // nothing until the server has said something of it. Nothing while the
    // list is not shown.
    std::optional<std::string> ask();
    bool asked() const { return asked_.has_value(); }

    // **What the server said**, for its own aircraft: the screen shows it.
    void told(const net::LessonTold& told);

    // **The words shown**, a line each; none while closed.
    std::vector<std::string> lines() const;

private:
    const sim::Lesson* find(const std::string& id) const;

    std::vector<sim::Lesson> lessons_;
    std::string aircraft_;
    std::string class_name_;
    State state_ = State::closed;
    std::size_t chosen_ = 0;
    std::optional<std::string> asked_;
    net::LessonTold last_;
};

} // namespace glideslope::frontend
