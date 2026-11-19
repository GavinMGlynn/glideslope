#include "sim/pacing.hpp"

#include "sim/fixed_step.hpp"
#include "sim/prediction.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace glideslope::sim {

double Pacing::at(double local_s) {
    if (!last_local_s_) {
        paced_s_ = local_s;
    } else if (local_s > *last_local_s_) {
        paced_s_ += flown_in(local_s - *last_local_s_);
    }
    last_local_s_ = std::max(local_s, last_local_s_.value_or(local_s));
    return paced_s_;
}

double Pacing::flown_in(double dt) const {
    const double steered = std::min(dt, std::max(steered_without_a_word_s,
                                                 steered_looks * between_looks_s_));
    if (!held_) {
        return dt * pace_;
    }
    const double learnt = std::clamp(1.0 + integral_, slowest, fastest);
    return steered * pace_ + (dt - steered) * learnt;
}

void Pacing::heard(std::int64_t difference, double local_s, std::optional<double> rate) {
    if (heard_at_s_ && local_s == *heard_at_s_ && before_read_together_) {
        // Read with the word before: taken in its place.
        const Loop& was = *before_read_together_;
        held_ = was.held;
        last_difference_ = was.last_difference;
        ever_held_ = was.ever_held;
        integral_ = was.integral;
        heard_at_s_ = was.heard_at_s;
        between_looks_s_ = was.between_looks_s;
        pace_ = was.pace;
        worst_off_ = was.worst_off;
        off_ = was.off;
    } else {
        before_read_together_ = Loop{held_,       last_difference_, ever_held_,
                                     integral_,   heard_at_s_,      between_looks_s_,
                                     pace_,       worst_off_,       off_};
    }
    if (!held_) {
        held_ = difference;
        if (!ever_held_ && rate) {
            pace_ = std::clamp(*rate, slowest, 1.0);
        }
        ever_held_ = true;
        // Bumpless: the integral carries the pace it starts at.
        integral_ = pace_ - 1.0;
        heard_at_s_ = local_s;
        off_ = 0;
        worst_off_ = 0;
        last_difference_ = difference;
        return;
    }
    const double dt = heard_at_s_ ? std::max(0.0, local_s - *heard_at_s_) : 0.0;
    heard_at_s_ = local_s;
    if (dt > 0.0) {
        between_looks_s_ += looks_settle * (std::min(dt, longest_look_s) - between_looks_s_);
    }
    // **Slower the sparser the looks**: the loop's natural frequency is
    // `damped_per_s` until fewer than `looks_per_radian` looks come in a
    // radian of it, and falls with the time between them after that. The
    // integral is kept as the pace it adds, so a change of gains moves no
    // pace already learnt.
    const double natural_per_s =
        std::min(damped_per_s, 1.0 / (looks_per_radian * between_looks_s_));
    const double proportional = 2.0 * natural_per_s;
    const double integral_gain = natural_per_s * natural_per_s;
    // **A jump is not a pace**: the difference moves by no more than the
    // most two paces can part by over the time since the last word, and a
    // step one past that - a starved server catching up at once - moves
    // what it is held to by the excess, rather than being chased.
    const std::int64_t moved = difference - last_difference_;
    last_difference_ = difference;
    const auto most = static_cast<std::int64_t>(
        std::ceil(most_parting * static_cast<double>(steps_per_second) * dt)) + 1;
    const std::int64_t taken = std::clamp(moved, -most, most);
    *held_ += moved - taken;
    off_ = difference - *held_;
    if (std::llabs(off_) > std::llabs(worst_off_)) {
        worst_off_ = off_;
    }
    const double error_s = static_cast<double>(off_) / static_cast<double>(steps_per_second);
    const double integral = integral_ + integral_gain * error_s * dt;
    const double wanted = 1.0 + proportional * error_s + integral;
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
    before_read_together_.reset();
}

} // namespace glideslope::sim
