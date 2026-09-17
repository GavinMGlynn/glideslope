#include "world/weather.hpp"

#include "world/json.hpp"

#include <algorithm>
#include <cmath>

namespace glideslope::world {

std::vector<SurfaceReport> parse_aviationweather(std::string_view text) {
    const Json doc = parse_json(text);
    std::vector<SurfaceReport> reports;
    for (const Json& item : doc.array()) {
        SurfaceReport r;
        r.metar = parse_metar(item.at("rawOb").string());
        r.latitude_deg = item.at("lat").number();
        r.longitude_deg = item.at("lon").number();
        r.elevation_m = item.at("elev").number();
        reports.push_back(std::move(r));
    }
    return reports;
}

SurfaceReport fetch_metar(const std::string& station, const Fetch& fetch) {
    // Checked before it goes into a URL.
    std::string id = station;
    const bool alphanumeric = std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9');
    });
    if (id.size() != 4 || !alphanumeric) {
        throw MetarError("\"" + station +
                         "\" is not a station's four-character ICAO code");
    }
    for (char& c : id) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    const std::string url =
        "https://aviationweather.gov/api/data/metar?ids=" + id + "&format=json";
    platform::HttpResponse r;
    try {
        r = fetch(url);
    } catch (const platform::HttpError& e) {
        throw DemError(std::string("could not download: ") + e.what());
    }
    // No content is how it says it has no report for a station.
    if (r.status == 204) {
        throw MetarError("aviationweather.gov has no METAR for " + id);
    }
    if (r.status != 200) {
        throw DemError("could not download " + url + ": status " +
                       std::to_string(r.status));
    }
    const auto reports = parse_aviationweather(
        std::string_view(reinterpret_cast<const char*>(r.body.data()), r.body.size()));
    if (reports.empty()) {
        throw MetarError("aviationweather.gov has no METAR for " + id);
    }
    return reports.front();
}

WeatherReport fetch_weather(const std::string& station, const std::string& time,
                            const Fetch& fetch) {
    WeatherReport report;
    report.surface = fetch_metar(station, fetch);
    report.aloft = fetch_winds_aloft(report.surface.latitude_deg,
                                     report.surface.longitude_deg, time, fetch);
    return report;
}

double isa_temperature_c(double height_m) {
    // Up to the tropopause, which is as high as a METAR's station goes.
    return 15.0 - 0.0065 * height_m;
}

sim::Conditions surface_conditions(const SurfaceReport& report) {
    constexpr double mps_per_knot = 1852.0 / 3600.0;
    constexpr double radians = 3.14159265358979323846 / 180.0;
    sim::Conditions c;
    const Metar& m = report.metar;
    if (m.wind_from_deg && m.wind_speed_kt) {
        // From a direction, so blowing towards its opposite.
        const double speed = *m.wind_speed_kt * mps_per_knot;
        c.wind_north_mps = -speed * std::cos(*m.wind_from_deg * radians);
        c.wind_east_mps = -speed * std::sin(*m.wind_from_deg * radians);
        c.wind_at_20ft_mps = speed;
    }
    if (m.temperature_c) {
        c.temperature_offset_c =
            *m.temperature_c - isa_temperature_c(report.elevation_m);
    }
    if (m.qnh_hpa) {
        c.sea_level_pressure_hpa = *m.qnh_hpa;
    }
    return c;
}

sim::Conditions conditions_at(const WeatherReport& report, double height_msl_m) {
    sim::Conditions surface = surface_conditions(report.surface);
    surface.turbulence_severity = report.turbulence_severity;
    const double surface_height = report.surface.elevation_m + 10.0;
    if (!report.aloft || height_msl_m <= surface_height) {
        return surface;
    }
    // The winds aloft above the surface wind's height; levels below the
    // ground, which a forecast still gives, are not the air here.
    WindsAloft aloft = *report.aloft;
    std::erase_if(aloft.levels,
                  [&](const AloftLevel& l) { return l.height_m <= surface_height; });
    if (aloft.levels.empty()) {
        return surface;
    }
    const AloftSample up = sample(aloft, height_msl_m);
    sim::Conditions c = surface;
    const double up_offset = up.temperature_c - isa_temperature_c(height_msl_m);
    const double lowest = aloft.levels.front().height_m;
    if (height_msl_m >= lowest) {
        c.wind_north_mps = up.wind_north_mps;
        c.wind_east_mps = up.wind_east_mps;
        c.temperature_offset_c = up_offset;
        return c;
    }
    const double t = (height_msl_m - surface_height) / (lowest - surface_height);
    c.wind_north_mps =
        surface.wind_north_mps + t * (up.wind_north_mps - surface.wind_north_mps);
    c.wind_east_mps =
        surface.wind_east_mps + t * (up.wind_east_mps - surface.wind_east_mps);
    c.temperature_offset_c =
        surface.temperature_offset_c + t * (up_offset - surface.temperature_offset_c);
    return c;
}

ReportedWeather::ReportedWeather(WeatherReport report, const Geoid* geoid,
                                 double blend_seconds)
    : current_(std::move(report)), geoid_(geoid), blend_seconds_(blend_seconds) {}

void ReportedWeather::update(WeatherReport report, double now_s) {
    previous_ = std::move(current_);
    current_ = std::move(report);
    changed_at_s_ = now_s;
}

sim::Conditions ReportedWeather::at(double latitude_deg, double longitude_deg,
                                    double height_m, double time_s) {
    const double msl = geoid_ != nullptr
                           ? height_m - geoid_->undulation(latitude_deg, longitude_deg)
                           : height_m;
    sim::Conditions now = conditions_at(current_, msl);
    if (!previous_ || blend_seconds_ <= 0.0) {
        return now;
    }
    const double w = std::clamp((time_s - changed_at_s_) / blend_seconds_, 0.0, 1.0);
    if (w >= 1.0) {
        previous_.reset();
        return now;
    }
    const sim::Conditions before = conditions_at(*previous_, msl);
    const auto mix = [w](double a, double b) { return a + w * (b - a); };
    sim::Conditions c;
    c.wind_north_mps = mix(before.wind_north_mps, now.wind_north_mps);
    c.wind_east_mps = mix(before.wind_east_mps, now.wind_east_mps);
    c.wind_down_mps = mix(before.wind_down_mps, now.wind_down_mps);
    c.temperature_offset_c = mix(before.temperature_offset_c, now.temperature_offset_c);
    c.sea_level_pressure_hpa =
        mix(before.sea_level_pressure_hpa, now.sea_level_pressure_hpa);
    c.wind_at_20ft_mps = mix(before.wind_at_20ft_mps, now.wind_at_20ft_mps);
    c.turbulence_severity =
        w < 0.5 ? before.turbulence_severity : now.turbulence_severity;
    return c;
}

} // namespace glideslope::world
