#pragma once

#include "sim/property_nodes.hpp"

#include <array>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace JSBSim {
class FGFDMExec;
}
class SGPropertyNode;

namespace glideslope::sim {

class Terrain;
class Weather;

// What an aircraft's model files say about it, as JSBSim read them.
struct AircraftFigures {
    std::string model;       // the model's name, as asked for: "c172p"
    std::string description; // the model's own name for itself
    double wing_area_sqft = 0.0;
    double wingspan_ft = 0.0;
    double chord_ft = 0.0;
    double empty_weight_lbs = 0.0;
    int engines = 0;
    // Every engine a jet - a turbojet or a turbofan, not a propeller: what
    // the FAA's Airplane Flying Handbook (FAA-H-8083-3C) teaches in its own
    // chapter 16, and lands differently (sim/lander.cpp).
    bool jet = false;
};

// Where and how an aircraft starts.
struct InitialConditions {
    double latitude_deg = 0.0; // geodetic, WGS84
    double longitude_deg = 0.0;
    double altitude_ft = 0.0;          // above sea level
    double terrain_elevation_ft = 0.0; // the ground beneath, above sea level
    double heading_deg = 0.0;
    double airspeed_kts = 0.0; // calibrated; 0 on the ground
    bool engine_running = true;
    // Retractable gear starts where this puts it, 1 down and 0 up, rather
    // than retracting in the first seconds of a flight begun in the air.
    double gear = 1.0;
    // **The flaps start where this puts them**, 0 up and 1 fully down, rather
    // than running out at their own rate from the first step. A flight begun
    // on an approach is begun at a speed that belongs to the landing flap,
    // and an A380 started clean at its landing reference speed of 136 knots
    // stalled before its flaps were a third of the way out: it fell from 688
    // feet to the runway in ten seconds at 31 degrees of alpha. 0, the
    // default, leaves the flaps up and every other start exactly as it was.
    double flaps = 0.0;
    // **And the speedbrakes likewise**, 0 in and 1 fully out: a B-2A begun
    // on its approach is begun with the drag rudders it lands with already
    // open, which is the drag its trim on the glidepath needs. 0, the
    // default, leaves every other start exactly as it was.
    double speedbrake = 0.0;
    // **The flight path starts at this angle**, degrees, negative descending.
    // A flight begun on an approach is begun on the glidepath, and one
    // started level at the glidepath's height has to be pitched over into
    // the descent first: the approach autopilot did it in the first few
    // seconds and overshot, the A320 reaching 30 ft/s of sink three seconds
    // in, which is a capture and not an approach. 0, the default, starts
    // level, as every other start does.
    double flight_path_deg = 0.0;
    // **Trimmed for that flight path**: JSBSim's longitudinal trim finds the
    // angle of attack, elevator and power that hold the speed on it. Without
    // it the aeroplane starts with its nose on the path and no angle of
    // attack at all, so no lift: an F-15C started on a three-degree approach
    // dropped at 41 ft/s and ran from 196 knots to 208 in the first two
    // seconds, before the autopilot's elevator had caught it. False, the
    // default, leaves every other start exactly as it was.
    bool trim = false;
    // **The attitude it starts at**, degrees: the nose above the horizon and
    // the right wing down. 0 and 0, the defaults, start it level, as every
    // start did before these were given (a test's, to shoot a frame at a
    // stated attitude).
    double pitch_deg = 0.0;
    double roll_deg = 0.0;
};

// What the pilot is doing, each in JSBSim's normalised command range.
struct Controls {
    double elevator = 0.0; // -1 (nose down) .. 1 (nose up)
    double aileron = 0.0;  // -1 .. 1
    double rudder = 0.0;   // -1 .. 1
    double throttle = 0.0; //  0 .. 1
    double mixture = 1.0;  //  0 .. 1
    double flaps = 0.0;    //  0 .. 1
    double left_brake = 0.0;
    double right_brake = 0.0;
    double pitch_trim = 0.0; // -1 (nose down) .. 1 (nose up)
    // The rpm lever of a constant-speed propeller: 0 its lowest rpm, 1 its
    // highest. A fixed-pitch propeller has none, and ignores it.
    double propeller = 1.0;
    // The landing gear: 1 down, 0 up. Fixed gear ignores it.
    double gear = 1.0;
    // A two-speed supercharger's gear change switch: 1 automatic, the
    // aircraft's aneroid choosing high gear with height; 0 held in low gear.
    // An engine without one ignores it.
    double supercharger = 1.0;
    // A twin's throttles set apart: each engine's is `throttle` plus its offset
    // here, the port engine's first. Every engine has `throttle`, `mixture` and
    // `propeller`.
    std::array<double, 2> throttle_offset{};
    // Each engine's radiator shutters or cowl flaps, the port engine's first:
    // 0 closed, 1 open. An engine without them ignores it.
    std::array<double, 2> cooling_flaps{};
    // The speedbrake lever: 0 stowed, 1 fully out - the flight spoilers, and
    // on the ground the ground spoilers too. An aircraft without them
    // ignores it.
    double speedbrake = 0.0;

    // **Every control as a flat list, and back again.** The wire sends a
    // client's inputs to the server (net/inputs.hpp), and the network knows
    // nothing about this structure: it is handed these numbers and hands
    // them back. Keeping the list here rather than in the network is what
    // stops the two drifting apart when a control is added - and
    // `sizeof(Controls)` is held by a test, so a new field that is not put
    // in here is caught rather than quietly left out of every flight.
    static constexpr std::size_t control_count = 17;
    std::array<double, control_count> as_list() const {
        return {elevator,      aileron,    rudder,         throttle,
                mixture,       flaps,      left_brake,     right_brake,
                pitch_trim,    propeller,  gear,           supercharger,
                speedbrake,    throttle_offset[0],         throttle_offset[1],
                cooling_flaps[0],          cooling_flaps[1]};
    }
    static Controls from_list(const std::array<double, control_count>& v) {
        Controls c;
        c.elevator = v[0];
        c.aileron = v[1];
        c.rudder = v[2];
        c.throttle = v[3];
        c.mixture = v[4];
        c.flaps = v[5];
        c.left_brake = v[6];
        c.right_brake = v[7];
        c.pitch_trim = v[8];
        c.propeller = v[9];
        c.gear = v[10];
        c.supercharger = v[11];
        c.speedbrake = v[12];
        c.throttle_offset = {v[13], v[14]};
        c.cooling_flaps = {v[15], v[16]};
        return c;
    }
    bool operator==(const Controls&) const = default;
};

// What is on board, by JSBSim's index for each point mass (seats, baggage) and
// each fuel tank. Anything not named keeps the model's own value.
struct Loading {
    std::map<int, double> pointmass_lbs;
    std::map<int, double> tank_lbs;
};

// Enough of an aircraft's state to compare two flights and to report one.
struct AircraftState {
    double sim_time_s = 0.0;
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double altitude_ft = 0.0; // above sea level
    double height_above_ground_ft = 0.0;
    double terrain_elevation_ft = 0.0; // the ground beneath
    bool on_water = false;             // and whether it is water
    bool ditched = false;              // come down on water, and at rest there
    double roll_deg = 0.0;
    double pitch_deg = 0.0;
    double heading_deg = 0.0;
    double u_fps = 0.0; // body-axis velocity
    double v_fps = 0.0;
    double w_fps = 0.0;
    double p_radps = 0.0; // body-axis rates
    double q_radps = 0.0;
    double r_radps = 0.0;
    double airspeed_kts = 0.0; // calibrated
    double climb_rate_fpm = 0.0;
    double engine_rpm = 0.0;

    bool operator==(const AircraftState&) const = default;
};

// Everything needed to put another instance of the same model into the state an
// aircraft was captured in, and have it fly on from there. Read only by
// Aircraft::restore(); its fields are public so that it can be copied, stored and
// sent, not so that anything else interprets them.
struct AircraftSnapshot {
    std::string model;
    double sim_time_s = 0.0;
    // Where it was, as a person would say it - used only to start a fresh
    // instance somewhere sensible before the exact state is applied.
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double altitude_ft = 0.0;
    double terrain_elevation_ft = 0.0;
    std::array<double, 3> location_ecef_ft{}; // Earth-centred, Earth-fixed
    std::array<double, 4>
        attitude_local{};              // quaternion, body relative to north-east-down
    std::array<double, 3> uvw_fps{};   // body-axis velocity relative to the Earth
    std::array<double, 3> pqr_radps{}; // body-axis rates relative to the Earth
    std::vector<bool> engines_running;
    std::vector<double> thruster_rpm;
    std::vector<std::pair<std::string, double>>
        properties; // every property that is both
                    // readable and writable
};

// **An aircraft's motion alone**: where it is, which way it points, how fast it
// goes and turns - what a client's prediction is put right by, many times a
// second. Not a snapshot: nothing of the engines, the actuators or the fuel,
// which a client flying the same inputs already has, and which a restore
// settles for two simulated seconds to write back - far too slow to do at
// the rate state updates arrive. (What a replay would otherwise fly on from
// where the newest step left it, a prediction keeps for itself: ReplayState.)
struct Motion {
    std::array<double, 3> location_ecef_m{}; // Earth-centred, Earth-fixed
    // A quaternion, north-east-down to the body (JSBSim's qAttitudeLocal).
    std::array<double, 4> attitude_local{};
    std::array<double, 3> uvw_mps{};         // body-axis velocity relative to the Earth
    std::array<double, 3> pqr_radps{};       // body-axis rates relative to the Earth
};

// **What a replay must start from besides the motion**: the state that is
// integrated step by step rather than set by the controls. A prediction
// flying the same steps again from the server's word flies them from this
// as it was at that step, not as the newest step left it: an engine winding
// down was otherwise wound down again by every replay, and a speedbrake
// moving moved on again (sim::Prediction).
//
// **Kept**: each engine's propeller or rotor rpm; a turbine's N1 and N2, and
// a turbine's or turboprop's phase (off, starting, running...); and every
// readable and writable value of the flight controls (`fcs/`, but for the
// commands, which the controls set) and the gear's position - which holds
// each kinematic actuator's position (flaps, speedbrakes, gear), read back by
// JSBSim from its output at every step.
// **Not kept**, as JSBSim holds them in members with no setter: a lag
// filter's or an actuator's own history (its lag, rate limit, hysteresis);
// a turbine's fuel flow, EGT, oil temperature and EPR, and a piston's
// temperatures; the fuel in the tanks. None moves the flight more than a
// step's worth between the server and a client flying the same inputs.
struct ReplayState {
    std::vector<double> thruster_rpm;
    // N1 and N2 in percent; NaN for an engine without that spool.
    std::vector<double> n1;
    std::vector<double> n2;
    // A turbine's or turboprop's phase as JSBSim numbers it; -1 for another.
    std::vector<int> phase;
    // The flight controls' values, in Aircraft::replay_properties' order.
    std::vector<double> controls;
};

// What an aircraft keeps from the catalogue, read once when its model loads.
struct CatalogueFacts {
    std::optional<double> climb_floor_kts;
    bool mixture_lever = false;
    double full_rich_below_ft = 0.0;
    bool speedbrakes = false;
    double yaw_damper_per_degps = 0.05;
    double rudder_integral_rate = 0.05;
    std::optional<double> go_around_flaps;
};

// One aircraft: a JSBSim instance loaded from model files.
//
// JSBSim's headers stay behind this class, so nothing that includes it compiles
// JSBSim's headers or is affected by them.
//
// **An Aircraft is read and written from one thread only**: even its const
// accessors fill a cache of JSBSim's property nodes (`nodes_`), so a const
// Aircraft& used from a second thread is a data race.
class Aircraft {
public:
    // Loads `model` from `jsbsim_root`, which holds aircraft/, engine/ and
    // systems/ in JSBSim's layout. Throws std::runtime_error if the model cannot
    // be loaded.
    Aircraft(const std::filesystem::path& jsbsim_root, const std::string& model);
    ~Aircraft();

    Aircraft(const Aircraft&) = delete;
    Aircraft& operator=(const Aircraft&) = delete;

    AircraftFigures figures() const;

    // **The least speed the autopilot's altitude hold flies at, clean**: a
    // light aeroplane's published best-climb speed, in knots calibrated, read
    // once when the model loads from the catalogue and figures beside the
    // JSBSim root (sim/autopilot.cpp says why). None for every other class,
    // and none for a model loaded where there is no catalogue. Loading a light
    // aeroplane that publishes no climb speed throws, rather than leave it
    // without the floor unnoticed.
    std::optional<double> climb_floor_kts() const {
        return climb_floor_kts_;
    }

    // **Whether its engines have a mixture lever for the autopilot to lean**
    // for best power (sim/leaner.hpp): the catalogue's `mixture-lever`, read
    // once when the model loads. False for a model loaded where there is no
    // catalogue.
    bool mixture_lever() const {
        return mixture_lever_;
    }

    // The pressure altitude below which its handbook has the mixture full
    // rich, and the autopilot does not lean it: the catalogue's
    // `mixture-lever FULL_RICH_BELOW_FT`.
    double full_rich_below_ft() const {
        return full_rich_below_ft_;
    }

    // **Whether the speedbrake lever does anything**: the catalogue's
    // `speedbrakes`, held to the flight model by a test that flies every
    // aircraft with the lever in and out. What the HUD and a state update
    // read to show the lever only where there is one. False for a model
    // loaded where there is no catalogue.
    bool speedbrakes() const {
        return speedbrakes_;
    }

    // **The autopilot's rudder gains**: its yaw damper's travel per degree a
    // second of yaw rate, and its sideslip integral's rate - the catalogue's
    // `yaw-damper PER_DEGPS INTEGRAL_RATE`, 0.05 and 0.05 where it says none
    // (sim/autopilot.cpp says why).
    double yaw_damper_per_degps() const {
        return yaw_damper_per_degps_;
    }
    double rudder_integral_rate() const {
        return rudder_integral_rate_;
    }

    // **The flap lever the stall recovery raises the flaps to**, 0 to 1,
    // where its published figures give one (`go_around_flaps_deg`,
    // sim/figures.hpp). None for an
    // aircraft whose figures give none, or a model loaded where there is no
    // catalogue.
    std::optional<double> go_around_flaps() const {
        return go_around_flaps_;
    }

    // **The flap lever's notches**, 0 to 1 and in order, 0 and 1 among
    // them: the settings of the kinematic its model's flap command drives,
    // each as the lever position that asks for it (aircraft.cpp). The
    // Cherokee's 0, 10, 25 and 40 degrees are 0, 0.25, 0.625 and 1. Empty
    // where the model has no such kinematic.
    const std::vector<double>& flap_notches() const {
        return flap_notches_;
    }

    // Stands the aircraft on `terrain` instead of JSBSim's level ground at one
    // elevation, from now on. Heights, InitialConditions' altitude included,
    // are then above the WGS84 ellipsoid, which is JSBSim's sea level; the
    // terrain elevation in InitialConditions is ignored. Before every step,
    // the ground is made water or land as the terrain says it is where the
    // aircraft is.
    //
    // **A landplane that comes down on water ditches.** Its wheels roll on
    // nothing there, and no flight model here says how an airframe meets water,
    // so the step any of its contact points - a wheel, lowered or not, or a
    // point of its structure - reaches the water, it is brought to rest where it
    // is and held there, as JSBSim holds a vehicle down, until it is started
    // again.
    //
    // **A flying boat floats.** A model with JSBSim's hydrodynamics - the
    // Short S.23's hull and floats - meets the water through them, and does
    // not ditch: before every step its water is put where the terrain's is,
    // and out of its reach over land. The aircraft keeps the terrain alive.
    void set_terrain(std::shared_ptr<Terrain> terrain);
    // The terrain she stands on, or none before one is set.
    const std::shared_ptr<Terrain>& terrain() const { return terrain_; }

    // Flies the aircraft in `weather` from now on: before every step, JSBSim's
    // wind, temperature, pressure and turbulence are set from the conditions
    // where the aircraft is. Without it, JSBSim's still standard atmosphere.
    // Turbulence is seeded the same every time, so a flight in it repeats:
    // with `turbulence_seed`, 1 unless a test asks for other gusts in the
    // same air (the gust landing's seeds).
    void set_weather(std::shared_ptr<Weather> weather, int turbulence_seed = 1);
    // **The runway condition code she last rolled on** (sim/runway_condition.hpp),
    // as her weather gave it before her last step: 6, dry, without weather.
    // Her braked wheels grip as it says.
    int runway_condition() const { return runway_condition_; }
    // **The gust factor of the air she flies in**, knots, as her weather gave
    // it before her last step (Conditions::gust_factor_kt): 0 without weather.
    double gust_factor_kt() const { return gust_factor_kt_; }

    // Sets what is on board. Call before initialize(); the weight is what JSBSim
    // computes from it once the aircraft is initialised.
    void load(const Loading& loading);
    // What each of its fuel tanks holds full, pounds, by JSBSim's index.
    std::vector<double> tank_capacities_lbs() const;
    // **What she weighs as loaded**, pounds - empty, what is on board and
    // her fuel - as JSBSim adds them up, and known before initialize(),
    // where `inertia/weight-lbs` is not yet: for a start's speed that
    // depends on it (sim::for_weight).
    double loaded_weight_lbs() const;

    // Puts the aircraft at `ic`, at rest in the sense that no time has passed,
    // with the engine running if asked. Throws std::runtime_error if JSBSim
    // refuses.
    void initialize(const InitialConditions& ic);
    // **Whether the hull is in the water**, as JSBSim's hydrodynamics has it:
    // for a flying boat, what the wheels' weight is for a landplane - a hull
    // afloat or planing has no weight on any wheel. False for an aircraft
    // with no hydrodynamics.
    bool in_water() const;
    // **What is touching the ground this step**: a wheel - a leg of the
    // undercarriage, which retracts, steers or brakes - and any other part of
    // the airframe: a wingtip, a nose, a belly, a flying boat's keel. What the
    // server judges a crash by (sim/crash.hpp).
    struct Contact {
        bool wheels = false;
        bool airframe = false;
    };
    Contact contact() const;
    // **Every point her model can touch the ground with**, touching or not:
    // where it is on the airframe - JSBSim's structural frame, inches, x aft,
    // y right, z up - and whether it is a wheel, in `contact`'s sense.
    struct ContactPoint {
        double x_in = 0.0;
        double y_in = 0.0;
        double z_in = 0.0;
        bool wheel = false;
    };
    std::vector<ContactPoint> contact_points() const;
    // **What she stands on, worked from those points**: whether on a tail
    // wheel, the attitude she stands at, and the attitude her tail strikes
    // the ground at, pivoting on her main wheels - 90 where nothing behind
    // them can, as for a tail-wheel aeroplane, whose tail is down already.
    // `found` is false where the points name no main wheels, or nothing she
    // falls on to from them; the rest is then as initialised.
    struct Stance {
        bool found = false;
        bool tail_wheel = false;
        double standing_pitch_deg = 0.0;
        double strike_pitch_deg = 90.0;
    };
    Stance stance() const;
    // Whether the last `initialize` asked to trim and JSBSim could.
    bool trimmed() const {
        return trimmed_;
    }

    // The controls take effect from the next step.
    void set_controls(const Controls& controls);

    // Stops engine `engine` - 0 the first, the port engine of a twin - as a
    // failure does, and feathers its propeller if `feather`.
    void fail_engine(int engine, bool feather);
    // Undoes `fail_engine`: ignition on (both magnetos) or fuel on, running,
    // and unfeathered.
    void restart_engine(int engine);
    // **Whether any of its engines has stopped** - failed, or run dry: what a
    // state update says as `engine_stopped`, and what a predicting client
    // holds its own flight model to.
    bool any_engine_stopped() const;
    // The first engine stopped, by its number from 0, or nothing.
    std::optional<int> first_stopped_engine() const;
    // How many engines it has, and whether engine `engine` runs.
    int engine_count() const;
    bool engine_running(int engine) const;

    // Fuel neither burns nor moves while `frozen`: an aircraft measured at a
    // weight - a fighter's minutes in afterburner would burn half its fuel -
    // stays at it.
    void freeze_fuel(bool frozen);

    // Advances the flight model by exactly one 120 Hz step.
    void step();

    AircraftState state() const;

    // The aircraft's state, as far as it can be read out of JSBSim.
    AircraftSnapshot capture() const;

    // Puts this aircraft - freshly loaded, or already flying - into the state
    // `snapshot` was captured in. Throws std::invalid_argument for a snapshot of
    // a different model. See aircraft.cpp for what is restored exactly, what
    // is settled, and why.
    void restore(const AircraftSnapshot& snapshot);
    // Its motion, and its motion set - nothing else about it touched, and
    // nothing settled: the integrator starts afresh from the new state.
    Motion motion() const;
    void set_motion(const Motion& m);
    // What a replay must start from besides the motion, and that set -
    // nothing else touched. set_replay_state throws std::invalid_argument for
    // a state of another model (a different number of engines or values).
    ReplayState replay_state() const;
    void set_replay_state(const ReplayState& state);
    // The flight controls' properties ReplayState::controls holds, by name.
    std::vector<std::string> replay_properties() const;

    // Any JSBSim property by name, for code that needs more than state()
    // reports - the published-figure checks read the gear, the flaps and the
    // propeller. Throws std::out_of_range for a property the model does not
    // have.
    double property(const std::string& name) const;

    // **Whether its angle of attack or sideslip is past what its
    // aerodynamics' tables hold** - the widest breakpoints of any table keyed
    // on either, read from the model when it loads - or is not a number. A
    // trial stops a flight there and judges it not held: past them the model
    // only holds its tables' last values, and a departure has carried the
    // state to a NaN that a debug JSBSim asserts on.
    bool outside_its_tables() const;
    std::pair<double, double> alpha_range_rad() const {
        return alpha_range_rad_;
    }
    std::pair<double, double> beta_range_rad() const {
        return beta_range_rad_;
    }
    // Whether this model has the property at all.
    bool has_property(const std::string& name) const;
    // Whether any of its gear retracts. JSBSim has a gear command for every
    // model, fixed gear or not, so the property alone does not say.
    bool gear_retracts() const;

private:
    Aircraft(const std::filesystem::path& jsbsim_root, const std::string& model,
             const CatalogueFacts& catalogue);

    // **A property by name, found once** - and one the model has not got,
    // found absent once, until it can have come (sim/property_nodes.hpp).
    // Null for a property the model does not have.
    SGPropertyNode* node(const std::string& name) const;
    double value(const std::string& name) const;
    void set(const std::string& name, double v);
    PropertyNodes nodes_;
    // ReplayState's flight-control nodes, found once, and their names.
    std::vector<SGPropertyNode*> replay_nodes_;
    std::vector<std::string> replay_names_;
    void find_replay_nodes();

    void apply_weather();
    void apply_ground(double latitude_deg, double longitude_deg);
    bool meets_the_surface() const;

    std::optional<double> climb_floor_kts_;
    bool mixture_lever_ = false;
    double full_rich_below_ft_ = 0.0;
    bool speedbrakes_ = false;
    std::pair<double, double> alpha_range_rad_{-std::numeric_limits<double>::infinity(),
                                               std::numeric_limits<double>::infinity()};
    std::pair<double, double> beta_range_rad_{-std::numeric_limits<double>::infinity(),
                                              std::numeric_limits<double>::infinity()};
    double yaw_damper_per_degps_ = 0.05;
    double rudder_integral_rate_ = 0.05;
    std::optional<double> go_around_flaps_;
    std::vector<double> flap_notches_;
    std::string model_;
    std::unique_ptr<JSBSim::FGFDMExec> exec_;
    bool trimmed_ = false;
    bool initialized_ = false;
    std::shared_ptr<Terrain> terrain_;
    // Each contact point's height above the surface, by JSBSim's property:
    // gear/unit[i] for a wheel, contact/unit[i] for structure.
    std::vector<std::string> contact_heights_;
    bool hydrodynamics_ = false; // the model has JSBSim's hydrodynamics
    std::shared_ptr<Weather> weather_;
    // What was last given to JSBSim's atmosphere, which rebuilds itself when
    // its sea-level values change and so is told only when they do.
    double applied_temperature_offset_c_ = 0.0;
    double applied_pressure_hpa_ = 1013.25;
    int applied_turbulence_ = 0;
    double dry_braking_friction_ = 0.0;
    double dry_rolling_friction_ = 0.0;
    int runway_condition_ = 6;
    double gust_factor_kt_ = 0.0;
    // The pedals as asked, and the share of them the runway takes.
    double left_brake_ = 0.0;
    double right_brake_ = 0.0;
    double brake_share_ = 1.0;
};

} // namespace glideslope::sim
