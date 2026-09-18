#pragma once

// The colour terrain is drawn in, until imagery covers it.
//
// **Height and light.** Ground is tinted by its height above sea level - green
// lowland, brown hills, grey rock, snow - and lit by a sun fixed in the north-west
// sky, 45 degrees up, as a relief map's hillshade is. The sea is flat blue. The
// same function colours the terrain's vertices and the reference frames its
// drawing is checked against.

#include "world/geodesy.hpp"

#include <array>

namespace glideslope::gfx {

// Linear RGBA for ground `height_above_sea_level_m` high whose surface faces
// `normal` - a unit vector in ECEF - at the place whose upward direction is
// `up`, also a unit ECEF vector.
std::array<float, 4> terrain_colour(double height_above_sea_level_m,
                                    const world::Ecef& normal, const world::Ecef& up);

// The unit vector straight up from the ellipsoid at a latitude and longitude.
world::Ecef up_at(double latitude_deg, double longitude_deg);

} // namespace glideslope::gfx
