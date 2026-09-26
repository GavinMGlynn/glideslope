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
    path_mps_ = {};
    // **Not asked for a while, it was not being drawn**, so there is no
    // guess to come back from and no jump to hide: taking up the difference
    // between a guess seconds old and where it is now moved an aircraft
    // hundreds of metres in a quarter of a second (a client's own, drawn
    // again at a hand-over after a guess before its prediction started,
    // 2026-09-26: 1,194 m/s).
    if (asked_ && now_s - asked_s_ > extrapolate_at_most_s) {
        was_extrapolating_ = false;
        has_guessed_from_ = false;
        has_offset_ = false;
    }
    asked_ = true;
    asked_s_ = now_s;
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
        if (want - held_.back().time_s < extrapolate_at_most_s) {
            path_mps_ = {held_.back().north_mps, held_.back().east_mps, held_.back().down_mps};
        }
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
                if (span > 0.0) {
                    path_mps_ = {(b.north_m - a.north_m) / span, (b.east_m - a.east_m) / span,
                                 (b.down_m - a.down_m) / span};
                }
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
            path_mps_[0] -= offset_.north_m / blend_s;
            path_mps_[1] -= offset_.east_m / blend_s;
            path_mps_[2] -= offset_.down_m / blend_s;
        }
    }
    return answer;
}

void SessionClock::heard(double session_s, double local_s) {
    heard_.push_back({session_s, local_s});
    while (heard_.size() > 2 && local_s - heard_.front().local_s > window_s) {
        heard_.pop_front();
    }
    // The rate: a least-squares line through what the window holds, once it
    // holds enough of a span to say anything - until then, real time. Held
    // between a half and twice, which no server that is running at all is
    // outside, so that a burst of updates cannot make it absurd.
    const double span = heard_.back().local_s - heard_.front().local_s;
    if (heard_.size() >= 5 && span >= window_s / 4.0) {
        double sl = 0.0;
        double ss = 0.0;
        for (const Heard& h : heard_) {
            sl += h.local_s;
            ss += h.session_s;
        }
        const double n = static_cast<double>(heard_.size());
        const double ml = sl / n;
        const double ms = ss / n;
        double sll = 0.0;
        double sls = 0.0;
        for (const Heard& h : heard_) {
            sll += (h.local_s - ml) * (h.local_s - ml);
            sls += (h.local_s - ml) * (h.session_s - ms);
        }
        if (sll > 0.0) {
            rate_ = std::clamp(sls / sll, 0.5, 2.0);
        }
    }
    // The offset: at that rate, the update that says the clock is furthest
    // on is the one that waited least. Only the last half of the window is
    // looked at, because the line is carried on from that update to now, and
    // any error in the rate grows with how far it is carried.
    double best = heard_.back().session_s - rate_ * heard_.back().local_s;
    for (const Heard& h : heard_) {
        if (heard_.back().local_s - h.local_s <= window_s / 2.0) {
            best = std::max(best, h.session_s - rate_ * h.local_s);
        }
    }
    at_zero_s_ = best;
}

double SessionClock::now(double local_s) const {
    return at_zero_s_ + rate_ * local_s;
}

} // namespace glideslope::net
