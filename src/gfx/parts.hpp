#pragma once

// An aeroplane's livery, and its control surfaces, both data.
//
// **The livery** is a colour scheme, in data/models/<id>.livery: a list of
// coats, each a colour and a box in the body frame - the model's own frame,
// gfx/model.hpp - painted over whatever the model's materials made of what the
// box holds. A box cuts the triangles it crosses, so a coat's edge is a
// straight line however coarse the mesh beneath it; a later coat goes over an
// earlier one.
//
// **The control surfaces** are in data/models/<id>.surfaces: each is a hinge
// line, two points in the body frame, with how far forward of it the surface
// reaches - nothing for a surface hinged at its leading edge; an all-moving
// tailplane's pivot is inside it - how far aft, and how thick it is about it,
// and the JSBSim property whose value is
// its deflection in radians, with which way a positive value moves its
// trailing edge. The box those make is cut out of the mesh the same way, and
// becomes a part of its own, drawn turned about its hinge.
//
// A file of either kind reads:
//
//   # anything after a hash is a comment
//   name TEXT...                               (a livery's name)
//   coat NAME R G B  X0 X1  Y0 Y1  Z0 Z1        (a livery's coat: 0-255; metres)
//   surface NAME PROPERTY down|up|left|right
//           AX AY AZ  BX BY BZ  LEAD CHORD DEPTH (a surface, on one line)
//
// The models carry neither: tools/make_models.py makes geometry, and these
// files are written for each aircraft by hand from that geometry; each file
// says how.

#include "gfx/model.hpp"
#include "gfx/scene.hpp"

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::gfx {

class PartsError : public std::runtime_error {
public:
    explicit PartsError(const std::string& what) : std::runtime_error(what) {}
};

struct Coat {
    std::string name;
    std::array<float, 3> colour{}; // 0..1, as a model's vertex colours are
    std::array<double, 3> low{};   // the box, body frame, metres
    std::array<double, 3> high{};
};

struct Livery {
    std::string name;
    std::vector<Coat> coats;
};

// Which way a positive deflection moves a surface's trailing edge, in the
// body frame: down (+z), up, left (-y) or right (+y).
enum class Toward { down, up, left, right };

struct Surface {
    std::string name;     // "left-aileron", "elevator", "rudder"...
    std::string property; // JSBSim's, in radians
    Toward toward = Toward::down;
    std::array<double, 3> a{}; // the hinge line's two ends, body frame, metres
    std::array<double, 3> b{};
    double lead_m = 0.0;  // how far forward of the hinge the surface reaches
    double chord_m = 0.0; // how far aft of it
    double depth_m = 0.0; // how far either side of the hinge's plane
};

// The files above. Each throws PartsError naming the line it cannot read: a
// coat or surface with too few numbers or too many, a box or a surface with
// no extent, a colour past 255, a word not one of the four, or a name given
// twice.
Livery read_livery(const std::filesystem::path& path);
Livery parse_livery(std::string_view text, const std::string& name);
std::vector<Surface> read_surfaces(const std::filesystem::path& path);
std::vector<Surface> parse_surfaces(std::string_view text, const std::string& name);

// A surface's hinge as the renderer turns it: the line through `a` along
// `axis`, a unit vector chosen so that turning by a positive angle about it,
// right-handed, moves the trailing edge the way `toward` says.
struct Hinge {
    std::array<double, 3> a{};
    std::array<double, 3> axis{};
};

Hinge hinge_of(const Surface& surface);

// **The model in its parts**: `body` with the livery painted on it and each
// surface's box cut out, and `surfaces[i]` what `surfaces[i]`'s box cut, in
// the same body frame. A triangle crossed by a box's face is cut along it, so
// the parts meet where they were cut and together cover what the model did,
// to rounding: at no deflection the aeroplane looks as it did, but painted.
struct ModelParts {
    Model body;
    std::vector<Model> surfaces;
};

ModelParts split_model(const Model& model, const Livery& livery,
                       const std::vector<Surface>& surfaces);

// The area of a model's triangles, square metres; and of those of its
// triangles coloured `colour`, to a 255th - what the tests weigh a coat by.
double area_of(const Model& model);
double area_coloured(const Model& model, const std::array<float, 3>& colour);

// Where a surface's part is drawn when the aeroplane is at `aeroplane` and the
// surface is deflected by `angle_rad`: the aeroplane's placement, turned about
// the hinge.
Placement hinged(const Placement& aeroplane, const Hinge& hinge, double angle_rad);

// A point of the body frame turned about the hinge by `angle_rad`, as
// `hinged` turns the part: for tests.
std::array<double, 3> turned(const Hinge& hinge, double angle_rad,
                             const std::array<double, 3>& point);

} // namespace glideslope::gfx
