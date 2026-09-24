#include "sim/prediction.hpp"

#include "sim/navigator.hpp"

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
    held_.push_back({sequence, controls});
    // A client this far behind has a problem this layer cannot solve; the
    // oldest inputs are dropped rather than held for ever.
    while (held_.size() > most_unacknowledged) {
        held_.pop_front();
    }
}

Prediction::Correction Prediction::reconcile(const AircraftSnapshot& server,
                                             std::uint32_t last_applied) {
    const AircraftState was = aircraft_.state();

    // Everything the server has already seen is done with.
    while (!held_.empty() && held_.front().sequence <= last_applied) {
        held_.pop_front();
    }

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
                                             std::uint32_t last_applied) {
    const AircraftState was = aircraft_.state();
    while (!held_.empty() && held_.front().sequence <= last_applied) {
        held_.pop_front();
    }
    aircraft_.set_motion(server);
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

} // namespace glideslope::sim
