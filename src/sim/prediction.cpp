#include "sim/prediction.hpp"

#include "sim/navigator.hpp"

#include <algorithm>
#include <cmath>

namespace glideslope::sim {
namespace {
constexpr double metres_per_foot = 0.3048;
} // namespace

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

void Prediction::put(const Controls& controls) {
    if (laws_) {
        laws_->set_pilot(controls);
        apply(aircraft_, *laws_);
    } else {
        aircraft_.set_controls(controls);
    }
}

void Prediction::step(std::uint32_t sequence, const Controls& controls) {
    ReplayState before = aircraft_.replay_state();
    std::optional<Controller::PilotsLaws> laws_before;
    if (laws_) {
        laws_before.emplace(laws_->pilots_laws());
    }
    flying_step_ = steps_;
    if (!wrecked_) {
        put(controls);
        aircraft_.step();
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
    held_.push_back({sequence, controls, steps_, std::move(before), std::move(laws_before)});
    ++steps_;
    // A client this far behind has a problem this layer cannot solve; the
    // oldest inputs are dropped rather than held for ever.
    while (held_.size() > most_unacknowledged) {
        held_.pop_front();
        began_.erase(began_.begin(), began_.lower_bound(held_.front().sequence));
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
                                             std::uint64_t server_steps) {
    const AircraftState was = aircraft_.state();
    Correction out;
    hear_clock(last_applied, steps_into, server_steps);
    if (!offsets_.empty()) {
        const std::int64_t offset = *std::min_element(offsets_.begin(), offsets_.end());
        const std::int64_t at = static_cast<std::int64_t>(server_steps) - offset;
        out.at_step = static_cast<std::uint64_t>(std::max<std::int64_t>(at, 0));
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
    aircraft_.set_motion(server);
    as_at_first_held();
    out.replayed = fly_held_again();
    out.moved_m = how_far_apart_m(was, aircraft_.state());
    out.snapped = out.moved_m > snap_beyond_m;
    return out;
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
        flying_step_ = a.step;
        // A wreck is held at the server's word (hear_wrecked).
        if (!wrecked_) {
            put(a.controls);
            aircraft_.step();
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
        // And the lift system's controller as it was then.
        if (laws_ && held_.front().laws_before) {
            laws_->restore(*held_.front().laws_before);
        }
    }
}

void Prediction::hear_clock(std::uint32_t last_applied, std::size_t steps_into,
                            std::uint64_t server_steps) {
    // **The clocks' difference this word implies**: where the server applied
    // `last_applied`, less where this client began it. An input never flown
    // here - sent before this prediction began - has no beginning, and says
    // nothing.
    const auto began = began_.find(last_applied);
    if (last_applied != 0 && began != began_.end() && steps_into <= server_steps) {
        offsets_.push_back(static_cast<std::int64_t>(server_steps - steps_into) -
                           static_cast<std::int64_t>(began->second));
        while (offsets_.size() > offset_window) {
            offsets_.pop_front();
        }
    }
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
