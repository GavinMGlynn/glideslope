#pragma once

// An autopilot that takes off.
//
// The other half of `sim::Lander`, and the piece whose absence the client
// still announces: "the AI cannot take off; --on-ground is flown by the
// pilot". It flies four stages, in order:
//
//   roll     the throttle opened, the nose held on the centreline by rudder
//            and, while the rudder is still soft, by differential brake
//   rotate   from the lift-off speed: the nose raised to a take-off attitude
//   climb    the wheels off, the flaps raised, the best climb speed held
//   done     through the height it was asked to reach
//
// **A tail-wheel aeroplane swings, and a twin swings harder.** Directional
// control on the ground is the whole difficulty of a take-off and is why the
// rudder is helped by the brakes until it bites.
//
// Nothing here draws, and nothing here reads the DEM: the ground it leaves is
// the runway's own elevation.

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/leaner.hpp"
#include "sim/lander.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace glideslope::sim {

// How this aeroplane is taken off, from its own published figures.
struct DepartureSpeeds {
    double rotate_kts = 55.0; // the nose comes up here
    double climb_kts = 75.0;  // the best climb speed, published or measured
    // **What the take-off itself climbs away at.** For an aeroplane that
    // publishes a take-off roll it is `climb_kts`; for a jet, whose best
    // climb speed is an en-route one of 260 knots or more, it is V2 and ten
    // knots - 1.2 times the stall at the take-off flap, and ten - which is
    // what a jet climbs out at before it cleans up and accelerates.
    double initial_climb_kts = 75.0;
    double flap = 0.0;        // the take-off flap setting, 0 to 1
    // **The height the take-off flap starts coming up at**, feet above the
    // runway, once she is at `initial_climb_kts`. A light aeroplane's is 50:
    // the FAA's Airplane Flying Handbook (FAA-H-8083-3C, chapter 6, the
    // short-field take-off) climbs over the 50 ft obstacle, then "when the
    // airplane is stabilized at Vy, the landing gear (if retractable) and
    // flaps should be retracted ... in increments to avoid sudden loss of
    // lift and settling of the airplane". Every other's is 400: no change of
    // configuration below 400 ft on the take-off path, 14 CFR 25.111(c)(4),
    // which binds the airliners and the business jet and is the most a
    // warbird, a flying boat or a fighter is held to without a handbook
    // height of its own. Speeds made by hand, with no class to go by, take
    // the higher (`flaps_up_ft`, below, gives either).
    double flaps_up_ft = 400.0;
    // **A flying boat's running attitude on the water**, degrees, held from
    // the start of the run until the rotation speed, and three more from
    // there until the hull is clear - as Arthur Gouge flew the Short S.23's
    // take-off tests and as its published water take-off figure records.
    // Zero for a landplane, which rolls with the stick where it sits.
    double running_pitch_deg = 0.0;
    // Whether `rotate_kts` is the aeroplane's own published lift-off speed or
    // was worked from its stall or measured from its model. The caller may
    // want to say so.
    bool rotate_is_published = false;
    // Whether both speeds were measured from the model, with nothing
    // published to give them (`<takeoff_speeds>` in its figures file).
    bool measured_from_model = false;
    // **The weight these speeds are for**, lb: the loading of the figure
    // they were taken from. A take-off at another weight flies them scaled
    // by the square root of the ratio. 0 where no figure names one.
    double reference_lbs = 0.0;
    // **The climb away is flown at `initial_climb_kts` whatever she weighs**
    // - only the rotation is scaled by `reference_lbs`. True for a light
    // aeroplane, whose climb speed is her handbook's best rate of climb, Vy,
    // and whose handbook gives it for any weight: the Cessna 172P's
    // (Pilot's Operating Handbook, 1986, page 4-3, Speeds for Normal
    // Operation), "Unless otherwise noted, the following speeds are based on
    // a maximum weight of 2400 pounds and may be used for any lesser weight.
    // However, to achieve the performance specified in Section 5 for takeoff
    // distance, the speed appropriate to the particular weight must be
    // used" - the take-off's speeds by weight, the climb's not. That is the
    // 172P's handbook's, taken as typical of the class, and deliberately the
    // handbook's simplification: Vy does fall a little with weight. It is the
    // speed the autopilot's best-climb floor holds after the hand-over
    // (Aircraft::climb_floor_kts), so the plan is handed her at the speed it
    // will hold. False for every other class, whose initial climb is V2 and
    // ten, or a speed measured at one weight, and goes with her weight.
    bool climb_for_any_weight = false;
};

// The speeds for an aircraft, from `data`/figures/MODEL.xml.
//
// The best climb speed is the speed its published climb rate was measured at.
// The lift-off speed is the one its published take-off roll was measured at
// where it has one; where it has not - the Cub publishes no take-off roll -
// it is a seventh above the published stall, which is the usual relation, and
// `rotate_is_published` says which it was.
// **A take-off roll measured from the model** gives the lift-off speed too,
// where the stall's cannot be flown on a runway: the F-35B's stall is at 31
// degrees of incidence (its figures). **An aeroplane with a published take-off
// field length** rotates from its stall at that field length's flap, and
// takes off with that flap, where it has a stall speed there; a jet's flaps-up
// stall is its landing stall's for want of any other, and 1.15 times that with
// the flaps up is below the speed a clean airliner flies at. **A lift-off
// speed worked from the flight manual's** (`PublishedFigures::takeoff_kcas`)
// comes after a flying boat's water take-off and before the rest: the
// F-15C's, whose stall is at 40 degrees of incidence. Throws
// std::runtime_error where the aircraft publishes neither a climb speed nor
// anything to work a rotation speed from.
DepartureSpeeds departure_speeds(const std::filesystem::path& data,
                                 const std::string& model);

// The height a take-off starts raising its flap at, feet above the runway,
// for an aeroplane of class `of` (DepartureSpeeds::flaps_up_ft).
double flaps_up_ft(AircraftClass of);

class Departure {
public:
    enum class Stage { roll, rotate, climb, done };

    // `to_ft` is the height above the runway at which the take-off is over.
    Departure(const Aircraft& aircraft, const Runway& runway,
              const DepartureSpeeds& speeds, double to_ft = 500.0);

    Controls fly();
    // The mixture it is handed, where that is not what the aircraft last
    // had - the controls a controller hands over - to lean from.
    void hand_mixture(double mixture);
    Stage stage() const { return stage_; }
    // **Through its height with its take-off trim off**: where the take-off
    // was over before it waited for the flap too. The take-off trials read
    // their climb away here, at the take-off flap their speeds were found at.
    bool climbed_out() const { return climbed_out_; }

    // Where the aeroplane is with respect to the runway, as the last `fly`
    // saw it: metres down the runway from the threshold, right of the
    // centreline, and above the threshold's elevation.
    double along_m() const { return along_m_; }
    double across_m() const { return across_m_; }
    double above_m() const { return above_m_; }

    // How far down the runway the wheels left it, in metres, or 0 before.
    double unstuck_along_m() const { return unstuck_along_m_; }

    // The speed the rotation began at, knots, or 0 before: a little short of
    // the rotation speed, by as much as she gains while the nose comes up.
    double rotation_began_kts() const { return rotation_began_kts_; }

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
    DepartureSpeeds speeds_;
    double to_ft_ = 500.0;
    // The flap lever as this take-off has it, the notch it is moving to,
    // and the flaps' own position a step ago and how many steps they have
    // stood still.
    double flap_lever_ = 0.0;
    double flap_aim_ = 0.0;
    double last_flap_deg_ = 0.0;
    int flaps_still_steps_ = 0;
    bool climbed_out_ = false;
    int steps_ = 0; // flown since the throttle began to open
    Stage stage_ = Stage::roll;

    double along_m_ = 0.0;
    double across_m_ = 0.0;
    double above_m_ = 0.0;
    double unstuck_along_m_ = 0.0;
    // Her height above the runway standing on it.
    double standing_m_ = 0.0;
    // Whether she stands on a tail wheel: a wheel on the centreline behind
    // her main wheels.
    bool tail_wheel_ = false;
    double tail_pitch_deg_ = 0.0; // the attitude the tail is being raised to
    double standing_pitch_deg_ = 0.0; // her attitude standing on the runway
    // The attitude her tail strikes the runway at, pivoting on her main
    // wheels; very large where nothing behind them can.
    double strike_pitch_deg_ = 90.0;
    double takeoff_trim_ = 0.0; // the take-off trim still on

    double throttle_ = 0.0;
    double pitch_trim_ = 0.0;
    double last_elevator_ = 0.0; // the stick as this autopilot last put it
    double climb_target_kts_ = 0.0; // the speed asked of her in the climb
    double climb_gain_ktps_ = 0.0;  // and how fast it builds
    double pull_ = 0.0; // the stick brought back while the nose will not come
    double rotate_pitch_ = 0.0;
    bool rotation_begun_ = false;
    double rotation_began_kts_ = 0.0;
    bool rotated_off_ = false; // whether she was rotated off the ground
    bool was_on_ground_ = true;
    double left_at_pitch_deg_ = 0.0; // her attitude as her wheels last left the ground
    double last_kcas_ = 0.0;
    double accel_ktps_ = 0.0;
    bool unstuck_ = false;

    void measure();
    void read_the_gear();
    void retract_flaps(double kcas);
    // The lever up and the flaps stopped.
    bool flaps_up() const;

public:
    // The speeds she is flown at, for what she weighs.
    const DepartureSpeeds& speeds() const { return speeds_; }
    // The attitude her tail strikes at on the wheels, degrees.
    double strike_pitch_deg() const { return strike_pitch_deg_; }
    bool tail_wheel() const { return tail_wheel_; }
    // The attitude she stands at on the runway, degrees.
    double standing_pitch_deg() const { return standing_pitch_deg_; }
};

} // namespace glideslope::sim
