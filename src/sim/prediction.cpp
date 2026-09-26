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
    aircraft_.step();
    began_.try_emplace(sequence, steps_);
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
        aircraft_.step();
        ++out.replayed;
    }

    out.moved_m = how_far_apart_m(was, aircraft_.state());
    out.snapped = out.moved_m > snap_beyond_m;
    return out;
}

Prediction::Correction Prediction::reconcile(const Motion& server,
                                             std::uint32_t last_applied,
                                             std::size_t steps_into,
                                             std::uint64_t server_steps) {
    const AircraftState was = aircraft_.state();
    Correction out;
    // **The clocks' difference this word implies**: where the server applied
    // `last_applied`, less where this client began it. An input never flown
    // here - sent before this prediction began - has no beginning, and says
    // nothing.
    const auto began = began_.find(last_applied);
    if (began != began_.end() && steps_into <= server_steps) {
        offsets_.push_back(static_cast<std::int64_t>(server_steps - steps_into) -
                           static_cast<std::int64_t>(began->second));
        while (offsets_.size() > offset_window) {
            offsets_.pop_front();
        }
    }
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
        aircraft_.step();
        ++out.replayed;
    }
    out.moved_m = how_far_apart_m(was, aircraft_.state());
    out.snapped = out.moved_m > snap_beyond_m;
    return out;
}

} // namespace glideslope::sim
