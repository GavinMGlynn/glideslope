#pragma once

// Weather from reports: a METAR at the surface, and Open-Meteo's winds aloft
// above it.

#include "sim/weather.hpp"
#include "world/air_motion.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"
#include "world/lift.hpp"
#include "world/metar.hpp"
#include "world/winds_aloft.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::world {

// A METAR with where its station is.
struct SurfaceReport {
    Metar metar;
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double elevation_m = 0.0;
};

// aviationweather.gov's JSON METAR response: every report in it, read from its
// raw text, with its station's position. Throws JsonError or MetarError.
std::vector<SurfaceReport> parse_aviationweather(std::string_view json);

// The latest METAR for a station - an ICAO location indicator, four letters or
// digits, in either case - from aviationweather.gov. Throws MetarError for a
// station that is not one or has no METAR, ServiceUnavailable if nothing
// answered or a server error was all it answered, DemError for any other
// answer that is not a report, or the parse errors above. `retry_wait` is
// fetch_with_retries' wait; a test shortens it.
SurfaceReport fetch_metar(const std::string& station, const Fetch& fetch,
                          std::chrono::milliseconds retry_wait = std::chrono::milliseconds(2000));

// The International Standard Atmosphere's temperature at a height, degrees C.
double isa_temperature_c(double height_m);

// The conditions a report describes, at its station: the wind it reports, the
// temperature offset from the standard atmosphere that makes the air at the
// station's elevation the temperature it reports, and its QNH. A variable or
// unreported wind is calm; an unreported temperature or pressure standard.
sim::Conditions surface_conditions(const SurfaceReport& report);

// What a flight's weather is made of.
struct WeatherReport {
    SurfaceReport surface;
    std::optional<WindsAloft> aloft;
    // The turbulence's severity, 0 none to 7 (world/air_motion.hpp), or, when
    // not given, what the METAR's gusts imply.
    std::optional<int> turbulence_severity;
    // What the gusts' and turbulence's pattern is drawn from: the same report
    // and seed give the same air everywhere (world/air_motion.hpp).
    std::uint64_t air_seed = 0;
    // Microbursts, placed by whoever sets the weather: the server, a lesson, a
    // test.
    std::vector<Microburst> microbursts;
};

// The seed a station's report at a time gives its air: from the station and
// the observation's day, hour and minute, so every machine that has the report
// has the seed.
std::uint64_t air_seed_of(const Metar& metar);

// The conditions a report describes at a height above mean sea level.
//
// **The wind as a boundary layer.** 10 m above the station - where a METAR's
// wind is measured - the METAR's; below that, falling logarithmically to
// nothing at a roughness length of 3 cm, open country's; above it, through the
// forecast's winds 80, 120 and 180 m above the ground where it has them,
// logarithmically in height between each; and from the highest of those to the
// forecast's lowest pressure level above it, linearly, and its levels above.
// The forecast's own 10 m wind is not used: the METAR measured it.
//
// The temperature is the METAR's up to 10 m, the forecast's from its lowest
// level above that, and linear in height between, as an offset from the
// standard atmosphere. The pressure is the METAR's everywhere.
sim::Conditions conditions_at(const WeatherReport& report, double height_msl_m);

// What a report's rising and sinking air is made in, worked out from it once:
// the convective layer over its station (world/lift.hpp), from its surface
// temperature and wind and the forecast's temperatures above - none without a
// forecast, whose temperatures alone can say whether there is one; the wind that
// carries the thermals, the layer's halfway up; and the wind and the air's
// buoyancy frequency the terrain's waves are made in - the wind 1,000 m above
// the station, and the frequency between 1,000 and 4,000 m above it, from the
// temperatures' lapse there against the dry adiabatic.
struct Lift {
    Convection convection;
    double drift_north_mps = 0.0;
    double drift_east_mps = 0.0;
    double wind_north_mps = 0.0;
    double wind_east_mps = 0.0;
    double buoyancy_frequency = 0.0; // radians a second
};
Lift lift_of(const WeatherReport& report);

// The ground's height above sea level at a place, for the air over it.
using GroundAt = std::function<double(double latitude_deg, double longitude_deg)>;

// The air's motion a report describes at a place and time beyond its mean
// wind - reported wind shear, microbursts, gusts and turbulence
// (world/air_motion.hpp), thermals and the terrain's lift (world/lift.hpp) -
// added to `mean`, the conditions there. A report of wind shear makes, within
// 8 km of the station, a 15 kt headwind on the named runway's approach - or
// along the surface wind, for all runways - between 60 and 600 m above the
// ground, rising from none at 60 m to all of it at 300 m. The gusts are the
// METAR's spread over its mean wind, along its wind, in full up to 10 m above
// the station and fading to none 600 m above that. The turbulence is Dryden's
// at its severity, over the height above the station. Both patterns are
// carried by the surface wind, from the station. The thermals are `lift`'s,
// over the ground beneath - or the station's elevation, without `ground` -
// carried by its drift. The terrain's lift is made from `ground`, and without
// it there is none. JSBSim's own turbulence is left off: the air is all here.
sim::Conditions with_air_motion(const WeatherReport& report, const Lift& lift,
                                const GroundAt& ground, sim::Conditions mean,
                                double latitude_deg, double longitude_deg,
                                double height_msl_m, double time_s);

// A station's weather now: its latest METAR, and Open-Meteo's winds aloft over
// it for the hour `time` ("YYYY-MM-DDTHH:00", UTC). Throws as fetch_metar and
// fetch_winds_aloft do, with a service that did not answer - and only that -
// as ServiceUnavailable beginning "the weather could not be had: ".
// Open-Meteo's data must be credited wherever it is shown: open_meteo_credit.
WeatherReport fetch_weather(const std::string& station, const std::string& time,
                            const Fetch& fetch,
                            std::chrono::milliseconds retry_wait = std::chrono::milliseconds(2000));

// What CC BY 4.0 and Open-Meteo's terms ask to be shown beside its data.
inline constexpr const char* open_meteo_credit = "Weather data by Open-Meteo.com";

// A flight's weather: its report, and each new one blended in over an interval
// rather than stepped to - every value moving linearly from what the old report
// gave to what the new one gives, and the turbulence changing halfway.
class ReportedWeather : public sim::Weather {
public:
    // `geoid` converts the aircraft's height above the ellipsoid to height above
    // mean sea level; without one they are taken as the same. `ground` gives
    // the terrain the air rises and sinks over; without it, there is none.
    ReportedWeather(WeatherReport report, const Geoid* geoid, double blend_seconds,
                    GroundAt ground = {});

    // A new report, blending in from simulation time `now_s`.
    void update(WeatherReport report, double now_s);

    double blend_seconds() const {
        return blend_seconds_;
    }

    // The report now: the latest, even while it is blending in.
    const WeatherReport& report() const {
        return current_;
    }

    sim::Conditions at(double latitude_deg, double longitude_deg, double height_m,
                       double time_s) override;

private:
    std::optional<WeatherReport> previous_;
    WeatherReport current_;
    Lift previous_lift_;
    Lift current_lift_;
    GroundAt ground_;
    double changed_at_s_ = 0.0;
    const Geoid* geoid_;
    double blend_seconds_;
};

} // namespace glideslope::world
