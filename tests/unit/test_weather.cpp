#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/test_pilot.hpp"
#include "sim/weather.hpp"
#include "world/json.hpp"
#include "world/metar.hpp"
#include "world/weather.hpp"
#include "world/winds_aloft.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::Metar;
using glideslope::world::MetarError;
using glideslope::world::parse_metar;

namespace {

std::string recorded_metars() {
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                         "data/weather/aviationweather-metars-2026-09-17T1600Z.json",
                     std::ios::binary);
    check(static_cast<bool>(in), "can read the recorded METARs");
    return std::string(std::istreambuf_iterator<char>(in), {});
}

bool near(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}

} // namespace

GLIDESLOPE_TEST(every_recorded_metar_is_read_as_aviationweather_decodes_it) {
    // aviationweather.gov sends its own decoding of each report beside the raw
    // text: an independent reading to hold this one to.
    const std::string text = recorded_metars();
    const auto doc = glideslope::world::parse_json(text);
    const auto reports = glideslope::world::parse_aviationweather(text);
    check(reports.size() == 10 && doc.array().size() == 10, "ten reports");
    for (std::size_t i = 0; i < reports.size(); ++i) {
        const Metar& m = reports[i].metar;
        const auto& theirs = doc.array()[i];
        const std::string name = m.station;
        check(m.station == theirs.at("icaoId").string(), name + ": the station");
        check(m.temperature_c &&
                  near(*m.temperature_c, theirs.at("temp").number(), 0.05),
              name + ": the temperature");
        check(m.dewpoint_c && near(*m.dewpoint_c, theirs.at("dewp").number(), 0.05),
              name + ": the dew point");
        // Their speeds are whole knots, converted from metres a second where
        // the report gives those.
        check(
            m.wind_speed_kt && near(*m.wind_speed_kt, theirs.at("wspd").number(), 0.5),
            name + ": the wind speed, " + std::to_string(m.wind_speed_kt.value_or(-1)));
        const auto& wdir = theirs.at("wdir");
        if (wdir.kind() == glideslope::world::Json::Kind::string) {
            check(!m.wind_from_deg, name + ": a variable wind has no direction");
        } else {
            check(m.wind_from_deg && *m.wind_from_deg == wdir.number(),
                  name + ": the direction");
        }
        // Their altimeter settings are in hectopascals to a tenth, and an
        // A group is in hundredths of an inch - to within 0.17 hPa - so they
        // can differ by a quarter of a hectopascal.
        check(m.qnh_hpa && near(*m.qnh_hpa, theirs.at("altim").number(), 0.25),
              name + ": the pressure, " + std::to_string(m.qnh_hpa.value_or(-1)));
    }
}

GLIDESLOPE_TEST(
    every_recorded_metars_visibility_weather_and_cloud_are_read_as_aviationweather_decodes_them) {
    constexpr double metres_per_mile = 1609.344;
    int count = 0;
    for (const char* file : {"aviationweather-metars-2026-09-17T1600Z.json",
                             "aviationweather-gusts-2026-09-18T0800Z.json",
                             "aviationweather-shear-2026-09-18.json"}) {
        std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                             "data/weather" / file,
                         std::ios::binary);
        check(static_cast<bool>(in), std::string("can read ") + file);
        const std::string text(std::istreambuf_iterator<char>(in), {});
        const auto doc = glideslope::world::parse_json(text);
        const auto reports = glideslope::world::parse_aviationweather(text);
        check(reports.size() == doc.array().size(),
              std::string("every report in ") + file);
        for (std::size_t i = 0; i < reports.size(); ++i) {
            ++count;
            const Metar& m = reports[i].metar;
            const auto& theirs = doc.array()[i];
            const std::string name = m.station;

            // Their visibility is in statute miles, to two decimals, or "6+"
            // and "10+" for at least that many; a report without one has none.
            const auto* visib = theirs.find("visib");
            if (visib == nullptr || visib->is_null()) {
                check(!m.visibility_m, name + ": no visibility reported, none read");
            } else if (visib->kind() == glideslope::world::Json::Kind::string) {
                const std::string v = visib->string();
                const double least = std::stod(v.substr(0, v.size() - 1));
                check(v.back() == '+' && m.visibility_m &&
                          *m.visibility_m >= least * metres_per_mile * 0.999,
                      name + ": a visibility of at least " + v + " miles");
            } else {
                check(m.visibility_m && near(*m.visibility_m / metres_per_mile,
                                             visib->number(), 0.006),
                      name + ": a visibility of " + std::to_string(visib->number()) +
                          " miles, read as " +
                          std::to_string(m.visibility_m.value_or(-1)) + " m");
            }

            // Their weather is the groups, as written, space-separated.
            std::string ours;
            for (const Metar::PresentWeather& w : m.weather) {
                if (!ours.empty()) {
                    ours += ' ';
                }
                ours += w.vicinity        ? "VC"
                        : w.intensity < 0 ? "-"
                        : w.intensity > 0 ? "+"
                                          : "";
                ours += w.descriptor;
                for (const std::string& p : w.phenomena) {
                    ours += p;
                }
            }
            const auto* wx = theirs.find("wxString");
            const std::string their_wx =
                wx != nullptr && !wx->is_null() ? wx->string() : "";
            check(ours == their_wx, name + ": the weather present, \"" + ours +
                                        "\" against \"" + their_wx + "\"");

            // Their cloud: each layer's cover and base, VV as OVX.
            const auto& clouds = theirs.at("clouds").array();
            check(m.clouds.size() == clouds.size(), name + ": every cloud layer");
            for (std::size_t c = 0; c < clouds.size() && c < m.clouds.size(); ++c) {
                using Cover = Metar::CloudLayer::Cover;
                const Cover cover = m.clouds[c].cover;
                const std::string code = cover == Cover::few         ? "FEW"
                                         : cover == Cover::scattered ? "SCT"
                                         : cover == Cover::broken    ? "BKN"
                                         : cover == Cover::overcast  ? "OVC"
                                                                     : "OVX";
                check(code == clouds[c].at("cover").string() &&
                          m.clouds[c].base_ft == clouds[c].at("base").number(),
                      name + ": cloud layer " + std::to_string(c + 1));
            }
        }
    }
    check(count == 18, "eighteen recorded reports");

    // What their decoding does not hold: cumulonimbus and towering cumulus,
    // CAVOK, and the reports of no cloud.
    const Metar abq = parse_metar("SPECI KABQ 180759Z COR 18027G37KT 3SM VCTS +RA BR "
                                  "SCT036CB BKN046 OVC100 18/16 "
                                  "A3039");
    check(abq.clouds.at(0).cumulonimbus && !abq.clouds.at(1).cumulonimbus,
          "KABQ's scattered layer at 3,600 ft is cumulonimbus");
    check(parse_metar("METAR XXXX 181200Z 27010KT 9999 FEW030TCU 20/10 Q1010")
              .clouds.at(0)
              .towering_cumulus,
          "towering cumulus");
    const Metar uuee = parse_metar(
        "METAR UUEE 171600Z 19004MPS CAVOK 18/09 Q1012 R24L/CLRD62 R24C/CLRD62 NOSIG");
    check(uuee.cavok && uuee.visibility_or_more && uuee.clouds.empty() &&
              uuee.weather.empty(),
          "CAVOK: 10 km or more, no cloud, no weather");
    check(parse_metar("METAR YSSY 171600Z AUTO 35005KT 9999 // NCD 11/07 Q1028")
                  .no_cloud &&
              parse_metar("METAR EGLL 171550Z AUTO 29014KT 9999 NCD 20/07 Q1010")
                  .no_cloud,
          "NCD: no cloud detected");

    // Statute miles as a whole number and a fraction, less than, and more than:
    // forms the recordings do not hold.
    const Metar mixed =
        parse_metar("METAR KXYZ 181200Z 00000KT 1 1/2SM BR OVC004 10/09 A2992");
    check(mixed.visibility_m &&
              near(*mixed.visibility_m, 1.5 * metres_per_mile, 1e-9) &&
              mixed.weather.size() == 1,
          "1 1/2SM is a mile and a half");
    const Metar low =
        parse_metar("METAR KXYZ 181200Z 00000KT M1/4SM FG VV002 10/10 A2992");
    check(low.visibility_or_less &&
              near(*low.visibility_m, 0.25 * metres_per_mile, 1e-9),
          "M1/4SM is less than a quarter of a mile");
    const Metar high = parse_metar("METAR KXYZ 181200Z 00000KT P6SM SKC 10/00 A2992");
    check(high.visibility_or_more &&
              near(*high.visibility_m, 6.0 * metres_per_mile, 1e-9) && high.no_cloud,
          "P6SM is more than six miles");
    // A trend's cloud is forecast, not observed.
    check(
        parse_metar("METAR RJAA 180630Z 03018KT 9999 BKN015 22/18 Q1021 WS R34R TEMPO "
                    "BKN014")
                .clouds.size() == 1,
        "a trend's cloud is not read as observed");
}

GLIDESLOPE_TEST(
    metar_groups_are_read_in_every_form_and_trends_are_not_read_as_observations) {
    Metar m = parse_metar(
        "SPECI KXYZ 051230Z COR 24015G28KT 200V280 3SM M05/M12 A2992 RMK AO2");
    check(m.station == "KXYZ" && m.day == 5 && m.hour == 12 && m.minute == 30,
          "station and time");
    check(*m.wind_from_deg == 240.0 && *m.wind_speed_kt == 15.0 && *m.gust_kt == 28.0,
          "wind with gusts");
    check(*m.wind_varies_from_deg == 200.0 && *m.wind_varies_to_deg == 280.0,
          "variability");
    check(*m.temperature_c == -5.0 && *m.dewpoint_c == -12.0, "negative temperatures");
    check(near(*m.qnh_hpa, 29.92 * 33.8639, 1e-9), "inches of mercury to hectopascals");

    m = parse_metar("UUEE 171600Z 19004G09MPS CAVOK 18/09 Q1012");
    check(near(*m.wind_speed_kt, 4 * 3600.0 / 1852.0, 1e-9) &&
              near(*m.gust_kt, 9 * 3600.0 / 1852.0, 1e-9),
          "metres a second to knots");
    m = parse_metar("LXXX 171600Z 27036KMH 9999 12/M01 Q0998");
    check(near(*m.wind_speed_kt, 36 / 1.852, 1e-9), "kilometres an hour to knots");
    check(*m.qnh_hpa == 998.0 && *m.dewpoint_c == -1.0,
          "a QNH below 1000 and a negative dew point");
    m = parse_metar("EGXX 171600Z 00000KT 9999 20/ Q1020");
    check(*m.wind_speed_kt == 0.0 && *m.wind_from_deg == 0.0, "calm");
    check(*m.temperature_c == 20.0 && !m.dewpoint_c, "a temperature with no dew point");
    m = parse_metar("EGXX 171600Z /////KT 9999 ///// Q////");
    check(!m.wind_speed_kt && !m.wind_from_deg && !m.temperature_c && !m.qnh_hpa,
          "unreported groups are unreported");
    m = parse_metar("KBOS 171554Z 08011KT 8SM 22/18 A3022 RMK AO2 SLP233 T02220178");
    check(*m.temperature_c == 22.2 && *m.dewpoint_c == 17.8, "the T remark's tenths");
    m = parse_metar("PABR 171553Z AUTO 30005KT 10SM 00/M04 A2984 RMK AO2 T00001039");
    check(*m.temperature_c == 0.0 && near(*m.dewpoint_c, -3.9, 1e-12),
          "a negative T remark");
    m = parse_metar(
        "ZBAA 171600Z 11003MPS 2800 BR 23/20 Q1018 BECMG TL1640 27015KT 3200 25/15");
    check(*m.wind_from_deg == 110.0 && *m.temperature_c == 23.0,
          "a trend's wind and temperature are not the observation's");

    for (const char* bad : {"", "171600Z 27010KT", "YSSY 27010KT 9999", "METAR"}) {
        try {
            parse_metar(bad);
            fail(std::string("accepted \"") + bad + "\"");
        } catch (const MetarError&) {
        }
    }
}

GLIDESLOPE_TEST(
    a_recorded_metar_sets_the_surface_wind_temperature_and_pressure_it_reports) {
    constexpr double fps_per_knot = 1852.0 / 3600.0 / 0.3048;
    constexpr double radians = 3.14159265358979323846 / 180.0;
    for (const auto& report :
         glideslope::world::parse_aviationweather(recorded_metars())) {
        const Metar& m = report.metar;
        const std::string name = m.station;
        glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = report.latitude_deg;
        ic.longitude_deg = report.longitude_deg;
        ic.terrain_elevation_ft = report.elevation_m / 0.3048;
        ic.altitude_ft = ic.terrain_elevation_ft;
        aircraft.initialize(ic);
        aircraft.set_weather(std::make_shared<glideslope::sim::SteadyWeather>(
            glideslope::world::surface_conditions(report)));
        aircraft.step();

        // The wind blows from where the report says, as fast.
        double north = 0.0;
        double east = 0.0;
        if (m.wind_from_deg) {
            north =
                -*m.wind_speed_kt * fps_per_knot * std::cos(*m.wind_from_deg * radians);
            east =
                -*m.wind_speed_kt * fps_per_knot * std::sin(*m.wind_from_deg * radians);
        }
        check(near(aircraft.property("atmosphere/wind-north-fps"), north, 0.01) &&
                  near(aircraft.property("atmosphere/wind-east-fps"), east, 0.01),
              name + ": the wind");
        // The air at the station is the temperature the report says.
        const double celsius = aircraft.property("atmosphere/T-R") / 1.8 - 273.15;
        check(near(celsius, *m.temperature_c, 0.05),
              name + ": " + std::to_string(celsius) + " C at the station, the report " +
                  std::to_string(*m.temperature_c));
        // And the sea-level pressure is its QNH.
        const double hpa = aircraft.property("atmosphere/P-sl-psf") / 2.0885434233;
        check(near(hpa, *m.qnh_hpa, 0.01), name + ": QNH " + std::to_string(hpa) +
                                               ", the report " +
                                               std::to_string(*m.qnh_hpa));
    }
}

namespace {

std::string recorded_winds_aloft() {
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                         "data/weather/open-meteo-sydney-2026-09-17.json",
                     std::ios::binary);
    check(static_cast<bool>(in), "can read the recorded winds aloft");
    return std::string(std::istreambuf_iterator<char>(in), {});
}

glideslope::world::SurfaceReport sydney_metar() {
    for (auto& r : glideslope::world::parse_aviationweather(recorded_metars())) {
        if (r.metar.station == "YSSY") {
            return r;
        }
    }
    fail("no YSSY report");
}

constexpr double fps_per_mps = 1.0 / 0.3048;

// The wind JSBSim is given at `height_m`, one step into a flight there.
std::pair<double, double> jsbsim_wind(std::shared_ptr<glideslope::sim::Weather> weather,
                                      double height_m) {
    glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.95;
    ic.longitude_deg = 151.18;
    ic.altitude_ft = height_m * fps_per_mps;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 100.0;
    aircraft.initialize(ic);
    aircraft.set_weather(std::move(weather));
    aircraft.step();
    return {aircraft.property("atmosphere/wind-north-fps"),
            aircraft.property("atmosphere/wind-east-fps")};
}

} // namespace

GLIDESLOPE_TEST(
    a_recorded_winds_aloft_response_sets_the_wind_at_every_level_it_reports_and_between) {
    const std::string text = recorded_winds_aloft();
    const auto profile = glideslope::world::parse_open_meteo(text, "2026-09-17T16:00");
    check(profile.levels.size() == 19, "all 19 pressure levels");

    // Each level as the response gives it, read here from the JSON directly.
    const auto doc = glideslope::world::parse_json(text);
    const auto& hourly = doc.at("hourly");
    constexpr std::size_t hour = 16;
    for (const auto& level : profile.levels) {
        const std::string l =
            std::to_string(static_cast<int>(level.pressure_hpa)) + "hPa";
        const double speed = hourly.at("wind_speed_" + l).array()[hour].number();
        const double from = hourly.at("wind_direction_" + l).array()[hour].number() *
                            3.14159265358979323846 / 180;
        const double h = hourly.at("geopotential_height_" + l).array()[hour].number();
        check(near(level.wind_north_mps, -speed * std::cos(from), 1e-9) &&
                  near(level.wind_east_mps, -speed * std::sin(from), 1e-9) &&
                  near(level.temperature_c,
                       hourly.at("temperature_" + l).array()[hour].number(), 1e-9),
              l + ": the wind and temperature the response gives");
        // Geometric height is geopotential height a little stretched: by 90 m
        // at 24 km.
        check(level.height_m >= h && level.height_m - h < h * h / 6.0e6,
              l + ": its height");
    }

    glideslope::world::WeatherReport report;
    report.surface = sydney_metar();
    report.aloft = profile;
    const auto weather =
        std::make_shared<glideslope::world::ReportedWeather>(report, nullptr, 0.0);
    const double surface = report.surface.elevation_m + 10.0;
    const auto surface_wind = glideslope::world::surface_conditions(report.surface);

    int levels = 0;
    const glideslope::world::AloftLevel* below = nullptr;
    for (const auto& level : profile.levels) {
        if (level.height_m <= surface) {
            continue; // under the ground's wind
        }
        const auto [north, east] = jsbsim_wind(weather, level.height_m);
        check(near(north, level.wind_north_mps * fps_per_mps, 0.01) &&
                  near(east, level.wind_east_mps * fps_per_mps, 0.01),
              std::to_string(level.pressure_hpa) +
                  " hPa: JSBSim's wind at the level's height");
        // Halfway up from the level below - or from the surface - the wind is
        // halfway between.
        const double from_height = below != nullptr ? below->height_m : surface;
        const double from_north =
            below != nullptr ? below->wind_north_mps : surface_wind.wind_north_mps;
        const double from_east =
            below != nullptr ? below->wind_east_mps : surface_wind.wind_east_mps;
        const auto [mid_north, mid_east] =
            jsbsim_wind(weather, (from_height + level.height_m) / 2);
        check(near(mid_north, (from_north + level.wind_north_mps) / 2 * fps_per_mps,
                   0.01) &&
                  near(mid_east, (from_east + level.wind_east_mps) / 2 * fps_per_mps,
                       0.01),
              std::to_string(level.pressure_hpa) +
                  " hPa: halfway up to it, the wind halfway");
        below = &level;
        ++levels;
    }
    check(levels == 19,
          "every level above the surface - all 19 at Sydney - was flown through");
}

namespace {

struct Drift {
    double east_m = 0.0; // over the measured minute
    double turbulence_rms_fps[3] = {0.0, 0.0, 0.0};
    double turbulence_max_fps = 0.0;
    double worst_bank_deg = 0.0;
    double final_latitude = 0.0;
};

// Sixty seconds held on a northerly heading at 3,000 ft, after ten to settle,
// in `weather`.
Drift fly_north(std::shared_ptr<glideslope::sim::Weather> weather) {
    glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = 0.0;
    ic.longitude_deg = 20.0;
    ic.altitude_ft = 3000.0;
    ic.terrain_elevation_ft = 0.0;
    ic.heading_deg = 0.0;
    ic.airspeed_kts = 100.0;
    aircraft.initialize(ic);
    if (weather) {
        aircraft.set_weather(std::move(weather));
    }
    glideslope::sim::TestPilot pilot(aircraft);
    Drift d;
    double start_longitude = 0.0;
    double sums[3] = {0.0, 0.0, 0.0};
    int samples = 0;
    for (int i = 0; i < 70 * 120; ++i) {
        const auto s = aircraft.state();
        double error = std::remainder(0.0 - s.heading_deg, 360.0);
        glideslope::sim::Controls c;
        c.throttle = 0.7;
        c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(3000.0));
        c.aileron = pilot.roll_to(std::clamp(error * 2.0, -15.0, 15.0));
        c.rudder = pilot.coordinate();
        aircraft.set_controls(c);
        aircraft.step();
        if (i == 10 * 120) {
            start_longitude = aircraft.state().longitude_deg;
        }
        if (i >= 10 * 120) {
            int k = 0;
            for (const char* p :
                 {"atmosphere/turb-north-fps", "atmosphere/turb-east-fps",
                  "atmosphere/turb-down-fps"}) {
                const double v = aircraft.property(p);
                sums[k++] += v * v;
                d.turbulence_max_fps = std::max(d.turbulence_max_fps, std::abs(v));
            }
            ++samples;
            d.worst_bank_deg =
                std::max(d.worst_bank_deg, std::abs(aircraft.state().roll_deg));
        }
    }
    const auto s = aircraft.state();
    // On the equator a degree of longitude is 111,319.5 m.
    d.east_m = (s.longitude_deg - start_longitude) * 111319.49;
    for (int k = 0; k < 3; ++k) {
        d.turbulence_rms_fps[k] = std::sqrt(sums[k] / samples);
    }
    d.final_latitude = s.latitude_deg;
    return d;
}

} // namespace

GLIDESLOPE_TEST(
    an_aircraft_in_a_steady_crosswind_drifts_with_it_and_turbulence_disturbs_it_only_when_on) {
    constexpr double knot = 1852.0 / 3600.0;
    const Drift still = fly_north(nullptr);

    // Twenty knots from the west, no turbulence: a minute on a northerly
    // heading drifts the aircraft east by what twenty knots covers in a minute.
    glideslope::sim::Conditions westerly;
    westerly.wind_east_mps = 20.0 * knot;
    westerly.wind_at_20ft_mps = 20.0 * knot;
    const Drift drifted =
        fly_north(std::make_shared<glideslope::sim::SteadyWeather>(westerly));
    const double expected = 20.0 * knot * 60.0;
    const double drift = drifted.east_m - still.east_m;
    check(std::abs(drift - expected) < 0.02 * expected,
          "drifted " + std::to_string(drift) + " m east, the wind predicts " +
              std::to_string(expected));
    check(drifted.turbulence_max_fps == 0.0,
          "no turbulence when it is off: not a single step's worth");

    // The same wind with moderate turbulence (severity 3): JSBSim's MIL-F-8785C
    // intensity at 3,000 ft is 7.21 ft/s. Each component's RMS over the minute
    // must be within 0.4 and 1.6 of it, no gust over five times it, and the
    // aircraft - still held on its heading - never banked past 30 degrees.
    glideslope::sim::Conditions rough = westerly;
    rough.turbulence_severity = 3;
    const Drift shaken =
        fly_north(std::make_shared<glideslope::sim::SteadyWeather>(rough));
    constexpr double sigma = 6.9 + (3000.0 - 1750.0) / (3750.0 - 1750.0) * (7.4 - 6.9);
    for (int k = 0; k < 3; ++k) {
        check(shaken.turbulence_rms_fps[k] > 0.4 * sigma &&
                  shaken.turbulence_rms_fps[k] < 1.6 * sigma,
              "turbulence component " + std::to_string(k) + " RMS " +
                  std::to_string(shaken.turbulence_rms_fps[k]) + " ft/s, sigma " +
                  std::to_string(sigma));
    }
    check(shaken.turbulence_max_fps < 5.0 * sigma,
          "no gust over five sigma: " + std::to_string(shaken.turbulence_max_fps));
    check(shaken.worst_bank_deg < 30.0,
          "banked at most " + std::to_string(shaken.worst_bank_deg));
    check(shaken.final_latitude != drifted.final_latitude,
          "and it did disturb the flight");

    // The same turbulence twice is the same flight.
    const Drift again =
        fly_north(std::make_shared<glideslope::sim::SteadyWeather>(rough));
    check(again.final_latitude == shaken.final_latitude &&
              again.east_m == shaken.east_m,
          "turbulence repeats exactly");
}

GLIDESLOPE_TEST(
    a_new_weather_report_blends_in_over_its_interval_with_no_step_in_the_wind) {
    // Ten knots from the west, then at thirty seconds a report of thirty knots
    // from the east and 10 hPa lower, blended over sixty seconds.
    glideslope::world::WeatherReport before;
    before.surface = sydney_metar();
    before.surface.metar.wind_from_deg = 270.0;
    before.surface.metar.wind_speed_kt = 10.0;
    before.surface.metar.qnh_hpa = 1013.0;
    glideslope::world::WeatherReport after = before;
    after.surface.metar.wind_from_deg = 90.0;
    after.surface.metar.wind_speed_kt = 30.0;
    after.surface.metar.qnh_hpa = 1003.0;
    const auto weather =
        std::make_shared<glideslope::world::ReportedWeather>(before, nullptr, 60.0);

    glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.95;
    ic.longitude_deg = 151.18;
    ic.altitude_ft = 3000.0;
    ic.airspeed_kts = 100.0;
    aircraft.initialize(ic);
    aircraft.set_weather(weather);

    constexpr double fps_per_knot = 1852.0 / 3600.0 / 0.3048;
    const double east_before = 10.0 * fps_per_knot;
    const double east_after = -30.0 * fps_per_knot;
    // The most the wind may change in a step: the whole change spread evenly.
    const double largest_step = std::abs(east_after - east_before) / (60.0 * 120.0);
    double previous = 0.0;
    double worst_step = 0.0;
    bool updated = false;
    for (int i = 0; i < 100 * 120; ++i) {
        const double t = aircraft.state().sim_time_s;
        if (!updated && t >= 30.0) {
            weather->update(after, t);
            updated = true;
        }
        aircraft.step();
        const double east = aircraft.property("atmosphere/wind-east-fps");
        if (i > 0) {
            worst_step = std::max(worst_step, std::abs(east - previous));
        }
        previous = east;
        const double now = aircraft.state().sim_time_s;
        if (i == 25 * 120) {
            check(near(east, east_before, 1e-6), "before the report, the old wind");
        }
        if (i == 60 * 120) {
            check(near(east, (east_before + east_after) / 2, 0.1),
                  "halfway through, the wind halfway: " + std::to_string(east) +
                      " at " + std::to_string(now) + " s");
            check(near(aircraft.property("atmosphere/P-sl-psf") / 2.0885434233, 1008.0,
                       0.1),
                  "and the pressure halfway");
        }
    }
    check(near(previous, east_after, 1e-6), "after the interval, the new wind");
    check(worst_step <= largest_step * 1.01 + 1e-9,
          "no step larger than an even spread of the change: " +
              std::to_string(worst_step) + " ft/s against " +
              std::to_string(largest_step));
}

GLIDESLOPE_TEST(
    an_instant_is_named_by_the_utc_hour_it_falls_in_as_open_meteo_names_hours) {
    using namespace std::chrono;
    using glideslope::world::utc_hour;
    check(utc_hour(sys_days{2026y / September / 17} + 16h + 59min + 59s) ==
              "2026-09-17T16:00",
          "the hour an instant falls in");
    check(utc_hour(sys_days{2024y / February / 29}) == "2024-02-29T00:00",
          "midnight on a leap day");
    check(utc_hour(sys_days{1969y / December / 31} + 23h + 30min) == "1969-12-31T23:00",
          "before 1970, still the hour it falls in");
}

GLIDESLOPE_TEST(
    the_weather_now_at_an_airfield_is_fetched_from_aviationweather_and_open_meteo) {
    using glideslope::world::DemError;
    // What is not a station never reaches a URL.
    const glideslope::world::Fetch never =
        [](const std::string& url) -> glideslope::platform::HttpResponse {
        fail("fetched " + url);
    };
    for (const char* bad : {"", "YSS", "YSSYX", "YS&Y", "YS Y", "Y%53Y"}) {
        try {
            glideslope::world::fetch_metar(bad, never);
            fail(std::string("accepted the station \"") + bad + "\"");
        } catch (const MetarError&) {
        }
    }

    const auto now = std::chrono::system_clock::now();
    const std::string hour = glideslope::world::utc_hour(now);
    const glideslope::world::Fetch fetch = glideslope::world::http_fetch();
    glideslope::world::WeatherReport report;
    try {
        report = glideslope::world::fetch_weather("yssy", hour, fetch);
    } catch (const DemError& e) {
        if (!glideslope::test::network_required()) {
            glideslope::test::skip(std::string("no network here: ") + e.what());
        }
        throw;
    }

    // Sydney's latest METAR: its station, where it is, and weather that could be
    // Sydney's, observed today or yesterday.
    const Metar& m = report.surface.metar;
    check(m.station == "YSSY", "the station asked for, in capitals");
    check(near(report.surface.latitude_deg, -33.946, 0.01) &&
              near(report.surface.longitude_deg, 151.177, 0.01) &&
              near(report.surface.elevation_m, 6.0, 10.0),
          "where Sydney airport is");
    const std::chrono::year_month_day today(std::chrono::floor<std::chrono::days>(now));
    const std::chrono::year_month_day yesterday(
        std::chrono::floor<std::chrono::days>(now) - std::chrono::days(1));
    check(m.day == static_cast<int>(static_cast<unsigned>(today.day())) ||
              m.day == static_cast<int>(static_cast<unsigned>(yesterday.day())),
          "observed today or yesterday: day " + std::to_string(m.day));
    check(m.wind_speed_kt && *m.wind_speed_kt >= 0.0 && *m.wind_speed_kt < 100.0,
          "a wind");
    check(m.temperature_c && *m.temperature_c > -10.0 && *m.temperature_c < 50.0,
          "a temperature");
    check(m.qnh_hpa && *m.qnh_hpa > 950.0 && *m.qnh_hpa < 1060.0, "a pressure");

    // The winds aloft over it, for this hour: every level, each higher and so
    // at lower pressure than the one below, colder in the jet stream's levels
    // than near the ground, and 500 hPa where 500 hPa is.
    const auto& aloft = *report.aloft;
    check(aloft.time == hour, "the hour asked for");
    check(near(aloft.latitude_deg, report.surface.latitude_deg, 0.25) &&
              near(aloft.longitude_deg, report.surface.longitude_deg, 0.25),
          "over the station");
    check(aloft.levels.size() == 19, "all 19 levels");
    for (std::size_t i = 1; i < aloft.levels.size(); ++i) {
        check(aloft.levels[i].height_m > aloft.levels[i - 1].height_m &&
                  aloft.levels[i].pressure_hpa < aloft.levels[i - 1].pressure_hpa,
              "levels in order of height and pressure");
    }
    for (const auto& level : aloft.levels) {
        if (level.pressure_hpa == 500.0) {
            check(level.height_m > 5000.0 && level.height_m < 6200.0,
                  "500 hPa at " + std::to_string(level.height_m) + " m");
        }
        if (level.pressure_hpa == 200.0) {
            check(level.temperature_c < -30.0, "200 hPa is cold");
        }
    }

    // A station with nothing to report says so.
    try {
        glideslope::world::fetch_metar("ZZZZ", fetch);
        fail("a METAR for ZZZZ");
    } catch (const MetarError& e) {
        check(std::string(e.what()).find("no METAR for ZZZZ") != std::string::npos,
              e.what());
    }
}

GLIDESLOPE_TEST(
    a_recorded_response_sets_the_wind_near_the_ground_through_its_boundary_layer) {
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                         "data/weather/open-meteo-sydney-2026-09-18.json",
                     std::ios::binary);
    check(static_cast<bool>(in),
          "can read the recorded forecast with near-ground winds");
    const std::string text(std::istreambuf_iterator<char>(in), {});
    const auto profile = glideslope::world::parse_open_meteo(text, "2026-09-18T08:00");

    // The forecast's winds near the ground, as the response gives them.
    const auto doc = glideslope::world::parse_json(text);
    const auto& hourly = doc.at("hourly");
    constexpr std::size_t hour = 8;
    constexpr double radians = 3.14159265358979323846 / 180.0;
    check(profile.near_ground.size() == 4, "four heights near the ground");
    for (const auto& w : profile.near_ground) {
        const std::string h = std::to_string(static_cast<int>(w.height_m)) + "m";
        const double speed = hourly.at("wind_speed_" + h).array()[hour].number();
        const double from =
            hourly.at("wind_direction_" + h).array()[hour].number() * radians;
        check(near(w.wind_north_mps, -speed * std::cos(from), 1e-9) &&
                  near(w.wind_east_mps, -speed * std::sin(from), 1e-9),
              h + ": the wind the response gives");
    }

    glideslope::world::WeatherReport report;
    report.surface = sydney_metar();
    report.aloft = profile;
    const double ground = report.surface.elevation_m;
    const auto surface = glideslope::world::surface_conditions(report.surface);
    const auto wind_at = [&](double above_ground) {
        return glideslope::world::conditions_at(report, ground + above_ground);
    };

    // At 80, 120 and 180 m the forecast's wind; at 10 m the METAR's.
    for (const auto& w : profile.near_ground) {
        const auto c = wind_at(w.height_m);
        const auto& expected = w.height_m > 10.0 ? w : profile.near_ground.front();
        const double north =
            w.height_m > 10.0 ? expected.wind_north_mps : surface.wind_north_mps;
        const double east =
            w.height_m > 10.0 ? expected.wind_east_mps : surface.wind_east_mps;
        check(near(c.wind_north_mps, north, 1e-9) && near(c.wind_east_mps, east, 1e-9),
              std::to_string(w.height_m) + " m: the wind there");
    }
    // Between them, logarithmic in height: at 30 m, between 10 and 80.
    const auto& at80 = profile.near_ground[1];
    const double t = std::log(30.0 / 10.0) / std::log(80.0 / 10.0);
    const auto c30 = wind_at(30.0);
    check(near(c30.wind_north_mps,
               surface.wind_north_mps +
                   t * (at80.wind_north_mps - surface.wind_north_mps),
               1e-9),
          "30 m: logarithmically between 10 and 80 m");
    // Below the anemometer, falling logarithmically to nothing at 3 cm.
    const double factor = std::log(2.0 / 0.03) / std::log(10.0 / 0.03);
    const auto c2 = wind_at(2.0);
    check(near(c2.wind_north_mps, surface.wind_north_mps * factor, 1e-9) &&
              near(c2.wind_east_mps, surface.wind_east_mps * factor, 1e-9),
          "2 m: the METAR's wind, " + std::to_string(factor) + " of it");
    check(wind_at(0.02).wind_north_mps == 0.0, "below 3 cm, still air");

    // A 3-degree approach from 300 m flies down through the profile: the wind
    // JSBSim is given, before every step, is the profile's at the aircraft's
    // height.
    auto weather =
        std::make_shared<glideslope::world::ReportedWeather>(report, nullptr, 0.0);
    glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = report.surface.latitude_deg;
    ic.longitude_deg = report.surface.longitude_deg;
    ic.altitude_ft = (ground + 300.0) / 0.3048;
    ic.terrain_elevation_ft = ground / 0.3048;
    ic.airspeed_kts = 70.0;
    ic.engine_running = true;
    aircraft.initialize(ic);
    aircraft.set_weather(weather);
    glideslope::sim::TestPilot pilot(aircraft);
    const double descent_mps = 70.0 * 1852.0 / 3600.0 * std::tan(3.0 * radians);
    double worst = 0.0;
    double lowest_m = 300.0;
    double first_north = 0.0;
    double last_north = 0.0;
    for (int i = 0; i < 180 * 120; ++i) {
        const double height =
            aircraft.property("position/geod-alt-ft") * 0.3048 - ground;
        if (height < 15.0) {
            break;
        }
        const double target = ground + std::max(10.0, 300.0 - descent_mps * i / 120.0);
        glideslope::sim::Controls c;
        c.throttle = 0.35;
        c.elevator = pilot.pitch_to(pilot.pitch_for_altitude(target / 0.3048));
        c.aileron = pilot.roll_to(0.0);
        c.rudder = pilot.coordinate();
        aircraft.set_controls(c);
        const auto expected = glideslope::world::conditions_at(report, ground + height);
        aircraft.step();
        const double north = aircraft.property("atmosphere/wind-north-fps");
        worst =
            std::max(worst, std::abs(north - expected.wind_north_mps * fps_per_mps));
        if (i == 0) {
            first_north = north;
        }
        last_north = north;
        lowest_m = std::min(lowest_m, height);
    }
    check(lowest_m < 30.0, "the approach reached 30 m: " + std::to_string(lowest_m));
    check(worst < 0.01,
          "the wind all the way down is the profile's, within 0.01 ft/s: " +
              std::to_string(worst));
    check(std::abs(first_north - last_north) > 0.5, "and it changed on the way down");
}

GLIDESLOPE_TEST(reported_wind_shear_is_read_and_gives_the_approach_the_models_shear) {
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                         "data/weather/aviationweather-shear-2026-09-18.json",
                     std::ios::binary);
    check(static_cast<bool>(in), "can read the recorded reports of shear");
    const auto reports = glideslope::world::parse_aviationweather(
        std::string(std::istreambuf_iterator<char>(in), {}));
    check(reports.size() == 5, "five reports");
    const auto& lisbon = reports[0].metar;
    const auto& narita = reports[1].metar;
    const auto& jackson = reports[2].metar;
    const auto& camp = reports[3].metar;
    const auto& albuquerque = reports[4].metar;
    check(lisbon.station == "LPPT" &&
              lisbon.wind_shear_runways == std::vector<std::string>{"02"} &&
              !lisbon.wind_shear_all_runways,
          "LPPT: WS R02");
    check(narita.station == "RJAA" &&
              narita.wind_shear_runways == std::vector<std::string>{"34R"},
          "RJAA: WS R34R, before its TEMPO");
    check(jackson.station == "KJAC" && jackson.wind_shift &&
              jackson.wind_shift->hour == 7 && jackson.wind_shift->minute == 43 &&
              !jackson.wind_shift->frontal,
          "KJAC: WSHFT 0743");
    check(camp.station == "KCQC" && camp.peak_wind &&
              camp.peak_wind->from_deg == 220.0 && camp.peak_wind->speed_kt == 33.0 &&
              camp.peak_wind->hour == 8 && camp.peak_wind->minute == 32,
          "KCQC: PK WND 22033/0832");
    check(albuquerque.peak_wind && albuquerque.peak_wind->speed_kt == 37.0 &&
              albuquerque.peak_wind->hour == 7 && albuquerque.peak_wind->minute == 59,
          "KABQ: PK WND 17037/0759");

    // The other forms, as written.
    Metar m = parse_metar("EGXX 181200Z 27015KT 9999 12/08 Q1010 WS ALL RWY");
    check(m.wind_shear_all_runways && m.wind_shear_runways.empty(), "WS ALL RWY");
    m = parse_metar("EGXX 181200Z 27015KT 9999 12/08 Q1010 WS RWY27");
    check(m.wind_shear_runways == std::vector<std::string>{"27"}, "WS RWY27");
    m = parse_metar(
        "EGXX 181200Z 27015KT 9999 12/08 Q1010 WS TKOF RWY20 WS LDG RWY09L");
    check(m.wind_shear_runways == std::vector<std::string>{"20", "09L"},
          "WS TKOF RWY20 and WS LDG RWY09L");
    m = parse_metar("KXYZ 181253Z 28030G45KT 10SM 12/08 A2992 RMK PK WND 28045/15 "
                    "WSHFT 30 FROPA");
    check(m.peak_wind && m.peak_wind->hour == 12 && m.peak_wind->minute == 15 &&
              m.peak_wind->speed_kt == 45.0,
          "PK WND with minutes only is in the report's hour");
    check(m.wind_shift && m.wind_shift->hour == 12 && m.wind_shift->minute == 30 &&
              m.wind_shift->frontal,
          "WSHFT 30 FROPA");

    // Lisbon's WS R02 on runway 02's approach: from the south-south-west, the
    // headwind along 020 degrees is the surface wind's plus the model's.
    glideslope::world::WeatherReport report;
    report.surface = reports[0];
    report.air_seed = glideslope::world::air_seed_of(lisbon);
    report.turbulence_severity = 0;
    glideslope::world::WeatherReport clear = report;
    clear.surface.metar.wind_shear_runways.clear();
    constexpr double radians = 3.14159265358979323846 / 180.0;
    constexpr double mps_per_knot = 1852.0 / 3600.0;
    const double heading = 20.0 * radians;
    const auto headwind = [&](const glideslope::world::WeatherReport& r, double back_m,
                              double above_m) {
        // A point `back_m` before the runway, on its extended centreline.
        const double lat =
            r.surface.latitude_deg - back_m * std::cos(heading) / 111319.49;
        const double lon = r.surface.longitude_deg -
                           back_m * std::sin(heading) /
                               (111319.49 * std::cos(r.surface.latitude_deg * radians));
        const auto c = glideslope::world::with_air_motion(
            r, glideslope::world::lift_of(r), {},
            glideslope::world::conditions_at(r, r.surface.elevation_m + above_m), lat,
            lon, r.surface.elevation_m + above_m, 100.0);
        // Air moving towards 200 degrees is a headwind on 020.
        return -(c.wind_north_mps * std::cos(heading) +
                 c.wind_east_mps * std::sin(heading));
    };
    const struct {
        double back_m;
        double above_m;
        double extra_kt;
    } points[] = {{8500.0, 450.0, 0.0}, {5000.0, 450.0, 15.0}, {5000.0, 700.0, 0.0},
                  {3400.0, 180.0, 7.5}, {1100.0, 60.0, 0.0},   {5700.0, 300.0, 15.0},
                  {4000.0, 525.0, 7.5}};
    for (const auto& p : points) {
        const double extra = (headwind(report, p.back_m, p.above_m) -
                              headwind(clear, p.back_m, p.above_m)) /
                             mps_per_knot;
        check(near(extra, p.extra_kt, 1e-9),
              std::to_string(p.back_m) + " m back, " + std::to_string(p.above_m) +
                  " m up: " + std::to_string(extra) + " kt more headwind, the model " +
                  std::to_string(p.extra_kt));
    }
}

// **An answer that is not JSON is fetched again, and then taken for a failed
// download.** Both services have now and then answered a 200 whose body was
// not their JSON, and a client gave up on it. Here a stand-in for the network
// answers with a page of HTML twice and then with a recorded report - which
// must be read - and, again, with nothing but HTML - which must be said to be
// a download that failed, after the three tries, and not a report that could
// not be read. Both services, both cases, each counted.
GLIDESLOPE_TEST(an_answer_that_is_not_json_is_fetched_again_and_then_taken_for_a_failed_download) {
    const auto recorded = [](const char* name) {
        std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) / "data/weather" / name,
                         std::ios::binary);
        const std::string text(std::istreambuf_iterator<char>(in), {});
        return std::vector<std::uint8_t>(text.begin(), text.end());
    };
    const std::string html = "<html><body>Service unavailable</body></html>";
    const auto flaky = [&](std::vector<std::uint8_t> good, int bad_first, int& calls) {
        return glideslope::world::Fetch([&calls, bad_first, good, html](const std::string&) {
            glideslope::platform::HttpResponse r;
            r.status = 200;
            ++calls;
            r.body = calls <= bad_first ? std::vector<std::uint8_t>(html.begin(), html.end())
                                        : good;
            return r;
        });
    };
    std::size_t cases = 0;

    int calls = 0;
    const auto metar = glideslope::world::fetch_metar(
        "CYYZ", flaky(recorded("aviationweather-metars-2026-09-17T1600Z.json"), 2, calls));
    check(metar.metar.station == "CYYZ" && calls == 3,
          "the METAR read on the third try, " + std::to_string(calls) + " fetches");
    ++cases;

    calls = 0;
    const auto aloft = glideslope::world::fetch_winds_aloft(
        -33.95, 151.18, "2026-09-18T08:00",
        flaky(recorded("open-meteo-sydney-2026-09-18.json"), 2, calls));
    check(!aloft.levels.empty() && calls == 3,
          "the forecast read on the third try, " + std::to_string(calls) + " fetches");
    ++cases;

    for (const bool is_metar : {true, false}) {
        calls = 0;
        const auto never = flaky({}, 100, calls);
        try {
            if (is_metar) {
                (void)glideslope::world::fetch_metar("CYYZ", never);
            } else {
                (void)glideslope::world::fetch_winds_aloft(-33.95, 151.18, "2026-09-18T08:00", never);
            }
            fail("an answer that is never JSON was read");
        } catch (const glideslope::world::DemError& e) {
            check(std::string(e.what()).find("could not download") != std::string::npos &&
                      std::string(e.what()).find("not JSON") != std::string::npos,
                  std::string("it is a download that failed: ") + e.what());
            check(calls == glideslope::world::parse_attempts,
                  std::to_string(calls) + " fetches, one for each try");
        }
        ++cases;
    }
    check(cases == 4, "both services, read on a retry and given up on: four cases");
}
