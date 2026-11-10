#include "frontend/same_air.hpp"

#include <cstdio>

#include <algorithm>

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

StationRunways::StationRunways(std::filesystem::path cache, world::Fetch fetch,
                               std::chrono::steady_clock::duration retry_after)
    : load_([cache = std::move(cache), fetch = std::move(fetch)] {
          return world::world_runways(cache, fetch);
      }),
      retry_after_(retry_after) {}

StationRunways::~StationRunways() {
    // Before loading_ is destroyed, which waits for its thread.
    give_up_ = true;
}

bool StationRunways::lay(world::ReportedWeather& air, std::string* why) {
    if (air.runways_laid() || !air.has_runway_states()) {
        return false;
    }
    if (!all_) {
        if (failed_at_ && std::chrono::steady_clock::now() - *failed_at_ < retry_after_) {
            return false;
        }
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
            failed_at_ = std::chrono::steady_clock::now();
            if (why != nullptr) {
                *why = std::string("the runways cannot be had, and a runway's reported state "
                                   "lies on none until they are asked for again: ") +
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

} // namespace

world::ContaminatedRunway condition_heard(const net::RunwayCondition& c) {
    world::ContaminatedRunway out;
    out.end.latitude_deg = c.latitude_deg;
    out.end.longitude_deg = c.longitude_deg;
    out.end.heading_deg = c.heading_deg;
    out.end.length_m = c.length_m;
    out.code = c.code;
    return out;
}

net::RunwayConditions runway_conditions_said(const world::ReportedWeather& air,
                                             double changed_at_s, double now_s,
                                             std::uint8_t station) {
    net::RunwayConditions out;
    out.station = station;
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
    if (!slots_ || conditions.station >= slots_->slots().size() ||
        !slots_->slots()[conditions.station]) {
        return false;
    }
    const auto changed = changed_at_s_.find(conditions.station);
    if (changed == changed_at_s_.end() || conditions.weather_changed_at_s != changed->second) {
        return false;
    }
    world::ReportedWeather& reported = *slots_->slots()[conditions.station];
    std::vector<world::ContaminatedRunway> now;
    std::vector<world::ContaminatedRunway> before;
    for (const net::RunwayCondition& c : conditions.now) {
        now.push_back(condition_heard(c));
    }
    for (const net::RunwayCondition& c : conditions.before) {
        before.push_back(condition_heard(c));
    }
    reported.lay(std::move(now), std::move(before),
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
                         double blend_s, std::uint8_t station, double stations_changed_at_s) {
    WeatherSaid out;
    out.weather.station = station;
    out.weather.stations_changed_at_s = stations_changed_at_s;
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
        a.station = station;
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
    // **Still air from the start is no weather at all**, as on a server
    // given none; after a station, a slot emptied, as the server's is.
    if (!report && !slots_) {
        return true;
    }
    if (!slots_) {
        slots_ = std::make_shared<world::StationSlots>(geoid_, ground_);
        air_ = std::make_shared<SessionClocked>(slots_, clock_);
    }
    changed_at_s_[weather.station] = weather.changed_at_s;
    slots_->set(weather.station, std::move(report), weather.changed_at_s, weather.blend_s,
                weather.stations_changed_at_s);
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

void AirFlown::note(const sim::Aircraft& aircraft, const world::StationSlots& stations) {
    if (!stations.any()) {
        return;
    }
    constexpr double mps_per_fps = 0.3048;
    const std::array<double, 3> wind{aircraft.property("atmosphere/wind-north-fps") * mps_per_fps,
                                     aircraft.property("atmosphere/wind-east-fps") * mps_per_fps,
                                     aircraft.property("atmosphere/wind-down-fps") * mps_per_fps};
    if (last_wind_) {
        worst_wind_step_mps_ = std::max(
            worst_wind_step_mps_, std::hypot(wind[0] - (*last_wind_)[0], wind[1] - (*last_wind_)[1],
                                             wind[2] - (*last_wind_)[2]));
    }
    last_wind_ = wind;
    std::vector<std::pair<double, double>> places;
    std::vector<std::string> names;
    for (const auto& slot : stations.slots()) {
        if (slot) {
            places.emplace_back(slot->report().surface.latitude_deg,
                                slot->report().surface.longitude_deg);
            names.push_back(slot->report().surface.metar.station);
        }
    }
    const std::vector<double> shares =
        world::station_weights(places, aircraft.property("position/lat-geod-deg"),
                               aircraft.property("position/long-gc-deg"));
    const auto most =
        static_cast<std::size_t>(std::max_element(shares.begin(), shares.end()) - shares.begin());
    if (steps_ == 0) {
        first_station_ = names[most];
        first_share_ = shares[most];
    }
    double first_share = 0.0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == first_station_) {
            first_share = shares[i];
        }
    }
    least_first_share_ = std::min(least_first_share_, first_share);
    last_station_ = names[most];
    last_share_ = shares[most];
    ++steps_;
}

std::string AirFlown::said() const {
    char line[320];
    std::snprintf(line, sizeof line,
                  "air flown: %zu steps, the wind changing by at most %.5f m/s in one; most of "
                  "it %s's at the first (%.3f of it), %s's at the last (%.3f of it), the "
                  "first's share at least %.3f",
                  steps_, worst_wind_step_mps_, first_station_.c_str(), first_share_,
                  last_station_.c_str(), last_share_, least_first_share_);
    return line;
}

} // namespace glideslope::frontend
