#include "sim/aircraft.hpp"

#include "sim/fixed_step.hpp"

#include <FGFDMExec.h>
#include <initialization/FGInitialCondition.h>
#include <models/FGAircraft.h>
#include <models/FGAuxiliary.h>
#include <models/FGPropagate.h>
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
    exec_->Setdt(FixedStep::step_seconds());
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

void Aircraft::initialize(const InitialConditions& ic) {
    const auto fgic = exec_->GetIC();
    fgic->SetLatitudeDegIC(ic.latitude_deg);
    fgic->SetLongitudeDegIC(ic.longitude_deg);
    fgic->SetTerrainElevationFtIC(ic.terrain_elevation_ft);
    fgic->SetAltitudeASLFtIC(ic.altitude_ft);
    fgic->SetPsiDegIC(ic.heading_deg);
    fgic->SetThetaDegIC(0.0);
    fgic->SetPhiDegIC(0.0);
    fgic->SetVcalibratedKtsIC(ic.airspeed_kts);
    if (!exec_->RunIC()) {
        throw std::runtime_error("JSBSim refused the initial conditions for " + model_);
    }
    if (ic.engine_running) {
        exec_->SetPropertyValue("propulsion/set-running", -1.0);
    }
}

void Aircraft::set_controls(const Controls& c) {
    exec_->SetPropertyValue("fcs/elevator-cmd-norm", -c.elevator);
    exec_->SetPropertyValue("fcs/aileron-cmd-norm", c.aileron);
    exec_->SetPropertyValue("fcs/rudder-cmd-norm", c.rudder);
    exec_->SetPropertyValue("fcs/throttle-cmd-norm[0]", c.throttle);
    exec_->SetPropertyValue("fcs/mixture-cmd-norm[0]", c.mixture);
    exec_->SetPropertyValue("fcs/flap-cmd-norm", c.flaps);
    exec_->SetPropertyValue("fcs/left-brake-cmd-norm", c.left_brake);
    exec_->SetPropertyValue("fcs/right-brake-cmd-norm", c.right_brake);
}

void Aircraft::step() {
    exec_->Run();
}

AircraftState Aircraft::state() const {
    AircraftState s;
    s.sim_time_s = exec_->GetSimTime();
    s.latitude_deg = exec_->GetPropertyValue("position/lat-geod-deg");
    s.longitude_deg = exec_->GetPropertyValue("position/long-gc-deg");
    s.altitude_ft = exec_->GetPropertyValue("position/h-sl-ft");
    s.roll_deg = exec_->GetPropertyValue("attitude/phi-deg");
    s.pitch_deg = exec_->GetPropertyValue("attitude/theta-deg");
    s.heading_deg = exec_->GetPropertyValue("attitude/psi-deg");
    s.u_fps = exec_->GetPropertyValue("velocities/u-fps");
    s.v_fps = exec_->GetPropertyValue("velocities/v-fps");
    s.w_fps = exec_->GetPropertyValue("velocities/w-fps");
    s.p_radps = exec_->GetPropertyValue("velocities/p-rad_sec");
    s.q_radps = exec_->GetPropertyValue("velocities/q-rad_sec");
    s.r_radps = exec_->GetPropertyValue("velocities/r-rad_sec");
    s.airspeed_kts = exec_->GetPropertyValue("velocities/vc-kts");
    s.climb_rate_fpm = exec_->GetPropertyValue("velocities/h-dot-fps") * 60.0;
    s.engine_rpm = exec_->GetPropertyValue("propulsion/engine[0]/engine-rpm");
    return s;
}

} // namespace glideslope::sim
