#pragma once

// Winds aloft, from Open-Meteo's forecast on pressure levels.
//
// Open-Meteo gives, for each hour and each pressure level from 1000 hPa to
// 30 hPa, the wind's speed and direction, the temperature and the level's
// geopotential height. A profile is one hour of that at one place: every
// level's height above mean sea level - geometric, not geopotential, metres -
// and its wind and temperature. Between levels the wind is interpolated as a
// vector, and the temperature linearly, in height.

#include "world/download.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::world {

struct AloftLevel {
    double pressure_hpa = 0.0;
    double height_m = 0.0;       // above mean sea level
    double wind_north_mps = 0.0; // the air's velocity, towards north
    double wind_east_mps = 0.0;
    double temperature_c = 0.0;
};

struct WindsAloft {
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    std::string time;               // "2026-09-17T16:00", UTC
    std::vector<AloftLevel> levels; // lowest first
};

// The pressure levels asked for, highest pressure first.
const std::vector<int>& open_meteo_levels();

// The forecast request for a place: every level's wind, temperature and height,
// hourly, for a day, in UTC and metres a second.
std::string open_meteo_url(double latitude_deg, double longitude_deg);

// The hour `t` falls in, as Open-Meteo names hours: "YYYY-MM-DDTHH:00", UTC.
std::string utc_hour(std::chrono::system_clock::time_point t);

// The profile for `time` ("YYYY-MM-DDTHH:00", UTC) from an Open-Meteo response.
// Throws JsonError if the response is not one, or std::runtime_error if it has
// no such hour or lacks a level.
WindsAloft parse_open_meteo(std::string_view json, const std::string& time);

// The profile now - the hour `time` - for a place. Throws DemError if the
// forecast cannot be had.
WindsAloft fetch_winds_aloft(double latitude_deg, double longitude_deg,
                             const std::string& time, const Fetch& fetch);

struct AloftSample {
    double wind_north_mps = 0.0;
    double wind_east_mps = 0.0;
    double temperature_c = 0.0;
};

// The wind and temperature at a height above mean sea level: interpolated
// between the levels around it, and the lowest or highest level's beyond them.
AloftSample sample(const WindsAloft& profile, double height_m);

} // namespace glideslope::world
