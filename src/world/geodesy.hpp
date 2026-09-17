#pragma once

// Positions on the Earth.
//
// **The world is Earth-centred and Earth-fixed (ECEF), in double-precision
// metres.** Latitude, longitude and height are for people and for data; ECEF is
// what positions are kept in, because it has no singularity at the poles, no
// seam at the date line, and subtracts cleanly into the camera-relative floats
// the renderer draws with. The ellipsoid is WGS84, the one GPS, the Copernicus
// DEM and every web map use.

namespace glideslope::world {

// WGS84's defining constants, and the ones derived from them.
struct Wgs84 {
    static constexpr double a = 6378137.0;           // semi-major axis, m
    static constexpr double f = 1.0 / 298.257223563; // flattening
    static constexpr double b = a * (1.0 - f);       // semi-minor axis, m
    static constexpr double e2 = f * (2.0 - f);      // first eccentricity squared
    static constexpr double ep2 = e2 / (1.0 - e2);   // second eccentricity squared
};

struct Ecef {
    double x = 0.0; // metres, towards latitude 0, longitude 0
    double y = 0.0; // metres, towards latitude 0, longitude 90 E
    double z = 0.0; // metres, towards the North Pole
};

struct Geodetic {
    double latitude_deg = 0.0;  // -90 .. 90, north positive
    double longitude_deg = 0.0; // -180 .. 180, east positive
    double height_m = 0.0;      // above the WGS84 ellipsoid
};

// Exact.
Ecef to_ecef(const Geodetic& g);

// Vermeille's closed form (2004): no iteration, and accurate to well under a
// millimetre from the Earth's centre out past geostationary orbit. At the poles
// the longitude is 0.
Geodetic to_geodetic(const Ecef& e);

// Distance between two points, straight through, in metres.
double distance(const Ecef& p, const Ecef& q);

} // namespace glideslope::world
