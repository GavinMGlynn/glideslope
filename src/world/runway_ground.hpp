#pragma once

// **The ground an aircraft meets**: the DEM, and under every runway the
// runway's own surface (REQUIREMENTS.md, section 9, decided 2026-10-01).
//
// **Why.** The Copernicus DEM is a radar-measured surface model with a sample
// every 30 m and metres of vertical error, so under a runway it rises and falls
// where the runway does not: at Sydney's 16R by several feet over a few cells,
// enough to pitch an airliner on its take-off roll into its own tail. A runway
// is graded flat; collision ground under one is made to be.
//
// **Which runways.** `assets/runways/strips.csv`, made by
// tools/make_runway_strips.py from OurAirports' pinned `runways.csv`: every
// open runway that is not a helipad or on water and whose ends the file
// places. It is part of the build - nothing fetches the ground - and is read
// once per process (runway_surfaces).
//
// **What a runway's surface is.** A rectangle between its two ends - the ends
// of the pavement, not displaced thresholds: between the pinned file's two ends
// is the runway's length, to a fraction of a percent, at the airports checked
// (PROJECT_STATUS.md), and the elevations are the ends' - as wide as the file
// says, `default_runway_width_m` where it gives no width or an implausible
// one. Its surface is a straight line along it from one end's elevation to
// the other's, level across it: the file's end elevations, above sea level,
// which the DEM's heights are too. Past an end it is held at that end's height.
//
// **Where the file cannot be believed**, the line is fitted to the DEM: least
// squares through the DEM's heights at points every `fit_spacing_m` along the
// centreline, end to end. That is used where either end has no elevation, or
// where either end's elevation is further than `elevation_tolerance_m` from
// that fit - an elevation entered wrongly, or in another datum, would
// otherwise put a cliff at the runway's edge.
//
// **Into the DEM**: beyond the rectangle, for `runway_shoulder_m`, the ground
// blends from the runway's surface to the DEM by a smoothstep of the distance
// from the rectangle; beyond the shoulder it is the DEM's, exactly.
//
// **Where runways meet, their lines are made to.** The file's elevations are
// whole feet, and two runways' lines, each through its own ends, miss each
// other where the runways meet - at Sydney, 07/25's by 2.3 m above 16R/34L's
// where they cross. Two runways are tied where their centrelines cross, and
// where an end of one is within reach of the other - its half-width, the
// other's and a shoulder - at the closest point to that end: so a cross, a T,
// a V and two runways end to end or side by side are all tied, and side by
// side at both ends of where they run together. Every runway tied to another
// is one of a group, and the group's lines are moved the least - in the sum
// of the squares of the changes to their ends' heights - that makes each tie
// meet.
//
// **A group the ties would move too far** - any end further from its own
// line than `elevation_tolerance_m`, which one wrong elevation in the file can
// do - is made from every member's fit to the DEM instead, and tied again.
//
// **Where runways overlap** - at a crossing, or one's shoulder over another -
// the ground is the mean of their surfaces weighted by each one's blend, and
// the DEM fills whatever weight they leave below one: continuous everywhere.
//
// **The same everywhere.** Every decision - which runways reach a place, which
// are tied, whether a line is the file's or the fit - follows from the same
// sorted data and the pinned DEM alone, never from which tiles are loaded or
// the order anything was read in. The arithmetic is floating point through
// each platform's own sine and cosine, so places and heights agree across
// platforms to about a nanometre, not bit for bit, and a decision could differ
// between two machines only for a runway within a nanometre of one of its
// thresholds; on one machine it is the same every time. It is collision ground
// only; what is drawn is the DEM as it is, and how
// far the two are apart is measured, as for every visual provider.
//
// **A change to these rules or to the strips is a change to the ground**,
// which a server and its clients must agree on: `collision_ground_rules`
// counts the rules' versions, and a test pins the protocol version to it and
// to the strips' SHA-256 (net/protocol.hpp).

#include "world/dem.hpp"
#include "world/runways.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace glideslope::world {

inline constexpr int collision_ground_rules = 1;
inline constexpr double runway_shoulder_m = 50.0;
inline constexpr double default_runway_width_m = 30.0;
inline constexpr double elevation_tolerance_m = 5.0;
inline constexpr double fit_spacing_m = 10.0;
inline constexpr double longest_runway_m = 8000.0;

// One runway of `assets/runways/strips.csv`.
struct RunwayStrip {
    std::string airport;
    std::string le_ident;
    std::string he_ident;
    double le_latitude_deg = 0.0;
    double le_longitude_deg = 0.0;
    double le_elevation_ft = std::numeric_limits<double>::quiet_NaN(); // NaN: not given
    double he_latitude_deg = 0.0;
    double he_longitude_deg = 0.0;
    double he_elevation_ft = std::numeric_limits<double>::quiet_NaN();
    double width_m = std::numeric_limits<double>::quiet_NaN();
};

// The strips in `text`, a strips.csv. Throws RunwayError, naming the line, for
// anything that is not one.
std::vector<RunwayStrip> read_runway_strips(std::string_view text);

// **Every runway in the world, indexed by where it is**: immutable once made,
// and so shared by every CollisionGround in a process.
class RunwaySurfaces {
public:
    // Sorted - by airport, idents and places - so that their order in the file
    // changes nothing.
    explicit RunwaySurfaces(std::vector<RunwayStrip> strips);

    // Where two runways are made to meet: `t` along this one, `other_t`
    // along the other.
    struct Tie {
        std::uint32_t other;
        double t;
        double other_t;
    };

    struct Runway {
        RunwayStrip strip;
        // A local east-north frame at the runway's middle, on the ellipsoid.
        double origin[3];
        double east[3];
        double north[3];
        // The ends in it, metres; the unit vector from le to he; the length
        // and half the width.
        double le_x, le_y;
        double along_x, along_y;
        double length_m;
        double half_width_m;
        // Its ties, by the other runway and then along it.
        std::vector<Tie> ties;
        // The group of runways tied to it and to each other, in ascending
        // order: itself alone if it is tied to none.
        std::vector<std::uint32_t> group;
    };

    std::size_t size() const {
        return runways_.size();
    }
    const Runway& at(std::size_t i) const {
        return runways_[i];
    }

    // The runways whose rectangle or shoulder may reach the place, in
    // ascending order: a superset, which `place` then decides.
    std::span<const std::uint32_t> reaching(double latitude_deg, double longitude_deg) const;

    // Where the place is relative to runway `i`: how far along it from the le
    // end, as a fraction clamped to 0..1, and how much the runway's surface
    // weighs there - 1 on the rectangle, falling to 0 over the shoulder.
    struct Placed {
        double t = 0.0;
        double weight = 0.0;
    };
    Placed place(std::size_t i, double latitude_deg, double longitude_deg) const;

private:
    std::vector<Runway> runways_;
    std::unordered_map<std::int64_t, std::vector<std::uint32_t>> cells_;
};

// **The runways, once per process**: `data`/runways/strips.csv read and
// indexed the first time it is asked for, and the same surfaces handed to
// every caller after. Throws RunwayError if it cannot be read.
std::shared_ptr<const RunwaySurfaces> runway_surfaces(const std::filesystem::path& data);

// **The collision ground**: a Dem with every runway made its own surface. Not
// thread-safe, as a Dem is not.
class CollisionGround {
public:
    // `tolerance_m` is how far an end's elevation may be from the fit to the
    // DEM and still be believed: `elevation_tolerance_m`, except in a test
    // that measures the file's elevations as they are.
    CollisionGround(std::shared_ptr<Dem> dem, std::shared_ptr<const RunwaySurfaces> runways,
                    double tolerance_m = elevation_tolerance_m);

    // Above the geoid - sea level - in metres.
    double height_above_geoid(double latitude_deg, double longitude_deg);
    // Above the WGS84 ellipsoid, in metres. Throws DemError without a geoid.
    double height_above_ellipsoid(double latitude_deg, double longitude_deg);
    // The DEM's water body mask, except that a runway is land.
    Water water(double latitude_deg, double longitude_deg);

    // A runway's surface: its height above sea level at each end, and whether
    // its line was through the file's elevations or the fit to the DEM - before
    // any tie moved it.
    struct Surface {
        double le_m = 0.0;
        double he_m = 0.0;
        bool from_file = false;
    };
    const Surface& surface(std::size_t runway);
    // The runway's line before any tie moved it.
    Surface own_line(std::size_t runway);

    const RunwaySurfaces& runways() const {
        return *runways_;
    }
    Dem& dem() {
        return *dem_;
    }

private:
    // The least-squares line through the DEM along its centreline.
    Surface fit_line(std::size_t runway);

    std::shared_ptr<Dem> dem_;
    std::shared_ptr<const RunwaySurfaces> runways_;
    double tolerance_m_;
    std::unordered_map<std::size_t, Surface> surfaces_;
};

} // namespace glideslope::world
