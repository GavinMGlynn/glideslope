#pragma once

// The air's motion beyond the mean wind: gusts and turbulence.
//
// **A pattern the wind carries, and a function of place and time alone.** The
// server's weather is authoritative, and every client's prediction must fly the
// same air as the server (REQUIREMENTS.md, section 6.2); an aircraft restored to
// a state must meet the same air again. So gusts and turbulence are not random
// processes stepped through time, with state a restore cannot carry. They are a
// fixed pattern in space, drawn from a seed the weather carries, that the mean
// wind carries along - Taylor's frozen turbulence: the air at a place and time
// is the pattern at that place less the distance the wind has carried it since.
//
// **Gusts** are the wind along its own direction rising from a report's mean
// speed towards its gust speed and falling back, as smooth noise over periods of
// four to sixteen seconds.
//
// **Turbulence** is MIL-F-8785C's Dryden model - its length scales and
// intensities by height above the ground, and by severity above 2,000 ft -
// made as a sum of cosine waves, every wave with its own direction, wavelength
// and phase from the seed, their amplitudes from Dryden's spectra. At every
// point each component's RMS is Dryden's intensity; along any line its
// correlation falls to 1/e at Dryden's length, on average. The spectrum's
// shape along a line is not Dryden's exactly: the waves run in every
// direction, where Dryden's run along the flight path.

#include <cstdint>

namespace glideslope::world {

// Metres, or metres a second, east, north and up.
struct Enu {
    double east = 0.0;
    double north = 0.0;
    double up = 0.0;
};

// A number in [0, 1) from a seed and two indices, the same everywhere
// (SplitMix64).
double seeded_uniform(std::uint64_t seed, std::uint64_t a, std::uint64_t b);

// How far towards its gust the wind is at `pattern_s` seconds along the gust
// pattern: 0 the mean wind, 1 the gust.
double gust_factor(std::uint64_t seed, double pattern_s);

// MIL-F-8785C's turbulence at a height above the ground.
struct DrydenScales {
    double length_u_m = 0.0;  // along and across the wind
    double length_w_m = 0.0;  // vertical
    double sigma_u_mps = 0.0; // RMS velocity along and across the wind
    double sigma_w_mps = 0.0; // vertical
};

// Below 1,000 ft its intensity is a tenth of the wind 20 ft up and its lengths
// grow with height; above 2,000 ft both come from `severity`, its curve of the
// probability of exceedance, 1 (light) to 7 (extreme), and 0 is none;
// between, the two are blended.
DrydenScales dryden_scales(int severity, double height_above_ground_m,
                           double wind_at_20ft_mps);

// The turbulent velocity at `position` in the pattern - metres from its origin,
// the wind's carrying already taken off - with the wind blowing towards
// `along`, a unit vector (north if there is no wind).
Enu turbulence(std::uint64_t seed, const DrydenScales& scales, const Enu& position,
               const Enu& along);

// The severity a report's gusts imply: their spread over the mean wind, in
// knots. Under 5 none; then light (1) from 5, 2 from 10, moderate (3) from 15,
// 4 from 20 and severe (5) from 30.
int severity_from_gust_spread(double spread_kt);

} // namespace glideslope::world
