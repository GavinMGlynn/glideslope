#include "sim/aircraft.hpp"

#include "sim/fixed_step.hpp"
#include "sim/terrain.hpp"
#include "sim/weather.hpp"

#include <FGFDMExec.h>
#include <input_output/FGGroundCallback.h>
#include <initialization/FGInitialCondition.h>
#include <input_output/FGPropertyManager.h>
#include <math/FGColumnVector3.h>
#include <math/FGLocation.h>
#include <math/FGMatrix33.h>
#include <math/FGQuaternion.h>
#include <models/FGAircraft.h>
#include <models/FGAuxiliary.h>
#include <models/FGGroundReactions.h>
#include <models/FGInertial.h>
#include <models/FGLGear.h>
#include <models/FGPropagate.h>
#include <models/FGPropulsion.h>
#include <models/propulsion/FGEngine.h>
#include <models/propulsion/FGPiston.h>
#include <models/propulsion/FGPropeller.h>
#include <models/propulsion/FGThruster.h>
#include <models/propulsion/FGTurbine.h>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>

#include <algorithm>
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

// Whether any of the aircraft's gear retracts. Fixed gear stays down whatever
// the pilot's lever says: some models, JSBSim's pa28 among them, charge their
// gear's drag by its position, which would otherwise go up with the lever.
bool retractable_gear(const JSBSim::FGFDMExec& exec) {
    const auto ground = exec.GetGroundReactions();
    for (int i = 0; i < ground->GetNumGearUnits(); ++i) {
        if (ground->GetGearUnit(i)->GetRetractable()) {
            return true;
        }
    }
    return false;
}

// JSBSim's ground, from a Terrain.
//
// The contact point is straight below the location asked about, at the
// terrain's height; the normal comes from the terrain's slope there, by central
// differences 15 m to each side - half a Copernicus DEM sample, so it is the
// slope of the cell the wheel is on. JSBSim works in feet.
class TerrainGround : public JSBSim::FGGroundCallback {
public:
    TerrainGround(std::shared_ptr<Terrain> terrain, double semimajor_ft,
                  double semiminor_ft)
        : terrain_(std::move(terrain)), a_(semimajor_ft), b_(semiminor_ft) {}

    double GetAGLevel(double, const JSBSim::FGLocation& location,
                      JSBSim::FGLocation& contact, JSBSim::FGColumnVector3& normal,
                      JSBSim::FGColumnVector3& velocity,
                      JSBSim::FGColumnVector3& angular_velocity) const override {
        constexpr double feet_per_metre = 1.0 / 0.3048;
        constexpr double degrees = 180.0 / 3.14159265358979323846;
        velocity.InitMatrix();
        angular_velocity.InitMatrix();
        JSBSim::FGLocation here = location;
        here.SetEllipse(a_, b_);
        const double latitude = here.GetGeodLatitudeRad();
        const double longitude = here.GetLongitude();
        const double lat_deg = latitude * degrees;
        const double lon_deg = longitude * degrees;
        const double height_m = terrain_->height_m(lat_deg, lon_deg);

        constexpr double half_span_m = 15.0;
        constexpr double earth_radius_m = 6378137.0;
        const double cos_lat = std::max(std::cos(latitude), 1e-6);
        const double d_lat = half_span_m / earth_radius_m * degrees;
        const double d_lon = half_span_m / (earth_radius_m * cos_lat) * degrees;
        const double north_slope = (terrain_->height_m(lat_deg + d_lat, lon_deg) -
                                    terrain_->height_m(lat_deg - d_lat, lon_deg)) /
                                   (2.0 * half_span_m);
        const double east_slope = (terrain_->height_m(lat_deg, lon_deg + d_lon) -
                                   terrain_->height_m(lat_deg, lon_deg - d_lon)) /
                                  (2.0 * half_span_m);

        const double sin_lat = std::sin(latitude);
        const double sin_lon = std::sin(longitude);
        const double cos_lon = std::cos(longitude);
        const JSBSim::FGColumnVector3 up(cos_lat * cos_lon, cos_lat * sin_lon, sin_lat);
        const JSBSim::FGColumnVector3 north(-sin_lat * cos_lon, -sin_lat * sin_lon,
                                            cos_lat);
        const JSBSim::FGColumnVector3 east(-sin_lon, cos_lon, 0.0);
        JSBSim::FGColumnVector3 n = up - north * north_slope - east * east_slope;
        normal = n / n.Magnitude();

        const double height_ft = height_m * feet_per_metre;
        contact.SetEllipse(a_, b_);
        contact.SetPositionGeodetic(longitude, latitude, height_ft);
        return here.GetGeodAltitude() - height_ft;
    }

    void SetEllipse(double semimajor_ft, double semiminor_ft) override {
        a_ = semimajor_ft;
        b_ = semiminor_ft;
    }

private:
    std::shared_ptr<Terrain> terrain_;
    double a_;
    double b_;
};

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

void Aircraft::set_terrain(std::shared_ptr<Terrain> terrain) {
    if (!terrain) {
        throw std::invalid_argument("no terrain");
    }
    const auto inertial = exec_->GetInertial();
    inertial->SetGroundCallback(
        new TerrainGround(terrain, inertial->GetSemimajor(), inertial->GetSemiminor()));
    terrain_ = std::move(terrain);
    contact_heights_.clear();
    const auto ground = exec_->GetGroundReactions();
    for (int i = 0; i < ground->GetNumGearUnits(); ++i) {
        contact_heights_.push_back((ground->GetGearUnit(i)->IsBogey() ? "gear/unit["
                                                                       : "contact/unit[") +
                                   std::to_string(i) + "]/AGL-ft");
    }
}

// Water, to JSBSim, is ground that is not solid: a BOGEY contact - a wheel -
// takes no weight on it, and a STRUCTURE contact still does. It has one ground
// for every contact, so the whole aircraft is on water or on land, as the
// terrain is below its centre.
void Aircraft::apply_ground() {
    const bool water =
        terrain_->water(exec_->GetPropertyValue("position/lat-geod-deg"),
                        exec_->GetPropertyValue("position/long-gc-deg"));
    exec_->GetGroundReactions()->SetSolid(!water);
}

// JSBSim gives each contact point's height above the surface, at its lowest
// nought, whether the point takes weight there or not.
bool Aircraft::meets_the_surface() const {
    for (const std::string& height : contact_heights_) {
        if (exec_->GetPropertyValue(height) <= 0.0) {
            return true;
        }
    }
    return false;
}

void Aircraft::initialize(const InitialConditions& ic) {
    const auto fgic = exec_->GetIC();
    // Geodetic, as every latitude here is. JSBSim's SetLatitudeDegIC is the
    // geocentric latitude: given 45 degrees it started the aircraft 0.19
    // degrees - 21 km - north of where it was asked to be.
    fgic->SetGeodLatitudeDegIC(ic.latitude_deg);
    fgic->SetLongitudeDegIC(ic.longitude_deg);
    fgic->SetTerrainElevationFtIC(ic.terrain_elevation_ft);
    fgic->SetAltitudeASLFtIC(ic.altitude_ft);
    fgic->SetPsiDegIC(ic.heading_deg);
    fgic->SetThetaDegIC(0.0);
    fgic->SetPhiDegIC(0.0);
    fgic->SetVcalibratedKtsIC(ic.airspeed_kts);
    if (retractable_gear(*exec_)) {
        exec_->SetPropertyValue("gear/gear-cmd-norm", ic.gear);
        exec_->SetPropertyValue("gear/gear-pos-norm", ic.gear);
    }
    exec_->SetHoldDown(false); // started again
    if (terrain_) {
        exec_->GetGroundReactions()->SetSolid(
            !terrain_->water(ic.latitude_deg, ic.longitude_deg));
    }
    if (!exec_->RunIC()) {
        throw std::runtime_error("JSBSim refused the initial conditions for " + model_);
    }
    // A start on the ground puts the wheels on it. The altitude is the centre
    // of gravity's, and set at the ground's own height it buries the wheels
    // as far as they hang below it - four feet for the Cessna 172P, which its
    // struts threw back into the air, nine for the A320, which they threw
    // hard enough to end its flight in NaNs. Raised by the deepest wheel's
    // compression, the aircraft starts with that wheel touching and settles
    // onto its struts.
    if (ic.airspeed_kts == 0.0) {
        double buried_ft = 0.0;
        const auto ground = exec_->GetGroundReactions();
        for (int i = 0; i < ground->GetNumGearUnits(); ++i) {
            const auto gear = ground->GetGearUnit(i);
            if (gear->IsBogey()) {
                buried_ft = std::max(buried_ft, gear->GetCompLen());
            }
        }
        if (buried_ft > 0.0) {
            fgic->SetAltitudeASLFtIC(ic.altitude_ft + buried_ft);
            if (!exec_->RunIC()) {
                throw std::runtime_error("JSBSim refused the initial conditions for " +
                                         model_ + " set down on the ground");
            }
        }
    }
    if (ic.engine_running) {
        exec_->SetPropertyValue("propulsion/set-running", -1.0);
        // JSBSim settles running engines by stepping them half a second at a
        // time, which a constant-speed propeller's governor cannot follow: it
        // overshoots, drives the blades to full coarse pitch and stalls the
        // engine. Those engines are started again here without settling, the
        // blades at the fine-pitch stop, to spin up in the first steps.
        const auto propulsion = exec_->GetPropulsion();
        for (unsigned i = 0; i < propulsion->GetNumEngines(); ++i) {
            const auto engine = propulsion->GetEngine(i);
            const auto* propeller =
                dynamic_cast<const JSBSim::FGPropeller*>(engine->GetThruster());
            if (propeller != nullptr && propeller->IsVPitch()) {
                engine->InitRunning();
                // The governor raises it to the fine stop on the first step.
                exec_->SetPropertyValue(
                    "propulsion/engine[" + std::to_string(i) + "]/blade-angle", 0.0);
            }
        }
    }
    initialized_ = true;
}

void Aircraft::set_controls(const Controls& c) {
    exec_->SetPropertyValue("fcs/elevator-cmd-norm", -c.elevator);
    exec_->SetPropertyValue("fcs/aileron-cmd-norm", c.aileron);
    exec_->SetPropertyValue("fcs/rudder-cmd-norm", c.rudder);
    const std::size_t engines = exec_->GetPropulsion()->GetNumEngines();
    for (std::size_t i = 0; i < engines; ++i) {
        const std::string n = "[" + std::to_string(i) + "]";
        const double offset = i < c.throttle_offset.size() ? c.throttle_offset[i] : 0.0;
        exec_->SetPropertyValue("fcs/throttle-cmd-norm" + n,
                                std::clamp(c.throttle + offset, 0.0, 1.0));
        exec_->SetPropertyValue("fcs/mixture-cmd-norm" + n, c.mixture);
        exec_->SetPropertyValue("fcs/advance-cmd-norm" + n, c.propeller);
        // Only a model that declares them has cooling flaps.
        const std::string cooling = "fcs/cooling-flaps-cmd-norm" + n;
        if (i < c.cooling_flaps.size() &&
            exec_->GetPropertyManager()->HasNode(cooling)) {
            exec_->SetPropertyValue(cooling, c.cooling_flaps[i]);
        }
    }
    exec_->SetPropertyValue("fcs/flap-cmd-norm", c.flaps);
    if (retractable_gear(*exec_)) {
        exec_->SetPropertyValue("gear/gear-cmd-norm", c.gear);
    }
    // Only a model that declares them has speedbrakes or ground spoilers.
    for (const char* spoilers : {"fcs/speedbrake-cmd-norm", "fcs/spoiler-cmd-norm"}) {
        if (exec_->GetPropertyManager()->HasNode(spoilers)) {
            exec_->SetPropertyValue(spoilers, c.speedbrake);
        }
    }
    // Only a model that declares the switch has one.
    if (exec_->GetPropertyManager()->HasNode("fcs/supercharger-cmd-norm")) {
        exec_->SetPropertyValue("fcs/supercharger-cmd-norm", c.supercharger);
    }
    exec_->SetPropertyValue("fcs/left-brake-cmd-norm", c.left_brake);
    exec_->SetPropertyValue("fcs/right-brake-cmd-norm", c.right_brake);
    exec_->SetPropertyValue("fcs/pitch-trim-cmd-norm", -c.pitch_trim);
}

void Aircraft::fail_engine(int engine, bool feather) {
    const std::string n = "[" + std::to_string(engine) + "]";
    const auto e = exec_->GetPropulsion()->GetEngine(static_cast<unsigned>(engine));
    // A stopped piston engine with spark and fuel starts again as soon as it
    // windmills fast enough, so its ignition goes off with it.
    if (const auto piston = std::dynamic_pointer_cast<JSBSim::FGPiston>(e)) {
        piston->SetMagnetos(0);
    }
    // A turbine with fuel relights as it spools down; its fuel is cut off.
    if (const auto turbine = std::dynamic_pointer_cast<JSBSim::FGTurbine>(e)) {
        turbine->SetCutoff(true);
    }
    e->SetRunning(false);
    if (feather) {
        exec_->SetPropertyValue("fcs/feather-cmd-norm" + n, 1.0);
    }
}

void Aircraft::freeze_fuel(bool frozen) {
    exec_->GetPropulsion()->SetFuelFreeze(frozen);
}

void Aircraft::set_weather(std::shared_ptr<Weather> weather) {
    weather_ = std::move(weather);
    // The same turbulence every flight, so a flight in it can be repeated.
    exec_->SetPropertyValue("atmosphere/randomseed", 1.0);
    applied_temperature_offset_c_ = std::nan("");
    applied_pressure_hpa_ = std::nan("");
    applied_turbulence_ = -1;
}

void Aircraft::apply_weather() {
    constexpr double feet_per_metre = 1.0 / 0.3048;
    constexpr double psf_per_hpa = 2.0885434233;
    const Conditions c = weather_->at(
        exec_->GetPropertyValue("position/lat-geod-deg"),
        exec_->GetPropertyValue("position/long-gc-deg"),
        exec_->GetPropertyValue("position/geod-alt-ft") * 0.3048, exec_->GetSimTime());
    exec_->SetPropertyValue("atmosphere/wind-north-fps",
                            c.wind_north_mps * feet_per_metre);
    exec_->SetPropertyValue("atmosphere/wind-east-fps",
                            c.wind_east_mps * feet_per_metre);
    exec_->SetPropertyValue("atmosphere/wind-down-fps",
                            c.wind_down_mps * feet_per_metre);
    if (c.temperature_offset_c != applied_temperature_offset_c_) {
        exec_->SetPropertyValue("atmosphere/delta-T", c.temperature_offset_c * 1.8);
        applied_temperature_offset_c_ = c.temperature_offset_c;
    }
    if (c.sea_level_pressure_hpa != applied_pressure_hpa_) {
        exec_->SetPropertyValue("atmosphere/P-sl-psf",
                                c.sea_level_pressure_hpa * psf_per_hpa);
        applied_pressure_hpa_ = c.sea_level_pressure_hpa;
    }
    if (c.turbulence_severity != applied_turbulence_) {
        // Type 3 is MIL-F-8785C; 0 is none.
        exec_->SetPropertyValue("atmosphere/turb-type",
                                c.turbulence_severity > 0 ? 3.0 : 0.0);
        exec_->SetPropertyValue("atmosphere/turbulence/milspec/severity",
                                static_cast<double>(c.turbulence_severity));
        applied_turbulence_ = c.turbulence_severity;
    }
    exec_->SetPropertyValue("atmosphere/turbulence/milspec/windspeed_at_20ft_AGL-fps",
                            c.wind_at_20ft_mps * feet_per_metre);
}

void Aircraft::step() {
    if (weather_) {
        apply_weather();
    }
    if (terrain_) {
        apply_ground();
    }
    exec_->Run();
    // Hold-down stops the aircraft - its velocities and rates to nothing - and
    // keeps it stopped, its attitude as it was.
    if (terrain_ && !exec_->GetGroundReactions()->GetSolid() && !exec_->GetHoldDown() &&
        meets_the_surface()) {
        exec_->SetHoldDown(true);
    }
}

AircraftState Aircraft::state() const {
    AircraftState s;
    s.sim_time_s = exec_->GetSimTime();
    s.latitude_deg = exec_->GetPropertyValue("position/lat-geod-deg");
    s.longitude_deg = exec_->GetPropertyValue("position/long-gc-deg");
    s.altitude_ft = exec_->GetPropertyValue("position/h-sl-ft");
    s.height_above_ground_ft = exec_->GetPropertyValue("position/h-agl-ft");
    s.terrain_elevation_ft =
        exec_->GetPropertyValue("position/terrain-elevation-asl-ft");
    s.on_water = !exec_->GetGroundReactions()->GetSolid();
    s.ditched = exec_->GetHoldDown();
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
        fgic->SetGeodLatitudeDegIC(s.latitude_deg);
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
