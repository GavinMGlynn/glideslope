#include "sim/aircraft.hpp"

#include "sim/fixed_step.hpp"

#include <FGFDMExec.h>
#include <initialization/FGInitialCondition.h>
#include <input_output/FGPropertyManager.h>
#include <math/FGColumnVector3.h>
#include <math/FGLocation.h>
#include <math/FGMatrix33.h>
#include <math/FGQuaternion.h>
#include <models/FGAircraft.h>
#include <models/FGAuxiliary.h>
#include <models/FGInertial.h>
#include <models/FGPropagate.h>
#include <models/FGPropulsion.h>
#include <models/propulsion/FGEngine.h>
#include <models/propulsion/FGThruster.h>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>

#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>

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

void Aircraft::load(const Loading& loading) {
    const auto set = [this](const std::string& name, double value) {
        if (!exec_->GetPropertyManager()->HasNode(name)) {
            throw std::out_of_range(model_ + " has no " + name);
        }
        exec_->SetPropertyValue(name, value);
    };
    for (const auto& [index, lbs] : loading.pointmass_lbs) {
        set("inertia/pointmass-weight-lbs[" + std::to_string(index) + "]", lbs);
    }
    for (const auto& [index, lbs] : loading.tank_lbs) {
        set("propulsion/tank[" + std::to_string(index) + "]/contents-lbs", lbs);
    }
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
    initialized_ = true;
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

// ---------------------------------------------------------------------------
// Capture and restore.
//
// **JSBSim has no snapshot call**, so a snapshot is assembled from what can be
// read out of it: the rigid-body state (position relative to the Earth,
// attitude, velocities and rates), each engine's running flag and propeller
// RPM, and every property that is both readable and writable - the controls,
// the control surfaces and flaps where they have got to, fuel, loads, mixture
// and magnetos. Properties describing the run, the weather or the rigid body are
// left out; see captured() below.
//
// That is not all of JSBSim's state. Its engine keeps a lagged manifold
// pressure and its temperatures, its actuators and filters keep their own
// histories, and its integrators keep past derivatives - none of it reachable.
// Restoring only what can be read left the engine 60 hp adrift of the original
// and the copy 80 ft and 3.6 degrees away after thirty seconds of manoeuvring.
// So restore() settles those hidden states before it finishes: for two seconds
// of simulated time it holds the aircraft at the captured state, re-applying
// it before every step, so the engine and actuators converge on what that
// flight condition implies; then it applies the state once more, exactly,
// recomputes the forces with no time passing, and seeds the integrators' history
// from them - which is what JSBSim itself does when it starts a flight.
//
// **The Earth's rotation is not part of the snapshot.** JSBSim integrates in an
// inertial frame whose angle to the Earth grows with simulated time, so an
// inertial position captured from one instance means a different place in
// another. The snapshot holds the Earth-relative state, and restore() rebuilds
// the inertial position, orientation and velocity in this instance's own
// frame. Getting this wrong put the copy 25,000 ft away after twenty seconds.
// ---------------------------------------------------------------------------

namespace {

constexpr int settle_steps = 2 * static_cast<int>(steps_per_second);

// Left out of the properties:
//   - the run's settings and the initial conditions it was started from;
//   - the atmosphere, which is the weather's to set. Writing it back also made
//     JSBSim complain, on every settling step, that the dew point it was given
//     was below what it allows;
//   - the rigid-body state - position, attitude, velocities - which the snapshot
//     holds in its own fields. Several of those properties' setters move the
//     aircraft as a side effect, and restoring the state both ways at once hid
//     whether the explicit restore below was right: removing its inertial
//     position did not change a single test.
bool captured(const std::string& name) {
    for (const char* excluded : {"simulation", "ic[", "ic/", "atmosphere", "position",
                                 "attitude", "velocities"}) {
        if (name.rfind(excluded, 0) == 0) {
            return false;
        }
    }
    return true;
}

void for_each_property(
    SGPropertyNode* base, SGPropertyNode* node,
    const std::function<void(const std::string&, SGPropertyNode*)>& fn) {
    for (int i = 0; i < node->nChildren(); ++i) {
        SGPropertyNode* child = node->getChild(i);
        if (child->nChildren() > 0) {
            for_each_property(base, child, fn);
            continue;
        }
        if (!child->hasValue() || !child->getAttribute(SGPropertyNode::READ) ||
            !child->getAttribute(SGPropertyNode::WRITE)) {
            continue;
        }
        switch (child->getType()) {
        case simgear::props::BOOL:
        case simgear::props::INT:
        case simgear::props::LONG:
        case simgear::props::FLOAT:
        case simgear::props::DOUBLE: {
            // Paths relative to this instance: two instances in one process
            // sit at different indices in the property tree.
            const std::string full = child->getPath();
            const std::string prefix = base->getPath() + "/";
            const std::string name = full.substr(prefix.size());
            if (captured(name)) {
                fn(name, child);
            }
            break;
        }
        default: break;
        }
    }
}

// This instance's inertial frame, from its own Earth position angle.
JSBSim::FGMatrix33 inertial_to_earth(double epa) {
    return {std::cos(epa), std::sin(epa), 0.0, -std::sin(epa), std::cos(epa), 0.0,
            0.0,           0.0,           1.0};
}

} // namespace

AircraftSnapshot Aircraft::capture() const {
    AircraftSnapshot s;
    s.model = model_;
    s.sim_time_s = exec_->GetSimTime();
    s.latitude_deg = exec_->GetPropertyValue("position/lat-geod-deg");
    s.longitude_deg = exec_->GetPropertyValue("position/long-gc-deg");
    s.altitude_ft = exec_->GetPropertyValue("position/h-sl-ft");
    s.terrain_elevation_ft =
        exec_->GetPropertyValue("position/terrain-elevation-asl-ft");

    const auto& vs = exec_->GetPropagate()->GetVState();
    for (unsigned i = 1; i <= 3; ++i) {
        s.location_ecef_ft[i - 1] = vs.vLocation(i);
        s.uvw_fps[i - 1] = vs.vUVW(i);
        s.pqr_radps[i - 1] = vs.vPQR(i);
    }
    for (unsigned i = 1; i <= 4; ++i) {
        s.attitude_local[i - 1] = vs.qAttitudeLocal(i);
    }

    const auto propulsion = exec_->GetPropulsion();
    for (unsigned i = 0; i < propulsion->GetNumEngines(); ++i) {
        const auto engine = propulsion->GetEngine(i);
        s.engines_running.push_back(engine->GetRunning());
        s.thruster_rpm.push_back(engine->GetThruster()->GetRPM());
    }

    SGPropertyNode* base = exec_->GetPropertyManager()->GetNode();
    for_each_property(base, base, [&](const std::string& name, SGPropertyNode* node) {
        s.properties.emplace_back(name, node->getDoubleValue());
    });
    return s;
}

void Aircraft::restore(const AircraftSnapshot& s) {
    if (s.model != model_) {
        throw std::invalid_argument("a snapshot of " + s.model +
                                    " cannot be restored into " + model_);
    }
    const auto propulsion = exec_->GetPropulsion();
    if (s.engines_running.size() != propulsion->GetNumEngines()) {
        throw std::invalid_argument("the snapshot has a different number of engines");
    }

    const JSBSim::FGColumnVector3 location_ecef(
        s.location_ecef_ft[0], s.location_ecef_ft[1], s.location_ecef_ft[2]);

    if (!initialized_) {
        // A fresh instance needs JSBSim's own start before anything else, from
        // where it is going. Starting it anywhere else is not harmless: a first
        // version estimated the altitude from the Earth's mean radius, started a
        // copy 37,000 ft underground at 34 degrees south, and JSBSim's start left
        // NaN in the engine that nothing afterwards could wash out.
        const auto fgic = exec_->GetIC();
        fgic->SetLatitudeDegIC(s.latitude_deg);
        fgic->SetLongitudeDegIC(s.longitude_deg);
        fgic->SetTerrainElevationFtIC(s.terrain_elevation_ft);
        fgic->SetAltitudeASLFtIC(s.altitude_ft);
        if (!exec_->RunIC()) {
            throw std::runtime_error("JSBSim refused to start " + model_ +
                                     " for a restore");
        }
        initialized_ = true;
    }

    // The location takes this instance's Earth ellipse from its current state.
    JSBSim::FGLocation location = exec_->GetPropagate()->GetVState().vLocation;
    location = location_ecef;

    for (unsigned i = 0; i < propulsion->GetNumEngines(); ++i) {
        if (s.engines_running[i]) {
            propulsion->InitRunning(static_cast<int>(i));
        } else {
            propulsion->GetEngine(i)->SetRunning(false);
        }
    }

    const auto apply = [&] {
        SGPropertyNode* base = exec_->GetPropertyManager()->GetNode();
        for (const auto& [name, value] : s.properties) {
            base->setDoubleValue(name.c_str(), value);
        }

        const auto propagate = exec_->GetPropagate();
        JSBSim::FGPropagate::VehicleState vs = propagate->GetVState();
        vs.vLocation = location;
        vs.vUVW = JSBSim::FGColumnVector3(s.uvw_fps[0], s.uvw_fps[1], s.uvw_fps[2]);
        vs.vPQR =
            JSBSim::FGColumnVector3(s.pqr_radps[0], s.pqr_radps[1], s.pqr_radps[2]);
        for (unsigned i = 1; i <= 4; ++i) {
            vs.qAttitudeLocal(i) = s.attitude_local[i - 1];
        }
        const JSBSim::FGMatrix33 i2ec =
            inertial_to_earth(propagate->GetEarthPositionAngle());
        vs.vInertialPosition = i2ec.Transposed() * location_ecef;
        const JSBSim::FGMatrix33 i2l = location.GetTec2l() * i2ec;
        vs.qAttitudeECI = i2l.GetQuaternion() * vs.qAttitudeLocal;
        propagate->SetVState(vs);
        const JSBSim::FGColumnVector3& omega = exec_->GetInertial()->GetOmegaPlanet();
        propagate->SetInertialVelocity(propagate->GetTb2i() * vs.vUVW +
                                       omega * vs.vInertialPosition);

        for (unsigned i = 0; i < propulsion->GetNumEngines(); ++i) {
            propulsion->GetEngine(i)->GetThruster()->SetRPM(s.thruster_rpm[i]);
        }
    };

    for (int i = 0; i < settle_steps; ++i) {
        apply();
        exec_->Run();
    }
    apply();
    exec_->SuspendIntegration();
    exec_->Run();
    exec_->ResumeIntegration();
    exec_->GetPropagate()->InitializeDerivatives();
    exec_->Setsim_time(s.sim_time_s);
}

double Aircraft::property(const std::string& name) const {
    if (!exec_->GetPropertyManager()->HasNode(name)) {
        throw std::out_of_range(model_ + " has no property " + name);
    }
    return exec_->GetPropertyValue(name);
}

} // namespace glideslope::sim
