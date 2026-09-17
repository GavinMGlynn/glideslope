#pragma once

// The ground the simulation stands on.
//
// JSBSim asks where the ground is for each wheel and for the aircraft itself,
// every step. By default that is a sphere-like surface at one elevation; given
// a Terrain, it is the Terrain's surface, with its slope - so an aircraft rests
// on a hillside tilted to it, and rolls where the ground runs downhill. The
// simulation does not know where heights come from: the frontends connect it
// to the DEM (world::Dem), and tests to whatever surface they need.

#include <functional>

namespace glideslope::sim {

class Terrain {
public:
    virtual ~Terrain() = default;

    // The ground's height above the WGS84 ellipsoid, in metres, at a geodetic
    // latitude and longitude in degrees. Called many times a step, from the
    // thread stepping the aircraft.
    virtual double height_m(double latitude_deg, double longitude_deg) = 0;
};

// A Terrain from a function.
class FunctionTerrain : public Terrain {
public:
    explicit FunctionTerrain(std::function<double(double, double)> height)
        : height_(std::move(height)) {}

    double height_m(double latitude_deg, double longitude_deg) override {
        return height_(latitude_deg, longitude_deg);
    }

private:
    std::function<double(double, double)> height_;
};

} // namespace glideslope::sim
