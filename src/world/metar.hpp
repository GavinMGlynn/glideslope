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
// the sky obscured; SKC, CLR, NSC and NCD, none); recent weather (RERA,
// RESHSN); and the state of the runway (R24/290195, 24290195, R/SNOCLO,
// R24L/CLRD//). Not read: runway visual ranges, a minimum visibility in one
// direction, and trends - groups after BECMG, TEMPO or NOSIG are forecasts, not
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
    // **The report as it was read**, its words joined by single spaces: what
    // a server sends its clients, who read it with these same rules
    // (net::Weather), rather than twenty fields re-encoded.
    std::string raw;
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
    // **Recent weather** (REw'w', WMO-No. 306 regulation 15.13.2): weather
    // seen since the last routine report or in the last hour, whichever is
    // shorter, but not now - "RERA", rain that has stopped. No intensity.
    std::vector<PresentWeather> recent_weather;

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

    // **The state of a runway**, as an airport's authority reported it
    // (WMO-No. 306, FM 15 regulation 15.13.6; ICAO Annex 3 until
    // 3 November 2021, and some States still): RDRDR/ERCReReRBRBR, or the
    // older eight figures DRDRERCReReRBRBR, in which 50 is added to a right
    // parallel's number. Each code is as reported, and absent where the
    // report has "/" for it.
    struct RunwayState {
        // The runway named, "24" or "24R"; empty for all of them - 88, 99
        // (the last report repeated, its runway not said) and SNOCLO.
        std::string runway;
        bool all_runways = false;
        bool repeated = false;    // designator 99
        bool snow_closed = false; // SNOCLO: the aerodrome closed by snow
        bool cleared = false;     // CLRD: what lay on it has gone
        std::optional<int> deposit;  // code table 0919, 0 to 9
        std::optional<int> extent;   // code table 0519: 1, 2, 5 or 9
        std::optional<int> depth;    // code table 1079: 0 to 90, 92 to 99
        std::optional<int> friction; // code table 0366: 0 to 95, 99
    };
    std::vector<RunwayState> runway_states;
    // **Each group read as a runway's state and not used**, with why: a
    // reserved code, a runway that cannot be one, a character no table has.
    std::vector<std::string> runway_states_refused;
};

// **How deep a deposit lies**, millimetres, from code table 1079: 00 less
// than 1 mm (0), 01 to 90 that many, 92 to 98 10 to 40 cm. Absent for 99 (the
// runway not operational, its depth not reported) and any other code.
std::optional<double> deposit_depth_mm(int depth_code);

// Throws MetarError if there is no station and time.
Metar parse_metar(std::string_view report);

} // namespace glideslope::world
