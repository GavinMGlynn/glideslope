#include "world/sky.hpp"

#include "world/air_motion.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace glideslope::world {

namespace {

constexpr double metres_per_foot = 0.3048;

// The pattern's wavelengths and how much each adds.
constexpr double wavelengths_m[] = {8000.0, 4000.0, 2000.0, 1000.0};
constexpr double weights[] = {0.5, 0.25, 0.15, 0.1};
// The cloud's edge: how far across the pattern's level density goes from 0 to 1.
constexpr double edge = 0.04;
// Where the level is found: a grid over the disc the decks are drawn on.
constexpr double sampled_m = 60000.0;
constexpr int samples = 257;
// Salted so the cloud is drawn apart from the air the same seed draws.
constexpr std::uint64_t cloud_salt = 0x636c6f756473ULL; // "clouds"

double smoothstep(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

std::uint64_t cell(std::int64_t i, std::int64_t j) {
    return static_cast<std::uint64_t>(i) * 0x9e3779b97f4a7c15ULL ^
           static_cast<std::uint64_t>(j);
}

double cover_of(Metar::CloudLayer::Cover cover) {
    switch (cover) {
    case Metar::CloudLayer::Cover::few: return 1.5 / 8.0;
    case Metar::CloudLayer::Cover::scattered: return 3.5 / 8.0;
    case Metar::CloudLayer::Cover::broken: return 6.0 / 8.0;
    case Metar::CloudLayer::Cover::overcast:
    case Metar::CloudLayer::Cover::obscured: return 1.0;
    }
    return 0.0;
}

} // namespace

double drawn_visibility_m(const Metar& metar) {
    if (!metar.visibility_m || metar.visibility_or_more) {
        return clear_visibility_m;
    }
    return std::min(*metar.visibility_m, clear_visibility_m);
}

std::vector<CloudDeck> cloud_decks(const Metar& metar, double elevation_m) {
    std::vector<CloudDeck> decks;
    for (const Metar::CloudLayer& layer : metar.clouds) {
        CloudDeck d;
        d.base_m = elevation_m + layer.base_ft * metres_per_foot;
        const double thickness = layer.cumulonimbus       ? 6000.0
                                 : layer.towering_cumulus ? 2000.0
                                                          : 300.0;
        d.top_m = d.base_m + thickness;
        d.cover = cover_of(layer.cover);
        d.cumulonimbus = layer.cumulonimbus;
        decks.push_back(d);
    }
    std::sort(decks.begin(), decks.end(), [](const CloudDeck& a, const CloudDeck& b) {
        return a.base_m < b.base_m;
    });
    return decks;
}

double haze_top_m(const std::vector<CloudDeck>& decks, double elevation_m) {
    double top = elevation_m + haze_depth_m;
    for (const CloudDeck& d : decks) {
        top = std::min(top, d.base_m);
    }
    return top;
}

CloudPattern::CloudPattern(std::uint64_t seed, int deck, double cover)
    : seed_(seed ^ cloud_salt), deck_(deck), cover_(std::clamp(cover, 0.0, 1.0)) {
    if (cover_ <= 0.0 || cover_ >= 1.0) {
        return;
    }
    // The level that leaves `cover` of the grid above it.
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(samples) * samples);
    for (int y = 0; y < samples; ++y) {
        for (int x = 0; x < samples; ++x) {
            values.push_back(noise((x / (samples - 1.0) - 0.5) * 2.0 * sampled_m,
                                   (y / (samples - 1.0) - 0.5) * 2.0 * sampled_m));
        }
    }
    const auto at = static_cast<std::size_t>(
        std::llround((1.0 - cover_) * static_cast<double>(values.size() - 1)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(at),
                     values.end());
    level_ = values[at];
}

double CloudPattern::noise(double east_m, double north_m) const {
    double sum = 0.0;
    for (int octave = 0; octave < 4; ++octave) {
        const double x = east_m / wavelengths_m[octave];
        const double y = north_m / wavelengths_m[octave];
        const double fx = std::floor(x);
        const double fy = std::floor(y);
        const auto i = static_cast<std::int64_t>(fx);
        const auto j = static_cast<std::int64_t>(fy);
        const auto a = static_cast<std::uint64_t>(deck_ * 16 + octave);
        const auto v = [&](std::int64_t di, std::int64_t dj) {
            return seeded_uniform(seed_, a, cell(i + di, j + dj));
        };
        const double tx = smoothstep(x - fx);
        const double ty = smoothstep(y - fy);
        const double bottom = v(0, 0) + (v(1, 0) - v(0, 0)) * tx;
        const double top = v(0, 1) + (v(1, 1) - v(0, 1)) * tx;
        sum += weights[octave] * (bottom + (top - bottom) * ty);
    }
    return sum;
}

double CloudPattern::density(double east_m, double north_m) const {
    if (cover_ <= 0.0) {
        return 0.0;
    }
    if (cover_ >= 1.0) {
        return 1.0;
    }
    return smoothstep((noise(east_m, north_m) - level_ + edge / 2.0) / edge);
}

Falling precipitation_of(const Metar& metar) {
    for (const Metar::PresentWeather& w : metar.weather) {
        if (w.vicinity) {
            continue;
        }
        for (const std::string& p : w.phenomena) {
            Precipitation what = Precipitation::none;
            if (p == "RA") {
                what = Precipitation::rain;
            } else if (p == "DZ") {
                what = Precipitation::drizzle;
            } else if (p == "SN" || p == "SG") {
                what = Precipitation::snow;
            }
            if (what != Precipitation::none) {
                return {what, w.intensity};
            }
        }
    }
    return {};
}

} // namespace glideslope::world
