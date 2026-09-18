#include "gfx/sky.hpp"

#include "world/air_motion.hpp"
#include "world/geodesy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace glideslope::gfx {

namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double radians = pi / 180.0;
constexpr double metres_per_degree = 111319.49;
// The decks' disc, the width its alpha fades over, and its grid.
constexpr double disc_m = 60000.0;
constexpr double fade_m = 10000.0;
constexpr int rings = 48;
constexpr int sectors = 96;
constexpr int texture_size = 512;
// How far below or above a deck's edge the whiteout takes to come on.
constexpr double cloud_edge_m = 10.0;
// The box the rain or snow falls through, and how many fall in it.
constexpr double box_m = 40.0;

double smoothstep(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// The east-north-up axes at a place, in ECEF.
Mat3 enu_at(double latitude_deg, double longitude_deg) {
    const double sp = std::sin(latitude_deg * radians);
    const double cp = std::cos(latitude_deg * radians);
    const double sl = std::sin(longitude_deg * radians);
    const double cl = std::cos(longitude_deg * radians);
    return Mat3::columns({-sl, cl, 0.0}, {-sp * cl, -sp * sl, cp},
                         {cp * cl, cp * sl, sp});
}

float lerp(float a, float b, double t) {
    return a + (b - a) * static_cast<float>(t);
}

Colour mix(const Colour& a, const Colour& b, double t) {
    return {lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), 1.0f};
}

} // namespace

Sky::Sky(Renderer& renderer, const world::Metar& metar, const Station& station,
         std::uint64_t seed)
    : renderer_(renderer), station_(station) {
    frame_.origin = world::to_ecef({station.latitude_deg, station.longitude_deg,
                                    station.elevation_m + station.geoid_m});
    frame_.world_from_local = enu_at(station.latitude_deg, station.longitude_deg);
    visibility_m_ = world::drawn_visibility_m(metar);
    decks_ = world::cloud_decks(metar, station.elevation_m);
    haze_top_m_ = world::haze_top_m(decks_, station.elevation_m) - station.elevation_m;
    falling_ = world::precipitation_of(metar);

    const double metres_east =
        metres_per_degree * std::cos(station.latitude_deg * radians);
    for (std::size_t d = 0; d < decks_.size(); ++d) {
        const world::CloudDeck& deck = decks_[d];
        patterns_.emplace_back(seed, static_cast<int>(d), deck.cover);
        const world::CloudPattern& pattern = patterns_.back();

        // The pattern as alpha, north up, fading out at the disc's edge.
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(texture_size) *
                                       texture_size * 4);
        for (int y = 0; y < texture_size; ++y) {
            for (int x = 0; x < texture_size; ++x) {
                const double east = ((x + 0.5) / texture_size - 0.5) * 2.0 * disc_m;
                const double north = (0.5 - (y + 0.5) / texture_size) * 2.0 * disc_m;
                const double fade =
                    smoothstep((disc_m - std::hypot(east, north)) / fade_m);
                const double alpha = pattern.density(east, north) * fade;
                const auto at = (static_cast<std::size_t>(y) * texture_size +
                                 static_cast<std::size_t>(x)) *
                                4;
                rgba[at] = rgba[at + 1] = rgba[at + 2] = 255;
                rgba[at + 3] = static_cast<std::uint8_t>(std::lround(alpha * 255.0));
            }
        }
        textures_.push_back(
            renderer_.add_texture(texture_size, texture_size, rgba.data()));

        // A sheet at the base and one at the top, curved with the Earth.
        for (const bool top : {false, true}) {
            const double height = top ? deck.top_m : deck.base_m;
            const Colour colour = top ? cloud_top
                                  : deck.cumulonimbus
                                      ? Colour{0.45f, 0.47f, 0.52f, 1.0f}
                                      : cloud_base;
            Placement placement;
            placement.origin =
                world::to_ecef({station.latitude_deg, station.longitude_deg,
                                height + station.geoid_m});
            Mesh mesh;
            for (int ring = 0; ring <= rings; ++ring) {
                const double r = disc_m * ring / rings;
                const int around = ring == 0 ? 1 : sectors;
                for (int s = 0; s < around; ++s) {
                    const double a = 2.0 * pi * s / sectors;
                    const double east = r * std::sin(a);
                    const double north = r * std::cos(a);
                    const world::Ecef p = world::to_ecef(
                        {station.latitude_deg + north / metres_per_degree,
                         station.longitude_deg + east / metres_east,
                         height + station.geoid_m});
                    Vertex v;
                    v.position = {static_cast<float>(p.x - placement.origin.x),
                                  static_cast<float>(p.y - placement.origin.y),
                                  static_cast<float>(p.z - placement.origin.z)};
                    v.colour = {colour.r, colour.g, colour.b, 1.0f};
                    v.uv = {static_cast<float>(east / (2.0 * disc_m) + 0.5),
                            static_cast<float>(0.5 - north / (2.0 * disc_m))};
                    mesh.vertices.push_back(v);
                }
            }
            // The centre's fan, then the rings' quads.
            const auto index = [](int ring, int s) {
                return ring == 0 ? 0u
                                 : static_cast<std::uint32_t>(1 + (ring - 1) * sectors +
                                                              (s % sectors));
            };
            for (int s = 0; s < sectors; ++s) {
                mesh.indices.insert(mesh.indices.end(),
                                    {index(0, 0), index(1, s), index(1, s + 1)});
            }
            for (int ring = 1; ring < rings; ++ring) {
                for (int s = 0; s < sectors; ++s) {
                    const std::uint32_t a = index(ring, s);
                    const std::uint32_t b = index(ring, s + 1);
                    const std::uint32_t c = index(ring + 1, s);
                    const std::uint32_t e = index(ring + 1, s + 1);
                    mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, e});
                }
            }
            sheets_.push_back({renderer_.add_mesh(mesh), height});
            placements_.push_back(placement);
        }
    }
}

Sky::~Sky() {
    for (const Sheet& sheet : sheets_) {
        renderer_.remove_mesh(sheet.mesh);
    }
    for (const TextureId texture : textures_) {
        renderer_.remove_texture(texture);
    }
    if (has_precipitation_) {
        renderer_.remove_mesh(precipitation_);
    }
}

world::Ecef Sky::local(const Camera& camera) const {
    return transpose(frame_.world_from_local) *
           world::Ecef{camera.position.x - frame_.origin.x,
                       camera.position.y - frame_.origin.y,
                       camera.position.z - frame_.origin.z};
}

double Sky::height(const Camera& camera) const {
    return world::to_geodetic(camera.position).height_m - station_.geoid_m;
}

double Sky::cloud_at(const Camera& camera) const {
    const double h = height(camera);
    const world::Ecef at = local(camera);
    double most = 0.0;
    for (std::size_t d = 0; d < decks_.size(); ++d) {
        const double inside = smoothstep(
            std::min(h - decks_[d].base_m, decks_[d].top_m - h) / cloud_edge_m);
        if (inside > 0.0) {
            most = std::max(most, inside * patterns_[d].density(at.x, at.y));
        }
    }
    return most;
}

Haze Sky::haze(const Camera& camera) const {
    Haze h;
    h.station = frame_;
    h.top_m = haze_top_m_;
    h.below_m = visibility_m_;
    h.above_m = world::clear_visibility_m;
    const double cloud = cloud_at(camera);
    if (cloud > 0.0) {
        // Inside cloud: its visibility and colour, everywhere.
        const auto towards = [&](double v) {
            return std::exp(std::log(v) +
                            (std::log(in_cloud_visibility_m) - std::log(v)) * cloud);
        };
        h.below_m = towards(h.below_m);
        h.above_m = towards(h.above_m);
        h.colour = mix(h.colour, in_cloud, cloud);
    }
    return h;
}

Colour Sky::background(const Camera& camera) const {
    const Haze h = haze(camera);
    // The sky seen through as much haze as lies within 4 km of the eye.
    const double eye = local(camera).z;
    const double v = eye < haze_top_m_ ? h.below_m : h.above_m;
    const Colour hazy = mix(h.colour, sky, std::exp(-std::log(20.0) * 4000.0 / v));
    // Under cloud, the sky beyond the decks' disc is as grey as the lowest deck
    // above is covered.
    const double at = height(camera);
    for (const world::CloudDeck& deck : decks_) {
        if (deck.base_m > at) {
            return mix(hazy, cloud_base, deck.cover);
        }
    }
    return hazy;
}

void Sky::draw(const Camera& camera, double time_s, std::vector<Draw>& draws) {
    const double h = height(camera);
    // The sheets farthest from the eye's height first, so nearer ones blend
    // over them.
    std::vector<std::size_t> order(sheets_.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return std::abs(sheets_[a].height_m - h) > std::abs(sheets_[b].height_m - h);
    });
    for (const std::size_t i : order) {
        Draw d;
        d.mesh = sheets_[i].mesh;
        d.placement = placements_[i];
        d.texture = textures_[i / 2];
        d.translucent = true;
        draws.push_back(d);
    }

    // Rain or snow, below the lowest cloud.
    if (has_precipitation_) {
        renderer_.remove_mesh(precipitation_);
        has_precipitation_ = false;
    }
    const double lowest = decks_.empty() ? 1e9 : decks_.front().base_m;
    if (falling_.what == world::Precipitation::none || h >= lowest) {
        return;
    }
    const bool snow = falling_.what == world::Precipitation::snow;
    const double speed = snow                                             ? 1.0
                         : falling_.what == world::Precipitation::drizzle ? 3.0
                                                                          : 7.0;
    const double length = snow                                             ? 0.03
                          : falling_.what == world::Precipitation::drizzle ? 0.2
                                                                           : 0.6;
    const double width = snow ? 0.03 : 0.012;
    const int count = (snow ? 2000 : 1500) *
                      (falling_.intensity < 0   ? 1
                       : falling_.intensity > 0 ? 4
                                                : 2) /
                      2;
    const float alpha = falling_.intensity > 0 ? 0.5f : 0.35f;

    // In a frame at the station's axes, its origin the box's corner nearest
    // below the eye, so the floats stay small.
    const world::Ecef eye = local(camera);
    const world::Ecef corner{std::floor(eye.x / box_m) * box_m,
                             std::floor(eye.y / box_m) * box_m,
                             std::floor(eye.z / box_m) * box_m};
    const world::Ecef offset = frame_.world_from_local * corner;
    Placement placement;
    placement.world_from_local = frame_.world_from_local;
    placement.origin = {frame_.origin.x + offset.x, frame_.origin.y + offset.y,
                        frame_.origin.z + offset.z};
    const world::Ecef e{eye.x - corner.x, eye.y - corner.y, eye.z - corner.z};
    Mesh mesh;
    const auto wrap = [](double v, double centre) {
        return centre + (v - centre - box_m * std::floor((v - centre) / box_m + 0.5));
    };
    for (int i = 0; i < count; ++i) {
        const auto k = static_cast<std::uint64_t>(i);
        // Where it is in the lattice of boxes, falling.
        const double x0 = world::seeded_uniform(0x7261696eULL, k, 0) * box_m - corner.x;
        const double y0 = world::seeded_uniform(0x7261696eULL, k, 1) * box_m - corner.y;
        const double z0 = world::seeded_uniform(0x7261696eULL, k, 2) * box_m -
                          speed * time_s - corner.z;
        const double x = wrap(x0, e.x);
        const double y = wrap(y0, e.y);
        const double z = wrap(z0, e.z);
        // Across the line of sight, level.
        double ax = -(y - e.y);
        double ay = x - e.x;
        const double across = std::hypot(ax, ay);
        if (across < 1e-6) {
            continue;
        }
        ax *= width / 2.0 / across;
        ay *= width / 2.0 / across;
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (const auto& [dx, dy, dz] :
             {std::array<double, 3>{-ax, -ay, 0.0}, std::array<double, 3>{ax, ay, 0.0},
              std::array<double, 3>{-ax, -ay, length},
              std::array<double, 3>{ax, ay, length}}) {
            Vertex v;
            v.position = {static_cast<float>(x + dx), static_cast<float>(y + dy),
                          static_cast<float>(z + dz)};
            v.colour = {0.85f, 0.87f, 0.90f, alpha};
            mesh.vertices.push_back(v);
        }
        mesh.indices.insert(mesh.indices.end(),
                            {base, base + 1, base + 2, base + 1, base + 3, base + 2});
    }
    if (mesh.indices.empty()) {
        return;
    }
    precipitation_ = renderer_.add_mesh(mesh);
    has_precipitation_ = true;
    Draw d;
    d.mesh = precipitation_;
    d.placement = placement;
    d.translucent = true;
    draws.push_back(d);
}

} // namespace glideslope::gfx
