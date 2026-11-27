#include "gfx/parts.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <sstream>

namespace glideslope::gfx {

namespace {

using Vec = std::array<double, 3>;

Vec sub(const Vec& a, const Vec& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Vec add(const Vec& a, const Vec& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

Vec times(const Vec& a, double s) {
    return {a[0] * s, a[1] * s, a[2] * s};
}

double dot(const Vec& a, const Vec& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Vec cross(const Vec& a, const Vec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

double length(const Vec& a) {
    return std::sqrt(dot(a, a));
}

Vec unit(const Vec& a) {
    const double l = length(a);
    return l > 0.0 ? times(a, 1.0 / l) : Vec{0.0, 0.0, 0.0};
}

Vec toward_vector(Toward toward) {
    switch (toward) {
    case Toward::down:
        return {0.0, 0.0, 1.0};
    case Toward::up:
        return {0.0, 0.0, -1.0};
    case Toward::left:
        return {0.0, -1.0, 0.0};
    case Toward::right:
        return {0.0, 1.0, 0.0};
    }
    return {0.0, 0.0, 1.0};
}

// A box in a frame of its own: `origin`, three orthonormal `axes`, and the
// range along each.
struct Region {
    Vec origin{};
    std::array<Vec, 3> axes{Vec{1.0, 0.0, 0.0}, Vec{0.0, 1.0, 0.0}, Vec{0.0, 0.0, 1.0}};
    Vec low{};
    Vec high{};

    double along(const Vec& p, std::size_t k) const {
        return dot(sub(p, origin), axes[k]);
    }
};

// A surface's box: along the hinge from `a` to `b`, aft of it by the chord,
// and the depth either side of the plane they make.
struct SurfaceFrame {
    Vec along{};
    Vec aft{};
    Vec across{};
    double span = 0.0;
};

SurfaceFrame frame_of(const Surface& s) {
    SurfaceFrame f;
    const Vec ab = sub(s.b, s.a);
    f.span = length(ab);
    f.along = unit(ab);
    const Vec back{-1.0, 0.0, 0.0};
    f.aft = unit(sub(back, times(f.along, dot(back, f.along))));
    f.across = cross(f.along, f.aft);
    return f;
}

Region region_of(const Surface& s) {
    const SurfaceFrame f = frame_of(s);
    Region r;
    r.origin = s.a;
    r.axes = {f.along, f.aft, f.across};
    r.low = {0.0, -s.lead_m, -s.depth_m};
    r.high = {f.span, s.chord_m, s.depth_m};
    return r;
}

Region region_of(const Coat& c) {
    Region r;
    r.low = c.low;
    r.high = c.high;
    return r;
}

struct Corner {
    Vec p{};
    Vec n{};
    Vec c{};
};

using Polygon = std::vector<Corner>;

Corner between(const Corner& a, const Corner& b, double t) {
    Corner out;
    for (std::size_t i = 0; i < 3; ++i) {
        out.p[i] = a.p[i] + t * (b.p[i] - a.p[i]);
        out.n[i] = a.n[i] + t * (b.n[i] - a.n[i]);
        out.c[i] = a.c[i] + t * (b.c[i] - a.c[i]);
    }
    return out;
}

// Splits `in` by the plane where `region.along(p, k)` is `value`: what is on
// the side `sign` points to (+1 above, -1 below) into `kept`, the rest into
// `cut`. Either may come out with fewer than three corners: nothing there.
void split(const Polygon& in, const Region& region, std::size_t k, double value,
           double sign, Polygon& kept, Polygon& cut) {
    kept.clear();
    cut.clear();
    const std::size_t n = in.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Corner& here = in[i];
        const Corner& next = in[(i + 1) % n];
        const double e_here = sign * (region.along(here.p, k) - value);
        const double e_next = sign * (region.along(next.p, k) - value);
        (e_here >= 0.0 ? kept : cut).push_back(here);
        if ((e_here >= 0.0) != (e_next >= 0.0)) {
            const Corner at = between(here, next, e_here / (e_here - e_next));
            kept.push_back(at);
            cut.push_back(at);
        }
    }
}

// What of `polygon` is inside `region`, and the pieces of it that are not.
std::optional<Polygon> cut_out(const Polygon& polygon, const Region& region,
                               std::vector<Polygon>& outside) {
    // Wholly outside one face, or wholly inside every one: no cut.
    bool inside_all = true;
    for (std::size_t k = 0; k < 3; ++k) {
        double least = std::numeric_limits<double>::infinity();
        double most = -least;
        for (const Corner& c : polygon) {
            const double v = region.along(c.p, k);
            least = std::min(least, v);
            most = std::max(most, v);
        }
        if (most <= region.low[k] || least >= region.high[k]) {
            outside.push_back(polygon);
            return std::nullopt;
        }
        inside_all = inside_all && least >= region.low[k] && most <= region.high[k];
    }
    if (inside_all) {
        return polygon;
    }
    Polygon inside = polygon;
    Polygon kept;
    Polygon cut;
    for (std::size_t k = 0; k < 3; ++k) {
        for (const double sign : {1.0, -1.0}) {
            split(inside, region, k, sign > 0.0 ? region.low[k] : region.high[k], sign,
                  kept, cut);
            if (cut.size() >= 3) {
                outside.push_back(cut);
            }
            inside = kept;
            if (inside.size() < 3) {
                return std::nullopt;
            }
        }
    }
    return inside;
}

void emit(Model& model, const Polygon& polygon) {
    const auto first = static_cast<std::uint32_t>(model.vertices.size());
    for (const Corner& c : polygon) {
        ModelVertex v;
        const Vec n = unit(c.n);
        for (std::size_t i = 0; i < 3; ++i) {
            v.position[i] = static_cast<float>(c.p[i]);
            v.normal[i] = static_cast<float>(n[i]);
            v.colour[i] = static_cast<float>(c.c[i]);
        }
        v.colour[3] = 1.0f;
        model.vertices.push_back(v);
    }
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
        model.indices.push_back(first);
        model.indices.push_back(first + static_cast<std::uint32_t>(i));
        model.indices.push_back(first + static_cast<std::uint32_t>(i + 1));
    }
}

void bound(Model& model) {
    if (model.vertices.empty()) {
        model.low = model.high = {0.0f, 0.0f, 0.0f};
        return;
    }
    model.low = model.high = model.vertices.front().position;
    for (const ModelVertex& v : model.vertices) {
        for (std::size_t i = 0; i < 3; ++i) {
            model.low[i] = std::min(model.low[i], v.position[i]);
            model.high[i] = std::max(model.high[i], v.position[i]);
        }
    }
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw PartsError("cannot open " + path.string());
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Each line of `text` with its comment taken off, numbered from 1, as words.
template <typename Each>
void each_line(std::string_view text, Each each) {
    std::istringstream lines{std::string(text)};
    std::string line;
    int number = 0;
    while (std::getline(lines, line)) {
        ++number;
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line.erase(hash);
        }
        std::istringstream words(line);
        std::string first;
        if (words >> first) {
            each(number, first, words);
        }
    }
}

bool nothing_after(std::istringstream& words) {
    std::string extra;
    return !(words >> extra);
}

} // namespace

Livery parse_livery(std::string_view text, const std::string& name) {
    Livery livery;
    std::set<std::string> names;
    each_line(text, [&](int number, const std::string& word, std::istringstream& words) {
        const auto wrong = [&](const std::string& what) {
            return PartsError(name + ":" + std::to_string(number) + ": " + what);
        };
        if (word == "name") {
            std::string rest;
            std::getline(words, rest);
            const std::size_t start = rest.find_first_not_of(" \t");
            livery.name = start == std::string::npos ? "" : rest.substr(start);
            const std::size_t end = livery.name.find_last_not_of(" \t\r");
            livery.name.erase(end == std::string::npos ? 0 : end + 1);
            return;
        }
        if (word != "coat") {
            throw wrong("'" + word + "' is not name or coat");
        }
        Coat coat;
        std::array<int, 3> rgb{};
        if (!(words >> coat.name >> rgb[0] >> rgb[1] >> rgb[2] >> coat.low[0] >>
              coat.high[0] >> coat.low[1] >> coat.high[1] >> coat.low[2] >> coat.high[2]) ||
            !nothing_after(words)) {
            throw wrong("a coat is a name, R G B from 0 to 255, then X0 X1 Y0 Y1 Z0 Z1");
        }
        for (std::size_t i = 0; i < 3; ++i) {
            if (rgb[i] < 0 || rgb[i] > 255) {
                throw wrong(coat.name + "'s colour is past 0 to 255");
            }
            if (!(coat.high[i] > coat.low[i])) {
                throw wrong(coat.name + "'s box has no extent along " +
                            std::string(1, "xyz"[i]));
            }
            coat.colour[i] = static_cast<float>(rgb[i]) / 255.0f;
        }
        if (!names.insert(coat.name).second) {
            throw wrong(coat.name + " is named twice");
        }
        livery.coats.push_back(coat);
    });
    if (livery.name.empty()) {
        throw PartsError(name + " gives its livery no name");
    }
    if (livery.coats.empty()) {
        throw PartsError(name + " has no coat");
    }
    return livery;
}

Livery read_livery(const std::filesystem::path& path) {
    return parse_livery(read_file(path), path.string());
}

std::vector<Surface> parse_surfaces(std::string_view text, const std::string& name) {
    std::vector<Surface> out;
    std::set<std::string> names;
    each_line(text, [&](int number, const std::string& word, std::istringstream& words) {
        const auto wrong = [&](const std::string& what) {
            return PartsError(name + ":" + std::to_string(number) + ": " + what);
        };
        if (word != "surface") {
            throw wrong("'" + word + "' is not surface");
        }
        Surface s;
        std::string toward;
        if (!(words >> s.name >> s.property >> toward >> s.a[0] >> s.a[1] >> s.a[2] >>
              s.b[0] >> s.b[1] >> s.b[2] >> s.lead_m >> s.chord_m >> s.depth_m) ||
            !nothing_after(words)) {
            throw wrong("a surface is a name, a property, down|up|left|right, the "
                        "hinge's two ends AX AY AZ BX BY BZ, its lead, chord and depth");
        }
        if (toward == "down") {
            s.toward = Toward::down;
        } else if (toward == "up") {
            s.toward = Toward::up;
        } else if (toward == "left") {
            s.toward = Toward::left;
        } else if (toward == "right") {
            s.toward = Toward::right;
        } else {
            throw wrong("'" + toward + "' is not down, up, left or right");
        }
        if (!(s.lead_m >= 0.0) || !(s.chord_m > 0.0) || !(s.depth_m > 0.0) ||
            !(length(sub(s.b, s.a)) > 0.0)) {
            throw wrong(s.name + " has no extent: its hinge, chord or depth is nothing");
        }
        if (!names.insert(s.name).second) {
            throw wrong(s.name + " is named twice");
        }
        try {
            (void)hinge_of(s);
        } catch (const PartsError& e) {
            throw wrong(e.what());
        }
        out.push_back(s);
    });
    if (out.empty()) {
        throw PartsError(name + " names no surface");
    }
    return out;
}

std::vector<Surface> read_surfaces(const std::filesystem::path& path) {
    return parse_surfaces(read_file(path), path.string());
}

Hinge hinge_of(const Surface& surface) {
    const SurfaceFrame f = frame_of(surface);
    // Turning by a small positive angle about `along` moves a point aft of
    // the hinge along along x aft; the axis is turned round if that is not the
    // way the surface's trailing edge is to go.
    const double sense = dot(cross(f.along, f.aft), toward_vector(surface.toward));
    if (std::abs(sense) < 0.3) {
        throw PartsError(surface.name + "'s trailing edge cannot move " +
                         "the way it is said to: its hinge lies across that way");
    }
    Hinge h;
    h.a = surface.a;
    h.axis = sense > 0.0 ? f.along : times(f.along, -1.0);
    return h;
}

ModelParts split_model(const Model& model, const Livery& livery,
                       const std::vector<Surface>& surfaces) {
    std::vector<Region> coats;
    for (const Coat& c : livery.coats) {
        coats.push_back(region_of(c));
    }
    std::vector<Region> boxes;
    for (const Surface& s : surfaces) {
        boxes.push_back(region_of(s));
    }
    ModelParts parts;
    parts.surfaces.resize(surfaces.size());

    // **A triangle no box touched keeps its vertices**, shared with its
    // neighbours as the model has them; only what a box cut or painted is
    // given vertices of its own. `whole` is whether a piece is still the
    // triangle it came from, unpainted.
    struct Piece {
        Polygon polygon;
        bool whole = false;
    };
    // Each part's own index for a vertex of the model, once it has one.
    constexpr std::uint32_t none = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::vector<std::uint32_t>> index_in(
        surfaces.size() + 1, std::vector<std::uint32_t>(model.vertices.size(), none));
    const auto emit_whole = [&](Model& part, std::vector<std::uint32_t>& index,
                                std::size_t t) {
        for (std::size_t k = 0; k < 3; ++k) {
            const std::uint32_t v = model.indices[t + k];
            if (index[v] == none) {
                index[v] = static_cast<std::uint32_t>(part.vertices.size());
                part.vertices.push_back(model.vertices[v]);
            }
            part.indices.push_back(index[v]);
        }
    };

    std::vector<Piece> pieces;
    std::vector<Piece> next;
    std::vector<Polygon> outside;
    for (std::size_t t = 0; t + 2 < model.indices.size(); t += 3) {
        Polygon triangle;
        for (std::size_t k = 0; k < 3; ++k) {
            const ModelVertex& v = model.vertices[model.indices[t + k]];
            Corner c;
            for (std::size_t i = 0; i < 3; ++i) {
                c.p[i] = static_cast<double>(v.position[i]);
                c.n[i] = static_cast<double>(v.normal[i]);
                c.c[i] = static_cast<double>(v.colour[i]);
            }
            triangle.push_back(c);
        }
        pieces.assign(1, Piece{triangle, true});
        for (std::size_t i = 0; i < coats.size(); ++i) {
            next.clear();
            for (const Piece& piece : pieces) {
                outside.clear();
                std::optional<Polygon> in = cut_out(piece.polygon, coats[i], outside);
                if (in) {
                    for (Corner& c : *in) {
                        for (std::size_t j = 0; j < 3; ++j) {
                            c.c[j] = static_cast<double>(livery.coats[i].colour[j]);
                        }
                    }
                    next.push_back(Piece{*in, false});
                }
                const bool untouched = !in && outside.size() == 1 &&
                                       outside.front().size() == piece.polygon.size();
                for (const Polygon& o : outside) {
                    next.push_back(Piece{o, piece.whole && untouched});
                }
            }
            pieces.swap(next);
        }
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            next.clear();
            for (const Piece& piece : pieces) {
                outside.clear();
                const std::optional<Polygon> in = cut_out(piece.polygon, boxes[i], outside);
                if (in) {
                    if (piece.whole && outside.empty()) {
                        emit_whole(parts.surfaces[i], index_in[i + 1], t);
                    } else {
                        emit(parts.surfaces[i], *in);
                    }
                }
                const bool untouched = !in && outside.size() == 1 &&
                                       outside.front().size() == piece.polygon.size();
                for (const Polygon& o : outside) {
                    next.push_back(Piece{o, piece.whole && untouched});
                }
            }
            pieces.swap(next);
        }
        for (const Piece& piece : pieces) {
            if (piece.whole) {
                emit_whole(parts.body, index_in[0], t);
            } else {
                emit(parts.body, piece.polygon);
            }
        }
    }
    bound(parts.body);
    for (Model& m : parts.surfaces) {
        bound(m);
    }
    return parts;
}

namespace {

double triangle_area(const Model& model, std::size_t t) {
    const auto at = [&](std::size_t k) {
        const ModelVertex& v = model.vertices[model.indices[t + k]];
        return Vec{static_cast<double>(v.position[0]), static_cast<double>(v.position[1]),
                   static_cast<double>(v.position[2])};
    };
    return 0.5 * length(cross(sub(at(1), at(0)), sub(at(2), at(0))));
}

} // namespace

double area_of(const Model& model) {
    double sum = 0.0;
    for (std::size_t t = 0; t + 2 < model.indices.size(); t += 3) {
        sum += triangle_area(model, t);
    }
    return sum;
}

double area_coloured(const Model& model, const std::array<float, 3>& colour) {
    double sum = 0.0;
    for (std::size_t t = 0; t + 2 < model.indices.size(); t += 3) {
        bool all = true;
        for (std::size_t k = 0; k < 3 && all; ++k) {
            const ModelVertex& v = model.vertices[model.indices[t + k]];
            for (std::size_t i = 0; i < 3; ++i) {
                all = all && std::abs(v.colour[i] - colour[i]) < 0.5f / 255.0f;
            }
        }
        if (all) {
            sum += triangle_area(model, t);
        }
    }
    return sum;
}

namespace {

// The rotation by `angle` about the unit `axis`, as columns.
std::array<Vec, 3> rotation(const Vec& axis, double angle) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const auto turn = [&](const Vec& v) {
        // Rodrigues: v cos + (k x v) sin + k (k . v)(1 - cos).
        return add(add(times(v, c), times(cross(axis, v), s)),
                   times(axis, dot(axis, v) * (1.0 - c)));
    };
    return {turn({1.0, 0.0, 0.0}), turn({0.0, 1.0, 0.0}), turn({0.0, 0.0, 1.0})};
}

Vec rotate_by(const std::array<Vec, 3>& r, const Vec& v) {
    return add(add(times(r[0], v[0]), times(r[1], v[1])), times(r[2], v[2]));
}

} // namespace

std::array<double, 3> turned(const Hinge& hinge, double angle_rad,
                             const std::array<double, 3>& point) {
    const std::array<Vec, 3> r = rotation(hinge.axis, angle_rad);
    return add(hinge.a, rotate_by(r, sub(point, hinge.a)));
}

Placement hinged(const Placement& aeroplane, const Hinge& hinge, double angle_rad) {
    // world = O + W (A + R (v - A)) = (O + W (A - R A)) + (W R) v.
    const std::array<Vec, 3> r = rotation(hinge.axis, angle_rad);
    const Vec shift = sub(hinge.a, rotate_by(r, hinge.a));
    const auto ecef = [](const Vec& v) { return world::Ecef{v[0], v[1], v[2]}; };
    Placement out;
    const world::Ecef moved = aeroplane.world_from_local * ecef(shift);
    out.origin = {aeroplane.origin.x + moved.x, aeroplane.origin.y + moved.y,
                  aeroplane.origin.z + moved.z};
    out.world_from_local =
        aeroplane.world_from_local * Mat3::columns(ecef(r[0]), ecef(r[1]), ecef(r[2]));
    return out;
}

} // namespace glideslope::gfx
