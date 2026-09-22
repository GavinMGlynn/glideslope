#pragma once

// Other aircraft, shown a little in the past.
//
// **A client cannot show another aircraft where it is**, because it only ever
// learns where it was. Snapshots arrive 20 to 30 times a second, late,
// out of order and sometimes not at all, so an aircraft drawn at the newest
// snapshot would stutter every time one was late. It is drawn
// `shown_behind_s` in the past instead, between the two snapshots that
// straddle that moment, which is smooth as long as snapshots keep arriving.
//
// **When they stop, it carries on from the last velocity.** That is a guess,
// and it is marked as one: `extrapolating()` says so. A guess held too long
// is worse than a stutter, so it is held for `extrapolate_at_most_s` and no
// longer.
//
// **And when a snapshot finally arrives, it does not jump.** The aircraft is
// wherever the guess put it; the truth is somewhere else. Jumping there is
// the thing a player sees. So the difference is taken up over
// `blend_s`, which is short enough to be honest and long enough not to be a
// jolt.
//
// **Nothing here touches a socket or a flight model.** It takes states in and
// gives a state out, which is what lets it be walked against loss and jitter
// rather than against a network that happens to be working.

#include <cstddef>
#include <deque>

namespace glideslope::net {

// How far behind other aircraft are shown. REQUIREMENTS.md 6.4: about 100 ms.
inline constexpr double shown_behind_s = 0.100;
// How long a guess may be held before the aircraft is simply left where it
// was. Half a second of guessing at 200 m/s is 100 m, which is as far as this
// project is prepared to invent.
inline constexpr double extrapolate_at_most_s = 0.5;
// How long the difference between a guess and the truth is taken up over.
inline constexpr double blend_s = 0.25;

// Where another aircraft was, as the server stamped it.
struct RemoteState {
    double time_s = 0.0; // the session clock
    double north_m = 0.0; // a local frame about the session's origin
    double east_m = 0.0;
    double down_m = 0.0;
    double north_mps = 0.0; // its velocity, for carrying on from
    double east_mps = 0.0;
    double down_mps = 0.0;
    double heading_deg = 0.0;
    double pitch_deg = 0.0;
    double roll_deg = 0.0;
};

// The shortest way round from `from` to `to`, degrees, in -180..180.
double shortest_turn_deg(double from, double to);

class Interpolated {
public:
    // A snapshot off the wire. One older than the newest already held is
    // taken - it may fill a gap - and one that is a duplicate is not.
    void received(const RemoteState& snapshot);

    // Where the aircraft is shown at session time `now_s`. Before anything
    // has arrived this is a state of noughts, which is what "not here yet"
    // looks like; `known()` says whether it is.
    RemoteState at(double now_s);

    // Whether the last answer was a guess rather than two snapshots.
    bool extrapolating() const { return extrapolating_; }
    bool known() const { return !held_.empty(); }
    std::size_t held() const { return held_.size(); }

private:
    // The snapshots, oldest first. Only a moment's worth is kept.
    std::deque<RemoteState> held_;
    bool extrapolating_ = false;
    // What was added to the answer to keep it from jumping, and when it
    // started being taken up.
    RemoteState offset_{};
    double offset_from_s_ = 0.0;
    bool has_offset_ = false;
    bool was_extrapolating_ = false;
    // The snapshot the guess was being carried on from, kept so that when
    // the guess ends the step can be worked out at one moment: what the
    // guess would have said for this instant, against what is now known for
    // the same instant. Comparing one frame's answer with the last frame's
    // would measure the aircraft's own motion between them instead.
    RemoteState guessed_from_{};
    bool has_guessed_from_ = false;
};

} // namespace glideslope::net
