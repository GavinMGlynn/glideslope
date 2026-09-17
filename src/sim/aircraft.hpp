#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace JSBSim {
class FGFDMExec;
}

namespace glideslope::sim {

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
    double latitude_deg = 0.0;
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

    // Any JSBSim property by name, for code that needs more than state()
    // reports - the published-figure checks read the gear, the flaps and the
    // propeller. Throws std::out_of_range for a property the model does not
    // have.
    double property(const std::string& name) const;

private:
    std::string model_;
    std::unique_ptr<JSBSim::FGFDMExec> exec_;
};

} // namespace glideslope::sim
