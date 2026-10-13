#include "frontend/same_air.hpp"

#include "world/digest.hpp"
#include "world/metar.hpp"
#include "world/runway_ground.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace glideslope::frontend {

StationRunways::StationRunways(std::filesystem::path cache, world::Fetch fetch)
    : load_([cache = std::move(cache), fetch = std::move(fetch)] {
          return world::world_runways(cache, fetch);
      }) {}

StationRunways::~StationRunways() {
    // Before loading_ is destroyed, which waits for its thread.
    give_up_ = true;
}

bool StationRunways::lay(world::ReportedWeather& air, std::string* why) {
    if (air.runways_laid() || !air.has_runway_states() || cannot_) {
        return false;
    }
    if (!all_) {
        if (!loading_.valid()) {
            loading_ = std::async(std::launch::async, [this] {
                const world::FetchesGivenUp given_up(give_up_);
                return std::make_shared<const std::vector<world::RunwayEnd>>(load_());
            });
            return false;
        }
        if (loading_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }
        try {
            all_ = loading_.get();
        } catch (const std::exception& e) {
            cannot_ = true;
            if (why != nullptr) {
                *why = std::string("the runways cannot be had, and a runway's reported state "
                                   "lies on none: ") +
                       e.what();
            }
            return false;
        }
    }
    air.lay_runways(*all_);
    return true;
}

namespace {

net::RunwayCondition condition_said(const world::ContaminatedRunway& c) {
    return {c.end.latitude_deg, c.end.longitude_deg, c.end.heading_deg, c.end.length_m,
            static_cast<std::uint8_t>(c.code)};
}

world::ContaminatedRunway condition_heard(const net::RunwayCondition& c) {
    world::ContaminatedRunway out;
    out.end.latitude_deg = c.latitude_deg;
    out.end.longitude_deg = c.longitude_deg;
    out.end.heading_deg = c.heading_deg;
    out.end.length_m = c.length_m;
    out.code = c.code;
    return out;
}

} // namespace

net::RunwayConditions runway_conditions_said(const world::ReportedWeather& air,
                                             double changed_at_s, double now_s) {
    net::RunwayConditions out;
    out.weather_changed_at_s = changed_at_s;
    if (std::isfinite(air.wet_until_s()) && air.wet_until_s() >= 0.0) {
        out.wet_until_s = air.wet_until_s();
    }
    for (const world::ContaminatedRunway& c : air.report().contaminated) {
        out.now.push_back(condition_said(c));
    }
    if (const world::WeatherReport* before = air.blending_from(now_s)) {
        for (const world::ContaminatedRunway& c : before->contaminated) {
            out.before.push_back(condition_said(c));
        }
    }
    return out;
}

bool HeardAir::conditions_heard(const net::RunwayConditions& conditions) {
    if (!reported_ || still_ || conditions.weather_changed_at_s != changed_at_s_) {
        return false;
    }
    std::vector<world::ContaminatedRunway> now;
    std::vector<world::ContaminatedRunway> before;
    for (const net::RunwayCondition& c : conditions.now) {
        now.push_back(condition_heard(c));
    }
    for (const net::RunwayCondition& c : conditions.before) {
        before.push_back(condition_heard(c));
    }
    reported_->lay(std::move(now), std::move(before),
                   conditions.wet_until_s.value_or(-std::numeric_limits<double>::infinity()));
    return true;
}

world::WeatherReport fit_to_send(world::WeatherReport report) {
    std::string raw = report.surface.metar.raw;
    if (raw.size() <= net::most_metar_bytes) {
        return report;
    }
    while (raw.size() > net::most_metar_bytes) {
        const std::size_t space = raw.rfind(' ');
        raw.resize(space == std::string::npos ? 0 : space);
    }
    report.surface.metar = world::parse_metar(raw);
    return report;
}

WeatherSaid weather_said(const world::WeatherReport* report, double changed_at_s,
                         double blend_s) {
    WeatherSaid out;
    out.weather.changed_at_s = changed_at_s;
    out.weather.blend_s = blend_s;
    if (report == nullptr) {
        return out;
    }
    net::Weather& w = out.weather;
    w.metar = report->surface.metar.raw;
    w.latitude_deg = report->surface.latitude_deg;
    w.longitude_deg = report->surface.longitude_deg;
    w.elevation_m = report->surface.elevation_m;
    if (report->turbulence_severity) {
        w.turbulence_severity = static_cast<std::uint8_t>(*report->turbulence_severity);
    }
    w.air_seed = report->air_seed;
    for (const world::Microburst& b : report->microbursts) {
        w.microbursts.push_back(
            {b.latitude_deg, b.longitude_deg, b.radius_m, b.downdraught_mps, b.start_s,
             b.duration_s});
    }
    if (report->aloft) {
        w.aloft_follows = true;
        net::WeatherAloft a;
        a.time = report->aloft->time;
        for (const world::AloftLevel& l : report->aloft->levels) {
            a.levels.push_back(
                {l.pressure_hpa, l.height_m, l.wind_north_mps, l.wind_east_mps, l.temperature_c});
        }
        for (const world::NearGroundWind& g : report->aloft->near_ground) {
            a.near_ground.push_back({g.height_m, g.wind_north_mps, g.wind_east_mps});
        }
        out.aloft = std::move(a);
    }
    return out;
}

std::optional<world::WeatherReport> weather_heard(const net::Weather& weather,
                                                  const std::optional<net::WeatherAloft>& aloft) {
    if (weather.metar.empty()) {
        return std::nullopt;
    }
    world::WeatherReport r;
    r.surface.metar = world::parse_metar(weather.metar);
    r.surface.latitude_deg = weather.latitude_deg;
    r.surface.longitude_deg = weather.longitude_deg;
    r.surface.elevation_m = weather.elevation_m;
    if (weather.turbulence_severity) {
        r.turbulence_severity = static_cast<int>(*weather.turbulence_severity);
    }
    r.air_seed = weather.air_seed;
    for (const net::Microburst& b : weather.microbursts) {
        world::Microburst m;
        m.latitude_deg = b.latitude_deg;
        m.longitude_deg = b.longitude_deg;
        m.radius_m = b.radius_m;
        m.downdraught_mps = b.downdraught_mps;
        m.start_s = b.start_s;
        m.duration_s = b.duration_s;
        r.microbursts.push_back(m);
    }
    if (aloft) {
        world::WindsAloft a;
        // The forecast is the station's: Open-Meteo's is asked for there.
        a.latitude_deg = weather.latitude_deg;
        a.longitude_deg = weather.longitude_deg;
        a.time = aloft->time;
        for (const net::AloftLevel& l : aloft->levels) {
            a.levels.push_back(
                {l.pressure_hpa, l.height_m, l.wind_north_mps, l.wind_east_mps, l.temperature_c});
        }
        for (const net::NearGroundWind& g : aloft->near_ground) {
            a.near_ground.push_back({g.height_m, g.wind_north_mps, g.wind_east_mps});
        }
        r.aloft = std::move(a);
    }
    return r;
}

bool HeardAir::heard(const net::Weather& weather, const std::optional<net::WeatherAloft>& aloft,
                     std::string* why) {
    std::optional<world::WeatherReport> report;
    try {
        report = weather_heard(weather, aloft);
    } catch (const world::MetarError& e) {
        if (why != nullptr) {
            *why = e.what();
        }
        return false;
    }
    if (!report) {
        if (air_) {
            air_ = std::make_shared<sim::SteadyWeather>(sim::Conditions{});
        }
        still_ = true;
        return true;
    }
    changed_at_s_ = weather.changed_at_s;
    if (!reported_ || still_) {
        reported_ = std::make_shared<world::ReportedWeather>(std::move(*report), geoid_,
                                                             weather.blend_s, ground_);
    } else {
        reported_->update(std::move(*report), weather.changed_at_s, weather.blend_s);
    }
    still_ = false;
    air_ = std::make_shared<SessionClocked>(reported_, clock_);
    return true;
}

namespace {

std::string file_sha256(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot read " + file.string());
    }
    const std::string text{std::istreambuf_iterator<char>(in), {}};
    return world::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

} // namespace

net::TerrainDataset collision_dataset(const std::filesystem::path& data) {
    const std::string coverage = file_sha256(data / "dem" / "coverage.txt");
    const std::string strips = file_sha256(data / "runways" / "strips.csv");
    const std::string rules = std::to_string(world::collision_ground_rules);
    const std::string lines = "coverage.txt " + coverage + "\nstrips.csv " + strips +
                              "\nground rules " + rules + "\n";
    const std::string hex = world::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(lines.data()), lines.size()));
    net::TerrainDataset d;
    d.name = "Copernicus DEM GLO-30 and GLO-90, with runway strips";
    d.version = "coverage " + coverage.substr(0, 8) + ", strips " + strips.substr(0, 8) +
                ", ground rules " + rules;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        d.sha256.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return d;
}

std::string hash_hex(const net::TerrainDataset& dataset) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (const std::uint8_t b : dataset.sha256) {
        out += digits[b >> 4];
        out += digits[b & 0x0F];
    }
    return out;
}

std::string describe(const net::TerrainDataset& dataset) {
    return dataset.name + " (" + dataset.version + "), SHA-256 " + hash_hex(dataset);
}

bool same_ground(const net::TerrainDataset& a, const net::TerrainDataset& b) {
    // The hash is what is compared; the name and version are for a person.
    return a.sha256 == b.sha256;
}

} // namespace glideslope::frontend
