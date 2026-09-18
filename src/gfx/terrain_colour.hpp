#pragma once

// The colour terrain is drawn in, until imagery covers it.
//
// **Height and light.** Ground is tinted by its height above sea level - green
// lowland, brown hills, grey rock, snow - and lit by a sun fixed in the north-west
// sky, 45 degrees up, as a relief map's hillshade is. The sea is flat blue. The
// same functions colour the terrain's vertices and the reference frames its
// drawing is checked against; with imagery, the light alone is kept.

#include "world/geodesy.hpp"

#include <array>

namespace glideslope::gfx {

// Linear RGBA for ground `height_above_sea_level_m` high whose surface faces
// `normal` - a unit vector in ECEF - at the place whose upward direction is
// `up`, also a unit ECEF vector.
std::array<float, 4> terrain_colour(double height_above_sea_level_m,
                                    const world::Ecef& normal, const world::Ecef& up);

// How lit ground facing `normal` is, where up is `up`: 1 for level ground,
// more on a slope towards the sun, down to 0.35 in shadow. Imagery is drawn
// times this; the tints above already are.
double terrain_light(const world::Ecef& normal, const world::Ecef& up);

// The unit vector straight up from the ellipsoid at a latitude and longitude.
world::Ecef up_at(double latitude_deg, double longitude_deg);

} // namespace glideslope::gfx
