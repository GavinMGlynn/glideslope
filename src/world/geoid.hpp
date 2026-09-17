#pragma once

// The geoid: how far mean sea level lies above the WGS84 ellipsoid.
//
// The Copernicus DEM gives heights above the EGM2008 geoid; positions here are
// heights above the ellipsoid. The difference - the geoid undulation - runs
// from about -107 m south of India to about +85 m over New Guinea, so a DEM
// height used as an ellipsoidal one puts the ground in the wrong place by that
// much. Height above the ellipsoid = DEM height + undulation.
//
// The grid is GeographicLib's EGM2008 5-minute grid, in its PGM form,
// interpolated bilinearly, which GeographicLib states is within 0.478 m of the
// full model everywhere (RMS 0.012 m).

#include "world/byte_source.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace glideslope::world {

struct GeoidError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Geoid {
public:
    // From a GeographicLib geoid grid in PGM form. Throws GeoidError if it is
    // not one.
    explicit Geoid(std::span<const std::uint8_t> pgm);

    // The undulation, in metres, at a latitude and longitude in degrees.
    // Longitude wraps; latitude is clamped to the poles.
    double undulation(double latitude_deg, double longitude_deg) const;

    // The grid's own description, e.g. "WGS84 EGM2008, 5-minute grid".
    const std::string& description() const {
        return description_;
    }

private:
    double sample(std::size_t column, std::size_t row) const;

    std::size_t width_ = 0;
    std::size_t height_ = 0;
    double offset_ = 0.0;
    double scale_ = 0.0;
    std::string description_;
    std::vector<std::uint16_t> grid_;
};

// The geoid in GeographicLib's zip distribution of one grid, such as
// egm2008-5.zip, which holds it as geoids/<name>.pgm.
Geoid load_geoid_zip(const ByteSource& archive);

} // namespace glideslope::world
