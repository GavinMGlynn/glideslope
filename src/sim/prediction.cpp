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

void Prediction::step(std::uint32_t sequence, const Controls& controls) {
    aircraft_.set_controls(controls);
    flying_step_ = steps_;
    aircraft_.step();
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
    held_.push_back({sequence, controls, steps_});
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

    aircraft_.restore(server);

    // And forward again through everything it had not.
    Correction out;
    for (const Applied& a : held_) {
        aircraft_.set_controls(a.controls);
        flying_step_ = a.step;
        aircraft_.step();
        ++out.replayed;
    }

    out.moved_m = how_far_apart_m(was, aircraft_.state());
    out.snapped = out.moved_m > snap_beyond_m;
    return out;
}

bool Prediction::hear_engine_stopped(bool stopped) {
    // **Running again on the server** - flown again after a wreck - and
    // stopped here for its word: started again.
    if (!stopped && stopped_for_the_server_) {
        aircraft_.restart_engine(0);
        stopped_for_the_server_ = false;
        return false;
    }
    if (!stopped || aircraft_.any_engine_stopped()) {
        return false;
    }
    aircraft_.fail_engine(0, false);
    stopped_for_the_server_ = true;
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
    for (const Applied& a : held_) {
        aircraft_.set_controls(a.controls);
        flying_step_ = a.step;
        aircraft_.step();
        ++out.replayed;
    }
    out.moved_m = how_far_apart_m(was, aircraft_.state());
    out.snapped = out.moved_m > snap_beyond_m;
    return out;
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
    for (const Applied& a : held_) {
        aircraft_.set_controls(a.controls);
        flying_step_ = a.step;
        aircraft_.step();
    }
    return held_.size();
}

} // namespace glideslope::sim
