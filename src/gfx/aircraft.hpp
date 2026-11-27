#pragma once

// The aeroplane the renderer draws: its mesh, and the views of it.
//
// **Where it is** is a `Placement` whose origin is the model's own - its
// flight model's visual reference point, moved by the alignment in
// assets/models/alignment.txt - and whose axes are the body's: +x forward out
// of the nose, +y out of the starboard wing, +z down. gfx/model.hpp says how
// a model comes to be in that frame, and docs/ASSETS.md what it is made from.
//
// **The light is the shader's.** A vertex carries its normal in the body
// frame, and a draw of the aeroplane carries the sun (gfx/renderer.hpp's
// Draw::sun) - the terrain's, north-west and 45 degrees up, so an aeroplane is
// lit as the ground beneath it is. The shader turns the sun into the mesh's
// frame, so the light follows the aeroplane as it banks, and each control
// surface as it turns about its hinge, with the mesh made once.

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

// A model as a mesh the renderer can draw: its colours as they are, and its
// normals, to be lit by the sun its draws are given. See the note above.
Mesh mesh_from_model(const Model& model);

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
