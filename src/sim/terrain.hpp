#pragma once

// The ground the simulation stands on.
//
// JSBSim asks where the ground is for each wheel and for the aircraft itself,
// every step. By default that is a sphere-like surface at one elevation; given
// a Terrain, it is the Terrain's surface, with its slope - so an aircraft rests
// on a hillside tilted to it, and rolls where the ground runs downhill. The
// simulation does not know where heights come from: the frontends connect it
// to the DEM (world::Dem), and tests to whatever surface they need.
//
// Where the ground is water - the sea, a lake, a river - JSBSim is told its
// surface is not solid, before every step, from where the aircraft is: its
// wheels find nothing to roll on there. A landplane that comes down on water
// ditches (see sim::Aircraft).

#include <functional>

namespace glideslope::sim {

class Terrain {
public:
    virtual ~Terrain() = default;

    // The ground's height above the WGS84 ellipsoid, in metres, at a geodetic
    // latitude and longitude in degrees. Called many times a step, from the
    // thread stepping the aircraft.
    virtual double height_m(double latitude_deg, double longitude_deg) = 0;

    // Whether the ground there is water. Called before every step, from the
    // thread stepping the aircraft. Land everywhere unless overridden.
    virtual bool water(double latitude_deg, double longitude_deg) {
        (void)latitude_deg;
        (void)longitude_deg;
        return false;
    }
};

// A Terrain from functions: its height, and where it is water - nowhere, if
// that is not given.
class FunctionTerrain : public Terrain {
public:
    explicit FunctionTerrain(std::function<double(double, double)> height,
                             std::function<bool(double, double)> water = {})
        : height_(std::move(height)), water_(std::move(water)) {}

    double height_m(double latitude_deg, double longitude_deg) override {
        return height_(latitude_deg, longitude_deg);
    }

    bool water(double latitude_deg, double longitude_deg) override {
        return water_ && water_(latitude_deg, longitude_deg);
    }

private:
    std::function<double(double, double)> height_;
    std::function<bool(double, double)> water_;
};

} // namespace glideslope::sim
