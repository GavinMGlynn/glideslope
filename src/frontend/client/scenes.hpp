#pragma once

// Scenes built into the client, for looking at and for tests to shoot.
//
// Each is built around a point on the Earth, `at`, with its axes along ECEF's
// own, so the same scene can be built anywhere - at the Earth's centre, or on
// its surface millions of metres away - and should look the same in both.

#include "gfx/scene.hpp"
#include "gfx/renderer.hpp"
#include "world/geodesy.hpp"

#include <string_view>
#include <vector>

namespace glideslope::client {

struct Scene {
    std::vector<gfx::Mesh> meshes;
    std::vector<gfx::Draw> draws; // draws[i].mesh indexes meshes
    gfx::Camera camera;
};

// "sky"    - nothing: the sky's clear colour.
// "origin" - boxes of 7 cm to 2 m, 2 to 15 m from the camera, each with its own
//            origin, for the floating origin: shot at the Earth's centre and on
//            its surface, the frames are the same.
// "depth"  - for reversed depth. The top half of the frame is a distant mountain
//            face, 40 km away, with a second face 1 m in front of it; the bottom
//            half a nearby aircraft's skin, 1 m away, with a decal 1 mm in front
//            of it. The nearer surface of each pair is drawn first, so a depth
//            buffer that cannot tell them apart shows the one behind. Top half
//            (204, 204, 204), bottom half (51, 153, 51), when it works.
//
// Throws std::invalid_argument for any other name.
Scene make_scene(std::string_view name, const world::Ecef& at);

} // namespace glideslope::client
