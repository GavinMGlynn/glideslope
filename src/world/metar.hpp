#pragma once

// METARs: routine aviation weather reports, as aviationweather.gov gives them.
//
// Read here: the station, the time, the surface wind (direction, speed, gusts,
// variability, in knots, metres a second or kilometres an hour), the
// temperature and dew point - to a tenth of a degree from the North American
// T remark when there is one - and the pressure, as QNH in hectopascals or an
// altimeter setting in inches of mercury; wind shear reported on a runway or
// all of them (WS R27, WS RWY27, WS TKOF RWY27, WS LDG RWY27, WS ALL RWY); and
// from the remarks the peak wind (PK WND) and a wind shift (WSHFT). The rest -
// visibility, cloud, weather, trends - is not read yet. Groups after a trend
// (BECMG, TEMPO, NOSIG) are forecasts, not observations, and are not read as
// either.

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::world {

struct MetarError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Metar {
    std::string station;
    int day = 0;
    int hour = 0;
    int minute = 0;

    // The direction the wind blows from, degrees true; absent when the report
    // says it varies (VRB) or does not say.
    std::optional<double> wind_from_deg;
    std::optional<double> wind_speed_kt; // absent when not reported
    std::optional<double> gust_kt;
    // The range the direction varies over, when reported as dddVddd.
    std::optional<double> wind_varies_from_deg;
    std::optional<double> wind_varies_to_deg;

    std::optional<double> temperature_c;
    std::optional<double> dewpoint_c;
    std::optional<double> qnh_hpa;

    // Wind shear reported on a runway's approach or climb-out: the runways
    // named - "02", "34R" - or all of them.
    std::vector<std::string> wind_shear_runways;
    bool wind_shear_all_runways = false;

    // The strongest wind since the last routine report (PK WND dddff/hhmm),
    // and when; the hour is the report's when only minutes are given.
    struct PeakWind {
        double from_deg = 0.0;
        double speed_kt = 0.0;
        int hour = 0;
        int minute = 0;
    };
    std::optional<PeakWind> peak_wind;

    // A shift in the wind's direction (WSHFT hhmm), and whether it came with a
    // front (FROPA).
    struct WindShift {
        int hour = 0;
        int minute = 0;
        bool frontal = false;
    };
    std::optional<WindShift> wind_shift;
};

// Throws MetarError if there is no station and time.
Metar parse_metar(std::string_view report);

} // namespace glideslope::world
