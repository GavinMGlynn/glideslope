#include "world/geodesy.hpp"

#include <cmath>
#include <numbers>

namespace glideslope::world {

namespace {

constexpr double to_rad = std::numbers::pi / 180.0;
constexpr double to_deg = 180.0 / std::numbers::pi;

} // namespace

Ecef to_ecef(const Geodetic& g) {
    const double lat = g.latitude_deg * to_rad;
    const double lon = g.longitude_deg * to_rad;
    const double sin_lat = std::sin(lat);
    const double cos_lat = std::cos(lat);
    // Prime vertical radius of curvature.
    const double n = Wgs84::a / std::sqrt(1.0 - Wgs84::e2 * sin_lat * sin_lat);
    return {(n + g.height_m) * cos_lat * std::cos(lon),
            (n + g.height_m) * cos_lat * std::sin(lon),
            (n * (1.0 - Wgs84::e2) + g.height_m) * sin_lat};
}

Geodetic to_geodetic(const Ecef& e) {
    // H. Vermeille, "Computing geodetic coordinates from geocentric
    // coordinates", Journal of Geodesy 78 (2004), with the stable form of its
    // final step for points near the equatorial plane.
    const double a2 = Wgs84::a * Wgs84::a;
    const double e4 = Wgs84::e2 * Wgs84::e2;
    const double p = (e.x * e.x + e.y * e.y) / a2;
    const double q = (1.0 - Wgs84::e2) / a2 * e.z * e.z;
    const double r = (p + q - e4) / 6.0;
    const double s = e4 * p * q / (4.0 * r * r * r);
    const double t = std::cbrt(1.0 + s + std::sqrt(s * (2.0 + s)));
    const double u = r * (1.0 + t + 1.0 / t);
    const double v = std::sqrt(u * u + e4 * q);
    const double w = Wgs84::e2 * (u + v - q) / (2.0 * v);
    const double k = std::sqrt(u + v + w * w) - w;
    const double d = k * std::hypot(e.x, e.y) / (k + Wgs84::e2);
    const double horizontal = std::hypot(d, e.z);

    Geodetic g;
    g.latitude_deg = 2.0 * std::atan2(e.z, d + horizontal) * to_deg;
    g.longitude_deg = (e.x == 0.0 && e.y == 0.0) ? 0.0 : std::atan2(e.y, e.x) * to_deg;
    g.height_m = (k + Wgs84::e2 - 1.0) / k * horizontal;
    return g;
}

double distance(const Ecef& p, const Ecef& q) {
    return std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) +
                     (p.z - q.z) * (p.z - q.z));
}

} // namespace glideslope::world
