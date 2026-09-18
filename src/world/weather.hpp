#pragma once

// Weather from reports: a METAR at the surface, and Open-Meteo's winds aloft
// above it.

#include "sim/weather.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"
#include "world/metar.hpp"
#include "world/winds_aloft.hpp"

#include <cstdint>
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
// station that is not one or has no METAR, DemError - the download error - if
// none can be had, or the parse errors above.
SurfaceReport fetch_metar(const std::string& station, const Fetch& fetch);

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

// The air's motion a report describes at a place and time beyond its mean
// wind - gusts and turbulence (world/air_motion.hpp) - added to `mean`, the
// conditions there. The gusts are the METAR's spread over its mean wind, along
// its wind, in full up to 10 m above the station and fading to none 600 m
// above that. The turbulence is Dryden's at its severity, over the height above
// the station. Both patterns are carried by the surface wind, from the station.
// JSBSim's own turbulence is left off: the air is all here.
sim::Conditions with_air_motion(const WeatherReport& report, sim::Conditions mean,
                                double latitude_deg, double longitude_deg,
                                double height_msl_m, double time_s);

// A station's weather now: its latest METAR, and Open-Meteo's winds aloft over
// it for the hour `time` ("YYYY-MM-DDTHH:00", UTC). Throws as fetch_metar and
// fetch_winds_aloft do. Open-Meteo's data must be credited wherever it is shown:
// open_meteo_credit.
WeatherReport fetch_weather(const std::string& station, const std::string& time,
                            const Fetch& fetch);

// What CC BY 4.0 and Open-Meteo's terms ask to be shown beside its data.
inline constexpr const char* open_meteo_credit = "Weather data by Open-Meteo.com";

// A flight's weather: its report, and each new one blended in over an interval
// rather than stepped to - every value moving linearly from what the old report
// gave to what the new one gives, and the turbulence changing halfway.
class ReportedWeather : public sim::Weather {
public:
    // `geoid` converts the aircraft's height above the ellipsoid to height above
    // mean sea level; without one they are taken as the same.
    ReportedWeather(WeatherReport report, const Geoid* geoid, double blend_seconds);

    // A new report, blending in from simulation time `now_s`.
    void update(WeatherReport report, double now_s);

    double blend_seconds() const {
        return blend_seconds_;
    }

    sim::Conditions at(double latitude_deg, double longitude_deg, double height_m,
                       double time_s) override;

private:
    std::optional<WeatherReport> previous_;
    WeatherReport current_;
    double changed_at_s_ = 0.0;
    const Geoid* geoid_;
    double blend_seconds_;
};

} // namespace glideslope::world
