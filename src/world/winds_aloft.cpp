#include "world/winds_aloft.hpp"

#include "world/json.hpp"

#include <algorithm>
#include <thread>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace glideslope::world {

namespace {

constexpr double radians = 3.14159265358979323846 / 180.0;
// The Earth radius geopotential heights are converted with (WMO No. 8).
constexpr double geopotential_radius_m = 6356766.0;

} // namespace

const std::vector<int>& open_meteo_levels() {
    static const std::vector<int> levels{1000, 975, 950, 925, 900, 850, 800,
                                         700,  600, 500, 400, 300, 250, 200,
                                         150,  100, 70,  50,  30};
    return levels;
}

const std::vector<int>& open_meteo_near_ground_heights() {
    static const std::vector<int> heights{10, 80, 120, 180};
    return heights;
}

std::string open_meteo_url(double latitude_deg, double longitude_deg) {
    std::string variables;
    for (const int level : open_meteo_levels()) {
        const std::string l = std::to_string(level) + "hPa";
        for (const char* v : {"wind_speed_", "wind_direction_", "geopotential_height_",
                              "temperature_"}) {
            variables += (variables.empty() ? "" : ",") + std::string(v) + l;
        }
    }
    for (const int height : open_meteo_near_ground_heights()) {
        const std::string h = std::to_string(height) + "m";
        variables += ",wind_speed_" + h + ",wind_direction_" + h;
    }
    char place[64];
    std::snprintf(place, sizeof place, "latitude=%.4f&longitude=%.4f", latitude_deg,
                  longitude_deg);
    return weather_host("https://api.open-meteo.com") + "/v1/forecast?" + place +
           "&hourly=" + variables + "&wind_speed_unit=ms&timezone=GMT&forecast_days=1";
}

std::string utc_hour(std::chrono::system_clock::time_point t) {
    const auto day = std::chrono::floor<std::chrono::days>(t);
    const std::chrono::year_month_day date(day);
    const auto hour = std::chrono::floor<std::chrono::hours>(t - day).count();
    char text[32];
    std::snprintf(text, sizeof text, "%04d-%02u-%02uT%02d:00",
                  static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
                  static_cast<unsigned>(date.day()), static_cast<int>(hour));
    return text;
}

WindsAloft parse_open_meteo(std::string_view text, const std::string& time) {
    const Json doc = parse_json(text);
    WindsAloft profile;
    profile.latitude_deg = doc.at("latitude").number();
    profile.longitude_deg = doc.at("longitude").number();
    profile.time = time;
    const Json& hourly = doc.at("hourly");
    const auto& times = hourly.at("time").array();
    std::size_t hour = times.size();
    for (std::size_t i = 0; i < times.size(); ++i) {
        if (times[i].string() == time) {
            hour = i;
        }
    }
    if (hour == times.size()) {
        throw std::runtime_error("the forecast has no hour " + time);
    }
    const auto value = [&](const std::string& name) {
        const auto& series = hourly.at(name).array();
        if (hour >= series.size() || series[hour].is_null()) {
            throw std::runtime_error("the forecast has no " + name + " at " + time);
        }
        return series[hour].number();
    };
    for (const int level : open_meteo_levels()) {
        const std::string l = std::to_string(level) + "hPa";
        AloftLevel a;
        a.pressure_hpa = level;
        const double geopotential = value("geopotential_height_" + l);
        a.height_m = geopotential_radius_m * geopotential /
                     (geopotential_radius_m - geopotential);
        const double speed = value("wind_speed_" + l);
        const double from = value("wind_direction_" + l) * radians;
        a.wind_north_mps = -speed * std::cos(from);
        a.wind_east_mps = -speed * std::sin(from);
        a.temperature_c = value("temperature_" + l);
        profile.levels.push_back(a);
    }
    std::sort(profile.levels.begin(), profile.levels.end(),
              [](const AloftLevel& a, const AloftLevel& b) {
                  return a.height_m < b.height_m;
              });
    // The winds near the ground, if the response has them; all of them or none.
    if (hourly.find("wind_speed_10m") != nullptr) {
        for (const int height : open_meteo_near_ground_heights()) {
            const std::string h = std::to_string(height) + "m";
            const double speed = value("wind_speed_" + h);
            const double from = value("wind_direction_" + h) * radians;
            profile.near_ground.push_back({static_cast<double>(height),
                                           -speed * std::cos(from),
                                           -speed * std::sin(from)});
        }
    }
    return profile;
}

WindsAloft fetch_winds_aloft(double latitude_deg, double longitude_deg,
                             const std::string& time, const Fetch& fetch) {
    const std::string url = open_meteo_url(latitude_deg, longitude_deg);
    // An answer that is not JSON is fetched again (weather.cpp says why).
    for (int attempt = 1;; ++attempt) {
        platform::HttpResponse r;
        try {
            r = fetch_with_retries(fetch, url);
        } catch (const platform::HttpError& e) {
            throw DemError(std::string("could not download: ") + e.what());
        }
        if (r.status != 200) {
            throw DemError("could not download " + url + ": status " +
                           std::to_string(r.status));
        }
        try {
            return parse_open_meteo(
                std::string_view(reinterpret_cast<const char*>(r.body.data()), r.body.size()),
                time);
        } catch (const JsonError& e) {
            if (attempt >= parse_attempts) {
                throw DemError("could not download " + url + ": its answer was not JSON (" +
                               e.what() + ")");
            }
            std::this_thread::sleep_for(parse_wait * attempt);
        }
    }
}

AloftSample sample(const WindsAloft& profile, double height_m) {
    const auto& levels = profile.levels;
    if (levels.empty()) {
        return {};
    }
    const auto of = [](const AloftLevel& l) {
        return AloftSample{l.wind_north_mps, l.wind_east_mps, l.temperature_c};
    };
    if (height_m <= levels.front().height_m) {
        return of(levels.front());
    }
    if (height_m >= levels.back().height_m) {
        return of(levels.back());
    }
    const auto above =
        std::upper_bound(levels.begin(), levels.end(), height_m,
                         [](double h, const AloftLevel& l) { return h < l.height_m; });
    const AloftLevel& hi = *above;
    const AloftLevel& lo = *(above - 1);
    const double t = (height_m - lo.height_m) / (hi.height_m - lo.height_m);
    return {lo.wind_north_mps + t * (hi.wind_north_mps - lo.wind_north_mps),
            lo.wind_east_mps + t * (hi.wind_east_mps - lo.wind_east_mps),
            lo.temperature_c + t * (hi.temperature_c - lo.temperature_c)};
}

} // namespace glideslope::world
