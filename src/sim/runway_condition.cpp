#include "sim/runway_condition.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace glideslope::sim {

double wet_wheel_braking_coefficient(double groundspeed_kts) {
    // 25.109(c)(2): fully modulating anti-skid, 80 per cent.
    constexpr double anti_skid_efficiency = 0.80;
    // Held to 300 knots, past which no wheel rolls and the cubic is no
    // longer a fit to anything.
    const double v = std::clamp(groundspeed_kts, 0.0, 300.0) / 100.0;
    return anti_skid_efficiency * (-0.0437 * v * v * v + 0.320 * v * v - 0.805 * v + 0.804);
}

double wheel_braking_coefficient(int code, double groundspeed_kts) {
    // AC 25-32 table 2's note 4: the hydroplaning speed is 9 sqrt(P), P the
    // tyre's pressure - the 100 psi of the wet curve's tyre.
    constexpr double hydroplaning_kts = 9.0 * 10.0;
    switch (code) {
    case 5:
        return wet_wheel_braking_coefficient(groundspeed_kts);
    case 4:
        return 0.203;
    case 3:
        return 0.163;
    case 2:
        return groundspeed_kts < 0.85 * hydroplaning_kts
                   ? std::min(0.163, 0.5 * wet_wheel_braking_coefficient(groundspeed_kts))
                   : 0.053;
    case 1:
        return 0.083;
    default:
        throw std::invalid_argument(
            std::string("no wheel braking coefficient for runway condition code ") +
            std::to_string(code));
    }
}

} // namespace glideslope::sim
