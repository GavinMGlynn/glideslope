#pragma once

// The aeroplane the renderer draws: its mesh, and the views of it.
//
// **Where it is** is a `Placement` whose origin is the model's own - its
// flight model's visual reference point, moved by the alignment in
// assets/models/alignment.txt - and whose axes are the body's: +x forward out
// of the nose, +y out of the starboard wing, +z down. gfx/model.hpp says how
// a model comes to be in that frame, and docs/ASSETS.md what it is made from.
//
// **The light is baked in.** The mesh shader has no normals - a vertex is a
// position, a colour and a texture coordinate - so an aeroplane is shaded on
// the way in, each vertex's colour multiplied by how much light its normal
// catches. The sun is the terrain's, north-west and 45 degrees up, so an
// aeroplane is lit as the ground beneath it is; it is given in the body
// frame, where the normals are, which is one vector rather than a rotation
// per vertex. Being baked, the light does not follow the aeroplane as it
// banks until the mesh is made again.

#include "gfx/model.hpp"
#include "gfx/scene.hpp"
#include "world/geodesy.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::gfx {

// The views of an aeroplane, in the order a key cycles them.
enum class View {
    cockpit, // the pilot's eye, looking out along the nose
    ahead,   // outside, in front of it, looking back
    behind,
    left,
    right,
    above,
    orbit, // circling it
};

// Every view, in that order.
const std::vector<View>& every_view();

// "cockpit", "ahead", ... Round trips with `view_named`.
std::string_view name_of(View view);

// The view of that name, or nothing if there is none.
std::optional<View> view_named(std::string_view name);

// Every name, "cockpit, ahead, ...", for saying what the views are.
std::string view_names();

// A model as a mesh the renderer can draw, lit by `sun_in_body` - the unit
// vector towards the sun, in the body frame. See the note above.
Mesh mesh_from_model(const Model& model, const world::Ecef& sun_in_body);

// How far from its origin the model reaches, in metres: the radius the
// outside views stand off by.
double model_radius(const Model& model);

// Where the camera goes for a view of an aeroplane.
//
// `aeroplane` is where the model is; `radius_m` how far it reaches;
// `eye_in_body` the pilot's eye in the body frame, relative to the model's
// origin, which only the cockpit view uses; `orbit_rad` where the orbit has
// got to, which only the orbit view uses.
//
// The cockpit looks out along the nose with the aeroplane's own roll. The
// others stand off in the body frame and look back at it, held upright, so
// they swing with it rather than staying level.
Camera camera_for(View view, const Placement& aeroplane, double radius_m,
                  const std::array<double, 3>& eye_in_body, double orbit_rad);

} // namespace glideslope::gfx
