#pragma once

// Every runway in the world, from OurAirports' `runways.csv` at a pinned
// commit (docs/ASSETS.md): what a plan that takes off takes off from, so that
// where a runway is comes from data and never from a language model's memory.
//
// **A runway has two ends**, each a way to take off: the file's `le_` end and
// its `he_` end, each with its own threshold, elevation and true heading. An
// end is kept only where the file gives its position and heading, the runway
// is open, and it is not a helipad; its elevation may be missing, and the
// ground's height there is the DEM's in any case.

#include "sim/plan.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::world {

struct RunwayError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct RunwayEnd {
    std::string airport; // its ICAO or local ident, "YSSY"
    std::string ident;   // the end's, "34L"
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double elevation_ft = std::numeric_limits<double>::quiet_NaN(); // NaN: not given
    double heading_deg = 0.0; // true
    double length_m = 0.0;
    // **How far along it the landing threshold is displaced**, metres, from
    // the file's `le_`/`he_displaced_threshold_ft`; 0 where none is given.
    // The position above is the runway's end, not the displaced threshold.
    double displaced_m = 0.0;
    std::string surface; // as the file has it: "ASP", "GRVL", ...
};

// The runway ends in `csv`, a `runways.csv`. Throws RunwayError for a file
// that is not one: no header, or a header without the columns read.
std::vector<RunwayEnd> read_runways(std::string_view csv);

// **The length an aircraft landing on `end` has to land on**, metres: the
// runway's length less its displaced threshold.
inline double landing_length_m(const RunwayEnd& end) {
    return end.length_m - end.displaced_m;
}

// The ends at `airport`, in the file's order; none if it has none.
std::vector<RunwayEnd> runways_at(const std::vector<RunwayEnd>& all, std::string_view airport);

// As the take-off and landing autopilots have a runway: the end's threshold,
// its heading and length, and `elevation_ft`, the ground's height there.
sim::Runway as_runway(const RunwayEnd& end, double elevation_ft);

} // namespace glideslope::world
