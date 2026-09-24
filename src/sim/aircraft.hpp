#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace JSBSim {
class FGFDMExec;
}

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

// One aircraft: a JSBSim instance loaded from model files.
//
// JSBSim's headers stay behind this class, so nothing that includes it compiles
// JSBSim's headers or is affected by them.
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

    // Flies the aircraft in `weather` from now on: before every step, JSBSim's
    // wind, temperature, pressure and turbulence are set from the conditions
    // where the aircraft is. Without it, JSBSim's still standard atmosphere.
    // Turbulence is seeded the same every time, so a flight in it repeats.
    void set_weather(std::shared_ptr<Weather> weather);

    // Sets what is on board. Call before initialize(); the weight is what JSBSim
    // computes from it once the aircraft is initialised.
    void load(const Loading& loading);

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
    // Whether the last `initialize` asked to trim and JSBSim could.
    bool trimmed() const {
        return trimmed_;
    }

    // The controls take effect from the next step.
    void set_controls(const Controls& controls);

    // Stops engine `engine` - 0 the first, the port engine of a twin - as a
    // failure does, and feathers its propeller if `feather`.
    void fail_engine(int engine, bool feather);

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

    // Any JSBSim property by name, for code that needs more than state()
    // reports - the published-figure checks read the gear, the flaps and the
    // propeller. Throws std::out_of_range for a property the model does not
    // have.
    double property(const std::string& name) const;

private:
    void apply_weather();
    void apply_ground(double latitude_deg, double longitude_deg);
    bool meets_the_surface() const;

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
};

} // namespace glideslope::sim
