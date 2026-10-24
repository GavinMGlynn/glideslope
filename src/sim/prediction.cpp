#include "sim/prediction.hpp"

#include "sim/navigator.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <string>
#include <cmath>

namespace glideslope::sim {
namespace {
constexpr double metres_per_foot = 0.3048;

// The controls' names, in Controls::as_list's order.
constexpr std::array<const char*, Controls::control_count> control_names = {
    "elevator",          "aileron",           "rudder",           "throttle",
    "mixture",           "flaps",             "left brake",       "right brake",
    "pitch trim",        "propeller",         "gear",             "supercharger",
    "speedbrake",        "throttle offset 0", "throttle offset 1", "cooling flaps 0",
    "cooling flaps 1",   "reverse",           "stovl",            "roll trim",
    "yaw trim"};
// A control added to Controls without a name here would leave a null name.
static_assert(control_names.back() != nullptr, "every control needs its name");

} // namespace

// The first number of a snapshot not finite, by name.
std::optional<std::string> first_not_finite(const AircraftSnapshot& s) {
    const auto first = [](const auto& v, const char* name) -> std::optional<std::string> {
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (!std::isfinite(v[i])) {
                return std::string(name) + "[" + std::to_string(i) + "]";
            }
        }
        return std::nullopt;
    };
    for (const auto& [name, value] : std::initializer_list<std::pair<const char*, double>>{
             {"sim_time_s", s.sim_time_s},
             {"latitude_deg", s.latitude_deg},
             {"longitude_deg", s.longitude_deg},
             {"altitude_ft", s.altitude_ft},
             {"terrain_elevation_ft", s.terrain_elevation_ft}}) {
        if (!std::isfinite(value)) {
            return std::string(name);
        }
    }
    if (auto b = first(s.location_ecef_ft, "location_ecef_ft")) {
        return b;
    }
    if (auto b = first(s.attitude_local, "attitude_local")) {
        return b;
    }
    if (auto b = first(s.uvw_fps, "uvw_fps")) {
        return b;
    }
    if (auto b = first(s.pqr_radps, "pqr_radps")) {
        return b;
    }
    if (auto b = first(s.thruster_rpm, "thruster_rpm")) {
        return b;
    }
    for (const auto& [name, value] : s.properties) {
        if (!std::isfinite(value)) {
            return name;
        }
    }
    return std::nullopt;
}

std::optional<std::string> first_not_finite(const Controls& c) {
    const auto v = c.as_list();
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (!std::isfinite(v[i])) {
            return std::string(control_names[i]);
        }
    }
    return std::nullopt;
}

std::optional<std::string> first_not_finite(const Motion& m) {
    const auto first = [](const auto& v, const char* name) -> std::optional<std::string> {
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (!std::isfinite(v[i])) {
                return std::string(name) + "[" + std::to_string(i) + "]";
            }
        }
        return std::nullopt;
    };
    if (auto b = first(m.location_ecef_m, "location_ecef_m")) {
        return b;
    }
    if (auto b = first(m.attitude_local, "attitude_local")) {
        return b;
    }
    if (auto b = first(m.uvw_mps, "uvw_mps")) {
        return b;
    }
    return first(m.pqr_radps, "pqr_radps");
}

double how_far_apart_m(const AircraftState& a, const AircraftState& b) {
    const double over_ground =
        distance_m(a.latitude_deg, a.longitude_deg, b.latitude_deg, b.longitude_deg);
    const double up = (a.altitude_ft - b.altitude_ft) * metres_per_foot;
    return std::sqrt(over_ground * over_ground + up * up);
}

Prediction::Prediction(Aircraft& aircraft) : aircraft_(aircraft) {
    if (aircraft_.has_lift_system()) {
        laws_.emplace(aircraft_, Controls{});
    }
}

void Prediction::held_by_server(const Controls& held) {
    // The server's levers are an input too.
    if (!lost_) {
        if (const auto bad = first_not_finite(held)) {
            lost_ = Lost{"the server's held " + *bad, flying_step_};
        }
    }
    if (lost_) {
        return;
    }
    aircraft_.set_controls(held);
    commanded_ = held;
}

void Prediction::put(const Controls& controls) {
    if (laws_) {
        laws_->set_pilot(controls);
        commanded_ = apply(aircraft_, *laws_);
    } else {
        aircraft_.set_controls(controls);
        commanded_ = controls;
    }
}

void Prediction::step(std::uint32_t sequence, const Controls& controls) {
    ReplayState before = aircraft_.replay_state();
    std::optional<Controller::PilotsLaws> laws_before;
    if (laws_) {
        laws_before.emplace(laws_->pilots_laws());
    }
    const std::optional<Controls> commanded_before = commanded_;
    flying_step_ = steps_;
    if (!wrecked_ && !lost_) {
        fly(controls, steps_);
    }
    // **Where an input began here**, known only where the sequence changes
    // after a step of another: the first input a prediction flies may have
    // begun before it did - one begun from the first update, or after a
    // take-back, begins part-way through - and taken as begun on this step
    // it put the clocks' difference short by as much, for two seconds of
    // updates (a metre at 200 ms).
    if (steps_ > 0 && sequence != flying_) {
        began_.try_emplace(sequence, steps_);
    }
    flying_ = sequence;
    held_.push_back({sequence, controls, steps_, std::move(before), std::move(laws_before),
                     commanded_before});
    ++steps_;
    // A client this far behind has a problem this layer cannot solve; the
    // oldest inputs are dropped rather than held for ever.
    while (held_.size() > most_unacknowledged) {
        held_.pop_front();
        began_.erase(began_.begin(), began_.lower_bound(held_.front().sequence));
        sent_.erase(sent_.begin(), sent_.lower_bound(held_.front().sequence));
    }
}

void Prediction::sent(std::uint32_t sequence, std::uint64_t step) {
    stamps_ = true;
    sent_.try_emplace(sequence, step);
    while (sent_.size() > most_unacknowledged) {
        sent_.erase(sent_.begin());
    }
}

Prediction::Correction Prediction::reconcile(const AircraftSnapshot& server,
                                             std::uint32_t last_applied) {
    const AircraftState was = aircraft_.state();

    // Everything the server has already seen is done with.
    while (!held_.empty() && held_.front().sequence <= last_applied) {
        held_.pop_front();
    }
    began_.erase(began_.begin(), began_.upper_bound(last_applied));
    sent_.erase(sent_.begin(), sent_.upper_bound(last_applied));
    // The server's snapshot is an input too.
    if (!lost_) {
        if (const auto bad = first_not_finite(server)) {
            lost_ = Lost{"the server's snapshot's " + *bad, flying_step_};
        }
    }
    if (lost_) {
        return {};
    }

    // **The snapshot is the whole of what the replay starts from**: every
    // writable property - the controls, the actuators, the tanks - and the
    // engines, the server's own. So no held ReplayState is put back here, as
    // the motion's reconcile does (as_at_first_held): that would put this
    // client's own over the server's, and the first step held is kept, by
    // fly_held_again, as the server had it.
    aircraft_.restore(server);
    if (laws_ && !held_.empty() && held_.front().laws_before) {
        laws_->restore(*held_.front().laws_before);
    }

    // And forward again through everything it had not.
    Correction out;
    out.replayed = fly_held_again();

    out.moved_m = how_far_apart_m(was, aircraft_.state());
    out.snapped = out.moved_m > snap_beyond_m;
    return out;
}

bool Prediction::hear_engine_stopped(std::optional<int> engine) {
    // **Running again on the server** - flown again after a wreck - or
    // another named, and stopped here for its word: started again.
    if (stopped_for_the_server_ && stopped_for_the_server_ != engine) {
        aircraft_.restart_engine(*stopped_for_the_server_);
        stopped_for_the_server_.reset();
    }
    if (!engine || stopped_for_the_server_ == engine || !aircraft_.engine_running(*engine)) {
        return false;
    }
    aircraft_.fail_engine(*engine, false);
    stopped_for_the_server_ = engine;
    ++engines_stopped_;
    return true;
}

Prediction::Correction Prediction::reconcile(const Motion& server,
                                             std::uint32_t last_applied,
                                             std::size_t steps_into,
                                             std::uint64_t server_steps,
                                             std::optional<double> stovl_height_asked_ft,
                                             const Transition::Carried& stovl_carried) {
    const AircraftState was = aircraft_.state();
    Correction out;
    out.at_step = hear_clock(last_applied, steps_into, server_steps);
    if (out.at_step) {
        while (!held_.empty() && held_.front().step < *out.at_step) {
            held_.pop_front();
        }
    } else {
        // Nothing to place it by: everything up to the input's end is taken
        // as done.
        while (!held_.empty() && held_.front().sequence <= last_applied) {
            held_.pop_front();
        }
    }
    // Kept from `last_applied` on: a later word may say more of it.
    began_.erase(began_.begin(), began_.lower_bound(last_applied));
    sent_.erase(sent_.begin(), sent_.lower_bound(last_applied));
    // The server's word is an input too: its motion, and the STOVL law's
    // height asked and pitch integral where it carries them.
    if (!lost_) {
        if (const auto bad = first_not_finite(server)) {
            lost_ = Lost{"the server's word's " + *bad, flying_step_};
        } else if (stovl_height_asked_ft && !std::isfinite(*stovl_height_asked_ft)) {
            lost_ = Lost{"the server's word's stovl_height_asked_ft", flying_step_};
        } else if (stovl_carried.pitch_integral_deg &&
                   !std::isfinite(*stovl_carried.pitch_integral_deg)) {
            lost_ = Lost{"the server's word's stovl pitch_integral_deg", flying_step_};
        }
    }
    if (lost_) {
        return out;
    }
    aircraft_.set_motion(server);
    as_at_first_held();
    // The server's height asked, where a law flies her here too.
    if (laws_ && laws_->transition() && stovl_height_asked_ft) {
        Controller::PilotsLaws told = laws_->pilots_laws();
        told.stovl_height_ft = *stovl_height_asked_ft;
        told.stovl->carry(stovl_carried);
        laws_->restore(told);
    }
    out.replayed = fly_held_again();
    out.moved_m = how_far_apart_m(was, aircraft_.state());
    out.snapped = out.moved_m > snap_beyond_m;
    return out;
}

void Prediction::fly(const Controls& controls, std::uint64_t step) {
    if (const auto bad = first_not_finite(controls)) {
        lost_ = Lost{"the input's " + *bad, step};
        return;
    }
    put(controls);
    // What a lift system's laws made of them, which is what JSBSim is given.
    if (laws_ && commanded_) {
        if (const auto bad = first_not_finite(*commanded_)) {
            lost_ = Lost{"the laws' commanded " + *bad, step};
            return;
        }
    }
    aircraft_.step();
    if (const auto bad = first_not_finite(aircraft_.motion())) {
        // Named by the numbers in her property tree gone with it, the first
        // few, which say where it went first better than the motion does.
        std::string what = "the motion's " + *bad;
        const auto numbers = aircraft_.not_finite_numbers(4);
        for (std::size_t i = 0; i < numbers.size(); ++i) {
            what += (i == 0 ? ", with " : ", ") + numbers[i];
        }
        lost_ = Lost{what, step};
    }
}

std::size_t Prediction::fly_held_again() {
    // **Each step held is kept as it was flown again**: what the motion does
    // not carry and the lift system's controller, as the step began, from
    // this replay - the server's word and the steps since - and no longer
    // from the pass that first flew it. Kept from the first pass, each word
    // put her controller back to whichever line of its own past that pass
    // had flown: an F-35B's transition law was put back, word after word, to
    // one of four such lines - its autopilot's engaging offset cycling
    // among four values the server's never had, and at one word its pitch
    // integral put back to a value its line had left 25 steps before
    // (PROJECT_STATUS.md, 2026-10-14).
    for (Applied& a : held_) {
        a.before = aircraft_.replay_state();
        if (laws_) {
            a.laws_before.emplace(laws_->pilots_laws());
        }
        a.commanded_before = commanded_;
        flying_step_ = a.step;
        // A wreck is held at the server's word (hear_wrecked), and a
        // prediction lost is flown no further (lost).
        if (!wrecked_ && !lost_) {
            fly(a.controls, a.step);
        }
    }
    return held_.size();
}

void Prediction::as_at_first_held() {
    // **As it was at the first step flown again**: the motion is the
    // server's at that step and the steps since are flown again from it, and
    // what the motion does not carry is put back with it (ReplayState). Left
    // as the newest step had it, an engine winding down was wound down again
    // by every replay: the Cessna's propeller, stopped on the server, took six
    // seconds to stop there and under two here, and for those seconds the
    // client was put right at every word by the drag it did not have
    // (PROJECT_STATUS.md, 2026-10-09). A speedbrake moving moved on the same
    // way.
    if (!held_.empty()) {
        aircraft_.set_replay_state(held_.front().before);
        // And the lift system's controller as it was then, and the
        // controls on her that a law made on the step is seeded from - put
        // on after the replay state, which runs her flight-control system
        // once on what she has.
        if (laws_ && held_.front().laws_before) {
            laws_->restore(*held_.front().laws_before);
        }
        if (laws_ && held_.front().commanded_before) {
            aircraft_.set_controls(*held_.front().commanded_before);
            commanded_ = held_.front().commanded_before;
        }
    }
}

std::optional<std::uint64_t> Prediction::hear_clock(std::uint32_t last_applied,
                                                    std::size_t steps_into,
                                                    std::uint64_t server_steps) {
    // **The clocks' difference this word implies**: where the server applied
    // `last_applied`, less where this client began it. An input never flown
    // here - sent before this prediction began - has no beginning, and says
    // nothing.
    // **Stamped where its client sent it** (`sent`), once a client stamps:
    // where it was flown from says nothing of when it left, and a sample not
    // stamped is not taken.
    const auto began = began_.find(last_applied);
    const auto sent = sent_.find(last_applied);
    if (last_applied != 0 && began != began_.end() && steps_into <= server_steps &&
        (!stamps_ || sent != sent_.end())) {
        const std::uint64_t from =
            stamps_ ? std::max(sent->second, began->second) : began->second;
        const std::int64_t difference = static_cast<std::int64_t>(server_steps - steps_into) -
                                        static_cast<std::int64_t>(from);
        newest_sample_ = Sample{difference, static_cast<std::int64_t>(from - began->second),
                                static_cast<std::int64_t>(server_steps - steps_into) -
                                    static_cast<std::int64_t>(began->second)};
        offsets_.push_back(difference);
        while (offsets_.size() > offset_window) {
            offsets_.pop_front();
        }
    }
    // **And the step of this client's it places the word at**: the server's
    // step less the clocks' difference as now known.
    if (offsets_.empty()) {
        return std::nullopt;
    }
    const std::int64_t offset = *std::min_element(offsets_.begin(), offsets_.end());
    const std::int64_t at = static_cast<std::int64_t>(server_steps) - offset;
    return static_cast<std::uint64_t>(std::max<std::int64_t>(at, 0));
}

std::optional<double> Prediction::session_time_s() const {
    if (offsets_.empty()) {
        return std::nullopt;
    }
    const std::int64_t offset = *std::min_element(offsets_.begin(), offsets_.end());
    return static_cast<double>(static_cast<std::int64_t>(flying_step_) + offset + 1) /
           static_cast<double>(steps_per_second);
}

std::size_t Prediction::adopt(const Motion& motion, std::uint64_t server_steps) {
    if (!lost_) {
        if (const auto bad = first_not_finite(motion)) {
            lost_ = Lost{"the server's word's " + *bad, flying_step_};
        }
    }
    if (lost_) {
        return 0;
    }
    if (offsets_.empty()) {
        held_.clear();
        aircraft_.set_motion(motion);
        return 0;
    }
    const std::int64_t offset = *std::min_element(offsets_.begin(), offsets_.end());
    const std::int64_t at = static_cast<std::int64_t>(server_steps) - offset;
    while (!held_.empty() && static_cast<std::int64_t>(held_.front().step) < at) {
        held_.pop_front();
    }
    aircraft_.set_motion(motion);
    as_at_first_held();
    return fly_held_again();
}

} // namespace glideslope::sim
