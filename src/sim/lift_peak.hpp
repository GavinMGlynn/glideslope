#pragma once

// **Where an aircraft's lift peaks, read from her own flight model**: her
// lift coefficient at every quarter degree of angle of attack her
// aerodynamics' tables hold (Aircraft::alpha_range_rad), each worked out by
// JSBSim with no time passing - started at that alpha, level, at 10,000 ft
// and her catalogue airspeed, her gear up - and the angle where it is
// greatest. With her flaps at `flaps`, 0 up to 1 fully down.
//
// **Her stall warning's angle** is taken from it at `speed_margin`, her
// warning speed over her stall speed: at the same weight and density a
// speed that much over the stall is a lift coefficient of CLmax over its
// square, so she is warned from the first angle on the rising side of her
// lift curve where the coefficient reaches that. stall_warning_margin says
// where the margin comes from.

#include <filesystem>
#include <string>

namespace glideslope::sim {

struct LiftPeak {
    double alpha_deg = 0.0;         // where the lift coefficient is greatest
    double cl_max = 0.0;
    double warning_alpha_deg = 0.0; // where it first reaches cl_max / margin^2
    // False where it is still rising at the end of her tables: no peak in
    // them, and `alpha_deg` is their last angle.
    bool peaked = false;
};

LiftPeak lift_peak(const std::filesystem::path& jsbsim_root, const std::string& model,
                   double airspeed_kts, double flaps, double speed_margin);

// **Her warning speed over her stall speed**, as a ratio: her landing stall
// speed and the margin her figures give over it (sim::stall_warning_kts -
// the certification rule's 5 knots or 5%, 14 CFR 23.2150 and 25.207, or
// her own where larger: the Mosquito's Pilot's Notes' 10 knots); 1.05, the
// rule's 5%, where her figures publish no stall speed. `data` is the data
// directory.
double stall_warning_margin(const std::filesystem::path& data, const std::string& model);

} // namespace glideslope::sim
