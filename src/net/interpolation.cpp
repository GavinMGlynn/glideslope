#include "net/interpolation.hpp"

#include <algorithm>
#include <cmath>

namespace glideslope::net {
namespace {

// How much of a moment's worth of snapshots to keep: enough to straddle the
// moment being shown, with room for one that arrives late and out of order.
constexpr double keep_s = 2.0;

double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

// The angle `t` of the way from `a` to `b`, the short way round.
double lerp_angle(double a, double b, double t) {
    return a + shortest_turn_deg(a, b) * t;
}

RemoteState between(const RemoteState& a, const RemoteState& b, double t) {
    RemoteState out;
    out.time_s = lerp(a.time_s, b.time_s, t);
    out.north_m = lerp(a.north_m, b.north_m, t);
    out.east_m = lerp(a.east_m, b.east_m, t);
    out.down_m = lerp(a.down_m, b.down_m, t);
    out.north_mps = lerp(a.north_mps, b.north_mps, t);
    out.east_mps = lerp(a.east_mps, b.east_mps, t);
    out.down_mps = lerp(a.down_mps, b.down_mps, t);
    out.heading_deg = lerp_angle(a.heading_deg, b.heading_deg, t);
    out.pitch_deg = lerp_angle(a.pitch_deg, b.pitch_deg, t);
    out.roll_deg = lerp_angle(a.roll_deg, b.roll_deg, t);
    return out;
}

// Carried on from `s` by `dt` seconds at the velocity it had.
RemoteState carried_on(const RemoteState& s, double dt) {
    RemoteState out = s;
    out.time_s = s.time_s + dt;
    out.north_m = s.north_m + s.north_mps * dt;
    out.east_m = s.east_m + s.east_mps * dt;
    out.down_m = s.down_m + s.down_mps * dt;
    return out;
}

} // namespace

double shortest_turn_deg(double from, double to) {
    double turn = std::fmod(to - from, 360.0);
    if (turn > 180.0) {
        turn -= 360.0;
    } else if (turn < -180.0) {
        turn += 360.0;
    }
    return turn;
}

void Interpolated::received(const RemoteState& snapshot) {
    // A snapshot already held, by its time, is a duplicate.
    for (const RemoteState& s : held_) {
        if (s.time_s == snapshot.time_s) {
            return;
        }
    }
    // Kept in time order, because one can arrive after a newer one.
    const auto at = std::lower_bound(
        held_.begin(), held_.end(), snapshot.time_s,
        [](const RemoteState& s, double t) { return s.time_s < t; });
    held_.insert(at, snapshot);
    while (held_.size() > 1 &&
           held_.back().time_s - held_.front().time_s > keep_s) {
        held_.pop_front();
    }
}

RemoteState Interpolated::at(double now_s) {
    if (held_.empty()) {
        extrapolating_ = false;
        return {};
    }
    const double want = now_s - shown_behind_s;

    RemoteState answer;
    bool guessing = false;
    if (want <= held_.front().time_s) {
        // Before anything known: hold the oldest rather than guess backwards.
        answer = held_.front();
    } else if (want >= held_.back().time_s) {
        // Past the newest: carry on from it, for a while.
        const double over = std::min(want - held_.back().time_s,
                                     extrapolate_at_most_s);
        answer = carried_on(held_.back(), over);
        guessing = want > held_.back().time_s;
        if (guessing && !was_extrapolating_) {
            guessed_from_ = held_.back();
            has_guessed_from_ = true;
        }
    } else {
        // Between two: the usual case.
        for (std::size_t i = 1; i < held_.size(); ++i) {
            const RemoteState& a = held_[i - 1];
            const RemoteState& b = held_[i];
            if (want >= a.time_s && want <= b.time_s) {
                const double span = b.time_s - a.time_s;
                answer = between(a, b, span > 0.0 ? (want - a.time_s) / span : 0.0);
                break;
            }
        }
    }

    // **Coming back from a guess without jumping.** The offset to take up is
    // simply the step the aircraft would otherwise make at this moment:
    // where it was last shown, less where it is now said to be. Working it
    // out from the snapshots instead would be wrong, because by now the
    // newest snapshot is the one that ended the guess, not the one the guess
    // was carried on from.
    if (was_extrapolating_ && !guessing && has_guessed_from_) {
        const RemoteState would_have = carried_on(
            guessed_from_,
            std::min(std::max(0.0, want - guessed_from_.time_s), extrapolate_at_most_s));
        offset_.north_m = would_have.north_m - answer.north_m;
        offset_.east_m = would_have.east_m - answer.east_m;
        offset_.down_m = would_have.down_m - answer.down_m;
        offset_from_s_ = now_s;
        has_offset_ = true;
        has_guessed_from_ = false;
    }
    was_extrapolating_ = guessing;
    extrapolating_ = guessing;

    if (has_offset_) {
        const double gone = (now_s - offset_from_s_) / blend_s;
        if (gone >= 1.0) {
            has_offset_ = false;
        } else {
            const double left = 1.0 - gone;
            answer.north_m += offset_.north_m * left;
            answer.east_m += offset_.east_m * left;
            answer.down_m += offset_.down_m * left;
        }
    }
    return answer;
}

} // namespace glideslope::net
