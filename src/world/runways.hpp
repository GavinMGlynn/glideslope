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
#include <optional>
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

// **Whether a place is on a runway**: along it from `end`'s threshold no
// farther than its length, and within 30 m of its centreline - half the
// widest runway ICAO Annex 14 asks for (60 m, code letter F), so that all of
// any runway is on it, and no parallel 120 m or more away is.
bool on_runway(const RunwayEnd& end, double latitude_deg, double longitude_deg);

// **A runway a reported state has laid a condition on**: the end it is
// measured from (its threshold, heading and length) and the code, the FAA's
// runway condition code (sim/runway_condition.hpp). What a server sends its
// clients (net::RunwayConditions), so that a client flies the server's
// runways without fetching any.
struct ContaminatedRunway {
    RunwayEnd end;
    int code = 6;
};

// **Where the runways are wet, and which are reported contaminated**: wet
// within `radius_m` of a place - a METAR's station, whose present weather is
// what is at the aerodrome - and nowhere where the radius is 0; and the
// runways a METAR's runway-state groups lay a condition on, wherever they
// are (frontend::wet_runways). Every other runway is dry.
struct WetRunways {
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double radius_m = 0.0;
    std::vector<ContaminatedRunway> contaminated;
};

// **The condition reported on the runway `end` is**: the most slippery code
// of `reported` whose runway the middle of `end`'s lies on (on_runway) -
// from either of its ends - or none where none is reported.
std::optional<int> reported_condition(const std::vector<ContaminatedRunway>& reported,
                                      const RunwayEnd& end);

// **AC 91-79B's landing distance factors** (FAA, 28 August 2023, table
// 3-1): what an aeroplane's unfactored dry landing distance from its flight
// manual is multiplied by for the runway condition code at the time of
// arrival, where its maker publishes nothing for that runway. Each includes
// a 15 per cent margin, an operational air distance and the runway's effect
// on the wheels. Two rows are taken: "Turbojet, No Reverse" - the same as
// the "Reciprocating" row, figure for figure - for every aeroplane that
// lands without reverse thrust, and "Turbojet, With Reverse" for one that
// reverses on a runway that is not dry (sim::Lander, the catalogue's
// `reversers`). No aircraft here takes the turboprop row.
//   code            6     5 grooved  5 smooth  4     3     2     1
//   no reverse      1.67  2.3        2.6       2.8   3.2   4.0   5.1
//   with reverse    1.67  1.92       2.2       2.3   2.5   2.9   3.4
// Code 5's is the smooth runway's, since nothing here says a runway is
// grooved. **Code 0, nil braking, has none**: "a report of nil braking
// action on a runway requires the closure of that runway" (4.8.1.4), so no
// landing is sized for it - none, as for any code not 0 to 6.
std::optional<double> landing_distance_factor(int code, bool reverse_thrust);

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
