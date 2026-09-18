#pragma once

// METARs: routine aviation weather reports, as aviationweather.gov gives them.
//
// Read here: the station, the time, the surface wind (direction, speed, gusts,
// variability, in knots, metres a second or kilometres an hour), the
// temperature and dew point - to a tenth of a degree from the North American
// T remark when there is one - and the pressure, as QNH in hectopascals or an
// altimeter setting in inches of mercury; wind shear reported on a runway or
// all of them (WS R27, WS RWY27, WS TKOF RWY27, WS LDG RWY27, WS ALL RWY); and
// from the remarks the peak wind (PK WND) and a wind shift (WSHFT); the
// prevailing visibility, in metres or statute miles (9999, 0800, CAVOK, 10SM,
// 1 1/2SM, M1/4SM, P6SM); the weather present (-RA, +TSRA, VCSH, FZFG, BR);
// and the cloud (FEW, SCT, BKN and OVC with their heights and CB or TCU; VV,
// the sky obscured; SKC, CLR, NSC and NCD, none). Not read: runway visual
// ranges, a minimum visibility in one direction, recent weather (RE), and
// trends - groups after BECMG, TEMPO or NOSIG are forecasts, not
// observations, and are not read as either.

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

    // The prevailing visibility, metres. 9999 and CAVOK are 10 km or more, and
    // P6SM more than 6 statute miles: `visibility_or_more`; M1/4SM less than a
    // quarter of one: `visibility_or_less`.
    std::optional<double> visibility_m;
    bool visibility_or_more = false;
    bool visibility_or_less = false;
    // Ceiling and visibility OK: 10 km or more, no cloud below 5,000 ft or the
    // highest minimum sector altitude, no cumulonimbus, no weather.
    bool cavok = false;

    // The weather present: one group each, "+TSRA" heavy thunderstorm with
    // rain, "VCSH" showers in the vicinity.
    struct PresentWeather {
        int intensity = 0; // -1 light, 0 moderate, 1 heavy
        bool vicinity = false;
        std::string descriptor;             // MI, PR, BC, DR, BL, SH, TS, FZ, or none
        std::vector<std::string> phenomena; // RA, SN, FG, BR, ...
    };
    std::vector<PresentWeather> weather;

    // The cloud, as reported, lowest first: its cover, in eighths - FEW 1 to
    // 2, SCT 3 to 4, BKN 5 to 7, OVC 8 - and its base above the station. VV is
    // the sky obscured, the base the vertical visibility into it.
    struct CloudLayer {
        enum class Cover { few, scattered, broken, overcast, obscured };
        Cover cover = Cover::few;
        double base_ft = 0.0;
        bool cumulonimbus = false;
        bool towering_cumulus = false;
    };
    std::vector<CloudLayer> clouds;
    // SKC, CLR, NSC or NCD: no cloud, or none that matters.
    bool no_cloud = false;

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
