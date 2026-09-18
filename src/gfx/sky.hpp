#pragma once

// Weather you can see, drawn: a report's cloud, the haze its visibility makes,
// and rain or snow about the eye (world/sky.hpp says what each is).
//
// Each cloud deck is two sheets over a disc 60 km in radius about the station,
// following the Earth's curve: its base, grey, and its top, white, each with
// the deck's pattern as its texture's alpha, fading out over the disc's last
// 10 km. Inside a deck where it is cloudy the haze is the cloud: 30 m of
// visibility, its grey. Rain falls as streaks and snow as flakes through a box
// 40 m across about the eye, below the lowest cloud - fixed in the world, so
// the eye moves through them - at 7 m/s for rain, 3 for drizzle and 1 for
// snow.

#include "gfx/renderer.hpp"
#include "world/metar.hpp"
#include "world/sky.hpp"

#include <cstdint>
#include <vector>

namespace glideslope::gfx {

// Where a report's station is: its latitude and longitude, its elevation
// above sea level, and the geoid's height there above the ellipsoid.
struct Station {
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double elevation_m = 0.0;
    double geoid_m = 0.0;
};

// The cloud's colours, below and above, and inside it.
inline constexpr Colour cloud_base{0.62f, 0.64f, 0.68f, 1.0f};
inline constexpr Colour cloud_top{0.95f, 0.95f, 0.97f, 1.0f};
inline constexpr Colour in_cloud{0.78f, 0.79f, 0.81f, 1.0f};
// Visibility inside cloud.
inline constexpr double in_cloud_visibility_m = 30.0;

class Sky {
public:
    // The weather `metar` reports at `station`, its cloud drawn from `seed`,
    // on `renderer`'s GPU.
    Sky(Renderer& renderer, const world::Metar& metar, const Station& station,
        std::uint64_t seed);
    ~Sky();

    Sky(const Sky&) = delete;
    Sky& operator=(const Sky&) = delete;

    // Adds to `draws` what the weather shows from `camera` at simulation time
    // `time_s`: the decks' sheets, farthest from the eye's height first, then
    // any rain or snow.
    void draw(const Camera& camera, double time_s, std::vector<Draw>& draws);

    // The haze, and the colour behind everything, from where the camera is:
    // the sky seen through the haze within 4 km, greyed under cloud as much as
    // the lowest deck above covers it.
    Haze haze(const Camera& camera) const;
    Colour background(const Camera& camera) const;

    // How far into cloud the camera is: 0 clear of it, 1 inside.
    double cloud_at(const Camera& camera) const;

private:
    struct Sheet {
        MeshId mesh = 0;
        double height_m = 0.0; // above sea level
    };

    // The camera's place in the station's east-north-up frame, and its height
    // above sea level.
    world::Ecef local(const Camera& camera) const;
    double height(const Camera& camera) const;

    Renderer& renderer_;
    Station station_;
    Placement frame_; // the station's, at its elevation
    double visibility_m_ = 0.0;
    double haze_top_m_ = 0.0;
    std::vector<world::CloudDeck> decks_;
    std::vector<world::CloudPattern> patterns_;
    std::vector<TextureId> textures_;
    std::vector<Sheet> sheets_; // two to a deck, base first
    std::vector<Placement> placements_;
    world::Falling falling_;
    MeshId precipitation_ = 0;
    bool has_precipitation_ = false;
};

} // namespace glideslope::gfx
