#include "sim/instructor.hpp"

#include "sim/figures.hpp"
#include "sim/plan.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace glideslope::sim {
namespace {

constexpr int steps_per_second = 120;
constexpr double degrees = 57.29577951308232;
constexpr double metres_per_nm = 1852.0;
constexpr double feet_per_metre = 3.280839895013123;

// **Where a lesson in the air is flown** when it names no height: high enough
// that a turn, a climb or a descent is nowhere near the ground.
constexpr double in_the_air_ft = 3000.0;

// The figures file of a model.
PublishedFigures figures_of(const std::filesystem::path& data, const std::string& model) {
    return read_published_figures(data / "figures" / (model + ".xml"));
}

void load_as_measured(Aircraft& aircraft, const PublishedFigures& figures,
                      const FigureSpec* spec) {
    const std::string& name = spec != nullptr ? spec->loading : std::string();
    const auto it = figures.loadings.find(name);
    aircraft.load(it != figures.loadings.end() ? it->second.loading : figures.loading);
}

// The pilot's hands, before a person's are on the controls: somewhere a pilot
// might hold them. The caller sets them from the first step it has its own.
Controls hands_at_rest(double flaps, double gear) {
    Controls c;
    c.throttle = 0.55;
    c.flaps = flaps;
    c.gear = gear;
    return c;
}

// **A flying boat set on the water** settles on her hull for half a minute
// at idle before anything is asked of her.
void settle_afloat(Aircraft& aircraft) {
    const Controls idle;
    for (int i = 0; i < 30 * steps_per_second; ++i) {
        aircraft.set_controls(idle);
        aircraft.step();
    }
}

} // namespace

LessonSpeeds lesson_speeds(const std::filesystem::path& data, const std::string& model) {
    LessonSpeeds speeds;
    try {
        const auto departure = book_departure_speeds(data, model);
        speeds.rotate_kts = departure.rotate_kts;
        speeds.climb_kts = departure.climb_kts;
    } catch (const std::exception&) {
    }
    try {
        const auto approach = approach_speeds(data, model);
        speeds.vref_kts = approach.vref_kts;
        speeds.stall_kts = approach.stall_kts;
    } catch (const std::exception&) {
    }
    return speeds;
}

DepartureSpeeds book_departure_speeds(const std::filesystem::path& data,
                                      const std::string& model) {
    DepartureSpeeds speeds = departure_speeds(data, model);
    if (speeds.measured_from_model) {
        throw std::runtime_error(model + "'s take-off speeds are measured from its model at "
                                         "one weight, for a plan, not taken from a book");
    }
    return speeds;
}

void load_as_its_figures_were_measured(Aircraft& aircraft, const std::filesystem::path& data,
                                       const std::string& model) {
    try {
        const auto figures = figures_of(data, model);
        const FigureSpec* climb = nullptr;
        for (const auto& spec : figures.figures) {
            if (spec.flight == "climb_rate") {
                climb = &spec;
            }
        }
        if (climb != nullptr) {
            load_as_measured(aircraft, figures, climb);
        } else if (!figures.approach_loading.empty()) {
            // With no climb figure, at her approach speed's loading: the
            // 747-400's speeds are Boeing's at her maximum landing weight.
            const auto it = figures.loadings.find(figures.approach_loading);
            if (it != figures.loadings.end()) {
                aircraft.load(it->second.loading);
            }
        }
    } catch (const std::exception&) {
    }
}

void load_for_the_take_off(Aircraft& aircraft, const std::filesystem::path& data,
                           const std::string& model) {
    const auto figures = figures_of(data, model);
    for (const auto& spec : figures.figures) {
        if (spec.flight == "takeoff_ground_roll" && spec.conditions.count("lift_off_kcas") != 0) {
            load_as_measured(aircraft, figures, &spec);
            return;
        }
    }
    // A jet rotates from its stall at its field length's take-off flap, as
    // `departure_speeds` works it out, so it flies at that stall's loading.
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
    const FigureSpec* chosen = nullptr;
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

void load_for_the_approach(Aircraft& aircraft, const std::filesystem::path& data,
                           const std::string& model) {
    const auto figures = figures_of(data, model);
    // A manual's own approach speed is for the weight it names.
    if (figures.approach_kcas > 0.0) {
        aircraft.load(figures.loadings.at(figures.approach_loading).loading);
        return;
    }
    const FigureSpec* chosen = nullptr;
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

double a_climb_it_can_manage(const std::filesystem::path& data, const std::string& model,
                             double most_fpm) {
    try {
        for (const auto& spec : figures_of(data, model).figures) {
            if (spec.flight == "climb_rate" && spec.published > 0.0) {
                return std::min(most_fpm, 0.7 * spec.published);
            }
        }
    } catch (const std::exception&) {
    }
    return most_fpm;
}

namespace {

Demonstration demonstrated_as(const Lesson& lesson) {
    if (!lesson.demonstration) {
        throw LessonError(lesson.id + ".lesson does not say how it is demonstrated: "
                                      "instructor EXERCISE");
    }
    return *lesson.demonstration;
}

// **A lesson teaches a class**, and its bands are that class's: a fighter's
// stall lesson flown in a Cessna judges her against a jet's. Throws why an
// aeroplane cannot be taught it.
void refuse_what_cannot_be_taught(const CatalogueEntry& entry, const Lesson& lesson,
                                  const LessonSpeeds& speeds) {
    if (entry.aircraft_class != lesson.teaches) {
        throw LessonError(entry.id + " is " + std::string(name_of(entry.aircraft_class)) +
                          ", and " + lesson.id + " teaches " +
                          std::string(name_of(lesson.teaches)));
    }
    if (const std::string why = cannot_be_taught(lesson, speeds); !why.empty()) {
        throw LessonError(entry.id + " cannot be taught " + lesson.id + ": " + why);
    }
}

} // namespace

Instructor::Instructor(const std::filesystem::path& data, const CatalogueEntry& entry,
                       Lesson lesson, const Runway& runway, std::shared_ptr<Terrain> terrain)
    : data_(data), entry_(entry), runway_(runway), speeds_(lesson_speeds(data, entry.model)),
      run_(lesson, speeds_), demonstration_(demonstrated_as(lesson)) {
    refuse_what_cannot_be_taught(entry, lesson, speeds_);
    own_aircraft_ = std::make_unique<Aircraft>(data / "jsbsim", entry.model);
    aircraft_ = own_aircraft_.get();
    aircraft_->set_terrain(std::move(terrain));
    making_ = &own_controller_;
    begin(lesson);
    making_ = nullptr;
}

Instructor::Instructor(const std::filesystem::path& data, const CatalogueEntry& entry,
                       Lesson lesson, const Runway& runway, Aircraft& aircraft,
                       std::unique_ptr<Controller>& controller)
    : data_(data), entry_(entry), runway_(runway), speeds_(lesson_speeds(data, entry.model)),
      run_(lesson, speeds_), demonstration_(demonstrated_as(lesson)) {
    refuse_what_cannot_be_taught(entry, lesson, speeds_);
    aircraft_ = &aircraft;
    making_ = &controller;
    begin(lesson);
    making_ = nullptr;
}

void Instructor::make_controller(const Controls& held) {
    *making_ = std::make_unique<Controller>(*aircraft_, held);
    controller_ = making_->get();
}

void Instructor::begin(const Lesson& lesson) {
    start_heading_deg_ = runway_.heading_deg;
    // **Every figure she is flown by, read first**, before she is loaded,
    // moved or given a controller: an aeroplane whose figures throw is
    // refused as she was, and an owner's aircraft is left as it was.
    const bool from_the_runway =
        demonstration_ == Demonstration::take_off || demonstration_ == Demonstration::circuit;
    const bool in_the_air = demonstration_ == Demonstration::climb_and_descent ||
                            demonstration_ == Demonstration::stalls ||
                            demonstration_ == Demonstration::turns;
    std::optional<DepartureSpeeds> dep;
    if (from_the_runway) {
        dep = book_departure_speeds(data_, entry_.model);
    }
    if (demonstration_ == Demonstration::circuit || demonstration_ == Demonstration::approach ||
        demonstration_ == Demonstration::stalls) {
        approach_ = approach_speeds(data_, entry_.model);
    }
    if (demonstration_ == Demonstration::circuit) {
        joined_.climb_fpm = a_climb_it_can_manage(data_, entry_.model, 2000.0);
    }
    if (in_the_air) {
        climb_fpm_ = a_climb_it_can_manage(data_, entry_.model);
    }

    switch (demonstration_) {
    case Demonstration::take_off:
    case Demonstration::circuit: {
        // On the threshold, lined up, the engine running.
        InitialConditions ic;
        ic.latitude_deg = runway_.threshold_lat_deg;
        ic.longitude_deg = runway_.threshold_lon_deg;
        ic.altitude_ft = runway_.elevation_ft + (entry_.seaplane ? 6.0 : 0.0);
        ic.terrain_elevation_ft = runway_.elevation_ft;
        ic.heading_deg = runway_.heading_deg;
        ic.airspeed_kts = 0.0;
        ic.engine_running = true;
        ic.gear = 1.0;
        // **A circuit is flown at the weight its approach speed belongs
        // to**: a training circuit is flown light, and the approach at the
        // end of it is judged against a third above the landing stall.
        if (demonstration_ == Demonstration::circuit) {
            load_for_the_approach(*aircraft_, data_, entry_.model);
        } else {
            load_for_the_take_off(*aircraft_, data_, entry_.model);
        }
        aircraft_->initialize(ic);
        if (entry_.seaplane) {
            settle_afloat(*aircraft_);
        }
        make_controller(Controls{});
        if (demonstration_ == Demonstration::take_off) {
            controller_->to_ai_take_off(runway_, *dep);
            controller_->set_pilot(hands_at_rest(0.0, 1.0));
        } else {
            // The climb out: straight ahead to 650 ft, clear of where the
            // lesson's climb out ends, and joined to the go-around's
            // circuit there.
            controller_->to_ai_take_off(runway_, *dep, 650.0);
            joined_.from_take_off = true;
            joined_.climb_kts = dep->initial_climb_kts;
            joined_.flaps = dep->flap;
            Controls pilot;
            pilot.throttle = 0.25;
            pilot.gear = 1.0;
            controller_->set_pilot(pilot);
        }
        part_ = Part::demonstrating;
        break;
    }
    case Demonstration::approach: {
        // **Two miles out on the glidepath, established**: the landing flap
        // down and coming down the path, at the weight her reference speed
        // was measured at.
        load_as_its_figures_were_measured(*aircraft_, data_, entry_.model);
        const double out_m = 2.0 * metres_per_nm;
        const double heading = runway_.heading_deg / degrees;
        InitialConditions ic;
        ic.latitude_deg = runway_.threshold_lat_deg +
                          (-out_m * std::cos(heading)) /
                              metres_per_degree_latitude(runway_.threshold_lat_deg);
        ic.longitude_deg = runway_.threshold_lon_deg +
                           (-out_m * std::sin(heading)) /
                               metres_per_degree_longitude(runway_.threshold_lat_deg);
        ic.altitude_ft = runway_.elevation_ft +
                         (out_m + approach_.aim_m) * std::tan(3.0 / degrees) * feet_per_metre;
        ic.terrain_elevation_ft = runway_.elevation_ft;
        ic.heading_deg = runway_.heading_deg;
        ic.airspeed_kts = approach_.vref_kts;
        ic.engine_running = true;
        ic.gear = 1.0;
        load_for_the_approach(*aircraft_, data_, entry_.model);
        ic.flaps = approach_.flap;
        ic.speedbrake = approach_.speedbrake;
        ic.flight_path_deg = -3.0;
        ic.trim = true;
        aircraft_->initialize(ic);
        Controls flying;
        flying.throttle = 0.4;
        flying.gear = 1.0;
        make_controller(flying);
        controller_->to_ai_approach(runway_, approach_);
        Controls pilot;
        pilot.throttle = 0.25;
        pilot.gear = 1.0;
        controller_->set_pilot(pilot);
        part_ = Part::demonstrating;
        break;
    }
    case Demonstration::climb_and_descent:
        begin_in_the_air(in_the_air_ft, 0.0);
        settle_until_ = 30 * steps_per_second;
        break;
    case Demonstration::stalls:
        // Flaps as they are for the landing, which is what `stall` is the
        // stall speed in; at the height the lesson is practised at.
        begin_in_the_air(lesson.practised_at_ft.value_or(in_the_air_ft), approach_.flap);
        settle_until_ = 20 * steps_per_second;
        break;
    case Demonstration::turns:
        begin_in_the_air(in_the_air_ft, 0.0);
        settle_until_ = 15 * steps_per_second;
        break;
    }
}

Instructor::~Instructor() = default;

void Instructor::begin_in_the_air(double agl_ft, double flaps) {
    InitialConditions ic;
    ic.latitude_deg = runway_.threshold_lat_deg;
    ic.longitude_deg = runway_.threshold_lon_deg;
    ic.altitude_ft = runway_.elevation_ft + agl_ft;
    ic.terrain_elevation_ft = runway_.elevation_ft;
    ic.heading_deg = start_heading_deg_;
    ic.airspeed_kts = entry_.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    load_as_its_figures_were_measured(*aircraft_, data_, entry_.model);
    aircraft_->initialize(ic);

    Controls held;
    held.throttle = 0.7;
    held.flaps = flaps;
    make_controller(held);
    controller_->to_ai();
    AutopilotModes modes = controller_->autopilot()->modes();
    modes.heading_deg = start_heading_deg_;
    modes.altitude_ft = ic.altitude_ft;
    modes.airspeed_kts = entry_.start_airspeed_kts;
    if (demonstration_ == Demonstration::climb_and_descent) {
        // Climbing on rate alone while she settles: the height to level off
        // at is set once the lesson has begun, because the lesson asks for
        // nine hundred feet from there.
        modes.altitude_ft.reset();
        modes.vertical_speed_fpm = climb_fpm_;
        modes.airspeed_kts = speeds_.climb_kts;
    }
    controller_->autopilot()->set(modes);
    Controls pilot = hands_at_rest(flaps, 0.0);
    pilot.elevator = 0.02;
    controller_->set_pilot(pilot);
    part_ = Part::settling;
}

void Instructor::leave_in_the_stall(double seconds) {
    dawdle_steps_ = static_cast<std::int64_t>(std::lround(seconds * steps_per_second));
}

const LessonStage* Instructor::stage() const {
    return run_.finished() ? nullptr : &run_.lesson().stages[run_.stage()];
}

// **What the instructor asks for, this step**, as the tests' demonstrations
// asked it (tests/unit/test_lesson.cpp): only while demonstrating.
void Instructor::instruct() {
    const std::int64_t since = tick_ - settle_until_;
    switch (demonstration_) {
    case Demonstration::take_off:
    case Demonstration::approach:
        return; // the take-off's and the approach's autopilots fly it all
    case Demonstration::turns:
        if (since == 0) {
            AutopilotModes m = controller_->autopilot()->modes();
            m.heading_deg = start_heading_deg_ - 90.0;
            m.altitude_ft = runway_.elevation_ft + in_the_air_ft;
            m.airspeed_kts = entry_.start_airspeed_kts;
            controller_->autopilot()->set(m);
        }
        return;
    case Demonstration::climb_and_descent: {
        AutopilotModes m = controller_->autopilot()->modes();
        const double asl = aircraft_->property("position/h-sl-ft");
        if (since == 0) {
            m.altitude_ft = asl + 1100.0;
            controller_->autopilot()->set(m);
        } else if (!descending_ && run_.stage() >= 2) {
            descending_ = true;
            m.altitude_ft = asl - 700.0;
            m.vertical_speed_fpm = 600.0;
            m.airspeed_kts = speeds_.climb_kts + 25.0;
            controller_->autopilot()->set(m);
        }
        return;
    }
    case Demonstration::stalls: {
        AutopilotModes m = controller_->autopilot()->modes();
        if (since == 0) {
            fly_the_stall_entry(m, speeds_);
            controller_->autopilot()->set(m);
        } else if (!recovering_ && run_.stage() >= 1) {
            if (stalled_at_ < 0) {
                stalled_at_ = since;
            }
            if (since - stalled_at_ < dawdle_steps_) {
                return;
            }
            recovering_ = true;
            fly_the_stall_recovery(m, run_.lesson(), speeds_);
            controller_->autopilot()->set(m);
        }
        return;
    }
    case Demonstration::circuit: {
        // **The pattern is the go-around's circuit** (sim/circuit.hpp),
        // joined from the take-off at 650 ft; on final, the approach.
        if (climbing_out_ && aircraft_->property("position/h-agl-ft") >= 650.0) {
            climbing_out_ = false;
            controller_->to_ai();
            if (entry_.aircraft_class == AircraftClass::light_aircraft) {
                joined_.flaps = flown_flaps_;
            }
            pattern_.emplace(*aircraft_, runway_, approach_, joined_);
        }
        if (pattern_ && !pattern_->on_final()) {
            AutopilotModes m = pattern_->modes();
            if (pattern_->on_final()) {
                approach_flown_ = true;
                controller_->to_ai_approach(runway_, approach_);
            } else {
                if (fault_) {
                    fault_(m);
                }
                controller_->autopilot()->set(m);
            }
        }
        return;
    }
    }
}

Controls Instructor::step() {
    const Controls flown = fly();
    aircraft_->set_controls(flown);
    aircraft_->step();
    stepped();
    return flown;
}

Controls Instructor::fly() {
    if (part_ == Part::settling && tick_ >= settle_until_) {
        part_ = Part::demonstrating;
    }
    if (part_ == Part::demonstrating && run_.finished() && !(showing_ && showing_())) {
        part_ = Part::handed_over;
        handed_over_at_ = tick_;
        controller_->to_pilot();
    }
    if (part_ == Part::demonstrating) {
        instruct();
    }
    Controls flown = controller_->fly();
    if (demonstration_ == Demonstration::circuit && part_ == Part::demonstrating) {
        flown_flaps_ = flown.flaps;
        if (pattern_ && !approach_flown_) {
            pattern_->configure(flown);
        }
    }
    return flown;
}

void Instructor::stepped() {
    if (part_ == Part::demonstrating) {
        run_.update(*aircraft_, tick_);
    }
    ++tick_;
}

} // namespace glideslope::sim
