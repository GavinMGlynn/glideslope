#include "shown.hpp"

#include "sim/prediction.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace glideslope::client {

double OwnShown::left_at(double t, double from_s, double over_s, bool eased) {
    const double gone = std::clamp((t - from_s) / over_s, 0.0, 1.0);
    return eased ? 1.0 - gone * gone * (3.0 - 2.0 * gone) : 1.0 - gone;
}

void OwnShown::taken_over(std::uint8_t number) {
    // What it had shown of that aircraft as another carries on as its own -
    // never the aircraft left behind, which is somewhere else. Seen in one
    // frame only, it blends from that one, and the step is measured from the
    // next; seen in none, there is nothing to blend from.
    const auto found = others_.find(number);
    before_.reset();
    before_before_.reset();
    if (found != others_.end()) {
        before_ = found->second.second;
        if (before_) {
            before_before_ = found->second.first;
        }
    }
    others_.erase(number);
    switching_ = true;
}

void OwnShown::seen(std::uint8_t number, double local_s, const world::Ecef& at,
                    const std::array<double, 3>& v) {
    auto& last = others_[number];
    last.first = last.second;
    last.second = Shown{local_s, {at.x, at.y, at.z}, v, {}, 0.0, 1.0, false};
}

world::Ecef OwnShown::frame(double local_s, const Source& source) {
    const std::array<double, 3> at{source.at.x, source.at.y, source.at.z};
    // **A blend wherever what it is shown from changes** - predicted, or
    // drawn from the updates - and at a take-over, where the aircraft itself
    // does.
    const bool switched =
        switching_ || (shown_predicted_ && *shown_predicted_ != source.predicted);
    // **Where the last frame carries it now**: shown there, moving as it was
    // moving then - its source's velocity and the blend's, both known, not
    // guessed from the frames' positions. What a blend starts from, and what
    // the step is measured against.
    std::optional<std::array<double, 3>> carried;
    if (before_) {
        const Shown& b = *before_;
        const double left_now = left_at(local_s, b.blend_from_s, b.blend_over_s, b.eased);
        carried = std::array<double, 3>{};
        for (std::size_t i = 0; i < 3; ++i) {
            (*carried)[i] = b.source[i] + b.v[i] * (local_s - b.s) + b.blend[i] * left_now;
        }
    }
    // A switch is counted whether or not anything is blended across it:
    // were the blend what marked it, taking the blend away would take the
    // measurement with it.
    if (switched && before_) {
        frames_since_switch_ = 0;
        ++switches_;
    }
    if ((switched || source.corrected) && carried) {
        // **From where it was going, not where it was**: a blend started
        // from the last frame shown holds the aircraft still for a frame.
        for (std::size_t i = 0; i < 3; ++i) {
            blend_[i] = (*carried)[i] - at[i];
        }
        blend_from_s_ = local_s;
        blend_over_s_ = switched ? blend_s : sim::correction_blend_s;
        blend_eased_ = switched;
    }
    switching_ = false;
    shown_predicted_ = source.predicted;
    const double left = left_at(local_s, blend_from_s_, blend_over_s_, blend_eased_);
    std::array<double, 3> shown{};
    for (std::size_t i = 0; i < 3; ++i) {
        shown[i] = at[i] + blend_[i] * left;
    }
    // **The step, against time and not frames**: how far what is shown is
    // from where the last frame, moving as it was, carries it - nought at a
    // steady speed whatever the frames' timing - and the blend's own motion,
    // at the pace it went this frame over a sixtieth of a second, over the
    // part of the frame it was going in. Carried on along its own curve, a
    // blend cancels out of the first; one that crossed the whole gap in a
    // single frame would read as nought without the second.
    if (carried && before_before_) {
        const Shown& b = *before_;
        const double left_then = left_at(b.s, b.blend_from_s, b.blend_over_s, b.eased);
        const double left_now = left_at(local_s, b.blend_from_s, b.blend_over_s, b.eased);
        const double going_s =
            std::min(local_s, b.blend_from_s + b.blend_over_s) - std::max(b.s, b.blend_from_s);
        const double over_a_frame = going_s > 0.0 ? std::min(1.0, (1.0 / 60.0) / going_s) : 1.0;
        double step = 0.0;
        double strayed = 0.0;
        double blended = 0.0;
        double speed = 0.0;
        for (std::size_t i = 0; i < 3; ++i) {
            const double moved = b.blend[i] * (left_now - left_then) * over_a_frame;
            const double d = shown[i] - (*carried)[i] + moved;
            step += d * d;
            strayed += (shown[i] - (*carried)[i]) * (shown[i] - (*carried)[i]);
            blended += moved * moved;
            speed += b.v[i] * b.v[i];
        }
        step = std::sqrt(step);
        if (frames_since_switch_ <= 4) {
            if (step > worst_at_switch_m_) {
                char what[256];
                std::snprintf(what, sizeof what,
                              "frame %d after a switch, %.0f ms long, %s: %.3f m from where "
                              "it was carried at %.1f m/s, the blend moving %.3f m",
                              frames_since_switch_, (local_s - b.s) * 1000.0,
                              source.predicted ? "predicted" : "drawn from the updates",
                              std::sqrt(strayed), std::sqrt(speed), std::sqrt(blended));
                worst_what_ = what;
            }
            worst_at_switch_m_ = std::max(worst_at_switch_m_, step);
        } else {
            worst_otherwise_m_ = std::max(worst_otherwise_m_, step);
        }
    }
    ++frames_since_switch_;
    before_before_ = before_;
    before_ = Shown{local_s, at, source.v, blend_, blend_from_s_, blend_over_s_, blend_eased_};
    return {shown[0], shown[1], shown[2]};
}

} // namespace glideslope::client
