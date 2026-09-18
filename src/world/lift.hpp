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
// the sink between them is added.
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

#include <cstdint>
#include <functional>

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

} // namespace glideslope::world
