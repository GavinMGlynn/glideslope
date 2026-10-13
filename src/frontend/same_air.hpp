#pragma once

// **The same air and the same ground on the server and every client**
// (REQUIREMENTS.md section 6.3): what the server sends of its weather and its
// collision ground, made from what it flies, and what a client makes of it -
// in one place, shared by the server and both clients, so that the two ends
// cannot turn a message into different air.
//
// `src/net/` holds the messages and nothing of the world; turning a
// `world::WeatherReport` into a `net::Weather` is the server's, and turning it
// back the client's (net/messages.hpp). Both are here.

#include "net/messages.hpp"
#include "sim/weather.hpp"
#include "world/dem.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"
#include "world/runway_ground.hpp"
#include "world/weather.hpp"

#include <filesystem>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::frontend {

// **A report made fit to send**: its METAR cut, word by word from the end, to
// what a `WEATHER` may carry (net::most_metar_bytes), and read again, so that
// the server flies what it sends - a report cut on the wire alone would be
// one the server flew and its clients did not. A report that fits is
// returned as it was.
world::WeatherReport fit_to_send(world::WeatherReport report);

// **The runways a report's runway states lie on**, laid on the air flown
// once they are had - and never waited for. OurAirports' runways
// (world::world_runways) are fetched and read on a thread of their own, begun
// the first time the air flown has a runway's state, so that air without one
// reaches no network; the stepping thread and the frame loop only look
// whether they have come. **Until they have**, a state reported lies on no
// runway, and the runways are as the weather alone says - wet in rain, dry
// otherwise - and from the step after they come, on the runway it names.
// Runways that cannot be had are never laid and not asked for again, and
// `lay` says why, once. Let go while they are being fetched, the fetch is
// given up (world::FetchesGivenUp) and only that is waited for.
class StationRunways {
public:
    using Load = std::function<std::vector<world::RunwayEnd>()>;
    StationRunways(std::filesystem::path cache, world::Fetch fetch);
    // Any other source, for a test: a slow one, or one that fails.
    explicit StationRunways(Load load) : load_(std::move(load)) {}
    ~StationRunways();
    StationRunways(const StationRunways&) = delete;
    StationRunways& operator=(const StationRunways&) = delete;

    // **Lays the runways on `air` if it wants them and they are had**: true
    // when it has just laid them - what `air` flies has changed. Never waits.
    bool lay(world::ReportedWeather& air, std::string* why = nullptr);

private:
    Load load_;
    std::atomic<bool> give_up_{false};
    std::future<std::shared_ptr<const std::vector<world::RunwayEnd>>> loading_;
    std::shared_ptr<const std::vector<world::RunwayEnd>> all_;
    bool cannot_ = false;
};

// **What a server says of the runways its air lays a condition on**
// (net::RunwayConditions), for the weather that took over at `changed_at_s`,
// at `now_s` on the session's clock.
net::RunwayConditions runway_conditions_said(const world::ReportedWeather& air,
                                             double changed_at_s, double now_s);

// What the server sends of the weather it flies: still air (an empty METAR)
// for none, and the forecast above the station as a `WEATHER_ALOFT` where the
// report has one. `changed_at_s` and `blend_s` are when it took over on the
// session's clock and how long it blends in over (net::Weather).
struct WeatherSaid {
    net::Weather weather;
    std::optional<net::WeatherAloft> aloft;
};
WeatherSaid weather_said(const world::WeatherReport* report, double changed_at_s,
                         double blend_s);

// **What a client makes of it**: the report the server flies, read with the
// same rules, or nothing for still air. Throws world::MetarError for a METAR
// these rules cannot read, which a server of this version never sends.
std::optional<world::WeatherReport> weather_heard(const net::Weather& weather,
                                                  const std::optional<net::WeatherAloft>& aloft);

// **The collision ground's identity** (REQUIREMENTS.md section 4.1): the
// Copernicus DEM as this build knows it - `dem/coverage.txt`, made from the
// two buckets' pinned tile lists - with the runway strips the ground under
// runways is made from (`runways/strips.csv`) and the rules it is made by
// (world::collision_ground_rules). The hash is SHA-256 over a line for each:
// "coverage.txt <its SHA-256>\nstrips.csv <its SHA-256>\nground rules <n>\n".
// Throws std::runtime_error if either file cannot be read.
net::TerrainDataset collision_dataset(const std::filesystem::path& data);
// Its hash as hexadecimal, and the whole of it as a line for a person.
std::string hash_hex(const net::TerrainDataset& dataset);
std::string describe(const net::TerrainDataset& dataset);
bool same_ground(const net::TerrainDataset& a, const net::TerrainDataset& b);

// **Weather on the session's clock**: the air at the session's time, not at
// the aircraft's own. Each JSBSim instance counts its own time from when it
// was made, and an aircraft made later - a player joining, a wreck flying
// again - would fly gusts, a blend and a microburst at a time of its own,
// apart from the aircraft alongside it. `clock` reads the session's time.
class SessionClocked : public sim::Weather {
public:
    SessionClocked(std::shared_ptr<sim::Weather> air, std::function<double()> clock)
        : air_(std::move(air)), clock_(std::move(clock)) {}
    sim::Conditions at(double latitude_deg, double longitude_deg, double height_m,
                       double) override {
        return air_->at(latitude_deg, longitude_deg, height_m, clock_());
    }

private:
    std::shared_ptr<sim::Weather> air_;
    std::function<double()> clock_;
};

// **A client's copy of the server's air**: each whole weather heard
// (net::Told), made into the air a predicted aircraft flies - over `ground`
// (none, as on the server: glideslope_server's Fleet::fly_in),
// on the session's `clock`, blended from when the server blended it and over
// as long - so that the client flies the server's weather and not a fetch of
// its own. Still air from the start is no weather at all, as on a server given
// none; still air after a weather is the standard atmosphere with no wind,
// because an aircraft given no weather keeps the last it was given.
class HeardAir {
public:
    HeardAir(const world::Geoid* geoid, world::GroundAt ground, std::function<double()> clock)
        : geoid_(geoid), ground_(std::move(ground)), clock_(std::move(clock)) {}

    // **A METAR these rules cannot read is refused**, and the air flown
    // before it kept: false, with why. A server of this version never sends
    // one, and a client is not to be ended by one that does.
    bool heard(const net::Weather& weather, const std::optional<net::WeatherAloft>& aloft,
               std::string* why = nullptr);

    // **The runways the server's air lays a condition on**, and until when
    // the runways near the station stay wet - laid on the weather flown if
    // they are for it (the one that took over when they say), and dropped
    // otherwise: false. So a client flies the server's runways without
    // fetching any, and a client joined after the rain the wet runway the
    // server still flies.
    bool conditions_heard(const net::RunwayConditions& conditions);

    // What an aircraft is to fly: null for still air from the start.
    const std::shared_ptr<sim::Weather>& air() const { return air_; }
    // The report flown, for drawing its sky; null for still air.
    std::shared_ptr<world::ReportedWeather> reported() const { return still_ ? nullptr : reported_; }

private:
    const world::Geoid* geoid_;
    world::GroundAt ground_;
    std::function<double()> clock_;
    std::shared_ptr<world::ReportedWeather> reported_;
    // When the weather flown took over, as its server said.
    double changed_at_s_ = 0.0;
    std::shared_ptr<sim::Weather> air_;
    bool still_ = true;
};

} // namespace glideslope::frontend
