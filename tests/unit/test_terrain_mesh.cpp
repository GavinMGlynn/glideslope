#include "harness.hpp"

#include "world/geodesy.hpp"
#include "world/terrain_mesh.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::Ecef;
using glideslope::world::GeoRectangle;
using glideslope::world::GroundHeight;
using glideslope::world::make_terrain_mesh;
using glideslope::world::TerrainMesh;
using glideslope::world::to_ecef;

namespace {

// Ground that rises 2,000 m a degree northwards and 1,000 m a degree eastwards
// from 100 m at 39.5 S, 174 E, on a geoid 30 m above the ellipsoid.
GroundHeight slope(double latitude_deg, double longitude_deg) {
    return {100.0 + 2000.0 * (latitude_deg + 39.5) + 1000.0 * (longitude_deg - 174.0),
            30.0};
}

Ecef at(const TerrainMesh& mesh, std::size_t vertex) {
    const auto& p = mesh.positions.at(vertex);
    return {mesh.origin.x + static_cast<double>(p[0]),
            mesh.origin.y + static_cast<double>(p[1]),
            mesh.origin.z + static_cast<double>(p[2])};
}

Ecef minus(const Ecef& a, const Ecef& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double dot(const Ecef& a, const Ecef& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Ecef cross(const Ecef& a, const Ecef& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

double length(const Ecef& a) {
    return std::sqrt(dot(a, a));
}

} // namespace

GLIDESLOPE_TEST(
    a_terrain_mesh_puts_its_vertices_on_the_ground_and_meets_its_neighbour_exactly) {
    constexpr int cells = 8;
    constexpr std::size_t side = cells + 1;
    const GeoRectangle rectangle{-39.5, 174.0, -39.25, 174.25};
    const TerrainMesh mesh = make_terrain_mesh(rectangle, cells, 50.0, slope);

    check(mesh.positions.size() == side * side + 4 * side &&
              mesh.normals.size() == mesh.positions.size() &&
              mesh.heights_above_sea_level.size() == mesh.positions.size(),
          "a vertex for every grid point and every skirt point");
    check(mesh.indices.size() == 6 * cells * cells + 4 * 6 * cells,
          "two triangles a cell, and two a skirt segment");
    for (const auto i : mesh.indices) {
        check(i < mesh.positions.size(), "every index is a vertex");
    }

    // Every grid vertex is where the ground is, a row at a time from the south.
    const Ecef up = [&] {
        const Ecef o = mesh.origin;
        const double l = length(o);
        return Ecef{o.x / l, o.y / l, o.z / l};
    }();
    for (std::size_t j = 0; j < side; ++j) {
        for (std::size_t i = 0; i < side; ++i) {
            const double lat = -39.5 + 0.25 * static_cast<double>(j) / cells;
            const double lon = 174.0 + 0.25 * static_cast<double>(i) / cells;
            const GroundHeight g = slope(lat, lon);
            const std::size_t v = j * side + i;
            const Ecef expected = to_ecef({lat, lon, g.above_sea_level_m + g.geoid_m});
            check(length(minus(at(mesh, v), expected)) < 0.01,
                  "vertex " + std::to_string(v) + " is on the ground, within 1 cm");
            check(std::abs(static_cast<double>(mesh.heights_above_sea_level[v]) -
                           g.above_sea_level_m) < 0.01,
                  "vertex " + std::to_string(v) + " knows its height above sea level");
        }
    }

    // Each normal is perpendicular to the ground around it, within half a
    // degree: the ground between its neighbours east and west, and north and
    // south.
    for (std::size_t j = 1; j + 1 < side; ++j) {
        for (std::size_t i = 1; i + 1 < side; ++i) {
            const std::size_t v = j * side + i;
            const auto& n = mesh.normals[v];
            const Ecef normal{static_cast<double>(n[0]), static_cast<double>(n[1]),
                              static_cast<double>(n[2])};
            check(std::abs(length(normal) - 1.0) < 1e-5, "a unit normal");
            const Ecef across = minus(at(mesh, v + 1), at(mesh, v - 1));
            const Ecef along = minus(at(mesh, v + side), at(mesh, v - side));
            check(std::abs(dot(normal, across)) / length(across) < 0.009 &&
                      std::abs(dot(normal, along)) / length(along) < 0.009,
                  "vertex " + std::to_string(v) + "'s normal is the slope's");
            check(dot(normal, up) > 0.9, "and points up");
        }
    }

    // The grid's triangles face up.
    for (std::size_t t = 0; t < 6 * cells * cells; t += 3) {
        const Ecef a = at(mesh, mesh.indices[t]);
        const Ecef face = cross(minus(at(mesh, mesh.indices[t + 1]), a),
                                minus(at(mesh, mesh.indices[t + 2]), a));
        check(dot(face, up) > 0.0, "triangle " + std::to_string(t / 3) +
                                       " is counter-clockwise seen from above");
    }

    // The skirt hangs 50 m straight below each edge: south, east, north, west.
    for (std::size_t k = 0; k < 4 * side; ++k) {
        const std::size_t skirt = side * side + k;
        const std::size_t edge = k / side;
        const std::size_t step = k % side;
        const std::size_t top = edge == 0   ? step
                                : edge == 1 ? step * side + cells
                                : edge == 2 ? cells * side + (cells - step)
                                            : (cells - step) * side;
        const Ecef drop = minus(at(mesh, top), at(mesh, skirt));
        check(std::abs(length(drop) - 50.0) < 0.01 && dot(drop, up) > 49.0,
              "skirt vertex " + std::to_string(skirt) + " is 50 m under the edge");
    }
    // Its lowest point is the lowest corner's skirt, and its highest the
    // highest corner.
    check(std::abs(mesh.minimum_height_m - (100.0 + 30.0 - 50.0)) < 1e-6 &&
              std::abs(mesh.maximum_height_m - (100.0 + 500.0 + 250.0 + 30.0)) < 1e-6,
          "the heights it spans: " + std::to_string(mesh.minimum_height_m) + " to " +
              std::to_string(mesh.maximum_height_m));

    // The tile to the east shares this one's east edge, vertex for vertex.
    const TerrainMesh east =
        make_terrain_mesh({-39.5, 174.25, -39.25, 174.5}, cells, 50.0, slope);
    for (std::size_t j = 0; j < side; ++j) {
        check(length(minus(at(mesh, j * side + cells), at(east, j * side))) < 0.002,
              "row " + std::to_string(j) + " meets its neighbour within 2 mm");
    }
    // And a tile of the next level down meets it at every one of its vertices.
    const TerrainMesh finer =
        make_terrain_mesh({-39.5, 174.25, -39.375, 174.375}, cells, 50.0, slope);
    for (std::size_t j = 0; j < side; j += 2) {
        check(length(minus(at(mesh, (j / 2) * side + cells), at(finer, j * side))) <
                  0.002,
              "a finer tile meets it at row " + std::to_string(j));
    }
}

GLIDESLOPE_TEST(a_terrain_mesh_needs_a_rectangle_on_the_earth_and_a_cell) {
    const auto refused = [](const GeoRectangle& r, int cells, const char* what) {
        try {
            make_terrain_mesh(r, cells, 10.0, slope);
            fail(std::string("made a mesh for ") + what);
        } catch (const std::invalid_argument&) {
        }
    };
    refused({-39.5, 174.0, -39.25, 174.25}, 0, "no cells");
    refused({-39.25, 174.0, -39.5, 174.25}, 4, "south above north");
    refused({-39.5, 174.25, -39.25, 174.0}, 4, "west east of east");
    refused({-91.0, 0.0, -89.0, 1.0}, 4, "a rectangle past the pole");

    // At the pole itself, and past the antimeridian, it is still a mesh.
    const TerrainMesh pole =
        make_terrain_mesh({89.0, 179.0, 90.0, 181.0}, 4, 10.0,
                          [](double, double) { return GroundHeight{0.0, 0.0}; });
    for (const auto& n : pole.normals) {
        check(std::isfinite(n[0]) && std::isfinite(n[1]) && std::isfinite(n[2]),
              "normals at the pole are numbers");
    }
}
