#pragma once

// Air that rises and sinks: thermals, and the ground's own lift - ridge lift
// and mountain waves.
//
// Like gusts and turbulence (world/air_motion.hpp), each is a function of
// place, time and the weather alone, so every machine flies the same air.
//
// **Thermals** are Allen's updraft model (NASA/TM-2006-213477, 2006),
// transcribed from his MATLAB (its appendix B): in a convective layer zi deep,
// with a convective velocity w*, an updraft's mean speed at height z is
// w* (z/zi)^(1/3) (1 - 1.1 z/zi) over an outer radius of
// max(10 m, 0.102 (z/zi)^(1/3) (1 - 0.25 z/zi) zi); its speed across that is
// his bell, fitted to Konovalov's measured updrafts; a ring of sinking air
// rings it in the layer's upper half; and between updrafts the air sinks as
// much as they raise. Where his model places updrafts at random, here one
// stands in each cell of a square grid, its place and strength drawn from the
// seed - his count in an area, 0.6 X Y / (zi r2), taken at z/zi = 0.4 where
// Lenschow measured their spacing, sets the grid's size. Each lives twenty
// minutes, growing over its first three and fading over its last three, and
// the next rises elsewhere in its cell; where two overlap, what each adds to
// the sink between them is added. His mean updraught at the layer's top is
// not nought but -0.1 w*, and above it there are none, so they fade to
// nothing over the layer's top tenth: the air steps nothing there.
//
// **Ridge lift and mountain waves** are linear theory: air of buoyancy
// frequency N crossing terrain h(x) at a speed U is displaced upwards by
//   eta(k, z) = h(k) e^(i m z),   m = sign(k) sqrt(l^2 - k^2),   l = N / U,
// for each wavenumber k along the wind - a wave carrying its energy upwards
// where |k| < l, and dying away with height, e^(-sqrt(k^2 - l^2) z), where it
// is shorter - and the vertical wind is U d(eta)/dx. At the ground it is the
// terrain's own slope - lift on the windward side, sink in the lee - and
// above, waves leaning into the wind. Over a ridge so wide that its waves are
// all much longer than 2 pi / l this is Queney's hydrostatic solution (1948),
// and with no stability, potential flow. The terrain is taken
// along the wind through the aircraft, 256 samples 250 m apart, less their
// median and tapered to it over the outermost 8 km at each end; nothing across
// the wind is seen. Heights are measured from that median, except near the
// ground, where the ground beneath takes its place, fading over 500 m.

#include "world/air_motion.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace glideslope::world {

// A convective layer: its depth above the ground, zi, and its convective
// velocity, w*. No thermals rise in a layer with no velocity.
struct Convection {
    double depth_m = 0.0;
    double velocity_mps = 0.0;
};

// The convective layer over ground `ground_m` above sea level whose air is at
// `surface_temperature_c`, under an environment whose temperature at a height
// above sea level is `environment_c`: a parcel 1 C warmer than the surface
// air, cooling at the dry adiabatic 9.8 C a kilometre, rises to where the
// environment is as warm, up to 4 km. A layer under 300 m has no velocity;
// otherwise it grows with the layer's depth as Deardorff's scale does under a
// steady heating, 2 m/s for 1,500 m - within the spread of Allen's
// measurements (his table 2). In a surface wind over 25 kt there are none, as
// in Allen's.
Convection convection(double surface_temperature_c, double ground_m,
                      double surface_wind_mps,
                      const std::function<double(double height_msl_m)>& environment_c);

// Allen's mean updraft (his equation 11) and outer radius (12) at a height
// above the ground.
double allen_mean_updraught(const Convection& convection, double height_m);
double allen_radius(const Convection& convection, double height_m);

// The side of the grid's cells, one thermal to each.
double thermal_spacing_m(const Convection& convection);

// The sink between thermals at a height: Allen's (his equation 21), for one
// thermal to each cell, as many alive on average as their lives allow.
double environment_sink(const Convection& convection, double height_m);

// One updraft's vertical wind `distance_m` from its centre, `height_m` above
// the ground, `gain` times Allen's mean strength, in air sinking at `sink_mps`
// between updrafts: his run_model2_3 for that updraft alone.
double allen_updraught(const Convection& convection, double height_m, double distance_m,
                       double gain, double sink_mps);

// The thermal in the cell holding a point - metres east and north of the
// pattern's origin - at `time_s`: where it stands, its strength as a multiple
// of Allen's, and how alive it is, 0 not at all to 1 in its prime.
struct Thermal {
    double east = 0.0;
    double north = 0.0;
    double gain = 0.0;
    double life = 0.0;
};
Thermal thermal_in_cell(std::uint64_t seed, const Convection& convection, double east,
                        double north, double time_s);

// The thermals' vertical wind at `position` - metres east and north of the
// pattern's origin, the wind's carrying already taken off, and up from the
// ground - at `time_s`: the sink between them, and what each thermal near adds
// to it as alive as it is.
double thermal_updraught(std::uint64_t seed, const Convection& convection,
                         const Enu& position, double time_s);

// The terrain along the wind: the ground's height above sea level `along_m`
// metres downwind of the aircraft (upwind negative).
using TerrainAlong = std::function<double(double along_m)>;

inline constexpr int terrain_samples = 256;
inline constexpr double terrain_sample_spacing_m = 250.0;

// The vertical wind the terrain makes `height_msl_m` above sea level, in a
// wind of `speed_mps` across it - none under 1 m/s - and air of buoyancy
// frequency `buoyancy_frequency`, in radians a second; 0 for none, when every
// disturbance dies away with height.
double terrain_updraught(const TerrainAlong& terrain, double height_msl_m,
                         double speed_mps, double buoyancy_frequency);

// The terrain's transform: its samples, and as many again of level ground
// beyond them (lift.cpp says why).
inline constexpr int terrain_transform_size = 2 * terrain_samples;

// **The terrain along the wind through a place, transformed**: the median of
// its samples, and the transform of the samples less it, tapered, for each
// wavenumber below the highest - what terrain_updraught makes of them before
// it looks at the height. Index 0, the mean, is nought.
struct TerrainSpectrum {
    double reference_m = 0.0;
    std::array<double, terrain_transform_size / 2> re{};
    std::array<double, terrain_transform_size / 2> im{};
};
TerrainSpectrum terrain_spectrum(const TerrainAlong& terrain);

// terrain_updraught from the terrain's transform and the ground beneath:
// the same to the last bit, given terrain_spectrum(terrain) and terrain(0).
double terrain_updraught(const TerrainSpectrum& spectrum, double beneath_m,
                         double height_msl_m, double speed_mps, double buoyancy_frequency);

// **How far apart the places are whose terrain is transformed**, degrees of
// latitude and of longitude: 1/480, 232 m north and south, and less east
// and west by the cosine of the latitude. The lift's shortest waves are
// twice the samples' spacing, 500 m.
inline constexpr double ground_lift_lattice_deg = 1.0 / 480.0;

// **Ground not had yet**: what a ground that never waits throws for a place
// whose ground it has asked for and has not got - a DEM tile being fetched.
struct GroundNotHad : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// **The ground's lift, cheaply enough to be flown again**: terrain_updraught
// over a lattice of places fixed on the Earth, each place's terrain along the
// wind transformed once and kept, and the transforms of the four about a
// place blended by how near it is to each - linearly, as is the vertical wind
// they make - over the ground beneath it. A place's transform is made the
// first time it is wanted, 256 heights of the ground and a transform; after
// that the lift is one sum over the wavenumbers. **Pure in place**: what was
// made before, and in what order, changes nothing - a client flying a step
// again meets what the server met there. Made for one thread.
//
// **A place whose ground is not had** - the ground throws a
// std::runtime_error: GroundNotHad, a DEM tile not yet fetched, or a fetch
// that failed - lifts nothing, nor does anywhere it is one of the four
// about, and **is not kept**: it is made when it is next wanted, and once its
// ground is had it lifts as it would have. So the step never waits on the
// network where the ground does not (GroundNotHad), and two machines differ
// only while one has a tile the other has not yet: by the lift there, no
// more, until it arrives. The ground beneath not had, nothing lifts.
class GroundLift {
public:
    using Ground = std::function<double(double latitude_deg, double longitude_deg)>;
    explicit GroundLift(Ground ground) : ground_(std::move(ground)) {}

    // The vertical wind the terrain makes at a place, `height_msl_m` above
    // the sea, in a wind blowing towards north and east at those speeds, in
    // air of `buoyancy_frequency`; none in a wind under 1 m/s.
    double updraught(double latitude_deg, double longitude_deg, double height_msl_m,
                     double wind_north_mps, double wind_east_mps, double buoyancy_frequency);

    // The ground beneath a place, as it was given.
    double ground(double latitude_deg, double longitude_deg) const {
        return ground_(latitude_deg, longitude_deg);
    }

    // How many places' transforms have been made, all told: what the lift
    // has cost beyond its sums.
    std::size_t transforms_made() const { return made_; }
    // How many times a place's ground was not had, all told.
    std::size_t not_had() const { return not_had_; }

    // **The most transforms kept**, 2,048 of 4 kB: past that all are let go
    // and made again as they are wanted.
    static constexpr std::size_t most_kept = 2048;

private:
    struct Place {
        TerrainSpectrum spectrum;
    };
    // The place's transform, or null when its ground is not had.
    const Place* place(std::size_t direction, long row, long column);

    Ground ground_;
    // Each wind's direction, north and east, and its places by row and column.
    struct Direction {
        double north = 0.0;
        double east = 0.0;
        std::map<std::pair<long, long>, Place> places;
    };
    std::vector<Direction> directions_;
    std::size_t kept_ = 0;
    std::size_t made_ = 0;
    std::size_t not_had_ = 0;
};

} // namespace glideslope::world
