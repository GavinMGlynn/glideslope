#include "world/terrain_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace glideslope::world {

namespace {

constexpr double radians = 3.14159265358979323846 / 180.0;

// The i-th of `cells` + 1 evenly spaced values from `from` to `to`, landing on
// both ends exactly.
double along(double from, double to, int i, int cells) {
    if (i == 0) {
        return from;
    }
    if (i == cells) {
        return to;
    }
    return from + (to - from) * i / cells;
}

} // namespace

TerrainMesh make_terrain_mesh(const GeoRectangle& rectangle, int cells, double skirt_m,
                              const HeightSource& heights) {
    if (cells < 1 || !(rectangle.south_deg < rectangle.north_deg) ||
        !(rectangle.west_deg < rectangle.east_deg) || rectangle.south_deg < -90.0 ||
        rectangle.north_deg > 90.0) {
        throw std::invalid_argument("a terrain mesh needs a rectangle on the Earth and "
                                    "at least one cell");
    }
    const auto side = static_cast<std::size_t>(cells) + 1;
    const double step_lat = (rectangle.north_deg - rectangle.south_deg) / cells;
    const double step_lon = (rectangle.east_deg - rectangle.west_deg) / cells;

    // Latitudes and longitudes of the grid, and of one cell past each edge.
    const auto latitude = [&](int j) {
        if (j < 0 || j > cells) {
            return std::clamp(rectangle.south_deg + j * step_lat, -90.0, 90.0);
        }
        return along(rectangle.south_deg, rectangle.north_deg, j, cells);
    };
    const auto longitude = [&](int i) {
        if (i < 0 || i > cells) {
            return rectangle.west_deg + i * step_lon;
        }
        return along(rectangle.west_deg, rectangle.east_deg, i, cells);
    };

    const std::size_t wide = side + 2;
    std::vector<GroundHeight> ground(wide * wide);
    const auto at = [&](int i, int j) -> GroundHeight& {
        return ground[static_cast<std::size_t>(j + 1) * wide +
                      static_cast<std::size_t>(i + 1)];
    };
    for (int j = -1; j <= cells + 1; ++j) {
        for (int i = -1; i <= cells + 1; ++i) {
            at(i, j) = heights(latitude(j), longitude(i));
        }
    }
    const auto ellipsoid_height = [&](int i, int j) {
        const GroundHeight& g = at(i, j);
        return g.above_sea_level_m + g.geoid_m;
    };

    TerrainMesh mesh;
    mesh.origin = to_ecef({(rectangle.south_deg + rectangle.north_deg) / 2,
                           (rectangle.west_deg + rectangle.east_deg) / 2, 0.0});
    mesh.minimum_height_m = std::numeric_limits<double>::infinity();
    mesh.maximum_height_m = -std::numeric_limits<double>::infinity();
    const auto add_vertex = [&](double lat, double lon, double height,
                                const std::array<float, 3>& normal, double sea_level) {
        const Ecef p = to_ecef({lat, lon, height});
        mesh.positions.push_back({static_cast<float>(p.x - mesh.origin.x),
                                  static_cast<float>(p.y - mesh.origin.y),
                                  static_cast<float>(p.z - mesh.origin.z)});
        mesh.normals.push_back(normal);
        mesh.heights_above_sea_level.push_back(static_cast<float>(sea_level));
        mesh.minimum_height_m = std::min(mesh.minimum_height_m, height);
        mesh.maximum_height_m = std::max(mesh.maximum_height_m, height);
    };

    // The grid. Slopes are central differences over a cell each way, with the
    // cell's size on the ellipsoid's equatorial sphere: shading, not surveying.
    const double dy = Wgs84::a * step_lat * radians;
    for (int j = 0; j <= cells; ++j) {
        const double lat = latitude(j);
        const double phi = lat * radians;
        const double dx = Wgs84::a * std::cos(phi) * step_lon * radians;
        for (int i = 0; i <= cells; ++i) {
            const double lon = longitude(i);
            const double lambda = lon * radians;
            const double east_slope =
                dx > 1e-3 ? (ellipsoid_height(i + 1, j) - ellipsoid_height(i - 1, j)) /
                                (2.0 * dx)
                          : 0.0;
            const double north_slope =
                (ellipsoid_height(i, j + 1) - ellipsoid_height(i, j - 1)) / (2.0 * dy);
            // Up the slope's normal in east, north and up, then into ECEF.
            const double length =
                std::sqrt(east_slope * east_slope + north_slope * north_slope + 1.0);
            const double ne = -east_slope / length;
            const double nn = -north_slope / length;
            const double nu = 1.0 / length;
            const double sp = std::sin(phi);
            const double cp = std::cos(phi);
            const double sl = std::sin(lambda);
            const double cl = std::cos(lambda);
            const std::array<float, 3> normal{
                static_cast<float>(-sl * ne - sp * cl * nn + cp * cl * nu),
                static_cast<float>(cl * ne - sp * sl * nn + cp * sl * nu),
                static_cast<float>(cp * nn + sp * nu)};
            add_vertex(lat, lon, ellipsoid_height(i, j), normal,
                       at(i, j).above_sea_level_m);
        }
    }
    const auto grid_index = [&](int i, int j) {
        return static_cast<std::uint32_t>(static_cast<std::size_t>(j) * side +
                                          static_cast<std::size_t>(i));
    };
    for (int j = 0; j < cells; ++j) {
        for (int i = 0; i < cells; ++i) {
            const std::uint32_t sw = grid_index(i, j);
            const std::uint32_t se = grid_index(i + 1, j);
            const std::uint32_t nw = grid_index(i, j + 1);
            const std::uint32_t ne = grid_index(i + 1, j + 1);
            mesh.indices.insert(mesh.indices.end(), {sw, se, ne, sw, ne, nw});
        }
    }

    // The skirt: each edge again, `skirt_m` lower, joined to the edge above it.
    const auto add_skirt = [&](int i0, int j0, int di, int dj) {
        const auto first = static_cast<std::uint32_t>(mesh.positions.size());
        for (int k = 0; k <= cells; ++k) {
            const int i = i0 + k * di;
            const int j = j0 + k * dj;
            const std::uint32_t top = grid_index(i, j);
            add_vertex(latitude(j), longitude(i), ellipsoid_height(i, j) - skirt_m,
                       mesh.normals[top], at(i, j).above_sea_level_m);
        }
        for (int k = 0; k < cells; ++k) {
            const std::uint32_t a = grid_index(i0 + k * di, j0 + k * dj);
            const std::uint32_t b = grid_index(i0 + (k + 1) * di, j0 + (k + 1) * dj);
            const std::uint32_t a_low = first + static_cast<std::uint32_t>(k);
            const std::uint32_t b_low = a_low + 1;
            mesh.indices.insert(mesh.indices.end(), {a, a_low, b_low, a, b_low, b});
        }
    };
    add_skirt(0, 0, 1, 0);          // south, west to east
    add_skirt(cells, 0, 0, 1);      // east, south to north
    add_skirt(cells, cells, -1, 0); // north, east to west
    add_skirt(0, cells, 0, -1);     // west, north to south
    return mesh;
}

} // namespace glideslope::world
