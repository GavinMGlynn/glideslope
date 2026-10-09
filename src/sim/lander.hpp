#pragma once

// An autopilot that flies an approach and lands.
//
// **This is not the cruise autopilot** (sim/autopilot.hpp), which holds a
// heading, an altitude and a speed, never touches the flaps, the gear or the
// brakes, and holds no path at all. Here the elevator flies the glidepath and
// the throttle holds the speed, as an autopilot and an autothrottle do, and
// the flaps, the gear and the brakes are worked as the stages need them.
//
// It flies four stages, in order:
//
//   approach   down the extended centreline and a glidepath to the threshold,
//              the flaps and gear put where the landing needs them, the speed
//              held at the reference speed
//   flare      from the flare height, of the wheels: the nose raised and the power closed to
//              arrest the sink, so that the wheels meet the ground at the sink set for her
//   rollout    on the ground: the nose held on the centreline, the brakes on
//   stopped    still
//
// or, from a balloon in the flare, a fifth:
//
//   go_around  full power and the approach's incidence, climbing away, until
//              she is `go_around_ft` over the runway and the landing is given up
//
// **The runway is a datum, not a database.** A threshold, an elevation, a
// heading and a length are all it is; where those come from is the caller's.
//
// Nothing here draws, and nothing here reads the DEM: the ground it lands on
// is the runway's own elevation, which is what the aircraft's terrain must
// agree with.

#include "sim/aircraft.hpp"
#include "sim/leaner.hpp"
#include "sim/plan.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::sim {

// How this aeroplane is flown down an approach. `vref_kts` is the speed over
// the threshold - by convention 1.3 times the stall speed in the landing
// configuration, which `approach_speeds` works out from the aircraft's own
// published figures.
struct ApproachSpeeds {
    double vref_kts = 60.0;
    // The stall speed in the landing configuration that `vref_kts` is
    // usually 1.3 times - not always: a manual that gives its own approach
    // speed is flown at that (`PublishedFigures::approach_kcas`).
    double stall_kts = 0.0;
    // **The loading those speeds are for**, by name in her figures file: the
    // one her landing stall was measured at. A B-2A's 124 kt is for her
    // light loading, 177,160 lb, and at her model's own weight she stalls
    // above it. Empty where no figures were read.
    std::string loading;
    double flap = 1.0;     // the landing flap setting, 0 to 1
    double speedbrake = 0.0; // the speedbrake lever down the approach, 0 to 1
    double flare_ft = 15.0; // height of the wheels above the threshold to begin the flare
    // **The sink the flare brings her wheels to the runway at**, feet a
    // minute: forty, a light aeroplane held off to touch gently, unless her
    // figures give `touchdown_fpm` - a jet's give two hundred, flown on to
    // the runway rather than held off to float. Under the 600 her gear is
    // judged to take (sim/crash.hpp), or refused; and every aircraft's is
    // within the 6 ft/s, 360 ft/min, NASA's go-around criteria study takes
    // as its touchdown criterion (Zaal et al., AIAA Journal of Air
    // Transportation, NTRS 20205010611), which a test holds them to.
    double touchdown_fpm = 40.0;
    // **The glidepath aims past the threshold, not at it.** An approach flown
    // at the threshold puts the flare before it and the wheels on the grass;
    // aiming three hundred metres down the runway puts the aeroplane about
    // fifty feet up as it crosses, which is where an aeroplane should be.
    double aim_m = 300.0;
};

// The speeds for an aircraft, from `data`/figures/MODEL.xml: the lowest
// published stall speed in the landing configuration, times 1.3 - or, where
// the file gives the flight manual's own approach speed, that. Throws
// std::runtime_error where the aircraft publishes no stall speed, because a
// reference speed guessed is a reference speed that means nothing.
ApproachSpeeds approach_speeds(const std::filesystem::path& data,
                               const std::string& model);

// Whether `approach_speeds` has a stall speed to work from: false for the
// 747-400 and the F-22A, whose measured stalls would not hold still.
bool publishes_approach_speed(const std::filesystem::path& data, const std::string& model);

// **How she is landed, for a controller to be told** (`Controller::lands_with`):
// her approach speeds, or none where her figures publish no stall speed. The
// server and the client both tell their controllers from this. Throws
// std::runtime_error where her figures cannot be read.
std::optional<ApproachSpeeds> landing_speeds(const std::filesystem::path& data,
                                             const std::string& model);

// **A stabilized approach, or a go-around.** The FAA's criteria for a visual
// approach: stabilized by 500 ft over the runway (the FAA Safety Team's "It's
// All in Your Approach", and FSF ALAR Briefing Note 7.1's VMC gate), on speed
// within +10/-5 knots of the approach speed (the Private Pilot Airman
// Certification Standards, FAA-S-ACS-6, "Normal Approach and Landing": the
// recommended approach speed or 1.3 Vso, +10/-5 knots), and down in the
// touchdown zone - the first third of the runway (Airplane Flying Handbook,
// FAA-H-8083-3C, chapter 9) or its first 3,000 ft (the Pilot/Controller
// Glossary's touchdown zone), whichever is less. An approach that is not, at
// and below the gate, is gone around from: the AFH (ch. 9, "Go-Arounds
// (Rejected Landings)", https://www.faa.gov/sites/faa.gov/files/
// regulations_policies/handbooks_manuals/aviation/airplane_handbook/
// 10_afh_ch9.pdf) gives an unstable approach as a reason to go around, 500
// ft typical in VMC, and "the need to discontinue a landing may arise at any
// point in the landing process" - so there is no height below which the
// touchdown zone is not judged, though "the most critical go-around is one
// started when very close to the ground". Nothing here is per aircraft: the
// speed is each one's own `vref_kts`, from its figures.
struct StabilizedApproach {
    static constexpr double gate_ft = 500.0;
    static constexpr double most_fast_kts = 10.0;
    static constexpr double most_slow_kts = 5.0;
    // **The speed is judged down to fifty feet**, the height an approach
    // crosses the threshold at: below it the flare takes the speed off on
    // purpose.
    static constexpr double speed_judged_down_to_ft = 50.0;
    static constexpr double longest_zone_m = 914.4; // 3,000 ft
    // **Sustained, not momentary.** FSF ALAR Briefing Note 7.1: "momentary
    // overshoots made necessary by atmospheric conditions are acceptable",
    // frequent or sustained ones are not. Two seconds running is the stated
    // choice for sustained: judged step by step, a gust's spike sent twelve
    // of fourteen aeroplanes round from a well-flown approach in severity-3
    // turbulence. (The target already carries half the gust factor where
    // the approach is flown so - the AFH's practice, ApproachSpeeds' own.)
    static constexpr double sustained_s = 2.0;
    // **Two go-arounds for being unstabilized, and the third approach is
    // landed with the gate waived** (Controller). Airline practice commonly
    // allows two approaches and then a diversion; this AI has nowhere to
    // divert to, and a circuit flown for ever is no answer, so the third is
    // flown as the approach was before the gate - a balloon, or a runway
    // not clear, still sends her round. A stated choice, not a rule found.
    static constexpr int most_go_arounds = 2;
    // The touchdown zone's far end, metres past the threshold.
    static double touchdown_zone_m(const Runway& runway) {
        return std::min(runway.length_m / 3.0, longest_zone_m);
    }
};

// **Why an approach not yet down must be gone around from**, in words, or
// empty while it is stabilized: `along_m` before the threshold (negative
// past it) and `above_m` over its elevation, as the Lander measures them,
// at `kcas`. Above the gate nothing is judged. Below it, the speed outside
// +10/-5 knots of `vref_kts` down to fifty feet; and, to the touch, a
// touchdown that will not be in the zone - where her path down the glidepath
// from here would meet the runway (her height over the threshold's elevation
// at the glidepath's angle, ahead of where she is) is past the zone's end,
// which once she is past the end herself and still in the air it is.
std::string unstabilized(const Runway& runway, const ApproachSpeeds& speeds, double glidepath_deg,
                         double along_m, double above_m, double kcas);

class Lander {
public:
    enum class Stage { approach, flare, rollout, stopped, go_around };

    // **How high a go-around climbs before the landing is given up**, feet
    // over the runway, and whoever has her - the plain autopilot - holds
    // what she is doing from there.
    static constexpr double go_around_ft = 500.0;
    // Whether a go-around has climbed to `go_around_ft`: the landing is over.
    bool gone_around() const;
    // **Go around now**, as told to - by a controller, or the runway not
    // clear: from the approach or the flare, not once she has touched. The
    // go-around is flown as one from a balloon is. `why`, when it is her own
    // reason (an approach not stabilized), for `why_gone_around`.
    void go_around(std::string why = {});
    // **Why she went around by herself**, from an approach not stabilized
    // (`unstabilized`) or a balloon; empty if she has not, or was told to.
    const std::string& why_gone_around() const { return why_gone_around_; }
    // Whether she went around for an approach not stabilized.
    bool went_around_unstabilized() const { return unstabilized_; }
    // **The gate waived** (StabilizedApproach::most_go_arounds): this
    // approach is landed however it is flown.
    void waive_the_gate() { judges_the_gate_ = false; }
    bool judges_the_gate() const { return judges_the_gate_; }

    Lander(const Aircraft& aircraft, const Runway& runway, const ApproachSpeeds& speeds,
           double glidepath_deg = 3.0);

    // **A landing the pilot made, taken over on its roll**: no approach was
    // given, so there is no runway but the one she is on. Its centreline is
    // her track through where she is, its elevation the ground beneath her;
    // she has touched, there, at the attitude she has; and its brakes are
    // set to autobrake 3's deceleration, with the runway's length unknown.
    // To be `resume`d with the pilot's throttle, as a landing given back is.
    static Lander on_its_roll(const Aircraft& aircraft, const ApproachSpeeds& speeds);
    // **And on a runway known**, the one she is rolling on: its centreline
    // held, and braked - as her own landing would be - for what is left of
    // it past where she is, keeping the 300 m that leaves.
    static Lander on_its_roll(const Aircraft& aircraft, const ApproachSpeeds& speeds,
                              const Runway& runway);
    // Autobrake 3's deceleration on the 737, feet a second squared: what a
    // landing taken over on its roll is braked to.
    static constexpr double roll_autobrake_fps2 = 7.2;

    // One 120 Hz step's controls. Call once a step, as the aircraft is now.
    Controls fly();
    // The mixture it is handed, where that is not what the aircraft last
    // had - the controls a controller hands over - to lean from.
    void hand_mixture(double mixture);

    Stage stage() const { return stage_; }
    // The runway and the speeds it lands with: what a go-around is flown
    // round to again (sim/circuit.hpp).
    const Runway& runway() const { return runway_; }
    const ApproachSpeeds& speeds() const { return speeds_; }
    double glidepath_deg() const;

    // **The most incidence the flare raises the nose to**: twelve degrees,
    // or four over what she flew the glidepath at, short of the stall
    // (lander.cpp says how). The path's, learnt before the flare - while a
    // pilot flies it too - and kept when she is given back.
    double most_flare_alpha_deg() const;
    // Whether the wheels have touched the runway yet.
    bool touched() const { return touched_; }

    // **Whether she is still landing on this runway**, for an AI given her
    // back after a pilot had her (sim/controller.hpp), with the throttle at
    // `throttle` as she is flown now. Not yet stopped; on the runway -
    // within `runway_half_width_m` of its centreline, from the flare's
    // 400 m short of the threshold to its far end, and within 30 degrees of
    // its heading; and either on the ground, whoever put her there, or in
    // the air no higher over the ground beneath her than fifty feet - over
    // where she touched, once she has - and not a go-around, climbing with
    // the throttle open.
    bool still_landing(double throttle) const;
    // While the pilot has her, once a step: where she is, and whether and
    // where she touched - so that, given her back, it knows she has landed.
    // Nothing it flies with is moved.
    void watch();
    // Flown again after the pilot had her, the throttle at `throttle`: what
    // its loops carry from step to step - rates, trims, the throttle and the
    // attitude it last asked for, the brake - starts again from how she is,
    // not from before the gap.
    // Where and how she touched, and the autobrake set for the runway left,
    // are kept: they are the landing's, not the loops'.
    void resume(double throttle);

    // Runways are not given a width; this is half the widest, 60 m.
    static constexpr double runway_half_width_m = 30.0;

    // Where the aeroplane is with respect to the runway, as the last `fly`
    // saw it.
    //
    // `along_m` is positive before the threshold and negative past it;
    // `across_m` is positive right of the centreline looking along the
    // landing direction; `above_m` is above the threshold's elevation.
    double along_m() const { return along_m_; }
    double across_m() const { return across_m_; }
    double above_m() const { return above_m_; }

    // The sink at the moment the wheels first touched, feet a minute, or 0
    // before that.
    double touchdown_sink_fpm() const { return touchdown_sink_fpm_; }
    // Where it touched down: across the centreline, and along from the
    // threshold. Meaningless before touchdown.
    double touchdown_across_m() const { return touchdown_across_m_; }
    double touchdown_along_m() const { return touchdown_along_m_; }

private:
    // The controls, but for the mixture.
    Controls fly_laws();

    const Aircraft& a_;
    // **The mixture as the AI leans it** (sim/leaner.hpp), where the
    // aircraft has a lever: from the mixture it was handed, holding the ratio
    // the engine had then and leaning for best power at full throttle. Set
    // full rich in one step, as it once was, an aeroplane handed over leaned
    // high up had its engine stopped; JSBSim's piston engine burns nothing
    // past eight parts of air to one of fuel.
    std::optional<MixtureLeaner> leaner_;
    Runway runway_;
    ApproachSpeeds speeds_;
    double glidepath_rad_ = 0.0;
    Stage stage_ = Stage::approach;
    std::string why_gone_around_;
    bool unstabilized_ = false;
    bool judges_the_gate_ = true;
    long unstable_steps_ = 0; // steps running the gate has found her unstabilized

    double along_m_ = 0.0;
    double across_m_ = 0.0;
    double above_m_ = 0.0;

    double touchdown_sink_fpm_ = 0.0;
    double touchdown_across_m_ = 0.0;
    double touchdown_along_m_ = 0.0;

    // The loops' own state, advanced one step a call.
    double pitch_trim_ = 0.0;
    double throttle_ = 0.0;
    double last_kcas_ = -1.0;  // the last step's airspeed, for its trend
    double kcas_rate_ = 0.0;   // knots a second, smoothed over half a second
    double last_throttle_ = 0.0; // the last step's, which near the ground opens slowly
    double rudder_trim_ = 0.0;
    double aileron_trim_ = 0.0;
    double autobrake_fps2_ = 0.0; // set at the touch, for the runway left
    double brake_ = 0.0;         // held to the autobrake's deceleration
    double last_vg_fps_ = -1.0;  // the last step's groundspeed
    double decel_fps2_ = 0.0;    // how fast she is slowing, smoothed
    double flare_pitch_ = 0.0;
    // The climb rate's last step and its trend, feet a minute a second,
    // smoothed over a quarter of a second: the flare flies the sink a second
    // ahead.
    double last_climb_fpm_ = -1e9;
    double climb_trend_fpm_s_ = 0.0;
    // The most the flare raises the nose to: two degrees short of the
    // attitude her tail strikes the runway at, or her three-point attitude
    // on a tail wheel; with nothing behind her main wheels to strike, only
    // the wing's own incidence limits it. Read from her contacts
    // (Aircraft::stance) as she is built.
    double most_flare_pitch_deg_ = 90.0;
    // The attitude that holds the glidepath, learnt as she flies it; taken
    // from the attitude she has on the first step of the approach.
    double path_pitch_ = 0.0;
    bool path_pitch_set_ = false;
    // Where the flare began: the attitude and the throttle on its first
    // step, from which its nose is let down in a float and its power found.
    bool flare_begun_ = false;
    double flare_begun_pitch_ = 0.0;
    double flare_throttle_ = 0.0;
    // The flare's attitude loop's gain over the path's, brought in over a
    // second as it begins.
    double flare_gain_ = 1.0;
    // Given back by a pilot (`resume`).
    bool resumed_ = false;
    // The incidence she flew the glidepath at, on its last step before the
    // flare - learnt as the pilot flies it too, and kept when she is given
    // back.
    double path_alpha_deg_ = 0.0;
    bool path_alpha_known_ = false;

    // The height of her wheels the flare begins at, feet.
    double flare_height_ft() const;
    bool touched_ = false;
    double touchdown_pitch_deg_ = 0.0; // held through the rollout while she can fly
    double touchdown_above_m_ = 0.0;   // above the runway as the wheels met it
    double touchdown_agl_ft_ = 0.0;    // above the ground beneath, as they met it
    // A jet is landed as a jet (the constructor reads it from the model): its
    // nose lowered as soon as it touches, towards `lowering_pitch_deg_`,
    // which falls from the attitude it touched at, and its spoilers out.
    bool jet_ = false;
    double lowering_pitch_deg_ = 0.0;

    // Her main wheels, where her model puts them: JSBSim's structural frame,
    // inches, x aft, y right, z up (Aircraft::contact_points).
    struct Wheel {
        double x_in, y_in, z_in;
    };
    std::vector<Wheel> main_wheels_;
    // How far her lowest main wheel hangs below her centre of gravity, feet,
    // at her attitude now; 0 where her model names no main wheels.
    double wheels_hang_ft(const AircraftState& s) const;

    void measure();
    // Whether she is on the ground or the water now; the first time she is,
    // where and how she touched is kept.
    bool notice_the_touch(const AircraftState& s);
    // A jet's elevator from the touch on: the nose lowered to the runway.
    double lower_the_nose(const AircraftState& s);
};

} // namespace glideslope::sim
