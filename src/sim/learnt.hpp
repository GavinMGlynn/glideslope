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
// It lands, and hands the rollout to the approach autopilot (below).

#include "sim/aircraft.hpp"
#include "sim/lander.hpp"
#include "sim/plan.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
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

    static constexpr std::size_t observations = 25;
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
    static constexpr std::size_t count = 17;
    static const std::array<const char*, count>& names();
    std::array<double, count> values{};

    static LandingReadings of(const Aircraft& aircraft);
};

// The observation, unnormalised, from the readings, the runway, the last
// action taken and the remembered drift (`remember_drift`). **The wind in
// it is what the instruments would estimate**, as a flight management system
// does: the ground velocity less the true airspeed along the heading turned
// by the sideslip. Nothing reads JSBSim's wind itself.
std::vector<double> landing_observation(const LandingReadings& r, const Runway& runway,
                                        const LearntPolicy& policy,
                                        const std::array<double, LearntPolicy::actions>& previous,
                                        double integral);

// An action's controls: the elevator, ailerons and rudder as they are, the
// throttle from -1..1 to 0..1, the flaps at the landing flap, no trim, no
// brakes, the gear down.
Controls landing_controls(const std::array<double, LearntPolicy::actions>& action,
                          const LearntPolicy& policy);

// **The drift the policy remembers**: a leaky integral of its distance
// across the centreline, metre-seconds, advanced once a decision - decayed by
// the ten-second memory, and the distance now added for the decision's time.
// tools/rl/landing.py's `remember`.
double remember_drift(double integral, double across_m, const LearntPolicy& policy);

// **The gate the learnt landing is offered at**: a box the policy has been
// flown from, at every one of its corners, and landed within its own limits
// - 5 m of the centreline, under 300 ft/min, stopped on the runway - in every
// wind the box admits (the_learnt_policy_lands_within_its_limits_from_every_
// corner_of_its_gate_in_every_wind_it_admits: 160 landings, worst 3.90 m and
// 277 ft/min). Between 1.6 and 2.4 miles before the threshold, no more than
// 60 m either side of the extended centreline, 20 m above or below the
// glidepath and 5 degrees off the runway's heading; from 3 kt under the
// policy's reference speed to 8 kt over it; the landing flap out and not
// moving; and the wind as the instruments estimate it no more than 15 kt
// across, 8 kt ahead and 5 kt behind. A 15-knot headwind is not admitted:
// from every corner at that speed she bounced 7 ft, and at 10 kt once 3 ft.
// The CLI's start (`glideslope_cli land --learnt`: two miles out on the
// centreline and the glidepath, at the reference speed) is its middle.
// Outside it she is not offered: nothing says the policy lands from there.
struct LearntGate {
    static constexpr double nearest_m = 1.6 * 1852.0;
    static constexpr double furthest_m = 2.4 * 1852.0;
    static constexpr double most_across_m = 60.0;
    static constexpr double most_off_glidepath_m = 20.0;
    static constexpr double most_off_heading_deg = 5.0;
    static constexpr double most_under_vref_kts = 3.0;
    static constexpr double most_over_vref_kts = 8.0;
    static constexpr double most_off_flap = 0.01; // of the flaps' travel: out, not moving
    // The wind, as the instruments estimate it (landing_observation's).
    static constexpr double most_crosswind_kts = 15.0;
    static constexpr double most_headwind_kts = 8.0;
    static constexpr double most_tailwind_kts = 5.0;
};

// **Why `aircraft` is not at `runway`'s gate for `policy`**, in words a pilot
// reads - "4.1 miles out; the gate is 1.6 to 2.4" - the first reason found,
// the aircraft first, then the wind, then the rest in LearntGate's order; empty
// when she is at it.
std::string outside_learnt_gate(const Aircraft& aircraft, const Runway& runway,
                                const LearntPolicy& policy);

// **On final to `runway`**, as the CLI's landings and the learnt landing's
// training start: `out_m` before the threshold on the extended centreline,
// on a `glidepath_deg` glidepath aimed `aim_m` past it, pointing down the
// runway at `airspeed_kts` with `flaps` out and the gear down, the engine
// running, trimmed down the glidepath.
InitialConditions final_approach_start(const Runway& runway, double out_m, double airspeed_kts,
                                       double flaps, double aim_m, double glidepath_deg);

// **The controls JSBSim has her at now** - after a trim, what it found: the
// elevator, the first engine's throttle for every engine, the flaps and the
// pitch trim, and the rest as Controls has them. What a player's aircraft
// started trimmed holds until its player moves anything.
Controls trimmed_controls(const Aircraft& aircraft);

// **The learnt landing for `model`**, `data`/rl/`model`-landing.txt: null
// when there is none, which is how an aircraft is known not to have one.
// Throws std::runtime_error for a file that cannot be read or was trained on
// another model.
std::shared_ptr<const LearntPolicy> learnt_landing(const std::filesystem::path& data,
                                                   const std::string& model);

// Flies `aircraft` down to `runway` with `policy`, as sim::Lander does with
// its own laws: call `fly` once a step for that step's controls.
//
// **The policy lands; the approach autopilot rolls out.** The policy flies
// from wherever it is given the aeroplane to the moment the wheels first
// touch, and then hands the rollout to a sim::Lander on the same runway,
// which has watched the landing all along, knows where she touched, and
// brakes her to a stop on the centreline. Its controls are reached from the
// policy's at a hand's pace, full travel in a second, so the change steps
// nothing. **At the touch, not later**: the policy was trained to five
// seconds past it, and in them it let her swing thirteen degrees off the
// runway's heading and run 60 m off its centreline; the autopilot's rollout,
// given her at the touch, stops her within 3.4 m of it from every start.
class LearntLander {
public:
    enum class Stage { flying, rollout, stopped };

    LearntLander(const Aircraft& aircraft, const Runway& runway,
                 std::shared_ptr<const LearntPolicy> policy, const ApproachSpeeds& speeds);

    Controls fly();

    Stage stage() const { return stage_; }

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
    // The rollout's lander, for where she is on the runway.
    const Lander& rollout() const { return rollout_; }

private:
    const Aircraft& a_;
    Runway runway_;
    std::shared_ptr<const LearntPolicy> policy_;
    Lander rollout_;
    Stage stage_ = Stage::flying;
    std::array<double, LearntPolicy::actions> previous_{};
    double integral_ = 0.0;
    Controls held_;
    long steps_ = 0;
    long decisions_ = 0;
    bool easing_ = false;
    bool touched_ = false;
    double touchdown_sink_fpm_ = 0.0;
    double touchdown_across_m_ = 0.0;
    double touchdown_along_m_ = 0.0;
};

} // namespace glideslope::sim
