#pragma once

// The ground as triangles: a grid of heights over a rectangle of latitude and
// longitude, placed on the Earth, for drawing.
//
// **What the terrain tiles are made of.** The visual terrain is a quadtree of
// rectangles, each drawn as the same grid of cells at its own size, so a tile
// far away is coarse and one underfoot is as fine as the DEM. This is one
// tile's grid, with nothing about drawing in it: positions, normals and
// heights, and the triangles between them.
//
// **Neighbours meet exactly.** A vertex on an edge is at the same latitude and
// longitude, and so the same height, as the vertex its neighbour puts there;
// tiles at different levels meet at every vertex of the coarser one, and
// between them a skirt - the edge repeated some way below, and joined to it -
// covers the gap. Its normals are from the heights one cell to each side, taken
// from the ground past the edge where the cell is on it, so shading is
// continuous across edges too.

#include "world/geodesy.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace glideslope::world {

// A rectangle of latitude and longitude, in degrees; west is less than east.
struct GeoRectangle {
    double south_deg = 0.0;
    double west_deg = 0.0;
    double north_deg = 0.0;
    double east_deg = 0.0;
};

// The ground's height at a place: above the geoid - sea level - and the geoid's
// own height above the ellipsoid, both in metres.
struct GroundHeight {
    double above_sea_level_m = 0.0;
    double geoid_m = 0.0;
};

using HeightSource =
    std::function<GroundHeight(double latitude_deg, double longitude_deg)>;

struct TerrainMesh {
    // Where the positions are measured from: the rectangle's centre, on the
    // ellipsoid.
    Ecef origin;
    // Each vertex: its ECEF position less the origin, its unit normal in ECEF,
    // and its ground's height above sea level. The grid's (cells + 1) squared
    // vertices come first, a row at a time from the south, each row from the
    // west; then the skirt's.
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> normals;
    std::vector<float> heights_above_sea_level;
    // Triangles, counter-clockwise seen from above.
    std::vector<std::uint32_t> indices;
    // Above the ellipsoid, of every vertex, skirt included.
    double minimum_height_m = 0.0;
    double maximum_height_m = 0.0;
};

// The grid of `cells` by `cells` over `rectangle`, with a skirt `skirt_m` deep.
// Heights come from `heights`, at every vertex and one cell beyond each edge.
TerrainMesh make_terrain_mesh(const GeoRectangle& rectangle, int cells, double skirt_m,
                              const HeightSource& heights);

} // namespace glideslope::world
