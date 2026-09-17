#include "sim/aircraft.hpp"

#include <FGFDMExec.h>
#include <models/FGAircraft.h>
#include <models/FGPropulsion.h>
#include <simgear/misc/sg_path.hxx>

#include <stdexcept>

namespace glideslope::sim {

namespace {

// JSBSim reports what it is doing on standard output unless told otherwise. The
// simulation is a library; what reaches the user's terminal is the frontend's
// call, so JSBSim is kept quiet unless JSBSIM_DEBUG asks for more.
std::unique_ptr<JSBSim::FGFDMExec> quiet_exec() {
    JSBSim::FGJSBBase::debug_lvl = 0;
    return std::make_unique<JSBSim::FGFDMExec>();
}

} // namespace

Aircraft::Aircraft(const std::filesystem::path& jsbsim_root, const std::string& model)
    : model_(model), exec_(quiet_exec()) {
    const std::u8string utf8 = jsbsim_root.u8string();
    const SGPath root = SGPath::fromUtf8(std::string(utf8.begin(), utf8.end()));
    exec_->SetRootDir(root);
    exec_->SetAircraftPath(SGPath("aircraft"));
    exec_->SetEnginePath(SGPath("engine"));
    exec_->SetSystemsPath(SGPath("systems"));
    if (!exec_->LoadModel(model)) {
        throw std::runtime_error("JSBSim could not load aircraft '" + model +
                                 "' from " +
                                 (jsbsim_root / "aircraft" / model).string());
    }
}

Aircraft::~Aircraft() = default;

AircraftFigures Aircraft::figures() const {
    AircraftFigures f;
    f.model = model_;
    f.description = exec_->GetAircraft()->GetAircraftName();
    f.wing_area_sqft = exec_->GetPropertyValue("metrics/Sw-sqft");
    f.wingspan_ft = exec_->GetPropertyValue("metrics/bw-ft");
    f.chord_ft = exec_->GetPropertyValue("metrics/cbarw-ft");
    f.empty_weight_lbs = exec_->GetPropertyValue("inertia/empty-weight-lbs");
    f.engines = static_cast<int>(exec_->GetPropulsion()->GetNumEngines());
    return f;
}

} // namespace glideslope::sim
