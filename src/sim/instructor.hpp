#pragma once

// **The instructor**: the AI pilot flying a lesson first, as a demonstration,
// and then handing the aeroplane to its pilot - a controller swap and nothing
// more (REQUIREMENTS.md 4.3 and section 5).
//
// A lesson says how it is demonstrated (`instructor` in its file, src/sim/
// lesson.hpp); this is what each of those is. They are the six the tests'
// demonstrations flew - a take-off from the runway, an approach from two miles
// out on the glidepath, a whole circuit from the runway, and in the air a
// climb and descent, a stall and its recovery, and a turn - and they fly the
// same autopilots the AI pilot flies everything with: the take-off's, the
// approach's, the go-around's circuit, and the plain one asked for a height,
// a heading or a speed.
//
// **Where a lesson begins is the instructor's too**, since it is where the
// demonstration starts: on the runway's threshold, two miles out on its
// approach, or in the air over it at the height the lesson is practised at
// (3,000 ft where it names none). The aeroplane is loaded as the figures its
// lesson names were measured at, because a reference speed belongs to a
// weight.
//
// **The demonstration is judged as it is flown** by the lesson's own
// `LessonRun`, and once that is over the controls are handed to the pilot
// on the next step. The pilot's controls are the caller's to set
// (`controller().set_pilot`) from then on - a person's hands, or a test's.
//
// This is the simulation's: it reads and flies an aircraft, and draws
// nothing.

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/circuit.hpp"
#include "sim/controller.hpp"
#include "sim/departure.hpp"
#include "sim/lander.hpp"
#include "sim/lesson.hpp"
#include "sim/lesson_run.hpp"
#include "sim/terrain.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace glideslope::sim {

// **This aeroplane's own figures, for a lesson**, resolved without throwing:
// what she does not publish reads nought (and `cannot_be_taught` says so).
// Her take-off speeds are her book's alone - an aeroplane whose take-off
// speeds are measured from her model, for a plan, has none here.
LessonSpeeds lesson_speeds(const std::filesystem::path& data, const std::string& model);

// The take-off speeds her book gives (sim::departure_speeds); throws for an
// aeroplane whose are measured from her model, as a lesson teaches the book.
DepartureSpeeds book_departure_speeds(const std::filesystem::path& data,
                                      const std::string& model);

// **Loaded as her figures were measured**: at her climbing speed's loading,
// or with none, her approach speed's.
void load_as_its_figures_were_measured(Aircraft& aircraft, const std::filesystem::path& data,
                                       const std::string& model);
// At the loading her lift-off speed was measured at.
void load_for_the_take_off(Aircraft& aircraft, const std::filesystem::path& data,
                           const std::string& model);
// At the loading her approach speed belongs to.
void load_for_the_approach(Aircraft& aircraft, const std::filesystem::path& data,
                           const std::string& model);
// A rate of climb she has, for a demonstration to ask for: seven tenths of
// her published rate, and never more than `most_fpm`; `most_fpm` where she
// publishes none.
double a_climb_it_can_manage(const std::filesystem::path& data, const std::string& model,
                             double most_fpm = 600.0);

class Instructor {
public:
    enum class Part {
        settling,      // in the air, the autopilot settling her before the lesson
        demonstrating, // the instructor flying the lesson, judged as it goes
        handed_over,   // the pilot's
    };

    // **The lesson begun**: `entry`'s aeroplane built from `data`, over
    // `terrain`, put where `lesson` begins at `runway`, and given to the AI.
    // Throws LessonError for a lesson that does not say how it is
    // demonstrated, for an aeroplane not of the class it teaches, or for one
    // it cannot be taught to (`cannot_be_taught`).
    Instructor(const std::filesystem::path& data, const CatalogueEntry& entry, Lesson lesson,
               const Runway& runway, std::shared_ptr<Terrain> terrain);
    ~Instructor();
    Instructor(const Instructor&) = delete;
    Instructor& operator=(const Instructor&) = delete;

    // One step: the instructor says what to fly, the controller flies it, the
    // aircraft steps, and the lesson judges it. Once the lesson is over, the
    // controls are handed to the pilot. Returns the controls flown.
    Controls step();

    Part part() const { return part_; }
    // The step the controls were handed over at, or -1.
    std::int64_t handed_over_at() const { return handed_over_at_; }
    std::int64_t steps() const { return tick_; }

    const LessonRun& run() const { return run_; }
    const Lesson& lesson() const { return run_.lesson(); }
    Aircraft& aircraft() { return *aircraft_; }
    const Aircraft& aircraft() const { return *aircraft_; }
    Controller& controller() { return *controller_; }

    // The stage being flown, or nothing once the lesson is over.
    const LessonStage* stage() const;

    // **For a test that watches a stall's recovery**: the instructor leaves
    // her this long in the stall before asking for the recovery; and keeps
    // the demonstration going past the lesson's end for as long as `showing`
    // says so, before handing over. `recovering` is whether it has asked.
    void leave_in_the_stall(double seconds);
    void keep_showing_while(std::function<bool()> showing) { showing_ = std::move(showing); }
    bool recovering() const { return recovering_; }

private:
    void begin_in_the_air(double agl_ft, double flaps);
    void instruct();

    std::filesystem::path data_;
    CatalogueEntry entry_;
    Runway runway_;
    LessonSpeeds speeds_;
    std::unique_ptr<Aircraft> aircraft_;
    std::unique_ptr<Controller> controller_;
    LessonRun run_;
    Demonstration demonstration_ = Demonstration::turns;
    Part part_ = Part::settling;
    std::int64_t tick_ = 0;
    std::int64_t settle_until_ = 0;
    std::int64_t handed_over_at_ = -1;
    // In the air: where she began, and what the autopilot was first asked.
    double start_heading_deg_ = 0.0;
    double climb_fpm_ = 600.0;
    // A stall: recovering. A climb: descending. Both once, on the stage.
    bool recovering_ = false;
    std::int64_t dawdle_steps_ = 0;
    std::int64_t stalled_at_ = -1;
    std::function<bool()> showing_;
    bool descending_ = false;
    // A circuit.
    std::optional<GoAroundCircuit> pattern_;
    CircuitEntry joined_;
    ApproachSpeeds approach_;
    bool climbing_out_ = true;
    bool approach_flown_ = false;
    double flown_flaps_ = 0.0;
};

} // namespace glideslope::sim
