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
    // terrain elevation in InitialConditions is ignored. The aircraft keeps the
    // terrain alive.
    void set_terrain(std::shared_ptr<Terrain> terrain);

    // Sets what is on board. Call before initialize(); the weight is what JSBSim
    // computes from it once the aircraft is initialised.
    void load(const Loading& loading);

    // Puts the aircraft at `ic`, at rest in the sense that no time has passed,
    // with the engine running if asked. Throws std::runtime_error if JSBSim
    // refuses.
    void initialize(const InitialConditions& ic);

    // The controls take effect from the next step.
    void set_controls(const Controls& controls);

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
    std::string model_;
    std::unique_ptr<JSBSim::FGFDMExec> exec_;
    bool initialized_ = false;
};

} // namespace glideslope::sim
