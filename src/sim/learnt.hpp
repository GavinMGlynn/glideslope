#pragma once

// A landing flown by a policy learnt by reinforcement learning.
//
// **Trained outside the build, flown inside it.** tools/rl/ trains a small
// neural network - a multilayer perceptron - to fly the C172P's final
// approach and landing, in JSBSim's Python bindings on the repository's own
// flight model at the simulation's 120 Hz, and writes its weights to a text
// file under assets/rl/. The policy is data, as an aircraft or a flight plan
// is: this reads the file and evaluates the network in plain C++, with no
// dependency the simulation did not already have.
//
// **What the policy sees and does is defined twice, and must agree
// exactly**: here and in tools/rl/landing.py. The observation is made from
// `LandingReadings` - JSBSim properties, by name - and the runway; the action
// is four numbers from -1 to 1, the elevator, the ailerons, the rudder and
// the throttle. tests/data/rl/ holds readings, observations and actions the
// training scripts recorded, and a test holds these functions to them.
//
// **It decides ten times a second** - every `decision_steps` 120 Hz steps,
// twelve - and holds its controls between decisions, as it was trained. The
// simulation still steps at 120 Hz.
//
// It lands; it does not roll out. From the touch it keeps flying the policy,
// which was trained to five seconds after it; the brakes, and stopping, are
// not its job.

#include "sim/aircraft.hpp"
#include "sim/plan.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace glideslope::sim {

// The trained network and what it was trained to fly.
struct LearntPolicy {
    std::string aircraft; // the model it was trained on: "c172p"
    int decision_steps = 12;
    double vref_kts = 0.0;
    double flaps = 0.0; // the landing flap, held throughout
    double glidepath_deg = 3.0;
    double aim_m = 300.0; // where the glidepath aims, past the threshold

    struct Layer {
        std::size_t inputs = 0;
        std::size_t outputs = 0;
        bool tanh = false;               // tanh, or linear
        std::vector<double> weights;     // [output][input], row by row
        std::vector<double> bias;        // [output]
    };
    std::vector<double> obs_mean;
    std::vector<double> obs_scale;
    double obs_clip = 10.0;
    std::vector<Layer> layers;

    static constexpr std::size_t observations = 21;
    static constexpr std::size_t actions = 4;

    // Reads a policy file (tools/rl/policy_file.py describes the format).
    // Throws std::runtime_error for one that cannot be read, is not one, or
    // whose layers do not fit together or the observation and action here.
    static LearntPolicy read(const std::filesystem::path& file);

    // The action for an observation: normalised as it was in training, the
    // network evaluated in double precision, each output clipped to -1..1.
    std::array<double, actions> act(const std::vector<double>& observation) const;
};

// The JSBSim properties the observation is made from, in tools/rl/landing.py's
// order.
struct LandingReadings {
    static constexpr std::size_t count = 16;
    static const std::array<const char*, count>& names();
    std::array<double, count> values{};

    static LandingReadings of(const Aircraft& aircraft);
};

// The observation, unnormalised, from the readings, the runway and the last
// action taken.
std::vector<double> landing_observation(const LandingReadings& r, const Runway& runway,
                                        const LearntPolicy& policy,
                                        const std::array<double, LearntPolicy::actions>& previous);

// An action's controls: the elevator, ailerons and rudder as they are, the
// throttle from -1..1 to 0..1, the flaps at the landing flap, no trim, no
// brakes, the gear down.
Controls landing_controls(const std::array<double, LearntPolicy::actions>& action,
                          const LearntPolicy& policy);

// Flies `aircraft` down to `runway` with `policy`, as sim::Lander does with
// its own laws: call `fly` once a step for that step's controls.
class LearntLander {
public:
    LearntLander(const Aircraft& aircraft, const Runway& runway, const LearntPolicy& policy);

    Controls fly();

    // Whether the wheels have touched; and when they first did, the sink,
    // feet a minute, and where - across the centreline, positive right, and
    // along from the threshold, positive down the runway - as sim::Lander
    // measures them.
    bool touched() const { return touched_; }
    double touchdown_sink_fpm() const { return touchdown_sink_fpm_; }
    double touchdown_across_m() const { return touchdown_across_m_; }
    double touchdown_along_m() const { return touchdown_along_m_; }
    // The decisions taken so far.
    long decisions() const { return decisions_; }

private:
    const Aircraft& a_;
    Runway runway_;
    const LearntPolicy& policy_;
    std::array<double, LearntPolicy::actions> previous_{};
    Controls held_;
    long steps_ = 0;
    long decisions_ = 0;
    bool touched_ = false;
    double touchdown_sink_fpm_ = 0.0;
    double touchdown_across_m_ = 0.0;
    double touchdown_along_m_ = 0.0;
};

} // namespace glideslope::sim
