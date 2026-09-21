#include "flight.hpp"

#include "gfx/terrain_colour.hpp"
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
#include <map>
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

    // Its checklists. Every aircraft in the roster ships them and a test
    // holds that, so a missing file is a fault - but not one worth ending a
    // flight over, and the screen simply has none to show.
    try {
        checklist_.emplace(sim::find_checklists(data, aircraft_entry_.id));
    } catch (const sim::ChecklistError&) {
        checklist_.reset();
    }

    // Its visual model, where it ships one: two aircraft do not, because
    // FlightGear has no Learjet 35A and no F-35A, and docs/ASSETS.md says so.
    // Where there is one, there is an alignment saying where it sits on this
    // aeroplane, and a model without one is a mistake rather than an absence.
    const std::filesystem::path mesh =
        data / "models" / (aircraft_entry_.id + ".mesh");
    if (std::filesystem::exists(mesh)) {
        model_ = gfx::read_model(mesh);
        model_radius_ = gfx::model_radius(*model_);
        const std::map<std::string, gfx::ModelAlignment> aligned =
            gfx::read_alignments(data / "models" / "alignment.txt");
        const auto found = aligned.find(aircraft_entry_.id);
        if (found == aligned.end()) {
            throw std::runtime_error(aircraft_entry_.id +
                                     " has a visual model and no alignment in "
                                     "models/alignment.txt");
        }
        alignment_ = found->second;
    }
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
        // Where the DEM's mask says water, only a seaplane can stand: it floats,
        // where a landplane would ditch as it was put there.
        const bool water = dem_->water(start.latitude_deg, start.longitude_deg) !=
                           world::Water::none;
        if (water && !aircraft_entry_.seaplane) {
            throw std::runtime_error("the " + aircraft_entry_.name +
                                     " is a landplane, and --at is on water, where it would "
                                     "ditch; stand it on land");
        }
        afloat_at_start_ = water;
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
    if (checklist_) {
        checklist_->update(*aircraft_, tick_);
    }
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

Flight::Axes Flight::axes() const {
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
    Axes a;
    a.forward = ned(ct * cp, ct * sp, -st);
    a.right = ned(sr * st * cp - cr * sp, sr * st * sp + cr * cp, sr * ct);
    a.down = ned(cr * st * cp + sr * sp, cr * st * sp - sr * cp, cr * ct);
    a.position =
        world::to_ecef({s.latitude_deg, s.longitude_deg,
                        aircraft_->property("position/geod-alt-ft") * 0.3048});
    return a;
}

std::array<double, 3> Flight::from_model_origin(const char* what) const {
    // JSBSim's structural frame is +x aft, +y starboard, +z up, in inches;
    // the body frame is +x forward, +y starboard, +z down, in metres. The
    // model's origin is the visual reference point moved by the alignment,
    // so a structural point is measured from the reference point and the
    // alignment taken off it.
    constexpr double metres_per_inch = 0.0254;
    const auto inches = [&](const std::string& name) {
        return std::array<double, 3>{aircraft_->property("metrics/" + name + "-x-in"),
                                     aircraft_->property("metrics/" + name + "-y-in"),
                                     aircraft_->property("metrics/" + name + "-z-in")};
    };
    const std::array<double, 3> point = inches(what);
    const std::array<double, 3> vrp = inches("visualrefpoint");
    return {-(point[0] - vrp[0]) * metres_per_inch - alignment_.offset[0],
            (point[1] - vrp[1]) * metres_per_inch - alignment_.offset[1],
            -(point[2] - vrp[2]) * metres_per_inch - alignment_.offset[2]};
}

gfx::Placement Flight::model_placement() const {
    const Axes a = axes();
    // JSBSim reports the aircraft's position at its centre of gravity; the
    // model is drawn about the visual reference point, moved by the
    // alignment. See gfx/model.hpp.
    constexpr double metres_per_inch = 0.0254;
    const auto at = [&](const char* name, const char* axis) {
        return aircraft_->property(std::string("metrics/") + name + axis);
    };
    const double cg_x = aircraft_->property("inertia/cg-x-in");
    const double cg_y = aircraft_->property("inertia/cg-y-in");
    const double cg_z = aircraft_->property("inertia/cg-z-in");
    const std::array<double, 3> origin_in_body{
        -(at("visualrefpoint", "-x-in") - cg_x) * metres_per_inch +
            alignment_.offset[0],
        (at("visualrefpoint", "-y-in") - cg_y) * metres_per_inch +
            alignment_.offset[1],
        -(at("visualrefpoint", "-z-in") - cg_z) * metres_per_inch +
            alignment_.offset[2]};
    gfx::Placement placement;
    placement.origin = add(a.position,
                           add(add(scale(a.forward, origin_in_body[0]),
                                   scale(a.right, origin_in_body[1])),
                               scale(a.down, origin_in_body[2])));
    placement.world_from_local = gfx::Mat3::columns(a.forward, a.right, a.down);
    return placement;
}

world::Ecef Flight::sun_in_body() const {
    const sim::AircraftState s = aircraft_->state();
    const world::Ecef sun =
        gfx::sun_from(gfx::up_at(s.latitude_deg, s.longitude_deg));
    const Axes a = axes();
    // The same vector in the body frame: its components along the body's own
    // axes, which is the transpose of the rotation that takes body to world.
    return {a.forward.x * sun.x + a.forward.y * sun.y + a.forward.z * sun.z,
            a.right.x * sun.x + a.right.y * sun.y + a.right.z * sun.z,
            a.down.x * sun.x + a.down.y * sun.y + a.down.z * sun.z};
}

gfx::Camera Flight::camera(gfx::View view, double orbit_rad) const {
    const gfx::Placement placement = model_placement();
    return gfx::camera_for(view, placement, model_radius_,
                           from_model_origin("eyepoint"), orbit_rad);
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
    if (checklist_ && checklist_showing_) {
        r.checklist.phase = sim::phase_name(checklist_->showing());
        const sim::Checklist& list = checklist_->list();
        const std::vector<sim::ItemProgress>& progress = checklist_->progress();
        for (std::size_t i = 0; i < list.items.size() && i < progress.size(); ++i) {
            r.checklist.items.emplace_back(progress[i].ticked, list.items[i].text);
        }
    }
    return r;
}

void Flight::show_checklist(sim::Phase phase) {
    if (!checklist_) {
        return; // this aircraft ships none
    }
    checklist_->show(phase);
    checklist_showing_ = true;
}

void Flight::hide_checklist() {
    checklist_showing_ = false;
}

std::optional<sim::Phase> Flight::showing_checklist() const {
    if (!checklist_ || !checklist_showing_) {
        return std::nullopt;
    }
    return checklist_->showing();
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
