#pragma once

// **What the client with the window shows of its own aircraft on a server**,
// and how far it steps.
//
// Its own aircraft is shown from one of two sources: the flight here,
// predicted, while this client flies it; or the server's updates, drawn 100 ms
// behind the clock as any other aircraft is, while the AI does. At a switch -
// handed over, taken back, or another taken over - what it is shown from
// changes, and the two are not in the same place: the updates are behind, and
// the prediction is now. Shown as it stood, the aircraft would jump by its
// speed times that lag, and the camera in it with it.
//
// **It blends across every switch** instead, eased over half a second: what is
// shown is its source plus a blend, begun as where the frame before it, carried
// on, would have put it, less the source, and taken down to nothing. This is
// the network checks' model of a display (glideslope_cli's Predicting), and it
// measures the step the same way: how far what is shown is from where the
// frame before it, carried on part by part - its source at its own velocity,
// and its blend as the blend goes - puts it, and the blend's own pace. The
// lessons it keeps, each of which cost a regression there: a blend starts from
// where the aircraft was going, not where it was drawn; the velocities are the
// ones known, not guessed from two frames; and the step is measured against
// time, not frames.

#include "world/geodesy.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace glideslope::client {

class OwnShown {
public:
    // How long a switch is blended over, seconds.
    static constexpr double blend_s = 0.5;

    // What its own aircraft is shown from this frame: where, how fast it
    // moves there (m/s, the Earth-centred frame, per second of this
    // machine's clock), and whether that is the prediction or the updates.
    struct Source {
        world::Ecef at;
        std::array<double, 3> v{};
        bool predicted = false;
    };

    // **Another aircraft taken over**: what is shown of it goes on from where
    // it was last shown as another (`seen`), not from the aircraft left
    // behind.
    void taken_over(std::uint8_t number);

    // **One frame at `local_s`**: where its own is shown - its source moved by
    // what is left of any blend. The step it made is measured here.
    world::Ecef frame(double local_s, const Source& source);

    // Another aircraft shown this frame, where and moving how: what a
    // take-over of it blends from.
    void seen(std::uint8_t number, double local_s, const world::Ecef& at,
              const std::array<double, 3>& v);

    // **What it found**: how many switches it measured, the largest step at
    // one - within four frames of it, to the frame after a long one - and
    // elsewhere, and what made the largest at a switch.
    std::size_t switches() const { return switches_; }
    double worst_step_at_switch_m() const { return worst_at_switch_m_; }
    double worst_step_otherwise_m() const { return worst_otherwise_m_; }
    const std::string& worst_step_what() const { return worst_what_; }

private:
    // What was shown, and when, as its parts: the source it was shown from
    // and how fast that moved, and the blend on it. Carried on, each part
    // moves by its own rule - the source at its velocity, the blend as the
    // blend goes - where one velocity for the whole missed an eased blend's
    // curve over a long frame by metres.
    struct Shown {
        double s = 0.0;
        std::array<double, 3> source{};
        std::array<double, 3> v{};
        std::array<double, 3> blend{};
        double blend_from_s = 0.0;
        double blend_over_s = 1.0;
    };
    // How much of a blend is left at `t`, eased in and out.
    static double left_at(double t, double from_s, double over_s);

    std::optional<Shown> before_;
    std::optional<Shown> before_before_;
    std::optional<bool> shown_predicted_;
    bool switching_ = false;
    std::array<double, 3> blend_{};
    double blend_from_s_ = -1.0e9;
    int frames_since_switch_ = 1000;
    std::size_t switches_ = 0;
    double worst_at_switch_m_ = 0.0;
    double worst_otherwise_m_ = 0.0;
    std::string worst_what_;
    std::map<std::uint8_t, std::pair<std::optional<Shown>, std::optional<Shown>>> others_;
};

} // namespace glideslope::client
