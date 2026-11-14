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
#include <optional>
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
    // **The weights it was trained at**, pounds, least and most: `trained_lbs`
    // in its file. Outside them its gate refuses her (outside_learnt_gate)
    // and she is landed by the approach autopilot at the speed for her
    // weight - the policy flies `vref_kts` whatever she weighs.
    double trained_least_lbs = 0.0;
    double trained_most_lbs = 0.0;
    // **The crosswind her handbook says she was demonstrated in**, knots,
    // from her figures (PublishedFigures::demonstrated_crosswind_kts), set by
    // `learnt_landing`: her gate admits no more across
    // (learnt_gate_most_crosswind_kts). 0 where her figures give none.
    double demonstrated_crosswind_kts = 0.0;
    // **The wind she is put on final in**, from her figures
    // (PublishedFigures::on_final_headwind_kts and the rest), set by
    // `learnt_landing`; below nought, none given.
    double on_final_headwind_kts = -1.0;
    double on_final_tailwind_kts = -1.0;
    double on_final_crosswind_kts = -1.0;

    // Whether `weight_lbs` is within the weights it was trained at.
    bool trained_for(double weight_lbs) const {
        return weight_lbs >= trained_least_lbs && weight_lbs <= trained_most_lbs;
    }

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
// wind at the edges of what it admits, and stabilized by 500 ft since
// 2026-11-04 (the_learnt_landing_is_stabilized_by_500_ft_and_lands_within_
// its_limits_from_every_corner_of_its_gate_* in nine winds: 288 landings,
// worst 2.36 m and 244 ft/min). Between 1.7
// and 2.4 miles before the threshold, no more than 60 m either side of the
// extended centreline, 20 m above or below the glidepath and 5 degrees off
// the runway's heading; from 3 kt under the policy's reference speed to 8 kt
// over it; the landing flap out and not moving; and the wind as the
// instruments estimate it no more than 15 kt across, 15 kt ahead and 5 kt
// behind, and 15 kt in all - training's wind (tools/rl/env.py's `wind`).
// **15 kt across with the wind behind since 2026-11-03** (2.75 for a day
// before it). **15 kt ahead since 2026-11-02**
// (8 before): training's wind blows up to 15 kt from anywhere, and the
// policy of that day lands from every corner with 15 kt ahead at least 29 m
// past the threshold, under 165 ft/min and within 2.4 m - inside the 5 per
// cent margin it was chosen by - where its 2026-10-20 predecessor, which
// bounced at 10 and 15 kt, touched before the threshold from held-out starts
// 9.6 to 13.1 kt ahead (the_learnt_policy_lands_past_the_threshold_from_the_
// held_out_starts_it_once_touched_short_from_in_headwinds).
// The CLI's start (`glideslope_cli land --learnt`: two miles out on the
// centreline and the glidepath, at the reference speed) is its middle.
// Outside it she is not offered: nothing says the policy lands from there.
// **1.7 miles out at the nearest since 2026-11-04** (1.6 before): its
// nearest, lowest corner was 494 ft over the runway, under the stabilized
// gate's 500, and the approach autopilot that now flies the approach
// (LearntLander), climbing back to the glidepath from 20 m under it with
// the wind ahead or behind, was 5 kt slow there or 14 kt fast, and went
// round from 6 or 7 of the 32 corners in five of the nine winds. From 1.7
// (527 ft) it is stabilized from all 288.
struct LearntGate {
    static constexpr double nearest_m = 1.7 * 1852.0;
    static constexpr double furthest_m = 2.4 * 1852.0;
    static constexpr double most_across_m = 60.0;
    static constexpr double most_off_glidepath_m = 20.0;
    static constexpr double most_off_heading_deg = 5.0;
    static constexpr double most_under_vref_kts = 3.0;
    static constexpr double most_over_vref_kts = 8.0;
    static constexpr double most_off_flap = 0.01; // of the flaps' travel: out, not moving
    // The wind, as the instruments estimate it (landing_observation's).
    static constexpr double most_crosswind_kts = 15.0;
    static constexpr double most_headwind_kts = 15.0;
    static constexpr double most_tailwind_kts = 5.0;
    // **And in all no more than 15 kt**, as training's wind blew
    // (tools/rl/env.py's `wind`): 15 across with 15 ahead is not admitted.
    static constexpr double most_wind_kts = 15.0;
    // **With the wind behind, as much across as with none**, so that with
    // 5 behind the wind in all is the edge: 14.1 across. From 2026-11-02 to
    // 2026-11-03 a limit of its own held it to 2.75 kt, the policy of
    // 2026-11-02 touching 49 and 311 m short at two corners with 5 behind
    // and 14.1 across from the left; trained there (2026-11-03, the
    // policy's part 13), it lands from every corner of both sides with 5%
    // to spare, and the limit is gone.
};

// **The most crosswind her gate admits**: LearntGate::most_crosswind_kts,
// what training blew, or less where her figures give a demonstrated
// crosswind under it (LearntPolicy::demonstrated_crosswind_kts) - the J-3
// Cub's 12 kt. Never more: no policy was trained in more.
double learnt_gate_most_crosswind_kts(const LearntPolicy& policy);

// **Why `aircraft` is not at `runway`'s gate for `policy`**, in words a pilot
// reads - "4.1 miles out; the gate is 1.7 to 2.4" - the first reason found,
// the aircraft first, then the wind, then the rest in LearntGate's order; empty
// when she is at it.
std::string outside_learnt_gate(const Aircraft& aircraft, const Runway& runway,
                                const LearntPolicy& policy);

// **How far out a player put on final starts, for `policy`'s gate**: so
// that, joined and left alone, she is at the gate when she starts and still
// at it `LearntGateStart::held_s` later. In that time she flies at most
// `held_s` at the fastest the gate admits (her vref and its
// `most_over_vref_kts`, in still air); the start is halfway between the near
// edge that far out and the far edge - as much to spare at either end. For
// the C172P (59.8 kt) 2.33 miles, the C182 and the PA-28 (64.35) 2.35, the
// J-3 Cub (42.9) 2.26: at vref, 210 to 380 m short of the near edge after
// thirty seconds. Where that cannot be done - a gate too short for the time
// - the gate's middle.
struct LearntGateStart {
    static constexpr double held_s = 30.0;
};
double learnt_gate_start_m(const LearntPolicy& policy);

// **Whether a player waiting her turn on final may start**, judged against
// one aircraft ahead of her (a player's, flying): the server gives the
// waiting player her aircraft once every one ahead has done one of these.
//
// - **three miles on**: 3 nm from where she would start - the radar minimum
//   in trail on a final approach (FAA JO 7110.65, 5-5-4), between two light
//   aeroplanes with no wake minimum to add;
// - **landed**: within 5 ft of the ground - a start 2.3 miles out leaves
//   the one ahead stopped on the runway short of three miles from it;
// - **gone around**: 100 ft higher than the one ahead started - on final she
//   only comes down, so above her start she has climbed away, and a tower
//   sequences the next arrival behind a go-around afresh;
// - **left the final**: 1.5 nm across its course, the lateral minimum the
//   server keeps aircraft apart by (Separation::minimum_m);
// - **waited out**: the one ahead has been on final as long as three miles
//   take her at her approach speed, and a minute more - 240 s behind a C172P
//   at 59.8 kt, 312 s behind a J-3 Cub at 42.9 - and is at least 1.5 nm from
//   where the next would start, the lateral minimum the server keeps
//   aircraft apart by: past the bound the next is never put on top of her,
//   and waits on for one of the others. (At three miles she is let by
//   already, so the bound eases the spacing to the minimum, not to none.)
struct AheadOnFinal {
    double from_start_m = 0.0;            // from where the waiting player would start
    double height_above_ground_ft = 0.0;
    double above_her_start_ft = 0.0;      // above where the one ahead started
    double across_m = 0.0;                // across the final's course
    double on_final_s = 0.0;              // since the one ahead started on final
    double approach_kts = 0.0;            // the one ahead's, as she started on final
};
enum class TurnOnFinal { holds, three_miles_on, landed, gone_around, left_final, waited_out };
struct TurnOnFinalRule {
    static constexpr double in_trail_m = 3.0 * 1852.0;
    static constexpr double landed_ft = 5.0;
    static constexpr double gone_around_ft = 100.0;
    static constexpr double margin_s = 60.0;
};
TurnOnFinal turn_on_final(const AheadOnFinal& ahead);
// The longest the next waits behind one flying her final at `approach_kts`.
double longest_behind_s(double approach_kts);
// In words: "landed", "3 miles on", ...
const char* said(TurnOnFinal turn);

// **Why the wind refuses a final to a runway heading `heading_deg`** for a
// learnt landing's gate (`policy`, or none: the gate's own limits), the wind
// blowing towards the north and east at `wind_north_mps` and
// `wind_east_mps`; empty where it admits it. Its tailwind, headwind,
// crosswind and the wind in all, as outside_learnt_gate judges them: where
// a player held past the bound may be sent to a runway's other end.
std::string learnt_gate_wind_refuses(const LearntPolicy* policy, double heading_deg,
                                     double wind_north_mps, double wind_east_mps);

// **Why a player is not put on final to a runway heading `heading_deg`** in
// this wind (blowing towards the north and east, metres a second): more
// ahead, behind or across than she stays at her gate in, left alone, thirty
// seconds (`policy`'s on_final_* limits, measured and in her figures; the
// owner's ruling, 2026-11-11): any wind past a limit, however little, is
// outside it - a crosswind limit of nought refuses any across; empty where
// the wind is inside them, or her figures give none.
std::string on_final_wind_refuses(const LearntPolicy& policy, double heading_deg,
                                  double wind_north_mps, double wind_east_mps);

// **Where she is against `runway`'s final**: before its threshold along its
// centreline, right of it, and above its elevation, in metres.
struct OnFinal {
    double along_m = 0.0;
    double across_m = 0.0;
    double above_m = 0.0;
};
OnFinal on_final(const Aircraft& aircraft, const Runway& runway);

// **How far before `runway`'s threshold she is**, along its centreline, in
// metres - as her gate measures it (outside_learnt_gate's miles out).
double learnt_gate_along_m(const Aircraft& aircraft, const Runway& runway);

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

// **What a player's aircraft holds until she moves anything**: started
// trimmed (InitialConditions::trim), what the trim found - her aileron and
// rudder too where it was lateral; where JSBSim could not trim her, six
// tenths of her throttle and nothing else. Her gear lever either way is where
// she was started with it. Held at six tenths and the stick neutral, an F-35B
// whose player was slow to join was in a dive before she had touched anything.
Controls held_until_moved(const Aircraft& aircraft);

// **The learnt landing for `model`**, `data`/rl/`model`-landing.txt: null
// when there is none, which is how an aircraft is known not to have one.
// Throws std::runtime_error for a file that cannot be read or was trained on
// another model.
std::shared_ptr<const LearntPolicy> learnt_landing(const std::filesystem::path& data,
                                                   const std::string& model);

// **The speed the AI flies her final approach at, at her model's own
// loading** - what a server flies every aircraft at - for a brief to tell:
// her learnt landing's own where she has one trained at that weight (the AI
// flies her to its gate at it, sim::Controller::to_ai_approach), and
// otherwise her approach speed for that weight (sim::for_weight). 0 where
// her figures publish no stall speed. Throws as approach_speeds and
// learnt_landing do.
double approach_kts_flown(const std::filesystem::path& data, const std::string& model);

// Flies `aircraft` down to `runway` with `policy`, as sim::Lander does with
// its own laws: call `fly` once a step for that step's controls.
//
// **The approach autopilot flies the approach; the policy lands**
// (2026-11-04). The policy flies its approach up to 35 kt fast - it was
// rewarded for the touch alone, and dives down the glidepath on power
// (docs/status, 2026-10-10) - so the stabilized-approach gate would send it
// round from every corner of its gate. So a sim::Lander at the policy's own
// speed (`vref_kts`, the speed its gate admits her at) flies her from the
// gate to `policy_takes_her_ft` over the runway, judged by the gate as any
// approach of the autopilot's is, and the policy flies the rest - the round
// out and the touch - from there. Gone around by that lander, she is not
// the policy's: `unstabilized` says why, and the controller goes around.
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
    // **Who flies the approach**: the approach autopilot down to
    // `policy_takes_her_ft`, the policy from there (the default, and how
    // every landing in a session is flown); or the policy from wherever it
    // is given her, as it was trained - for the tests that hold the
    // simulation's policy to training's.
    enum class Approach { autopilot, policy };

    // **The height over the runway the policy takes her at**, feet: 35,
    // tuned. Measured from every corner of the gate in its nine edge winds
    // (2026-11-04): handed over at the autopilot's own flare, 19 ft, the
    // policy met the runway at 348 ft/min with 15 kt ahead; at 50, where the
    // gate stops judging the speed, at 315 with 5 kt behind and 14.1 across
    // from the right; at 30, 290 quartering ahead from the left; at 35 its
    // worst was 245, and over a grid of 85 winds, four corners and four
    // loads, 1,360 landings, 267.
    static constexpr double policy_takes_her_ft = 35.0;

    LearntLander(const Aircraft& aircraft, const Runway& runway,
                 std::shared_ptr<const LearntPolicy> policy, const ApproachSpeeds& speeds,
                 Approach approach = Approach::autopilot);

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
    // **Why the stabilized-approach gate sends her round**: empty until the
    // approach autopilot flying her approach has gone around for it - or,
    // once the policy has her, the gate (sim::unstabilized, at the policy's
    // own speed) has found her unstabilized for StabilizedApproach::
    // sustained_s running - and then the first such reason, kept. The
    // controller goes around for it.
    const std::string& unstabilized() const { return unstabilized_; }
    // The longest the gate has found her unstabilized, running, seconds,
    // below 500 ft and before the touch, whoever flew her.
    double most_unstable_s() const;

private:
    const Aircraft& a_;
    Runway runway_;
    std::shared_ptr<const LearntPolicy> policy_;
    Lander rollout_;
    // The approach autopilot, at the policy's speed, until the policy has her.
    std::optional<Lander> approach_;
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
    long unstable_steps_ = 0;
    long most_unstable_steps_ = 0;
    std::string unstabilized_;
};

} // namespace glideslope::sim
