#include "gfx/aircraft.hpp"

#include "gfx/terrain_colour.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace glideslope::gfx {

namespace {

// How far off an outside view stands, as a multiple of the model's radius.
constexpr double stand_off = 3.0;
// How far above the aeroplane an outside view sits, the same way. +z is down.
constexpr double raised = 0.35;

world::Ecef operator+(const world::Ecef& a, const world::Ecef& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

world::Ecef times(const world::Ecef& v, double s) {
    return {v.x * s, v.y * s, v.z * s};
}

// A point given in the body frame, in ECEF.
world::Ecef in_world(const Placement& p, const std::array<double, 3>& body) {
    return p.origin + (p.world_from_local * world::Ecef{body[0], body[1], body[2]});
}

const std::vector<View>& views() {
    static const std::vector<View> all{View::cockpit, View::ahead, View::behind,
                                       View::left,    View::right, View::above,
                                       View::orbit};
    return all;
}

} // namespace

const std::vector<View>& every_view() {
    return views();
}

std::string_view name_of(View view) {
    switch (view) {
    case View::cockpit:
        return "cockpit";
    case View::ahead:
        return "ahead";
    case View::behind:
        return "behind";
    case View::left:
        return "left";
    case View::right:
        return "right";
    case View::above:
        return "above";
    case View::orbit:
        return "orbit";
    }
    return "cockpit";
}

std::optional<View> view_named(std::string_view name) {
    for (const View view : views()) {
        if (name_of(view) == name) {
            return view;
        }
    }
    return std::nullopt;
}

std::string view_names() {
    std::string out;
    for (const View view : views()) {
        out += (out.empty() ? "" : ", ") + std::string(name_of(view));
    }
    return out;
}

Mesh mesh_from_model(const Model& model, const world::Ecef& sun_in_body) {
    Mesh mesh;
    mesh.vertices.reserve(model.vertices.size());
    for (const ModelVertex& v : model.vertices) {
        const world::Ecef normal{static_cast<double>(v.normal[0]),
                                 static_cast<double>(v.normal[1]),
                                 static_cast<double>(v.normal[2])};
        const double light = light_on(normal, sun_in_body);
        Vertex out;
        out.position = v.position;
        for (std::size_t i = 0; i < 3; ++i) {
            out.colour[i] = static_cast<float>(
                std::clamp(static_cast<double>(v.colour[i]) * light, 0.0, 1.0));
        }
        out.colour[3] = v.colour[3];
        mesh.vertices.push_back(out);
    }
    mesh.indices = model.indices;
    return mesh;
}

double model_radius(const Model& model) {
    double most = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        most = std::max(most, std::max(std::abs(static_cast<double>(model.low[i])),
                                       std::abs(static_cast<double>(model.high[i]))));
    }
    return most;
}

Camera camera_for(View view, const Placement& aeroplane, double radius_m,
                  const std::array<double, 3>& eye_in_body, double orbit_rad) {
    if (view == View::cockpit) {
        // Out along the nose, rolling with the aeroplane: the body's forward
        // is the way it looks, and the body's down is the top of the frame
        // upside down. The camera's axes are right, up and back.
        const world::Ecef forward =
            aeroplane.world_from_local * world::Ecef{1.0, 0.0, 0.0};
        const world::Ecef right =
            aeroplane.world_from_local * world::Ecef{0.0, 1.0, 0.0};
        const world::Ecef down =
            aeroplane.world_from_local * world::Ecef{0.0, 0.0, 1.0};
        Camera camera;
        camera.position = in_world(aeroplane, eye_in_body);
        camera.world_from_camera =
            Mat3::columns(right, times(down, -1.0), times(forward, -1.0));
        camera.near_m = 0.3;
        return camera;
    }

    const double off = std::max(radius_m, 1.0) * stand_off;
    const double up = std::max(radius_m, 1.0) * raised;
    std::array<double, 3> where{0.0, 0.0, -up};
    switch (view) {
    case View::ahead:
        where = {off, 0.0, -up};
        break;
    case View::behind:
        where = {-off, 0.0, -up};
        break;
    case View::left:
        where = {0.0, -off, -up};
        break;
    case View::right:
        where = {0.0, off, -up};
        break;
    case View::above:
        where = {0.0, 0.0, -off};
        break;
    case View::orbit:
        where = {off * std::cos(orbit_rad), off * std::sin(orbit_rad), -up};
        break;
    case View::cockpit:
        break; // returned above
    }
    Camera camera = look_at(in_world(aeroplane, where), aeroplane.origin);
    // Near enough that the aeroplane is never clipped, far enough that the
    // depth buffer is not wasted: a tenth of the stand-off.
    camera.near_m = std::max(0.3, off * 0.1);
    return camera;
}

} // namespace glideslope::gfx
