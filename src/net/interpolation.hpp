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

#include <array>
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

    // **How fast the last answer moves along the session's clock**, north,
    // east and down, m/s: the path's own - between two snapshots the
    // straight line joining them, carried on the velocity carried on at, held
    // still, and less the part of an offset being taken up each second. Not
    // the velocity a snapshot reports: in a turn, or with jitter, that is not
    // where the answer goes next.
    std::array<double, 3> path_velocity() const { return path_mps_; }

    // Whether the last answer was a guess rather than two snapshots.
    bool extrapolating() const { return extrapolating_; }
    bool known() const { return !held_.empty(); }
    std::size_t held() const { return held_.size(); }

private:
    // The snapshots, oldest first. Only a moment's worth is kept.
    std::deque<RemoteState> held_;
    bool extrapolating_ = false;
    std::array<double, 3> path_mps_{};
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
    // When it was last asked. A guess, and what taking it up adds, are kept
    // only for an aircraft that is being drawn: asked again after longer
    // than a guess is held, nothing was shown to keep on from.
    double asked_s_ = 0.0;
    bool asked_ = false;
};

// **The server's clock, as seen from here.** A client draws other aircraft
// 100 ms behind the session's clock, so it must know what that clock says
// now - and all it has are updates, each stamped with the session time and
// each arriving late by a latency that varies.
//
// **Its rate is fitted, not assumed.** A server that cannot keep real time -
// a debug build on a loaded machine - runs its clock slower than this one.
// An estimate that took the fastest update ever heard, and assumed the two
// clocks ran together, ran further ahead of such a server every second, and
// the aircraft drawn from it were guesses carried past everything known: a
// server at 80% of real time had them drawn 190 m from where they were. So
// the rate is fitted over the last `window_s` of updates, and the offset is
// then the one the fastest-arriving of them gives at that rate - the
// update that waited least in the network says most nearly what the clock
// was when it arrived.
class SessionClock {
public:
    // How much of the past the fit is made over.
    static constexpr double window_s = 2.0;

    // An update stamped `session_s`, heard at this machine's `local_s`.
    void heard(double session_s, double local_s);

    bool known() const { return !heard_.empty(); }
    // The session's time at this machine's `local_s`, as nearly as the
    // updates say.
    double now(double local_s) const;
    // How fast the session's clock runs against this one: 1 for a server
    // keeping real time.
    double rate() const { return rate_; }

private:
    struct Heard {
        double session_s = 0.0;
        double local_s = 0.0;
    };
    std::deque<Heard> heard_;
    double rate_ = 1.0;
    // The session's time at local nought, on the fitted line through the
    // fastest-arriving update.
    double at_zero_s_ = 0.0;
};

} // namespace glideslope::net
