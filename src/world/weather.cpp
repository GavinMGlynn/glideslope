#include "world/weather.hpp"

#include "sim/plan.hpp"
#include "sim/runway_condition.hpp"
#include "world/air_motion.hpp"
#include "world/json.hpp"

#include <algorithm>
#include <thread>
#include <utility>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <stdexcept>

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

namespace {

// **aviationweather.gov's METARs at a URL**: every report its answer holds.
// An answer that is not JSON is fetched again, as a failed download is
// (answered_json says why); a well-formed answer that says something
// unwelcome is not retried. No content (204) is none.
std::vector<SurfaceReport> fetch_reports(const std::string& url, const Fetch& fetch,
                                         std::chrono::milliseconds retry_wait) {
    for (int attempt = 1;; ++attempt) {
        platform::HttpResponse r;
        try {
            r = fetch_with_retries(fetch, url, 5, retry_wait);
        } catch (const platform::HttpError& e) {
            throw ServiceUnavailable(std::string("could not download: ") + e.what());
        }
        // No content is how it says it has no report.
        if (r.status == 204) {
            return {};
        }
        // A server error, or turned away for now (429), to every retry.
        if (r.status >= 500 || r.status == 429) {
            throw ServiceUnavailable("could not download " + url + ": status " +
                                     std::to_string(r.status));
        }
        if (r.status != 200) {
            throw DemError("could not download " + url + ": status " + std::to_string(r.status));
        }
        const std::string_view text(reinterpret_cast<const char*>(r.body.data()), r.body.size());
        if (answered_json(text, url, attempt)) {
            // JSON that is not a report is ours to mend: a failure, named
            // for where it came from.
            try {
                return parse_aviationweather(text);
            } catch (const JsonError& e) {
                throw DemError(url + ": its answer is JSON and not aviationweather.gov's "
                                     "METARs (" + e.what() + ")");
            }
        }
    }
}

} // namespace

SurfaceReport fetch_metar(const std::string& station, const Fetch& fetch,
                          std::chrono::milliseconds retry_wait) {
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
        weather_host("https://aviationweather.gov") + "/api/data/metar?ids=" + id +
        "&format=json";
    std::vector<SurfaceReport> reports = fetch_reports(url, fetch, retry_wait);
    if (reports.empty()) {
        throw MetarError("aviationweather.gov has no METAR for " + id);
    }
    return reports.front();
}

std::vector<SurfaceReport> fetch_metars_around(double latitude_deg, double longitude_deg,
                                               const Fetch& fetch,
                                               std::chrono::milliseconds retry_wait) {
    // The box, in degrees, clipped to the Earth: a box across the
    // antimeridian is cut at it, and its far side left out.
    const double north = stations_radius_m / sim::metres_per_degree_latitude(latitude_deg);
    const double east =
        stations_radius_m / std::max(1000.0, sim::metres_per_degree_longitude(latitude_deg));
    char box[160];
    std::snprintf(box, sizeof(box), "%.4f,%.4f,%.4f,%.4f", std::max(-90.0, latitude_deg - north),
                  std::max(-180.0, longitude_deg - east), std::min(90.0, latitude_deg + north),
                  std::min(180.0, longitude_deg + east));
    const std::string url = weather_host("https://aviationweather.gov") +
                            "/api/data/metar?bbox=" + box + "&format=json";
    return fetch_reports(url, fetch, retry_wait);
}

std::vector<SurfaceReport> stations_around(const std::vector<SurfaceReport>& reports,
                                           const std::vector<std::pair<double, double>>& places) {
    std::vector<SurfaceReport> out;
    const auto have = [&out](const SurfaceReport& r) {
        return std::any_of(out.begin(), out.end(), [&r](const SurfaceReport& o) {
            return o.metar.station == r.metar.station;
        });
    };
    for (const auto& [lat, lon] : places) {
        std::vector<std::pair<double, const SurfaceReport*>> near;
        for (const SurfaceReport& r : reports) {
            const double d = sim::distance_m(lat, lon, r.latitude_deg, r.longitude_deg);
            if (d <= stations_radius_m && !r.metar.station.empty()) {
                near.emplace_back(d, &r);
            }
        }
        std::stable_sort(near.begin(), near.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        // A station reported twice counts once: its nearer report.
        std::vector<std::string> named;
        for (const auto& [d, r] : near) {
            if (named.size() == stations_per_aircraft || out.size() == most_stations) {
                break;
            }
            if (std::find(named.begin(), named.end(), r->metar.station) != named.end()) {
                continue;
            }
            named.push_back(r->metar.station);
            if (!have(*r)) {
                out.push_back(*r);
            }
        }
    }
    return out;
}

WeatherReport fetch_weather(const std::string& station, const std::string& time,
                            const Fetch& fetch, std::chrono::milliseconds retry_wait) {
    WeatherReport report;
    // **A service that did not answer says it was the weather's**, so that
    // what runs a flight can tell a weather service that did not answer -
    // live, somebody else's, and never kept - from data it fetches once and
    // keeps. Only that: a refusal or an answer that does not parse is ours to
    // mend, and stays the error it was.
    try {
        report.surface = fetch_metar(station, fetch, retry_wait);
        report.air_seed = air_seed_of(report.surface.metar);
        report.aloft = fetch_winds_aloft(report.surface.latitude_deg,
                                         report.surface.longitude_deg, time, fetch,
                                         retry_wait);
    } catch (const ServiceUnavailable& e) {
        throw ServiceUnavailable(std::string("the weather could not be had: ") + e.what());
    }
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

int runway_condition_of(const Metar& metar) {
    const auto wets = [](const Metar::PresentWeather& w) {
        if (w.vicinity) {
            return false;
        }
        for (const std::string& p : w.phenomena) {
            if (p == "RA" || p == "DZ" || p == "SN" || p == "SG" || p == "PL" || p == "GS" ||
                p == "GR" || p == "UP") {
                return true;
            }
        }
        return false;
    };
    for (const Metar::PresentWeather& w : metar.weather) {
        if (wets(w)) {
            return sim::wet_runway;
        }
    }
    for (const Metar::PresentWeather& w : metar.recent_weather) {
        if (wets(w)) {
            return sim::wet_runway;
        }
    }
    return sim::dry_runway;
}

std::optional<int> runway_condition_of(const Metar::RunwayState& state,
                                       std::optional<double> temperature_c) {
    if (state.snow_closed) {
        return sim::most_slippery_runway;
    }
    if (state.cleared) {
        return sim::dry_runway;
    }
    if (state.extent && (*state.extent == 1 || *state.extent == 2)) {
        return sim::dry_runway;
    }
    std::optional<int> code;
    if (state.deposit) {
        // 3 mm, 1/8 in: what divides code 5 from the deeper codes. No depth
        // reported (//) is a depth that does not matter.
        const std::optional<double> depth =
            state.depth ? deposit_depth_mm(*state.depth) : std::optional<double>(0.0);
        const bool deep = depth && *depth > 3.0;
        const bool closed = !depth; // 99: not operational, its depth not said
        switch (*state.deposit) {
        case 0:
            code = sim::dry_runway;
            break;
        case 1: // damp
        case 3: // rime or frost
            code = sim::wet_runway;
            break;
        case 2: // wet, water patches
        case 6: // slush
            code = closed ? sim::most_slippery_runway : deep ? 2 : sim::wet_runway;
            break;
        case 4: // dry snow
        case 5: // wet snow
            code = closed ? sim::most_slippery_runway : deep ? 3 : sim::wet_runway;
            break;
        case 7: // ice
            code = sim::most_slippery_runway;
            break;
        default: // 8 compacted or rolled snow, 9 frozen ruts or ridges
            code = temperature_c && *temperature_c <= -15.0 ? 4 : 3;
            break;
        }
    }
    if (state.friction && *state.friction >= 91 && *state.friction <= 95) {
        const int braking = *state.friction - 90; // 91 poor, 1 ... 95 good, 5
        code = code ? std::min(*code, braking) : braking;
    }
    return code;
}

namespace {

// "24R" as its number and letter; -1 for an ident with no number.
std::pair<int, std::string> designator(const std::string& ident) {
    std::size_t digits = 0;
    while (digits < ident.size() &&
           std::isdigit(static_cast<unsigned char>(ident[digits])) != 0) {
        ++digits;
    }
    if (digits == 0 || digits > 2) {
        return {-1, ident};
    }
    return {std::stoi(ident.substr(0, digits)), ident.substr(digits)};
}

} // namespace

bool names(const Metar::RunwayState& state, const RunwayEnd& end) {
    if (state.all_runways) {
        return true;
    }
    const auto [number, letter] = designator(state.runway);
    const auto [end_number, end_letter] = designator(end.ident);
    if (end_number < 0) {
        return false;
    }
    // The end's other end: 18 on, left and right swapped.
    const int other_number = (end_number + 17) % 36 + 1;
    const std::string other_letter = end_letter == "L"   ? "R"
                                     : end_letter == "R" ? "L"
                                                         : end_letter;
    const auto matches = [said_number = number, said_letter = letter](int n,
                                                                       const std::string& l) {
        return n == said_number && (l == said_letter || (said_letter.empty() && l == "L"));
    };
    return matches(end_number, end_letter) || matches(other_number, other_letter);
}

std::vector<ContaminatedRunway> contaminated_runways(const Metar& metar,
                                                     const std::vector<RunwayEnd>& station_runways) {
    std::vector<ContaminatedRunway> out;
    for (const Metar::RunwayState& state : metar.runway_states) {
        const std::optional<int> code = runway_condition_of(state, metar.temperature_c);
        if (!code || *code >= sim::dry_runway) {
            continue;
        }
        for (const RunwayEnd& end : station_runways) {
            const bool readable =
                std::abs(end.latitude_deg) <= 90.0 && std::abs(end.longitude_deg) <= 180.0 &&
                end.heading_deg >= 0.0 && end.heading_deg <= 360.0 && end.length_m > 0.0 &&
                end.length_m <= longest_runway_laid_m;
            if (readable && names(state, end) && out.size() < most_contaminated_runways) {
                out.push_back({end, *code});
            }
        }
    }
    return out;
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
    c.runway_condition = runway_condition_of(m);
    return c;
}

sim::Conditions conditions_at(const WeatherReport& report, double height_msl_m) {
    sim::Conditions c = surface_conditions(report.surface);
    // JSBSim's own turbulence is left off: with_air_motion makes the air's.
    c.turbulence_severity = 0;
    c.gust_factor_kt = gust_factor_kt(report);
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

double gust_factor_kt(const WeatherReport& report) {
    const Metar& m = report.surface.metar;
    const double mean_kt = m.wind_speed_kt.value_or(0.0);
    const double spread_kt = std::max(0.0, m.gust_kt.value_or(mean_kt) - mean_kt);
    return std::max(spread_kt,
                    gust_spread_of_severity(report.turbulence_severity.value_or(0)));
}

sim::Conditions with_air_motion(const WeatherReport& report, const Lift& lift,
                                const GroundAt& ground, sim::Conditions mean,
                                double latitude_deg, double longitude_deg,
                                double height_msl_m, double time_s) {
    // **The runway is the report's only near its station**: beyond what a
    // METAR's present weather reaches, dry unless another report says so.
    if (sim::distance_m(latitude_deg, longitude_deg, report.surface.latitude_deg,
                        report.surface.longitude_deg) > metar_radius_m) {
        mean.runway_condition = sim::dry_runway;
    } else {
        // A runway's reported state, on the runway it names.
        for (const ContaminatedRunway& c : report.contaminated) {
            if (c.code < mean.runway_condition &&
                on_runway(c.end, latitude_deg, longitude_deg)) {
                mean.runway_condition = c.code;
            }
        }
    }
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
    // Rain the old report said keeps the runways wet a while after the new
    // one takes over, at the blend's halfway as the runway does.
    if (runway_condition_of(current_.surface.metar) != sim::dry_runway) {
        wet_until_s_ = now_s + 0.5 * std::max(0.0, blend_seconds_) + still_wet_after_rain_s;
    }
    // The weather as it is, its own blend and all, is what the new report
    // blends from; a blend of its that ended blend_kept_s ago is let go.
    auto was = std::make_shared<ReportedWeather>(*this);
    if (was->before_ && now_s >= was->changed_at_s_ + std::max(0.0, was->blend_seconds_) +
                                     blend_kept_s) {
        was->before_.reset();
    }
    before_ = std::move(was);
    runways_laid_ = false;
    current_ = std::move(report);
    current_lift_ = lift_of(current_);
    changed_at_s_ = now_s;
}

bool ReportedWeather::has_runway_states() const {
    return !current_.surface.metar.runway_states.empty() ||
           (before_ && !before_->current_.surface.metar.runway_states.empty());
}

void ReportedWeather::lay_runways(const std::vector<RunwayEnd>& all) {
    current_.contaminated = contaminated_runways(
        current_.surface.metar, runways_at(all, current_.surface.metar.station));
    if (before_) {
        before_->current_.contaminated = contaminated_runways(
            before_->current_.surface.metar,
            runways_at(all, before_->current_.surface.metar.station));
    }
    runways_laid_ = true;
}

void ReportedWeather::lay(std::vector<ContaminatedRunway> now,
                          std::vector<ContaminatedRunway> before, double wet_until_s) {
    current_.contaminated = std::move(now);
    if (before_) {
        before_->current_.contaminated = std::move(before);
    }
    wet_until_s_ = wet_until_s;
    runways_laid_ = true;
}

sim::Conditions ReportedWeather::at(double latitude_deg, double longitude_deg,
                                    double height_m, double time_s) {
    const double msl = geoid_ != nullptr
                           ? height_m - geoid_->undulation(latitude_deg, longitude_deg)
                           : height_m;
    sim::Conditions now =
        with_air_motion(current_, current_lift_, ground_, conditions_at(current_, msl),
                        latitude_deg, longitude_deg, msl, time_s);
    // Still wet from the rain before, near the station.
    if (time_s < wet_until_s_ && now.runway_condition == sim::dry_runway &&
        sim::distance_m(latitude_deg, longitude_deg, current_.surface.latitude_deg,
                        current_.surface.longitude_deg) <= metar_radius_m) {
        now.runway_condition = sim::wet_runway;
    }
    if (!before_ || blend_seconds_ <= 0.0) {
        return now;
    }
    const double w = std::clamp((time_s - changed_at_s_) / blend_seconds_, 0.0, 1.0);
    if (w >= 1.0) {
        return now;
    }
    const sim::Conditions before = before_->at(latitude_deg, longitude_deg, height_m, time_s);
    const auto mix = [w](double a, double b) { return a + w * (b - a); };
    sim::Conditions c;
    c.wind_north_mps = mix(before.wind_north_mps, now.wind_north_mps);
    c.wind_east_mps = mix(before.wind_east_mps, now.wind_east_mps);
    c.wind_down_mps = mix(before.wind_down_mps, now.wind_down_mps);
    c.temperature_offset_c = mix(before.temperature_offset_c, now.temperature_offset_c);
    c.sea_level_pressure_hpa =
        mix(before.sea_level_pressure_hpa, now.sea_level_pressure_hpa);
    c.wind_at_20ft_mps = mix(before.wind_at_20ft_mps, now.wind_at_20ft_mps);
    // The runway changes halfway, as a code cannot be mixed.
    c.runway_condition = w < 0.5 ? before.runway_condition : now.runway_condition;
    c.gust_factor_kt = mix(before.gust_factor_kt, now.gust_factor_kt);
    return c;
}

WeatherFetch::WeatherFetch(std::string station, std::string time, Fetch fetch,
                           std::chrono::milliseconds retry_wait)
    : report_(std::async(std::launch::async,
                         [this, station = std::move(station), time = std::move(time),
                          fetch = std::move(fetch), retry_wait] {
                             const FetchesGivenUp given_up(give_up_);
                             return fetch_weather(station, time, fetch, retry_wait);
                         })) {}

WeatherFetch::~WeatherFetch() {
    // Before report_ is destroyed, which waits for the thread.
    give_up_ = true;
}

bool WeatherFetch::done() const {
    return report_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

WeatherReport WeatherFetch::get() {
    return report_.get();
}

std::vector<double> station_weights(const std::vector<std::pair<double, double>>& stations,
                                    double latitude_deg, double longitude_deg) {
    std::vector<double> distance;
    distance.reserve(stations.size());
    double nearest = std::numeric_limits<double>::infinity();
    for (const auto& [lat, lon] : stations) {
        distance.push_back(sim::distance_m(latitude_deg, longitude_deg, lat, lon));
        nearest = std::min(nearest, distance.back());
    }
    std::vector<double> weight;
    weight.reserve(stations.size());
    double sum = 0.0;
    for (const double d : distance) {
        weight.push_back(std::max(0.0, 1.0 - (d - nearest) / station_blend_m));
        sum += weight.back();
    }
    // The nearest has a share of one before dividing, so the sum is never
    // below one.
    for (double& w : weight) {
        w /= sum;
    }
    return weight;
}

std::vector<std::pair<double, double>> StationsWeather::places() const {
    std::vector<std::pair<double, double>> out;
    out.reserve(stations_.size());
    for (const auto& s : stations_) {
        out.emplace_back(s->report().surface.latitude_deg, s->report().surface.longitude_deg);
    }
    return out;
}

std::size_t StationsWeather::nearest(double latitude_deg, double longitude_deg) const {
    const std::vector<double> w = station_weights(places(), latitude_deg, longitude_deg);
    return w.empty() ? 0
                     : static_cast<std::size_t>(std::max_element(w.begin(), w.end()) - w.begin());
}

sim::Conditions StationsWeather::at(double latitude_deg, double longitude_deg, double height_m,
                                    double time_s) {
    const std::vector<double> w = station_weights(places(), latitude_deg, longitude_deg);
    if (w.empty()) {
        return {};
    }
    const std::size_t most =
        static_cast<std::size_t>(std::max_element(w.begin(), w.end()) - w.begin());
    // **The nearer's alone, as it is**, outside the band: not a sum that
    // might differ from it in its last bit.
    if (w[most] >= 1.0) {
        return stations_[most]->at(latitude_deg, longitude_deg, height_m, time_s);
    }
    sim::Conditions c;
    c.sea_level_pressure_hpa = 0.0;
    for (std::size_t i = 0; i < stations_.size(); ++i) {
        if (w[i] <= 0.0) {
            continue;
        }
        const sim::Conditions s = stations_[i]->at(latitude_deg, longitude_deg, height_m, time_s);
        c.wind_north_mps += w[i] * s.wind_north_mps;
        c.wind_east_mps += w[i] * s.wind_east_mps;
        c.wind_down_mps += w[i] * s.wind_down_mps;
        c.temperature_offset_c += w[i] * s.temperature_offset_c;
        c.sea_level_pressure_hpa += w[i] * s.sea_level_pressure_hpa;
        c.wind_at_20ft_mps += w[i] * s.wind_at_20ft_mps;
        c.gust_factor_kt += w[i] * s.gust_factor_kt;
        if (i == most) {
            c.turbulence_severity = s.turbulence_severity;
            c.runway_condition = s.runway_condition;
        }
    }
    return c;
}

namespace {

// The same station: its name and its place.
bool same_station(const WeatherReport& a, const WeatherReport& b) {
    return a.surface.metar.station == b.surface.metar.station &&
           a.surface.latitude_deg == b.surface.latitude_deg &&
           a.surface.longitude_deg == b.surface.longitude_deg;
}

} // namespace

void StationSlots::set(std::size_t slot, std::optional<WeatherReport> report,
                       double changed_at_s, double blend_s, double set_changed_at_s) {
    if (slot >= most_stations) {
        throw std::out_of_range("a weather station's slot " + std::to_string(slot) +
                                ", of at most " + std::to_string(most_stations));
    }
    if (slots_.size() <= slot) {
        slots_.resize(slot + 1);
    }
    std::shared_ptr<ReportedWeather>& s = slots_[slot];
    if (!report && !s) {
        return;
    }
    if (report && s && same_station(s->report(), *report)) {
        s->update(std::move(*report), changed_at_s, blend_s);
        return;
    }
    // **The stations change**: once for every change at one moment, from
    // the stations as they are - mid-blend or not - and from none flown
    // whole at once.
    if (set_changed_at_s != set_changed_at_s_) {
        auto next = std::make_shared<Stations>();
        next->stations = air_->stations;
        if (any()) {
            if (air_->from &&
                set_changed_at_s >= air_->at_s + std::max(0.0, air_->blend_s) + blend_kept_s) {
                air_->from.reset();
            }
            next->from = air_;
        }
        next->at_s = set_changed_at_s;
        next->blend_s = blend_s;
        air_ = std::move(next);
        set_changed_at_s_ = set_changed_at_s;
    }
    if (report) {
        s = std::make_shared<ReportedWeather>(std::move(*report), geoid_, blend_s, ground_);
    } else {
        s.reset();
    }
    std::vector<std::shared_ptr<ReportedWeather>> flown;
    for (const auto& each : slots_) {
        if (each) {
            flown.push_back(each);
        }
    }
    air_->stations = std::make_shared<StationsWeather>(std::move(flown));
}

std::vector<WeatherReport> StationSlots::stations_flown() const {
    std::vector<WeatherReport> out;
    for (const auto& slot : slots_) {
        if (slot) {
            out.push_back(slot->report());
        }
    }
    return out;
}

std::vector<SlotChange> slot_changes(const StationSlots& slots,
                                     std::vector<WeatherReport> stations) {
    const auto& now = slots.slots();
    std::vector<std::optional<WeatherReport>> next(now.size());
    std::vector<bool> placed(stations.size(), false);
    for (std::size_t i = 0; i < now.size(); ++i) {
        if (!now[i]) {
            continue;
        }
        for (std::size_t j = 0; j < stations.size(); ++j) {
            if (!placed[j] &&
                stations[j].surface.metar.station == now[i]->report().surface.metar.station) {
                next[i] = std::move(stations[j]);
                placed[j] = true;
                break;
            }
        }
    }
    for (std::size_t j = 0; j < stations.size(); ++j) {
        if (placed[j]) {
            continue;
        }
        // An empty slot, or one being emptied, before a new one.
        std::size_t i = 0;
        while (i < next.size() && next[i]) {
            ++i;
        }
        if (i == most_stations) {
            break;
        }
        if (i == next.size()) {
            next.emplace_back();
        }
        next[i] = std::move(stations[j]);
    }
    std::vector<SlotChange> out;
    for (std::size_t i = 0; i < next.size(); ++i) {
        const bool had = i < now.size() && now[i];
        if (next[i] || had) {
            out.push_back({i, std::move(next[i])});
        }
    }
    return out;
}

StationsAround::~StationsAround() = default;

void StationsAround::had(std::vector<std::pair<double, double>> places, double now_s) {
    looked_for_ = std::move(places);
    came_at_s_ = now_s;
}

bool StationsAround::looking() const {
    return under_way_ != nullptr;
}

void StationsAround::look(const std::vector<std::pair<double, double>>& places, double now_s,
                          const Kept& kept) {
    if (under_way_) {
        return;
    }
    bool moved = places.size() != looked_for_.size();
    for (std::size_t i = 0; !moved && i < places.size(); ++i) {
        moved = sim::distance_m(places[i].first, places[i].second, looked_for_[i].first,
                                looked_for_[i].second) > restation_m;
    }
    if (moved || now_s - came_at_s_ >= refresh_s_) {
        looked_for_ = places;
        under_way_ = std::make_unique<StationsFetch>(
            places, utc_hour(std::chrono::system_clock::now()), fetch_, kept());
    }
}

std::optional<StationsFound> StationsAround::found(double now_s) {
    if (!under_way_ || !under_way_->done()) {
        return std::nullopt;
    }
    const std::unique_ptr<StationsFetch> done = std::move(under_way_);
    came_at_s_ = now_s;
    return done->get();
}

bool StationSlots::any() const {
    return std::any_of(slots_.begin(), slots_.end(), [](const auto& s) { return s != nullptr; });
}

std::shared_ptr<ReportedWeather> StationSlots::nearest(double latitude_deg,
                                                       double longitude_deg) const {
    if (air_->stations->stations().empty()) {
        return nullptr;
    }
    return air_->stations->stations()[air_->stations->nearest(latitude_deg, longitude_deg)];
}

sim::Conditions StationSlots::at(double latitude_deg, double longitude_deg, double height_m,
                                 double time_s) {
    return air_->at(latitude_deg, longitude_deg, height_m, time_s);
}

sim::Conditions StationSlots::Stations::at(double latitude_deg, double longitude_deg,
                                           double height_m, double time_s) {
    const sim::Conditions now = stations->at(latitude_deg, longitude_deg, height_m, time_s);
    if (!from || blend_s <= 0.0) {
        return now;
    }
    const double w = std::clamp((time_s - at_s) / blend_s, 0.0, 1.0);
    if (w >= 1.0) {
        return now;
    }
    const sim::Conditions before = from->at(latitude_deg, longitude_deg, height_m, time_s);
    const auto mix = [w](double a, double b) { return a + w * (b - a); };
    sim::Conditions c;
    c.wind_north_mps = mix(before.wind_north_mps, now.wind_north_mps);
    c.wind_east_mps = mix(before.wind_east_mps, now.wind_east_mps);
    c.wind_down_mps = mix(before.wind_down_mps, now.wind_down_mps);
    c.temperature_offset_c = mix(before.temperature_offset_c, now.temperature_offset_c);
    c.sea_level_pressure_hpa = mix(before.sea_level_pressure_hpa, now.sea_level_pressure_hpa);
    c.wind_at_20ft_mps = mix(before.wind_at_20ft_mps, now.wind_at_20ft_mps);
    c.gust_factor_kt = mix(before.gust_factor_kt, now.gust_factor_kt);
    // Codes that cannot be mixed change halfway, as a report's do.
    c.runway_condition = w < 0.5 ? before.runway_condition : now.runway_condition;
    c.turbulence_severity = w < 0.5 ? before.turbulence_severity : now.turbulence_severity;
    return c;
}

StationsFound fetch_stations_weather(const std::vector<std::pair<double, double>>& places,
                                     const std::string& time, const Fetch& fetch,
                                     const std::vector<WeatherReport>& kept,
                                     std::chrono::milliseconds retry_wait) {
    StationsFound out;
    std::vector<SurfaceReport> reports;
    std::vector<std::pair<double, double>> answered;
    std::vector<std::pair<double, double>> not_answered;
    std::vector<std::pair<double, double>> looked;
    for (const auto& [lat, lon] : places) {
        const bool near_one = std::any_of(looked.begin(), looked.end(), [&](const auto& p) {
            return sim::distance_m(lat, lon, p.first, p.second) <= 1000.0;
        });
        if (near_one) {
            // As the place it is near was answered, or not.
            const bool that_answered =
                std::any_of(answered.begin(), answered.end(), [&](const auto& p) {
                    return sim::distance_m(lat, lon, p.first, p.second) <= 1000.0;
                });
            (that_answered ? answered : not_answered).emplace_back(lat, lon);
            continue;
        }
        looked.emplace_back(lat, lon);
        char where[64];
        std::snprintf(where, sizeof(where), "%.4f,%.4f", lat, lon);
        try {
            std::vector<SurfaceReport> here = fetch_metars_around(lat, lon, fetch, retry_wait);
            if (here.empty()) {
                out.unanswered.push_back(std::string("around ") + where + ": no report");
                not_answered.emplace_back(lat, lon);
                continue;
            }
            reports.insert(reports.end(), std::make_move_iterator(here.begin()),
                           std::make_move_iterator(here.end()));
            answered.emplace_back(lat, lon);
        } catch (const std::exception& e) {
            out.unanswered.push_back(std::string("around ") + where + ": " + e.what());
            not_answered.emplace_back(lat, lon);
        }
    }
    const auto kept_named = [&kept](const std::string& station) -> const WeatherReport* {
        for (const WeatherReport& k : kept) {
            if (k.surface.metar.station == station) {
                return &k;
            }
        }
        return nullptr;
    };
    for (SurfaceReport& surface : stations_around(reports, answered)) {
        WeatherReport r;
        r.surface = std::move(surface);
        r.air_seed = air_seed_of(r.surface.metar);
        try {
            r.aloft = fetch_winds_aloft(r.surface.latitude_deg, r.surface.longitude_deg, time,
                                        fetch, retry_wait);
        } catch (const std::exception& e) {
            const WeatherReport* had = kept_named(r.surface.metar.station);
            if (had != nullptr) {
                r.aloft = had->aloft;
            }
            out.unanswered.push_back("the forecast over " + r.surface.metar.station + ": " +
                                     e.what());
        }
        out.stations.push_back(std::move(r));
    }
    // **The places that answered nothing keep what they had.**
    if (!not_answered.empty()) {
        std::vector<SurfaceReport> kept_surfaces;
        for (const WeatherReport& k : kept) {
            kept_surfaces.push_back(k.surface);
        }
        for (const SurfaceReport& surface : stations_around(kept_surfaces, not_answered)) {
            const bool have = std::any_of(
                out.stations.begin(), out.stations.end(), [&surface](const WeatherReport& r) {
                    return r.surface.metar.station == surface.metar.station;
                });
            if (!have && out.stations.size() < most_stations) {
                out.stations.push_back(*kept_named(surface.metar.station));
            }
        }
    }
    if (answered.empty() && kept.empty() && !places.empty()) {
        std::string why = "the weather could not be had: ";
        for (const std::string& w : out.unanswered) {
            why += w + "; ";
        }
        throw ServiceUnavailable(why);
    }
    return out;
}

StationsFetch::StationsFetch(std::vector<std::pair<double, double>> places, std::string time,
                             Fetch fetch, std::vector<WeatherReport> kept,
                             std::chrono::milliseconds retry_wait)
    : reports_(std::async(std::launch::async,
                          [this, places = std::move(places), time = std::move(time),
                           fetch = std::move(fetch), kept = std::move(kept), retry_wait] {
                              const FetchesGivenUp given_up(give_up_);
                              return fetch_stations_weather(places, time, fetch, kept,
                                                            retry_wait);
                          })) {}

StationsFetch::~StationsFetch() {
    // Before reports_ is destroyed, which waits for the thread.
    give_up_ = true;
}

bool StationsFetch::done() const {
    return reports_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

StationsFound StationsFetch::get() {
    return reports_.get();
}

} // namespace glideslope::world
