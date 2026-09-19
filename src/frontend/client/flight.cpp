#include "flight.hpp"

#include "sim/terrain.hpp"
#include "world/download.hpp"
#include "world/geodesy.hpp"
#include "world/winds_aloft.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace glideslope::client {

namespace {

constexpr double radians = 3.14159265358979323846 / 180.0;
constexpr double feet_per_metre = 1.0 / 0.3048;

world::Ecef add(const world::Ecef& a, const world::Ecef& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

world::Ecef scale(const world::Ecef& a, double k) {
    return {a.x * k, a.y * k, a.z * k};
}

} // namespace

Flight::Flight(const std::filesystem::path& data, const std::filesystem::path& cache,
               const FlightStart& flight_start) {
    FlightStart start = flight_start;
    std::ifstream coverage_file(data / "dem" / "coverage.txt", std::ios::binary);
    if (!coverage_file) {
        throw std::runtime_error("cannot read " +
                                 (data / "dem" / "coverage.txt").string());
    }
    coverage_ = std::make_unique<world::DemCoverage>(
        std::string(std::istreambuf_iterator<char>(coverage_file), {}));
    fetch_ = world::http_fetch();
    tiles_ = std::make_unique<world::DownloadedTiles>(cache, fetch_);
    geoid_ = std::make_unique<world::Geoid>(world::egm2008_geoid(cache, fetch_));
    dem_ = std::make_shared<world::Dem>(*coverage_, *tiles_, geoid_.get());

    // A plan's altitudes are above sea level; the aircraft's, the ellipsoid.
    if (start.plan) {
        plan_ = start.plan;
        for (sim::Waypoint& w : plan_->waypoints) {
            w.altitude_ft +=
                geoid_->undulation(w.latitude_deg, w.longitude_deg) * feet_per_metre;
        }
        if (const auto& from = plan_->start) {
            start.latitude_deg = from->latitude_deg;
            start.longitude_deg = from->longitude_deg;
            start.height_m =
                from->altitude_ft / feet_per_metre +
                geoid_->undulation(from->latitude_deg, from->longitude_deg);
            start.heading_deg = from->heading_deg;
            start.airspeed_kts = from->airspeed_kts;
        }
        start.autopilot = true;
    }
    start_with_ai_ = start.autopilot;

    aircraft_entry_ = sim::find_aircraft(data, start.aircraft);
    aircraft_ = std::make_unique<sim::Aircraft>(data / "jsbsim", aircraft_entry_.model);
    const std::shared_ptr<world::Dem> dem = dem_;
    aircraft_->set_terrain(std::make_shared<sim::FunctionTerrain>(
        [dem](double lat, double lon) { return dem->height_above_ellipsoid(lat, lon); },
        [dem](double lat, double lon) {
            return dem->water(lat, lon) != world::Water::none;
        }));
    sim::InitialConditions ic;
    ic.latitude_deg = start.latitude_deg;
    ic.longitude_deg = start.longitude_deg;
    ic.altitude_ft = start.height_m * feet_per_metre;
    ic.heading_deg = start.heading_deg;
    ic.airspeed_kts = start.airspeed_kts.value_or(aircraft_entry_.start_airspeed_kts);
    ic.engine_running = true;
    ic.gear = 0.0; // begun in the air, with its wheels up
    if (start.on_ground) {
        // Standing on the DEM, its wheels down: sim::Aircraft raises it by
        // their springs' compression.
        ic.altitude_ft =
            dem_->height_above_ellipsoid(start.latitude_deg, start.longitude_deg) *
            feet_per_metre;
        ic.airspeed_kts = 0.0;
        ic.gear = 1.0;
    }
    aircraft_->initialize(ic);

    if (!start.weather_station.empty()) {
        weather_station_ = start.weather_station;
        world::WeatherReport report = world::fetch_weather(
            weather_station_, world::utc_hour(std::chrono::system_clock::now()),
            fetch_);
        report.microbursts = start.microbursts;
        microbursts_ = start.microbursts;
        // The air rises and sinks over the same ground the aircraft meets.
        weather_ = std::make_shared<world::ReportedWeather>(
            std::move(report), geoid_.get(), weather_blend_seconds,
            [dem](double lat, double lon) {
                return dem->height_above_geoid(lat, lon);
            });
        aircraft_->set_weather(weather_);
    }
}

void Flight::step(const sim::Controls& controls) {
    if (!controller_) {
        controller_ = std::make_unique<sim::Controller>(*aircraft_, controls);
        if (start_with_ai_) {
            swap_pilot();
        }
    }
    controller_->set_pilot(controls);
    aircraft_->set_controls(controller_->fly());
    aircraft_->step();
    ++tick_;
    report_navigation();
    if (weather_) {
        refresh_weather();
    }
}

void Flight::swap_pilot() {
    if (!controller_) {
        start_with_ai_ = !start_with_ai_;
        return;
    }
    if (ai_flying()) {
        controller_->to_pilot();
        return;
    }
    if (plan_ && passed_ < plan_->waypoints.size()) {
        // What is left of it.
        sim::FlightPlan rest = *plan_;
        rest.waypoints.erase(rest.waypoints.begin(),
                             rest.waypoints.begin() +
                                 static_cast<std::ptrdiff_t>(passed_));
        controller_->to_ai(rest);
        handed_from_ = passed_;
        closest_m_ = std::numeric_limits<double>::infinity();
    } else {
        controller_->to_ai();
    }
}

void Flight::report_navigation() {
    const sim::Navigator* nav = controller_ ? controller_->navigator() : nullptr;
    if (nav == nullptr || !plan_ || plan_flown_) {
        return;
    }
    const auto sea_level_ft = [&](double lat, double lon, double ellipsoid_ft) {
        return ellipsoid_ft - geoid_->undulation(lat, lon) * feet_per_metre;
    };
    // The navigator counts from where it was handed the plan.
    const std::size_t next = handed_from_ + nav->next();
    while (passed_ < next) {
        const sim::Waypoint& w = plan_->waypoints[passed_];
        std::printf("glideslope: passed %s %.1f m off at %.0f ft, tick %lld\n",
                    w.name.c_str(), closest_m_,
                    sea_level_ft(w.latitude_deg, w.longitude_deg, altitude_there_ft_),
                    static_cast<long long>(tick_));
        ++passed_;
        closest_m_ = std::numeric_limits<double>::infinity();
    }
    if (passed_ >= plan_->waypoints.size()) {
        std::printf("glideslope: the plan is flown, at tick %lld\n",
                    static_cast<long long>(tick_));
        plan_flown_ = true;
        return;
    }
    const sim::Waypoint& w = plan_->waypoints[passed_];
    const double d = sim::distance_m(aircraft_->property("position/lat-geod-deg"),
                                     aircraft_->property("position/long-gc-deg"),
                                     w.latitude_deg, w.longitude_deg);
    if (!(d >= closest_m_)) {
        closest_m_ = d;
        altitude_there_ft_ = aircraft_->property("position/h-sl-ft");
    }
}

void Flight::refresh_weather() {
    const double now = aircraft_->state().sim_time_s;
    if (next_weather_.valid()) {
        if (next_weather_.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready) {
            return;
        }
        try {
            world::WeatherReport report = next_weather_.get();
            report.microbursts = microbursts_;
            weather_->update(std::move(report), now);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "glideslope: the weather is not updated: %s\n",
                         e.what());
        }
        weather_fetched_at_s_ = now;
        return;
    }
    if (now - weather_fetched_at_s_ >= weather_refresh_seconds) {
        next_weather_ = std::async(
            std::launch::async, [station = weather_station_, fetch = fetch_] {
                return world::fetch_weather(
                    station, world::utc_hour(std::chrono::system_clock::now()), fetch);
            });
    }
}

gfx::Camera Flight::camera() const {
    const sim::AircraftState s = aircraft_->state();
    const double lat = s.latitude_deg * radians;
    const double lon = s.longitude_deg * radians;
    const world::Ecef north{-std::sin(lat) * std::cos(lon),
                            -std::sin(lat) * std::sin(lon), std::cos(lat)};
    const world::Ecef east{-std::sin(lon), std::cos(lon), 0.0};
    const world::Ecef down{-std::cos(lat) * std::cos(lon),
                           -std::cos(lat) * std::sin(lon), -std::sin(lat)};
    const auto ned = [&](double n, double e, double d) {
        return add(add(scale(north, n), scale(east, e)), scale(down, d));
    };
    const double cp = std::cos(s.heading_deg * radians);
    const double sp = std::sin(s.heading_deg * radians);
    const double ct = std::cos(s.pitch_deg * radians);
    const double st = std::sin(s.pitch_deg * radians);
    const double cr = std::cos(s.roll_deg * radians);
    const double sr = std::sin(s.roll_deg * radians);
    // The body's axes in north-east-down: forward, right and down.
    const world::Ecef forward = ned(ct * cp, ct * sp, -st);
    const world::Ecef right =
        ned(sr * st * cp - cr * sp, sr * st * sp + cr * cp, sr * ct);
    const world::Ecef body_down =
        ned(cr * st * cp + sr * sp, cr * st * sp - sr * cp, cr * ct);

    gfx::Camera camera;
    camera.position =
        world::to_ecef({s.latitude_deg, s.longitude_deg,
                        aircraft_->property("position/geod-alt-ft") * 0.3048});
    camera.world_from_camera =
        gfx::Mat3::columns(right, scale(body_down, -1.0), scale(forward, -1.0));
    camera.near_m = 0.3;
    return camera;
}

gfx::HudReadings Flight::hud() const {
    const sim::AircraftState s = aircraft_->state();
    gfx::HudReadings r;
    r.airspeed_kts = s.airspeed_kts;
    r.altitude_ft = sea_level_ft();
    r.heading_deg = s.heading_deg;
    r.vertical_speed_fpm = s.climb_rate_fpm;
    r.pitch_deg = s.pitch_deg;
    r.roll_deg = s.roll_deg;
    r.mach = aircraft_->property("velocities/mach");
    r.pressure_altitude_ft = aircraft_->property("atmosphere/pressure-altitude");
    if (ai_flying()) {
        const sim::Navigator* nav = controller_->navigator();
        if (nav != nullptr && plan_ && passed_ < plan_->waypoints.size()) {
            r.autopilot = "NAV " + plan_->waypoints[passed_].name;
        } else {
            r.autopilot = "HOLD";
        }
    }
    if (weather_) {
        r.credits.push_back(world::open_meteo_credit);
    }
    return r;
}

const world::WeatherReport* Flight::weather_report() const {
    return weather_ ? &weather_->report() : nullptr;
}

gfx::Station Flight::weather_station() const {
    gfx::Station s;
    if (weather_) {
        const world::SurfaceReport& surface = weather_->report().surface;
        s.latitude_deg = surface.latitude_deg;
        s.longitude_deg = surface.longitude_deg;
        s.elevation_m = surface.elevation_m;
        s.geoid_m = geoid_->undulation(surface.latitude_deg, surface.longitude_deg);
    }
    return s;
}

double Flight::sea_level_ft() const {
    const sim::AircraftState s = aircraft_->state();
    return s.altitude_ft -
           geoid_->undulation(s.latitude_deg, s.longitude_deg) * feet_per_metre;
}

std::string Flight::trace() const {
    const sim::AircraftState s = aircraft_->state();
    char line[512];
    std::snprintf(line, sizeof line,
                  "trace tick %lld time %.4f lat %.7f lon %.7f alt_ft %.3f ell_ft %.3f "
                  "agl_ft %.3f kcas %.3f heading %.3f vs_fpm %.3f pitch %.3f roll %.3f "
                  "wind_north_fps %.3f wind_east_fps %.3f wind_down_fps %.3f "
                  "mach %.4f pa_ft %.3f",
                  static_cast<long long>(tick_), s.sim_time_s, s.latitude_deg,
                  s.longitude_deg, sea_level_ft(), s.altitude_ft,
                  s.height_above_ground_ft, s.airspeed_kts, s.heading_deg,
                  s.climb_rate_fpm, s.pitch_deg, s.roll_deg,
                  aircraft_->property("atmosphere/wind-north-fps"),
                  aircraft_->property("atmosphere/wind-east-fps"),
                  aircraft_->property("atmosphere/wind-down-fps"),
                  aircraft_->property("velocities/mach"),
                  aircraft_->property("atmosphere/pressure-altitude"));
    return line;
}

} // namespace glideslope::client
