#pragma once

// What the renderer draws: meshes placed on the Earth, seen from a camera.
//
// **Positions are double-precision ECEF until the last moment.** A mesh's
// vertices are single-precision floats relative to its own origin, which is a
// double ECEF position; the camera's position is too. What reaches the GPU is
// each mesh's transform relative to the camera, worked out in double precision
// and only then narrowed to float - the camera-relative floating origin. Floats
// of whole ECEF coordinates would be millions of metres, and a float at six
// million metres cannot say anything finer than half a metre, so geometry would
// shake as the camera moved. Relative to the camera, the floats that reach the
// GPU are small where precision matters: near the eye.

#include "world/geodesy.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace glideslope::gfx {

// A rotation or other linear map in double precision, column-major: m[c * 3 + r].
struct Mat3 {
    std::array<double, 9> m{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

    // The matrix whose columns are x, y and z.
    static Mat3 columns(const world::Ecef& x, const world::Ecef& y,
                        const world::Ecef& z);
};

Mat3 operator*(const Mat3& a, const Mat3& b);
world::Ecef operator*(const Mat3& a, const world::Ecef& v);
Mat3 transpose(const Mat3& a);

// A single-precision 4x4 matrix, column-major, as a shader's mat4 is.
struct Mat4f {
    std::array<float, 16> m{};
};

Mat4f operator*(const Mat4f& a, const Mat4f& b);

struct Vertex {
    std::array<float, 3> position{}; // metres, relative to the mesh's origin
    std::array<float, 4> colour{};   // linear RGBA, 0..1
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices; // triangles, counter-clockwise from outside
};

// Where a mesh is: its origin in ECEF, and its axes in ECEF.
struct Placement {
    world::Ecef origin;
    Mat3 world_from_local;
};

struct Camera {
    world::Ecef position;
    // The camera's axes in ECEF: columns right, up and back. It looks along the
    // negative of the third.
    Mat3 world_from_camera;
    double vertical_fov_rad = 1.0471975511965976; // 60 degrees
    double near_m = 0.1;
};

// The projection: perspective with an infinitely distant far plane and reversed
// depth - the near plane at depth 1, infinity at depth 0. With a float depth
// buffer this spends its precision evenly across distance, rather than almost
// all of it within the first few metres, which is what lets a mountain range
// tens of kilometres away and an aircraft a metre away share one depth buffer.
// The depth buffer is cleared to 0 and a fragment is kept when its depth is
// greater.
Mat4f projection(double vertical_fov_rad, double aspect, double near_m);

// A mesh's vertices to the camera's space - the camera-relative floating
// origin. Worked out in double precision; narrowed to float at the end.
Mat4f camera_from_local(const Camera& camera, const Placement& placement);

} // namespace glideslope::gfx
