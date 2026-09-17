#include "scenes.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace glideslope::client {

namespace {

using Colour = std::array<float, 4>;
using Point = std::array<float, 3>;

// A parallelogram from `corner` along `u` and `v`, appended to `mesh`.
void add_quad(gfx::Mesh& mesh, const Point& corner, const Point& u, const Point& v,
              const Colour& colour) {
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const auto& [a, b] : {std::pair{0.0f, 0.0f}, std::pair{1.0f, 0.0f},
                               std::pair{1.0f, 1.0f}, std::pair{0.0f, 1.0f}}) {
        mesh.vertices.push_back(
            {{corner[0] + a * u[0] + b * v[0], corner[1] + a * u[1] + b * v[1],
              corner[2] + a * u[2] + b * v[2]},
             colour});
    }
    for (const std::uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) {
        mesh.indices.push_back(first + i);
    }
}

// An axis-aligned cube of edge `size`, centred on its mesh's origin.
gfx::Mesh cube(float size, const Colour& colour) {
    const float h = size / 2.0f;
    gfx::Mesh mesh;
    add_quad(mesh, {-h, -h, -h}, {size, 0, 0}, {0, size, 0}, colour);
    add_quad(mesh, {-h, -h, h}, {size, 0, 0}, {0, size, 0}, colour);
    add_quad(mesh, {-h, -h, -h}, {size, 0, 0}, {0, 0, size}, colour);
    add_quad(mesh, {-h, h, -h}, {size, 0, 0}, {0, 0, size}, colour);
    add_quad(mesh, {-h, -h, -h}, {0, size, 0}, {0, 0, size}, colour);
    add_quad(mesh, {h, -h, -h}, {0, size, 0}, {0, 0, size}, colour);
    return mesh;
}

world::Ecef offset(const world::Ecef& at, double x, double y, double z) {
    return {at.x + x, at.y + y, at.z + z};
}

// Looking along +x, with +z up: right is -y, back is -x.
gfx::Camera camera_along_x(const world::Ecef& position) {
    gfx::Camera camera;
    camera.position = position;
    camera.world_from_camera = gfx::Mat3::columns({0, -1, 0}, {0, 0, 1}, {-1, 0, 0});
    return camera;
}

void place(Scene& scene, gfx::Mesh mesh, const world::Ecef& origin) {
    gfx::Draw draw;
    draw.mesh = scene.meshes.size();
    draw.placement.origin = origin;
    scene.meshes.push_back(std::move(mesh));
    scene.draws.push_back(draw);
}

Scene origin_scene(const world::Ecef& at) {
    Scene scene;
    // Offsets with no round numbers in them, so edges fall across pixels rather
    // than between them.
    scene.camera = camera_along_x(offset(at, 0.123, -0.456, 0.789));
    place(scene, cube(0.5f, {0.8f, 0.2f, 0.2f, 1.0f}), offset(at, 4.03, 0.91, 0.27));
    place(scene, cube(1.1f, {0.2f, 0.6f, 0.2f, 1.0f}), offset(at, 7.31, -1.73, 1.42));
    place(scene, cube(0.07f, {1.0f, 0.8f, 0.2f, 1.0f}), offset(at, 2.21, -0.33, 0.61));
    place(scene, cube(2.0f, {0.2f, 0.4f, 0.8f, 1.0f}), offset(at, 15.07, 2.53, -0.97));
    gfx::Mesh ground;
    add_quad(ground, {0.0f, -20.0f, 0.0f}, {40.0f, 0, 0}, {0, 40.0f, 0},
             {0.4f, 0.4f, 0.4f, 1.0f});
    place(scene, std::move(ground), offset(at, 1.0, 0.0, -1.5));
    return scene;
}

Scene depth_scene(const world::Ecef& at) {
    Scene scene;
    scene.camera = camera_along_x(at);
    scene.camera.near_m = 0.1;

    // The mountain: a face 40 km away filling the top half, and another 1 m in
    // front of it, drawn first.
    gfx::Mesh mountain;
    add_quad(mountain, {-1.0f, -60000.0f, 0.0f}, {0, 120000.0f, 0}, {0, 0, 60000.0f},
             {0.8f, 0.8f, 0.8f, 1.0f});
    add_quad(mountain, {0.0f, -60000.0f, 0.0f}, {0, 120000.0f, 0}, {0, 0, 60000.0f},
             {0.4f, 0.4f, 0.4f, 1.0f});
    place(scene, std::move(mountain), offset(at, 40000.0, 0.0, 0.0));

    // The aircraft: skin 1 m away filling the bottom half, and a decal 1 mm in
    // front of it, drawn first.
    gfx::Mesh aircraft;
    add_quad(aircraft, {-0.001f, -3.0f, -3.0f}, {0, 6.0f, 0}, {0, 0, 3.0f},
             {0.2f, 0.6f, 0.2f, 1.0f});
    add_quad(aircraft, {0.0f, -3.0f, -3.0f}, {0, 6.0f, 0}, {0, 0, 3.0f},
             {0.8f, 0.2f, 0.2f, 1.0f});
    place(scene, std::move(aircraft), offset(at, 1.0, 0.0, 0.0));
    return scene;
}

} // namespace

Scene make_scene(std::string_view name, const world::Ecef& at) {
    if (name == "sky") {
        Scene scene;
        scene.camera = camera_along_x(at);
        return scene;
    }
    if (name == "origin") {
        return origin_scene(at);
    }
    if (name == "depth") {
        return depth_scene(at);
    }
    throw std::invalid_argument("no scene named " + std::string(name) +
                                "; there are sky, origin and depth");
}

} // namespace glideslope::client
