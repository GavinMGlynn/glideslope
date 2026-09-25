#pragma once

// The aircraft on offer, as data (assets/aircraft): one file each, named for
// the aircraft - `c172p.aircraft` - so an aircraft is added by adding its file
// and its flight model, and no code changes.
//
//   name TEXT...                 what it is called
//   model NAME                   its JSBSim model, in data/jsbsim/aircraft
//   start AIRSPEED_KT THROTTLE   a flight begun in the air: its calibrated
//                                airspeed, and the throttle that holds it
//   class NAME                   what kind of flying it is for, one of
//                                light-aircraft, seaplane, second-world-war,
//                                business-jet, airliner, fighter, bomber
//   seaplane                     it stands on water, and takes off from and
//                                alights on it, as a flying boat does
//
// with `#` beginning a comment. Its published figures, if it has them, are
// data/figures/MODEL.xml, and its selftest's log data/selftest/MODEL.log.

#include <filesystem>
#include <stdexcept>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::sim {

struct CatalogueError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// **What kind of flying an aircraft is for**, which is what a lesson is
// written against: a circuit in a Cub and a circuit in a 747 are not the same
// lesson. The seven are `REQUIREMENTS.md`'s own table in section 4.2, and a
// test holds this list and that table to each other.
enum class AircraftClass {
    light_aircraft,
    seaplane,
    second_world_war,
    business_jet,
    airliner,
    fighter,
    bomber,
};

inline constexpr std::size_t aircraft_class_count = 7;

// The name a `class` line gives, and the reverse. Nothing for a name that is
// not one of the seven.
std::string_view name_of(AircraftClass of);
std::optional<AircraftClass> class_from_name(std::string_view name);

struct CatalogueEntry {
    std::string id; // the file's name, less .aircraft
    std::string name;
    std::string model;
    double start_airspeed_kts = 0.0;
    double start_throttle = 0.0;
    bool seaplane = false;
    AircraftClass aircraft_class = AircraftClass::light_aircraft;
};

// One aircraft's file. Throws CatalogueError naming the line of anything it
// cannot read, and for a file missing its name, model, start or class.
CatalogueEntry parse_catalogue_entry(const std::string& id, std::string_view text);

// Every aircraft in `data`/aircraft, by id. Throws CatalogueError for a file
// it cannot read, or an aircraft whose model is not in `data`/jsbsim.
std::vector<CatalogueEntry> read_catalogue(const std::filesystem::path& data);

// One aircraft, by id. Throws CatalogueError if there is none.
CatalogueEntry find_aircraft(const std::filesystem::path& data, const std::string& id);

// **One aircraft, by an id that may have come from anywhere** - a server's
// `AIRCRAFT` message - or nothing. The id is looked up among the catalogue's,
// never joined to a path: an id of `../../somewhere` names no aircraft.
std::optional<CatalogueEntry> known_aircraft(const std::filesystem::path& data,
                                             const std::string& id);

} // namespace glideslope::sim
