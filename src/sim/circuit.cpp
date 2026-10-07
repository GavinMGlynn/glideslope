#include "sim/circuit.hpp"

#include <algorithm>
#include <cmath>

namespace glideslope::sim {

namespace {

constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double metres_per_nm = 1852.0;

// As sim::Lander measures a runway, on the WGS84 ellipsoid near its threshold.
double metres_per_degree_latitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111132.92 - 559.82 * std::cos(2.0 * lat) + 1.175 * std::cos(4.0 * lat) -
           0.0023 * std::cos(6.0 * lat);
}

double metres_per_degree_longitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111412.84 * std::cos(lat) - 93.5 * std::cos(3.0 * lat) +
           0.118 * std::cos(5.0 * lat);
}

} // namespace

GoAroundCircuit::GoAroundCircuit(const Aircraft& aircraft, const Runway& runway,
                                 const ApproachSpeeds& speeds)
    : a_(aircraft), runway_(runway), speeds_(speeds) {
    // **A faster aeroplane flies a bigger circuit, and the two numbers that
    // make it are one number**: circuit height from a thousand feet for a
    // light aeroplane to fifteen hundred for a jet, and the downwind leg left
    // where a three-degree glidepath passes through it, a third of a mile
    // more to be a little above the path at the hand-over.
    circuit_ft_ = std::clamp(1000.0 + (speeds_.vref_kts - 60.0) * 8.0, 1000.0, 1500.0);
    leave_downwind_nm_ = circuit_ft_ / 318.0 + 0.33;
    // Her turn at the downwind speed and a twenty-five-degree bank.
    const double downwind_mps = (speeds_.vref_kts + 20.0) * 0.514444;
    turn_radius_m_ = downwind_mps * downwind_mps / (9.80665 * std::tan(25.0 / degrees));
}

double GoAroundCircuit::along_nm() const {
    const AircraftState s = a_.state();
    const double north_m = (s.latitude_deg - runway_.threshold_lat_deg) *
                           metres_per_degree_latitude(runway_.threshold_lat_deg);
    const double east_m = (s.longitude_deg - runway_.threshold_lon_deg) *
                          metres_per_degree_longitude(runway_.threshold_lat_deg);
    const double h = runway_.heading_deg / degrees;
    return (north_m * std::cos(h) + east_m * std::sin(h)) / metres_per_nm;
}

double GoAroundCircuit::across_m() const {
    const AircraftState s = a_.state();
    const double north_m = (s.latitude_deg - runway_.threshold_lat_deg) *
                           metres_per_degree_latitude(runway_.threshold_lat_deg);
    const double east_m = (s.longitude_deg - runway_.threshold_lon_deg) *
                          metres_per_degree_longitude(runway_.threshold_lat_deg);
    const double h = runway_.heading_deg / degrees;
    return east_m * std::cos(h) - north_m * std::sin(h);
}

AutopilotModes GoAroundCircuit::modes() {
    const double agl_ft = a_.state().altitude_ft - runway_.elevation_ft;
    // Moved on a leg where she has reached the end of hers.
    if (leg_ == Leg::upwind && agl_ft >= circuit_ft_ - 300.0 && along_nm() > 0.0) {
        leg_ = Leg::crosswind;
    } else if (leg_ == Leg::crosswind && across_m() <= -(2.0 * turn_radius_m_ + 600.0)) {
        leg_ = Leg::downwind;
    } else if (leg_ == Leg::downwind && along_nm() <= -leave_downwind_nm_) {
        leg_ = Leg::base;
    } else if (leg_ == Leg::base && across_m() >= -(2.0 * turn_radius_m_ + 200.0)) {
        leg_ = Leg::intercept;
    } else if (leg_ == Leg::intercept && across_m() >= -turn_radius_m_) {
        leg_ = Leg::final;
    }
    // **The gear up once she is climbing**, where it retracts: a positive
    // rate of climb, as the handbook has it.
    if (!gear_up_ && a_.gear_retracts() && a_.state().climb_rate_fpm > 100.0) {
        gear_up_ = true;
    }
    AutopilotModes m;
    m.altitude_ft = runway_.elevation_ft + circuit_ft_;
    // A climb she can make: about ten feet a minute a knot of her reference
    // speed - 500 for a Cub, 1,500 for a jet.
    m.vertical_speed_fpm = std::clamp(speeds_.vref_kts * 10.0, 500.0, 1500.0);
    m.airspeed_kts = speeds_.vref_kts + 20.0;
    switch (leg_) {
    case Leg::upwind:
        m.heading_deg = runway_.heading_deg;
        break;
    case Leg::crosswind:
        m.heading_deg = runway_.heading_deg - 90.0;
        break;
    case Leg::downwind:
        m.heading_deg = runway_.heading_deg - 180.0;
        break;
    case Leg::base:
        m.heading_deg = runway_.heading_deg + 90.0;
        // Slowing, as a pilot does on base: 1.4 times the landing stall.
        m.airspeed_kts =
            speeds_.stall_kts > 0.0 ? speeds_.stall_kts * 1.4 : speeds_.vref_kts * 1.08;
        break;
    case Leg::intercept:
    case Leg::final:
        m.heading_deg = runway_.heading_deg + 30.0;
        m.airspeed_kts =
            speeds_.stall_kts > 0.0 ? speeds_.stall_kts * 1.4 : speeds_.vref_kts * 1.08;
        break;
    }
    m.heading_deg = std::fmod(*m.heading_deg + 360.0, 360.0);
    return m;
}

void GoAroundCircuit::configure(Controls& controls) const {
    // The flap to its go-around setting, half the landing flap, round the
    // whole circuit; the approach puts the landing flap out again.
    controls.flaps = 0.5 * speeds_.flap;
    controls.speedbrake = 0.0;
    if (gear_up_) {
        controls.gear = 0.0;
    }
}

} // namespace glideslope::sim
