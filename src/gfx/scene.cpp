#include "gfx/scene.hpp"

#include <cmath>

namespace glideslope::gfx {

Mat3 Mat3::columns(const world::Ecef& x, const world::Ecef& y, const world::Ecef& z) {
    return {{x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z}};
}

Mat3 operator*(const Mat3& a, const Mat3& b) {
    Mat3 out;
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            double sum = 0.0;
            for (int k = 0; k < 3; ++k) {
                sum += a.m[static_cast<std::size_t>(k * 3 + r)] *
                       b.m[static_cast<std::size_t>(c * 3 + k)];
            }
            out.m[static_cast<std::size_t>(c * 3 + r)] = sum;
        }
    }
    return out;
}

world::Ecef operator*(const Mat3& a, const world::Ecef& v) {
    const auto& m = a.m;
    return {m[0] * v.x + m[3] * v.y + m[6] * v.z, m[1] * v.x + m[4] * v.y + m[7] * v.z,
            m[2] * v.x + m[5] * v.y + m[8] * v.z};
}

Mat3 transpose(const Mat3& a) {
    const auto& m = a.m;
    return {{m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]}};
}

Mat4f operator*(const Mat4f& a, const Mat4f& b) {
    Mat4f out;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[static_cast<std::size_t>(k * 4 + r)] *
                       b.m[static_cast<std::size_t>(c * 4 + k)];
            }
            out.m[static_cast<std::size_t>(c * 4 + r)] = sum;
        }
    }
    return out;
}

Mat4f projection(double vertical_fov_rad, double aspect, double near_m) {
    const double f = 1.0 / std::tan(vertical_fov_rad / 2.0);
    Mat4f p;
    p.m[0] = static_cast<float>(f / aspect); // x
    p.m[5] = static_cast<float>(f);          // y
    p.m[11] = -1.0f;                         // w = -z
    p.m[14] = static_cast<float>(near_m);    // z = near, so depth = near / -z
    return p;
}

Camera look_at(const world::Ecef& eye, const world::Ecef& target) {
    const auto normalized = [](const world::Ecef& v) {
        const double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return world::Ecef{v.x / length, v.y / length, v.z / length};
    };
    const auto cross = [](const world::Ecef& a, const world::Ecef& b) {
        return world::Ecef{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                           a.x * b.y - a.y * b.x};
    };
    constexpr double radians = 3.14159265358979323846 / 180.0;
    const world::Geodetic g = world::to_geodetic(eye);
    const double phi = g.latitude_deg * radians;
    const double lambda = g.longitude_deg * radians;
    const world::Ecef up{std::cos(phi) * std::cos(lambda),
                         std::cos(phi) * std::sin(lambda), std::sin(phi)};
    const world::Ecef north{-std::sin(phi) * std::cos(lambda),
                            -std::sin(phi) * std::sin(lambda), std::cos(phi)};
    const world::Ecef back =
        normalized({eye.x - target.x, eye.y - target.y, eye.z - target.z});
    world::Ecef right = cross(up, back);
    if (right.x * right.x + right.y * right.y + right.z * right.z < 1e-12) {
        right = cross(north, back);
    }
    right = normalized(right);
    Camera camera;
    camera.position = eye;
    camera.world_from_camera = Mat3::columns(right, cross(back, right), back);
    return camera;
}

Mat4f camera_from_local(const Camera& camera, const Placement& placement) {
    const Mat3 camera_from_world = transpose(camera.world_from_camera);
    const Mat3 rotation = camera_from_world * placement.world_from_local;
    const world::Ecef offset{placement.origin.x - camera.position.x,
                             placement.origin.y - camera.position.y,
                             placement.origin.z - camera.position.z};
    const world::Ecef translation = camera_from_world * offset;
    Mat4f out;
    for (std::size_t i = 0; i < 9; ++i) {
        out.m[i / 3 * 4 + i % 3] = static_cast<float>(rotation.m[i]);
    }
    out.m[12] = static_cast<float>(translation.x);
    out.m[13] = static_cast<float>(translation.y);
    out.m[14] = static_cast<float>(translation.z);
    out.m[15] = 1.0f;
    return out;
}

} // namespace glideslope::gfx
