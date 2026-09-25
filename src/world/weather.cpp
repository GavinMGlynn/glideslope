#include "world/weather.hpp"

#include "world/air_motion.hpp"
#include "world/json.hpp"

#include <algorithm>
#include <thread>
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
    // **An answer that is not JSON is fetched again**, as a failed download
    // is: aviationweather.gov and Open-Meteo have each, now and then, answered
    // a 200 whose body was not what they serve, and CI's clients gave up on
    // it. A well-formed answer that says something unwelcome is not retried.
    std::vector<SurfaceReport> reports;
    for (int attempt = 1;; ++attempt) {
        platform::HttpResponse r;
        try {
            r = fetch_with_retries(fetch, url);
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
        try {
            reports = parse_aviationweather(std::string_view(
                reinterpret_cast<const char*>(r.body.data()), r.body.size()));
            break;
        } catch (const JsonError& e) {
            if (attempt >= parse_attempts) {
                throw DemError("could not download " + url + ": its answer was not JSON (" +
                               e.what() + ")");
            }
            std::this_thread::sleep_for(parse_wait * attempt);
        }
    }
    if (reports.empty()) {
        throw MetarError("aviationweather.gov has no METAR for " + id);
    }
    return reports.front();
}

WeatherReport fetch_weather(const std::string& station, const std::string& time,
                            const Fetch& fetch) {
    WeatherReport report;
    report.surface = fetch_metar(station, fetch);
    report.air_seed = air_seed_of(report.surface.metar);
    report.aloft = fetch_winds_aloft(report.surface.latitude_deg,
                                     report.surface.longitude_deg, time, fetch);
    return report;
}

namespace {

// Where a METAR's wind is measured, above the ground.
constexpr double anemometer_height_m = 10.0;
// The height at which the logarithmic profile below it reaches nothing: open,
// flat country - short grass - which is what an airfield is.
constexpr double roughness_length_m = 0.03;

} // namespace

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
    sim::Conditions c = surface_conditions(report.surface);
    // JSBSim's own turbulence is left off: with_air_motion makes the air's.
    c.turbulence_severity = 0;
    const double ground = report.surface.elevation_m;
    const double above_ground = height_msl_m - ground;
    const sim::Conditions surface = c;

    // The wind. Below the anemometer, the METAR's, falling logarithmically to
    // nothing at the ground's roughness length.
    if (above_ground <= anemometer_height_m) {
        const double factor =
            above_ground <= roughness_length_m
                ? 0.0
                : std::log(above_ground / roughness_length_m) /
                      std::log(anemometer_height_m / roughness_length_m);
        c.wind_north_mps = surface.wind_north_mps * factor;
        c.wind_east_mps = surface.wind_east_mps * factor;
    } else {
        // Above it, the forecast's winds near the ground, logarithmically
        // between them and the METAR's; then its levels above the highest of
        // those, linearly in height, as before.
        struct Point {
            double above_ground;
            double north;
            double east;
        };
        std::vector<Point> points{
            {anemometer_height_m, surface.wind_north_mps, surface.wind_east_mps}};
        if (report.aloft) {
            for (const NearGroundWind& w : report.aloft->near_ground) {
                if (w.height_m > anemometer_height_m) {
                    points.push_back({w.height_m, w.wind_north_mps, w.wind_east_mps});
                }
            }
        }
        const Point& top = points.back();
        if (above_ground <= top.above_ground) {
            std::size_t k = 1;
            while (points[k].above_ground < above_ground) {
                ++k;
            }
            const Point& a = points[k - 1];
            const Point& b = points[k];
            const double t = std::log(above_ground / a.above_ground) /
                             std::log(b.above_ground / a.above_ground);
            c.wind_north_mps = a.north + t * (b.north - a.north);
            c.wind_east_mps = a.east + t * (b.east - a.east);
        } else {
            c.wind_north_mps = top.north;
            c.wind_east_mps = top.east;
            if (report.aloft) {
                WindsAloft aloft = *report.aloft;
                std::erase_if(aloft.levels, [&](const AloftLevel& l) {
                    return l.height_m <= ground + top.above_ground;
                });
                if (!aloft.levels.empty()) {
                    const AloftSample up = sample(aloft, height_msl_m);
                    const double lowest = aloft.levels.front().height_m - ground;
                    const double t = above_ground >= lowest
                                         ? 1.0
                                         : (above_ground - top.above_ground) /
                                               (lowest - top.above_ground);
                    c.wind_north_mps = top.north + t * (up.wind_north_mps - top.north);
                    c.wind_east_mps = top.east + t * (up.wind_east_mps - top.east);
                }
            }
        }
    }

    // The temperature, as an offset from the standard atmosphere: the METAR's up
    // to its anemometer, the forecast's from its lowest level above that, and
    // linearly in height between.
    const double surface_height = ground + anemometer_height_m;
    if (report.aloft && height_msl_m > surface_height) {
        WindsAloft aloft = *report.aloft;
        std::erase_if(aloft.levels, [&](const AloftLevel& l) {
            return l.height_m <= surface_height;
        });
        if (!aloft.levels.empty()) {
            const AloftSample up = sample(aloft, height_msl_m);
            const double up_offset = up.temperature_c - isa_temperature_c(height_msl_m);
            const double lowest = aloft.levels.front().height_m;
            const double t = height_msl_m >= lowest ? 1.0
                                                    : (height_msl_m - surface_height) /
                                                          (lowest - surface_height);
            c.temperature_offset_c = surface.temperature_offset_c +
                                     t * (up_offset - surface.temperature_offset_c);
        }
    }
    return c;
}

std::uint64_t air_seed_of(const Metar& metar) {
    // FNV-1a over the station, then the minute of the month it observed.
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : metar.station) {
        h = (h ^ static_cast<unsigned char>(c)) * 0x100000001b3ULL;
    }
    const auto minute =
        static_cast<std::uint64_t>((metar.day * 24 + metar.hour) * 60 + metar.minute);
    return (h ^ minute) * 0x100000001b3ULL;
}

Lift lift_of(const WeatherReport& report) {
    const double ground = report.surface.elevation_m;
    const auto temperature = [&](double height_msl_m) {
        return isa_temperature_c(height_msl_m) +
               conditions_at(report, height_msl_m).temperature_offset_c;
    };
    const sim::Conditions surface = surface_conditions(report.surface);
    Lift lift;
    // Without the forecast's temperatures above it, the layer cannot be judged:
    // the standard atmosphere's lapse would make one at any hour, night or day.
    if (report.aloft && !report.aloft->levels.empty()) {
        lift.convection = convection(
            temperature(ground), ground,
            std::hypot(surface.wind_north_mps, surface.wind_east_mps), temperature);
    }
    const sim::Conditions halfway =
        conditions_at(report, ground + 0.5 * lift.convection.depth_m);
    lift.drift_north_mps = halfway.wind_north_mps;
    lift.drift_east_mps = halfway.wind_east_mps;
    const sim::Conditions above = conditions_at(report, ground + 1000.0);
    lift.wind_north_mps = above.wind_north_mps;
    lift.wind_east_mps = above.wind_east_mps;
    // N^2 = g / T (dT/dz + the dry adiabatic lapse), between 1 and 4 km up.
    constexpr double gravity = 9.80665;
    constexpr double dry_lapse = 0.0098;
    const double low = temperature(ground + 1000.0);
    const double high = temperature(ground + 4000.0);
    const double mean_k = 0.5 * (low + high) + 273.15;
    const double n2 = gravity / mean_k * ((high - low) / 3000.0 + dry_lapse);
    lift.buoyancy_frequency = n2 > 0.0 ? std::sqrt(n2) : 0.0;
    return lift;
}

sim::Conditions with_air_motion(const WeatherReport& report, const Lift& lift,
                                const GroundAt& ground, sim::Conditions mean,
                                double latitude_deg, double longitude_deg,
                                double height_msl_m, double time_s) {
    constexpr double mps_per_knot = 1852.0 / 3600.0;
    constexpr double radians = 3.14159265358979323846 / 180.0;
    constexpr double metres_per_degree = 111319.49; // on the equatorial sphere
    const Metar& m = report.surface.metar;
    const double mean_kt = m.wind_speed_kt.value_or(0.0);
    const double spread_kt = std::max(0.0, m.gust_kt.value_or(mean_kt) - mean_kt);
    const int severity =
        report.turbulence_severity.value_or(severity_from_gust_spread(spread_kt));
    const bool shear = m.wind_shear_all_runways || !m.wind_shear_runways.empty();
    const bool thermals = lift.convection.velocity_mps > 0.0;
    if (spread_kt <= 0.0 && severity <= 0 && !shear && report.microbursts.empty() &&
        !thermals && !ground) {
        return mean;
    }

    // The surface wind: which way the patterns are carried, and how fast.
    const sim::Conditions surface = surface_conditions(report.surface);
    const double speed = std::hypot(surface.wind_north_mps, surface.wind_east_mps);
    Enu along{0.0, 1.0, 0.0};
    if (speed > 0.5) {
        along = {surface.wind_east_mps / speed, surface.wind_north_mps / speed, 0.0};
    }
    // Where the aircraft is from the station, on a local flat Earth: patterns a
    // few hundred kilometres across at most need no more.
    const double longitude_offset =
        std::remainder(longitude_deg - report.surface.longitude_deg, 360.0);
    const Enu from_station{longitude_offset * metres_per_degree *
                               std::cos(report.surface.latitude_deg * radians),
                           (latitude_deg - report.surface.latitude_deg) *
                               metres_per_degree,
                           height_msl_m - report.surface.elevation_m};

    // Reported wind shear: within 8 km of the station, a 15 kt headwind on the
    // reported runway's approach that dies away between 300 m and 60 m above
    // the ground - the loss of airspeed on short final a shear report warns
    // of. On a runway, along its heading from its number, magnetic taken as
    // true; on all of them, along the surface wind.
    if (shear && std::hypot(from_station.east, from_station.north) <= 8000.0) {
        const double h = from_station.up;
        const double profile = h <= 60.0    ? 0.0
                               : h <= 300.0 ? (h - 60.0) / 240.0
                               : h <= 450.0 ? 1.0
                               : h <= 600.0 ? (600.0 - h) / 150.0
                                            : 0.0;
        const double headwind = 15.0 * mps_per_knot * profile;
        if (!m.wind_shear_runways.empty()) {
            // Air moving against an aircraft landing on the runway's heading.
            const double heading =
                std::stod(m.wind_shear_runways.front().substr(0, 2)) * 10.0 * radians;
            mean.wind_east_mps -= std::sin(heading) * headwind;
            mean.wind_north_mps -= std::cos(heading) * headwind;
        } else {
            mean.wind_east_mps += along.east * headwind;
            mean.wind_north_mps += along.north * headwind;
        }
    }

    // Microbursts, each on its own centre.
    for (const Microburst& burst : report.microbursts) {
        const double east_offset =
            std::remainder(longitude_deg - burst.longitude_deg, 360.0) *
            metres_per_degree * std::cos(burst.latitude_deg * radians);
        const double north_offset =
            (latitude_deg - burst.latitude_deg) * metres_per_degree;
        const Enu w = microburst_wind(
            burst, {east_offset, north_offset, from_station.up}, time_s);
        mean.wind_east_mps += w.east;
        mean.wind_north_mps += w.north;
        mean.wind_down_mps -= w.up;
    }

    // Gusts, along the wind, fading out above the surface.
    if (spread_kt > 0.0) {
        const double carried = std::max(speed, 1.0);
        const double pattern_s = time_s - (from_station.east * along.east +
                                           from_station.north * along.north) /
                                              carried;
        const double weight =
            std::clamp(1.0 - (from_station.up - 10.0) / 600.0, 0.0, 1.0);
        const double gust =
            spread_kt * mps_per_knot * gust_factor(report.air_seed, pattern_s) * weight;
        mean.wind_east_mps += along.east * gust;
        mean.wind_north_mps += along.north * gust;
    }

    // Turbulence, Dryden's at the height above the station.
    const DrydenScales scales =
        dryden_scales(severity, std::max(0.0, from_station.up), speed);
    const Enu pattern{from_station.east - surface.wind_east_mps * time_s,
                      from_station.north - surface.wind_north_mps * time_s,
                      from_station.up};
    const Enu t = turbulence(report.air_seed, scales, pattern, along);
    mean.wind_east_mps += t.east;
    mean.wind_north_mps += t.north;
    mean.wind_down_mps -= t.up;

    // Thermals, over the ground beneath, carried by the layer's wind.
    const double cosine = std::cos(latitude_deg * radians);
    if (thermals) {
        const double beneath =
            ground ? ground(latitude_deg, longitude_deg) : report.surface.elevation_m;
        const Enu drifted{from_station.east - lift.drift_east_mps * time_s,
                          from_station.north - lift.drift_north_mps * time_s,
                          height_msl_m - beneath};
        mean.wind_down_mps -=
            thermal_updraught(report.air_seed, lift.convection, drifted, time_s);
    }

    // The terrain's lift, along the wind through the aircraft.
    const double across = std::hypot(lift.wind_north_mps, lift.wind_east_mps);
    if (ground && across >= 1.0) {
        const double north = lift.wind_north_mps / across;
        const double east = lift.wind_east_mps / across;
        const TerrainAlong terrain = [&](double along_m) {
            return ground(latitude_deg + north * along_m / metres_per_degree,
                          longitude_deg +
                              east * along_m / (metres_per_degree * cosine));
        };
        mean.wind_down_mps -=
            terrain_updraught(terrain, height_msl_m, across, lift.buoyancy_frequency);
    }
    return mean;
}

ReportedWeather::ReportedWeather(WeatherReport report, const Geoid* geoid,
                                 double blend_seconds, GroundAt ground)
    : current_(std::move(report)), current_lift_(lift_of(current_)),
      ground_(std::move(ground)), geoid_(geoid), blend_seconds_(blend_seconds) {}

void ReportedWeather::update(WeatherReport report, double now_s) {
    previous_ = std::move(current_);
    previous_lift_ = current_lift_;
    current_ = std::move(report);
    current_lift_ = lift_of(current_);
    changed_at_s_ = now_s;
}

sim::Conditions ReportedWeather::at(double latitude_deg, double longitude_deg,
                                    double height_m, double time_s) {
    const double msl = geoid_ != nullptr
                           ? height_m - geoid_->undulation(latitude_deg, longitude_deg)
                           : height_m;
    sim::Conditions now =
        with_air_motion(current_, current_lift_, ground_, conditions_at(current_, msl),
                        latitude_deg, longitude_deg, msl, time_s);
    if (!previous_ || blend_seconds_ <= 0.0) {
        return now;
    }
    const double w = std::clamp((time_s - changed_at_s_) / blend_seconds_, 0.0, 1.0);
    if (w >= 1.0) {
        previous_.reset();
        return now;
    }
    const sim::Conditions before = with_air_motion(
        *previous_, previous_lift_, ground_, conditions_at(*previous_, msl),
        latitude_deg, longitude_deg, msl, time_s);
    const auto mix = [w](double a, double b) { return a + w * (b - a); };
    sim::Conditions c;
    c.wind_north_mps = mix(before.wind_north_mps, now.wind_north_mps);
    c.wind_east_mps = mix(before.wind_east_mps, now.wind_east_mps);
    c.wind_down_mps = mix(before.wind_down_mps, now.wind_down_mps);
    c.temperature_offset_c = mix(before.temperature_offset_c, now.temperature_offset_c);
    c.sea_level_pressure_hpa =
        mix(before.sea_level_pressure_hpa, now.sea_level_pressure_hpa);
    c.wind_at_20ft_mps = mix(before.wind_at_20ft_mps, now.wind_at_20ft_mps);
    return c;
}

} // namespace glideslope::world
