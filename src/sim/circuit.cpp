#include "sim/circuit.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <cmath>

namespace glideslope::sim {

namespace {

constexpr double degrees = 180.0 / 3.14159265358979323846;
constexpr double metres_per_nm = 1852.0;


} // namespace

double along_runway_nm(const Runway& r, double latitude_deg, double longitude_deg) {
    const double north_m = (latitude_deg - r.threshold_lat_deg) *
                           metres_per_degree_latitude(r.threshold_lat_deg);
    const double east_m = (longitude_deg - r.threshold_lon_deg) *
                          metres_per_degree_longitude(r.threshold_lat_deg);
    const double h = r.heading_deg / degrees;
    return (north_m * std::cos(h) + east_m * std::sin(h)) / metres_per_nm;
}

double across_runway_m(const Runway& r, double latitude_deg, double longitude_deg) {
    const double north_m = (latitude_deg - r.threshold_lat_deg) *
                           metres_per_degree_latitude(r.threshold_lat_deg);
    const double east_m = (longitude_deg - r.threshold_lon_deg) *
                          metres_per_degree_longitude(r.threshold_lat_deg);
    const double h = r.heading_deg / degrees;
    return east_m * std::cos(h) - north_m * std::sin(h);
}

bool same_runway(const Runway& a, const Runway& b) {
    return a.name == b.name && a.threshold_lat_deg == b.threshold_lat_deg &&
           a.threshold_lon_deg == b.threshold_lon_deg;
}

std::optional<double> circuit_ahead_alt_ft(std::size_t self,
                                           const std::vector<std::optional<CirclePlace>>& all) {
    if (self >= all.size() || !all[self]) {
        return std::nullopt;
    }
    const CirclePlace& mine = *all[self];
    std::optional<double> highest;
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (i == self || !all[i] || !same_runway(all[i]->runway, mine.runway)) {
            continue;
        }
        const CirclePlace& other = *all[i];
        const bool ahead =
            other.since_s < mine.since_s ||
            (other.since_s == mine.since_s &&
             (other.along_nm > mine.along_nm || (other.along_nm == mine.along_nm && i < self)));
        if (ahead) {
            highest = std::max(highest.value_or(all[i]->circuit_alt_ft), all[i]->circuit_alt_ft);
        }
    }
    return highest;
}

GoAroundCircuit::GoAroundCircuit(const Aircraft& aircraft, const Runway& runway,
                                 const ApproachSpeeds& speeds, const CircuitEntry& entry)
    // Her speeds for what she weighs (`for_weight`); already so from a
    // lander, and scaled again they are the same.
    : a_(aircraft), runway_(runway),
      speeds_(for_weight(speeds, aircraft.property("inertia/weight-lbs"))), entry_(entry),
      leg_(entry.from_take_off ? Leg::crosswind : Leg::upwind) {
    // **A faster aeroplane flies a bigger circuit, and the two numbers that
    // make it are one number**: circuit height from a thousand feet for a
    // light aeroplane to fifteen hundred for a jet, and the downwind leg left
    // where a three-degree glidepath passes through it, a third of a mile
    // more to be a little above the path at the hand-over.
    circuit_ft_ = std::clamp(1000.0 + (speeds_.vref_kts - 60.0) * 8.0, 1000.0, 1500.0);
    // Her turn at the downwind speed and a twenty-five-degree bank.
    const double downwind_mps = (speeds_.vref_kts + 20.0) * 0.514444;
    turn_radius_m_ = downwind_mps * downwind_mps / (9.80665 * std::tan(25.0 / degrees));
    // **Circuit height over the ground it is flown over**, not only over the
    // runway: the highest ground of her terrain - the collision DEM, on a
    // server or a client - over the whole of the circuit, sampled every
    // 250 m from eight miles before the threshold to four past the runway's
    // end and from 500 m right of the centreline to two turns and two
    // kilometres left of it, and the circuit raised to be its height over
    // that where the ground rises above the runway. The downwind leg is then
    // left further out, where the glidepath meets the raised height.
    if (const auto& terrain = a_.terrain()) {
        constexpr double feet_per_metre = 3.280839895013123;
        constexpr double spacing_m = 250.0;
        const double h = runway_.heading_deg / degrees;
        const double lat_m = metres_per_degree_latitude(runway_.threshold_lat_deg);
        const double lon_m = metres_per_degree_longitude(runway_.threshold_lat_deg);
        double highest_ft = runway_.elevation_ft;
        for (double along = -8.0 * metres_per_nm;
             along <= std::max(runway_.length_m, 3000.0) + 4.0 * metres_per_nm;
             along += spacing_m) {
            for (double across = 500.0; across >= -(4.0 * turn_radius_m_ + 2000.0);
                 across -= spacing_m) {
                const double north = along * std::cos(h) - across * std::sin(h);
                const double east = along * std::sin(h) + across * std::cos(h);
                highest_ft = std::max(highest_ft,
                                      terrain->height_m(runway_.threshold_lat_deg + north / lat_m,
                                                        runway_.threshold_lon_deg + east / lon_m) *
                                          feet_per_metre);
            }
        }
        circuit_ft_ += highest_ft - runway_.elevation_ft;
    }
    leave_downwind_nm_ = circuit_ft_ / 318.0 + 0.33;
}

void GoAroundCircuit::follow(std::optional<double> ahead_alt_ft) {
    following_ = ahead_alt_ft.has_value();
    if (!ahead_alt_ft) {
        return;
    }
    // Raised, and kept raised: the downwind leg is left where the glidepath
    // meets the height she is at.
    const double above_ft = *ahead_alt_ft + above_ahead_ft - runway_.elevation_ft;
    if (above_ft > circuit_ft_) {
        circuit_ft_ = above_ft;
        leave_downwind_nm_ = circuit_ft_ / 318.0 + 0.33;
    }
}

double GoAroundCircuit::along_nm() const {
    const AircraftState s = a_.state();
    return along_runway_nm(runway_, s.latitude_deg, s.longitude_deg);
}

double GoAroundCircuit::base_kts() const {
    if (speeds_.stall_kts <= 0.0) {
        return speeds_.vref_kts * 1.08;
    }
    if (speeds_.vref_kts > 1.3 * speeds_.stall_kts) {
        return speeds_.vref_kts * 1.4 / 1.3;
    }
    return speeds_.stall_kts * 1.4;
}

double GoAroundCircuit::across_m() const {
    const AircraftState s = a_.state();
    return across_runway_m(runway_, s.latitude_deg, s.longitude_deg);
}

AutopilotModes GoAroundCircuit::modes() {
    const double agl_ft = a_.state().altitude_ft - runway_.elevation_ft;
    // Moved on a leg where she has reached the end of hers.
    // Following, she turns crosswind only at her raised height, which is
    // 600 ft or more over the circuit ahead: she crosses its legs above it.
    if (leg_ == Leg::upwind && agl_ft >= circuit_ft_ - (following_ ? 100.0 : 300.0) &&
        along_nm() > 0.0) {
        leg_ = Leg::crosswind;
    } else if (leg_ == Leg::crosswind && across_m() <= -(2.0 * turn_radius_m_ + 600.0) &&
               agl_ft >= circuit_ft_ - 100.0) {
        // Far enough out to turn base and final from, and at circuit height:
        // downwind is flown level.
        leg_ = Leg::downwind;
    } else if (leg_ == Leg::downwind && along_nm() <= -leave_downwind_nm_ && !following_) {
        leg_ = Leg::base;
    } else if (leg_ == Leg::downwind && following_ &&
               along_nm() <= -(leave_downwind_nm_ +
                               std::max(0.0, longest_extension_nm -
                                                 2.0 * turn_radius_m_ / metres_per_nm))) {
        // Begun two turns' radius short of it: she rolls into her turn away
        // and carries on round it about one and a half (measured), and the
        // racetrack's far end is to be inside the extension.
        // **Following traffic, she extends her downwind until it is down** -
        // but no further than `longest_extension_nm`: there she turns away
        // from the runway and flies back up a racetrack outside her own
        // circuit, still at her raised height, to join the downwind leg
        // again where she would have turned base.
        // The downwind line is taken once, the first time: taken again each
        // time round, every turn back on to it carried her a turn's radius
        // nearer the runway, and four racetracks walked an F-15C across it
        // to join final from its far side.
        if (!downwind_m_) {
            downwind_m_ = across_m();
        }
        leg_ = Leg::turn_out;
    } else if (leg_ == Leg::turn_out && across_m() <= *downwind_m_ - 2.0 * turn_radius_m_) {
        leg_ = Leg::outbound;
        ++racetracks_;
    } else if (leg_ == Leg::outbound && along_nm() >= -leave_downwind_nm_) {
        leg_ = Leg::turn_in;
    } else if (leg_ == Leg::turn_in && across_m() >= *downwind_m_ - turn_radius_m_) {
        // A turn's radius short of the line: her right turn on to the
        // downwind heading rolls her out on it.
        leg_ = Leg::downwind;
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
    // Or the take-off's, joined from one.
    m.vertical_speed_fpm =
        entry_.climb_fpm.value_or(std::clamp(speeds_.vref_kts * 10.0, 500.0, 1500.0));
    m.airspeed_kts = speeds_.vref_kts + 20.0;
    switch (leg_) {
    case Leg::upwind:
        m.heading_deg = runway_.heading_deg;
        if (following_) {
            // **Offset to the side away from the circuit**, as a pilot going
            // around for another aeroplane does: `following_offset_m` right
            // of the centreline, steered on to it at up to 45 degrees.
            const double off_m = following_offset_m - across_m();
            m.heading_deg = runway_.heading_deg +
                            std::clamp(std::atan2(off_m, 1000.0) * degrees, -45.0, 45.0);
        }
        m.airspeed_kts = entry_.climb_kts.value_or(*m.airspeed_kts);
        break;
    case Leg::crosswind:
        m.heading_deg = runway_.heading_deg - 90.0;
        m.airspeed_kts = entry_.climb_kts.value_or(*m.airspeed_kts);
        break;
    case Leg::downwind:
        m.heading_deg = runway_.heading_deg - 180.0;
        break;
    // The racetrack: right turns, away from the runway and back, so that
    // she never crosses her own final approach.
    case Leg::turn_out:
        m.heading_deg = runway_.heading_deg - 90.0;
        break;
    case Leg::outbound:
        m.heading_deg = runway_.heading_deg;
        break;
    case Leg::turn_in:
        m.heading_deg = runway_.heading_deg + 90.0;
        break;
    case Leg::base:
        m.heading_deg = runway_.heading_deg + 90.0;
        // Slowing, as a pilot does on base: 1.4 times the stall, or the same
        // margin over the approach speed where it is flown faster (base_kts).
        m.airspeed_kts = base_kts();
        break;
    case Leg::intercept:
    case Leg::final:
        m.heading_deg = runway_.heading_deg + 30.0;
        m.airspeed_kts = base_kts();
        break;
    }
    m.heading_deg = std::fmod(*m.heading_deg + 360.0, 360.0);
    return m;
}

void GoAroundCircuit::configure(Controls& controls) const {
    // The flap to its go-around setting, half the landing flap, round the
    // whole circuit; the approach puts the landing flap out again.
    // Joined from a take-off, the take-off flap stays out round the
    // pattern, as a jet's downwind leg is flown with it.
    controls.flaps = entry_.flaps.value_or(0.5 * speeds_.flap);
    controls.speedbrake = 0.0;
    if (gear_up_) {
        controls.gear = 0.0;
    }
}

} // namespace glideslope::sim
