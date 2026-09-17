#include "harness.hpp"

#include "world/geodesy.hpp"

#include <math/FGLocation.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::distance;
using glideslope::world::Ecef;
using glideslope::world::Geodetic;
using glideslope::world::to_ecef;
using glideslope::world::to_geodetic;
using glideslope::world::Wgs84;

namespace {

// Every latitude that matters to a conversion's edge cases: both poles exactly
// and a hair away, the equator exactly and a hair away, and places in between.
const std::vector<double> latitudes = {-90.0, -89.9999999, -60.0, -45.0, -33.9461,
                                       -1e-9, 0.0,         1e-9,  30.0,  45.0,
                                       60.0,  89.9999999,  90.0};
// Both sides of the date line, the prime meridian, and places in between.
const std::vector<double> longitudes = {-180.0, -179.9999999, -90.0,       -1e-9, 0.0,
                                        90.0,   151.1772,     179.9999999, 180.0};
// From below sea level (the Dead Sea shore) to cruising altitude and beyond.
const std::vector<double> heights = {-430.0, 0.0,     6.4,    1000.0,
                                     8848.0, 12500.0, 40000.0};

double longitude_difference(double a, double b) {
    double d = std::fmod(std::abs(a - b), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

} // namespace

GLIDESLOPE_TEST(wgs84_reference_points_convert_exactly) {
    const auto near = [](const Ecef& e, double x, double y, double z) {
        return std::abs(e.x - x) < 1e-6 && std::abs(e.y - y) < 1e-6 &&
               std::abs(e.z - z) < 1e-6;
    };
    check(near(to_ecef({0.0, 0.0, 0.0}), Wgs84::a, 0.0, 0.0),
          "the equator at the prime meridian is a metres along x");
    check(near(to_ecef({0.0, 90.0, 0.0}), 0.0, Wgs84::a, 0.0),
          "90 E is a metres along y");
    check(near(to_ecef({0.0, 180.0, 0.0}), -Wgs84::a, 0.0, 0.0),
          "180 is a metres along -x");
    check(near(to_ecef({90.0, 0.0, 0.0}), 0.0, 0.0, Wgs84::b),
          "the North Pole is b along z");
    check(near(to_ecef({-90.0, 37.0, 100.0}), 0.0, 0.0, -Wgs84::b - 100.0),
          "100 m above the South Pole is b + 100 along -z, whatever the longitude");
    // b is defined by a and f; the published semi-minor axis is 6356752.314245 m.
    check(std::abs(Wgs84::b - 6356752.314245) < 1e-6, "WGS84's semi-minor axis");
}

GLIDESLOPE_TEST(geodetic_positions_round_trip_through_ecef_within_a_millimetre) {
    int walked = 0;
    double worst_m = 0.0;
    std::string failures;
    for (double lat : latitudes) {
        for (double lon : longitudes) {
            for (double h : heights) {
                const Geodetic g{lat, lon, h};
                const Ecef e = to_ecef(g);
                const Geodetic back = to_geodetic(e);
                const Ecef again = to_ecef(back);
                const double moved_m = distance(e, again);
                const double height_m = std::abs(back.height_m - h);
                const double lat_deg = std::abs(back.latitude_deg - lat);
                const bool pole = std::abs(lat) == 90.0;
                const double lon_deg =
                    pole ? 0.0 : longitude_difference(back.longitude_deg, lon);
                worst_m = std::max({worst_m, moved_m, height_m});
                // A billionth of a degree of latitude is 0.1 mm; of longitude, less.
                if (moved_m > 0.001 || height_m > 0.001 || lat_deg > 1e-9 ||
                    lon_deg > 1e-9) {
                    failures += "\n  " + std::to_string(lat) + ", " +
                                std::to_string(lon) + ", " + std::to_string(h) +
                                " m: moved " + std::to_string(moved_m) +
                                " m, height off " + std::to_string(height_m) + " m";
                }
                ++walked;
            }
        }
    }
    const int expected =
        static_cast<int>(latitudes.size() * longitudes.size() * heights.size());
    std::printf("walked %d of %d positions; worst %.3g m\n", walked, expected, worst_m);
    check(walked == expected, "walked every position");
    if (!failures.empty()) {
        fail("round trips off by more than a millimetre:" + failures);
    }
}

// JSBSim converts ECEF to geodetic its own way; the two must agree.
GLIDESLOPE_TEST(the_geodetic_conversion_agrees_with_jsbsims_within_a_millimetre) {
    constexpr double ft = 0.3048;
    constexpr double to_rad = 3.141592653589793 / 180.0;
    int walked = 0;
    std::string failures;
    for (double lat : latitudes) {
        for (double lon : longitudes) {
            for (double h : heights) {
                const Ecef e = to_ecef({lat, lon, h});
                JSBSim::FGLocation jsb;
                // JSBSim's own WGS84 axes, in feet, as its FGInertial states
                // them - not this project's constants, so that a wrong constant
                // here cannot agree with itself.
                jsb.SetEllipse(20925646.32546, 20855486.5951);
                jsb(1) = e.x / ft;
                jsb(2) = e.y / ft;
                jsb(3) = e.z / ft;
                const Geodetic ours = to_geodetic(e);
                const double lat_deg =
                    std::abs(jsb.GetGeodLatitudeRad() / to_rad - ours.latitude_deg);
                const double height_m =
                    std::abs(jsb.GetGeodAltitude() * ft - ours.height_m);
                if (lat_deg > 1e-8 || height_m > 0.001) {
                    failures += "\n  " + std::to_string(lat) + ", " +
                                std::to_string(lon) + ", " + std::to_string(h) +
                                " m: latitude differs by " + std::to_string(lat_deg) +
                                " deg, height by " + std::to_string(height_m) + " m";
                }
                ++walked;
            }
        }
    }
    check(walked ==
              static_cast<int>(latitudes.size() * longitudes.size() * heights.size()),
          "walked every position");
    if (!failures.empty()) {
        fail("disagrees with JSBSim:" + failures);
    }
}
