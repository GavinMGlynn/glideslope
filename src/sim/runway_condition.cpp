#include "sim/runway_condition.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace glideslope::sim {

namespace {

// 14 CFR 25.109(c)(1)'s curves (Amdt. 25-92, 63 FR 8318, the image
// ER18FE98.004): for each tyre pressure, the cubic's coefficients of
// (V/100)^3, ^2, ^1 and ^0.
struct WetCurve {
    double psi;
    double c3, c2, c1, c0;
};
constexpr std::array<WetCurve, 4> wet_curves{{
    {50.0, -0.0350, 0.306, -0.851, 0.883},
    {100.0, -0.0437, 0.320, -0.805, 0.804},
    {200.0, -0.0331, 0.252, -0.658, 0.692},
    {300.0, -0.0401, 0.263, -0.611, 0.614},
}};

double on(const WetCurve& c, double v) {
    return ((c.c3 * v + c.c2) * v + c.c1) * v + c.c0;
}

} // namespace

double anti_skid_efficiency(AntiSkid anti_skid) {
    switch (anti_skid) {
    case AntiSkid::fully_modulating:
        return 0.80;
    case AntiSkid::quasi_modulating:
        return 0.50;
    case AntiSkid::on_off:
        return 0.30;
    case AntiSkid::none:
        return no_anti_skid_share;
    }
    return 0.0;
}

double wet_runway_maximum_coefficient(double groundspeed_kts, double tyre_pressure_psi) {
    // Held to 200 knots, past which no wheel rolls: the 50 psi cubic turns
    // up again from about 240.
    const double v = std::clamp(groundspeed_kts, 0.0, 200.0) / 100.0;
    const double psi =
        std::clamp(tyre_pressure_psi, wet_curves.front().psi, wet_curves.back().psi);
    std::size_t i = 0;
    while (i + 2 < wet_curves.size() && psi > wet_curves[i + 1].psi) {
        ++i;
    }
    const WetCurve& low = wet_curves[i];
    const WetCurve& high = wet_curves[i + 1];
    const double w = (psi - low.psi) / (high.psi - low.psi);
    return on(low, v) + w * (on(high, v) - on(low, v));
}

double wet_wheel_braking_coefficient(double groundspeed_kts, const WheelBrakes& brakes) {
    return anti_skid_efficiency(brakes.anti_skid) *
           wet_runway_maximum_coefficient(groundspeed_kts, brakes.tyre_pressure_psi);
}

double wheel_braking_coefficient(int code, double groundspeed_kts, const WheelBrakes& brakes) {
    // AC 25-32 table 2's note 3: its figures are a fully modulating
    // anti-skid's, 0.80 of the runway's; another gets its own share.
    const double modulation = anti_skid_efficiency(brakes.anti_skid) / 0.80;
    // Note 4: the hydroplaning speed is 9 sqrt(P), P the tyre's pressure.
    const double hydroplaning_kts = 9.0 * std::sqrt(brakes.tyre_pressure_psi);
    switch (code) {
    case 5:
        return wet_wheel_braking_coefficient(groundspeed_kts, brakes);
    case 4:
        return 0.203 * modulation;
    case 3:
        return 0.163 * modulation;
    case 2:
        return groundspeed_kts < 0.85 * hydroplaning_kts
                   ? std::min(0.163 * modulation,
                              0.5 * wet_wheel_braking_coefficient(groundspeed_kts, brakes))
                   : 0.053 * modulation;
    case 1:
        return 0.083 * modulation;
    default:
        throw std::invalid_argument(
            std::string("no wheel braking coefficient for runway condition code ") +
            std::to_string(code));
    }
}

} // namespace glideslope::sim
