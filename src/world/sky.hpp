#pragma once

// Weather you can see, as a report describes it: how far one can see, the
// cloud's layers and where each is cloudy, and what is falling.
//
// **Visibility** is the report's, which a METAR measures at the surface: it
// holds in a layer of haze from the ground to 1,000 m above the station, or to
// the lowest cloud if that is lower, and above the layer one sees 40 km. A
// report of 10 km or more, or of none, is drawn as 40 km, a clear day's.
//
// **Cloud** is a deck for each layer the report gives: its base the report's
// height above the station; its top 300 m above that, or 2,000 m for towering
// cumulus and 6,000 m for cumulonimbus, which a METAR does not give; its cover
// the middle of the report's eighths - FEW 1.5, SCT 3.5, BKN 6, OVC and VV 8.
// Where it is cloudy is a pattern of the weather's seed, the same on every
// machine: value noise over wavelengths of 8 km down to 1 km, cloudy where it
// is above the level that leaves the deck's cover of the sky under cloud.
//
// **Precipitation** is the first reported group, not in the vicinity, holding
// drizzle, rain or snow, and how heavy it is.

#include "world/metar.hpp"

#include <cstdint>
#include <vector>

namespace glideslope::world {

// Visibility beyond the haze, and a report's of 10 km or more.
inline constexpr double clear_visibility_m = 40000.0;
// How deep the haze is at most, above the station.
inline constexpr double haze_depth_m = 1000.0;

// The visibility a report gives the haze.
double drawn_visibility_m(const Metar& metar);

struct CloudDeck {
    double base_m = 0.0; // above sea level
    double top_m = 0.0;
    double cover = 0.0; // of the sky, 0 to 1
    bool cumulonimbus = false;
};

// A report's cloud, lowest first, over a station `elevation_m` above sea
// level.
std::vector<CloudDeck> cloud_decks(const Metar& metar, double elevation_m);

// The top of the haze above sea level: 1,000 m above the station, or the
// lowest deck's base.
double haze_top_m(const std::vector<CloudDeck>& decks, double elevation_m);

// Where a deck is cloudy.
class CloudPattern {
public:
    CloudPattern(std::uint64_t seed, int deck, double cover);

    // How cloudy a place is, metres east and north of the station: 0 clear, 1
    // thick, 0.5 the cloud's edge.
    double density(double east_m, double north_m) const;

    double cover() const {
        return cover_;
    }

private:
    double noise(double east_m, double north_m) const;

    std::uint64_t seed_;
    int deck_;
    double cover_;
    double level_ = 0.0;
};

enum class Precipitation { none, drizzle, rain, snow };

struct Falling {
    Precipitation what = Precipitation::none;
    int intensity = 0; // -1 light, 0 moderate, 1 heavy
};

Falling precipitation_of(const Metar& metar);

} // namespace glideslope::world
