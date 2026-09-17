#pragma once

// The weather the simulation flies in.
//
// Like the terrain, the simulation does not know where weather comes from: the
// frontends connect it to reports (world/weather.hpp), and tests to whatever
// air they need. An aircraft asks its Weather for the conditions where it is
// before every step, and JSBSim's atmosphere is set from them.

namespace glideslope::sim {

struct Conditions {
    // The air's own velocity, metres a second: a wind from the west blows
    // towards the east, and has a positive east component.
    double wind_north_mps = 0.0;
    double wind_east_mps = 0.0;
    double wind_down_mps = 0.0;
    // How much warmer than the International Standard Atmosphere the air is,
    // the same at every height.
    double temperature_offset_c = 0.0;
    // The sea-level pressure the atmosphere is built on: a METAR's QNH.
    double sea_level_pressure_hpa = 1013.25;
    // JSBSim's MIL-F-8785C turbulence: severity 0 (none) to 7, and the wind 20
    // feet above the ground that scales it near the surface.
    int turbulence_severity = 0;
    double wind_at_20ft_mps = 0.0;
};

class Weather {
public:
    virtual ~Weather() = default;

    // The conditions at a geodetic latitude and longitude in degrees, a height
    // above the ellipsoid in metres, and simulation time in seconds.
    virtual Conditions at(double latitude_deg, double longitude_deg, double height_m,
                          double time_s) = 0;
};

// The same conditions everywhere, always.
class SteadyWeather : public Weather {
public:
    explicit SteadyWeather(const Conditions& conditions) : conditions_(conditions) {}
    Conditions at(double, double, double, double) override {
        return conditions_;
    }

private:
    Conditions conditions_;
};

} // namespace glideslope::sim
