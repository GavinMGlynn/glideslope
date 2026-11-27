#include "sim/lift_peak.hpp"

#include "sim/aircraft.hpp"
#include "sim/lander.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace glideslope::sim {

LiftPeak lift_peak(const std::filesystem::path& jsbsim_root, const std::string& model,
                   double airspeed_kts, double flaps, double speed_margin) {
    Aircraft aircraft(jsbsim_root, model);
    const auto [low_rad, high_rad] = aircraft.alpha_range_rad();
    const double degrees = 180.0 / std::numbers::pi;
    // Her tables' range, but not below level flight's.
    const double from = std::max(std::ceil(low_rad * degrees * 4.0) / 4.0, 0.0);
    const double to = std::min(high_rad * degrees, 60.0);
    std::vector<double> alphas;
    std::vector<double> cls;
    for (double alpha = from; alpha <= to; alpha += 0.25) {
        InitialConditions ic;
        ic.altitude_ft = 10000.0;
        ic.airspeed_kts = airspeed_kts;
        ic.engine_running = false;
        ic.gear = 0.0;
        ic.flaps = flaps;
        ic.alpha_deg = alpha;
        aircraft.initialize(ic);
        const double qs = aircraft.property("aero/qbar-psf") * aircraft.property("metrics/Sw-sqft");
        alphas.push_back(aircraft.property("aero/alpha-deg"));
        cls.push_back(aircraft.property("forces/fwz-aero-lbs") / std::max(qs, 1.0));
    }
    LiftPeak peak;
    if (cls.empty()) {
        return peak;
    }
    const auto most = static_cast<std::size_t>(std::max_element(cls.begin(), cls.end()) -
                                               cls.begin());
    peak.alpha_deg = alphas[most];
    peak.cl_max = cls[most];
    peak.peaked = most + 1 < cls.size();
    const double warn = peak.cl_max / (speed_margin * speed_margin);
    for (std::size_t i = 0; i <= most; ++i) {
        if (cls[i] >= warn) {
            // Between this angle and the one below, where the curve crosses.
            if (i > 0 && cls[i] > cls[i - 1]) {
                const double f = (warn - cls[i - 1]) / (cls[i] - cls[i - 1]);
                peak.warning_alpha_deg = alphas[i - 1] + f * (alphas[i] - alphas[i - 1]);
            } else {
                peak.warning_alpha_deg = alphas[i];
            }
            break;
        }
    }
    return peak;
}

double stall_warning_margin(const std::filesystem::path& data, const std::string& model) {
    const std::optional<ApproachSpeeds> speeds = landing_speeds(data, model);
    if (!speeds || !(speeds->stall_kts > 0.0)) {
        return 1.05;
    }
    return stall_warning_kts(speeds->stall_kts, speeds->warning_margin_kts) / speeds->stall_kts;
}

} // namespace glideslope::sim
