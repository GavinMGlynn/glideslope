#pragma once

#include <filesystem>
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

private:
    std::string model_;
    std::unique_ptr<JSBSim::FGFDMExec> exec_;
};

} // namespace glideslope::sim
