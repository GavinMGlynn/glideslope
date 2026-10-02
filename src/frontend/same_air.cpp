#include "frontend/same_air.hpp"

#include "world/digest.hpp"
#include "world/metar.hpp"
#include "world/runway_ground.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace glideslope::frontend {

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

GroundForAir::GroundForAir(const std::filesystem::path& data, const std::filesystem::path& cache) {
    std::ifstream in(data / "dem" / "coverage.txt", std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot read " + (data / "dem" / "coverage.txt").string());
    }
    coverage_ = std::make_unique<world::DemCoverage>(std::string(std::istreambuf_iterator<char>(in), {}));
    fetch_ = world::http_fetch();
    tiles_ = std::make_unique<world::DownloadedTiles>(cache, fetch_);
    geoid_ = std::make_unique<world::Geoid>(world::egm2008_geoid(cache, fetch_));
    collision_ = std::make_shared<world::CollisionGround>(
        std::make_shared<world::Dem>(*coverage_, *tiles_, geoid_.get()), world::runway_surfaces(data));
}

world::GroundAt GroundForAir::ground() const {
    const std::shared_ptr<world::CollisionGround> collision = collision_;
    return [collision](double lat, double lon) { return collision->height_above_geoid(lat, lon); };
}

void HeardAir::heard(const net::Weather& weather, const std::optional<net::WeatherAloft>& aloft) {
    std::optional<world::WeatherReport> report = weather_heard(weather, aloft);
    if (!report) {
        if (air_) {
            air_ = std::make_shared<sim::SteadyWeather>(sim::Conditions{});
        }
        still_ = true;
        return;
    }
    if (!reported_ || still_) {
        reported_ = std::make_shared<world::ReportedWeather>(std::move(*report), geoid_,
                                                             weather.blend_s, ground_);
    } else {
        reported_->update(std::move(*report), weather.changed_at_s, weather.blend_s);
    }
    still_ = false;
    air_ = std::make_shared<SessionClocked>(reported_, clock_);
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
