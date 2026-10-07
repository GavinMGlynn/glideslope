#include "sim/pacing.hpp"

#include "sim/fixed_step.hpp"
#include "sim/prediction.hpp"

#include <algorithm>
#include <cstdlib>

namespace glideslope::sim {

double Pacing::at(double local_s) {
    if (!last_local_s_) {
        paced_s_ = local_s;
    } else if (local_s > *last_local_s_) {
        paced_s_ += (local_s - *last_local_s_) * pace_;
    }
    last_local_s_ = std::max(local_s, last_local_s_.value_or(local_s));
    return paced_s_;
}

void Pacing::heard(std::int64_t difference, double local_s, std::optional<double> rate) {
    if (!held_) {
        held_ = difference;
        if (!ever_held_ && rate) {
            pace_ = std::clamp(*rate, slowest, 1.0);
        }
        ever_held_ = true;
        // Bumpless: the integral carries the pace it starts at.
        integral_ = (pace_ - 1.0) / integral_per_s2;
        heard_at_s_ = local_s;
        off_ = 0;
        worst_off_ = 0;
        return;
    }
    off_ = difference - *held_;
    if (std::llabs(off_) > std::llabs(worst_off_)) {
        worst_off_ = off_;
    }
    const double error_s = static_cast<double>(off_) / static_cast<double>(steps_per_second);
    const double dt = heard_at_s_ ? std::max(0.0, local_s - *heard_at_s_) : 0.0;
    heard_at_s_ = local_s;
    const double integral = integral_ + error_s * dt;
    const double wanted = 1.0 + proportional_per_s * error_s + integral_per_s2 * integral;
    pace_ = std::clamp(wanted, slowest, fastest);
    // Not wound up past what it can fly.
    if (wanted >= slowest && wanted <= fastest) {
        integral_ = integral;
    }
}

void Pacing::before_held(double rate) {
    if (!ever_held_) {
        pace_ = std::clamp(rate, slowest, 1.0);
    }
}

void Pacing::follow(const Prediction& prediction, double local_s, std::optional<double> rate) {
    if (rate) {
        before_held(*rate);
    }
    if (prediction.settled()) {
        if (const auto difference = prediction.recent_clocks_difference()) {
            heard(*difference, local_s, rate);
        }
    }
}

void Pacing::hold() {
    held_.reset();
}

} // namespace glideslope::sim
