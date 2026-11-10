#pragma once

// Weather from reports: a METAR at the surface, and Open-Meteo's winds aloft
// above it.

#include "sim/weather.hpp"
#include "world/air_motion.hpp"
#include "world/download.hpp"
#include "world/geoid.hpp"
#include "world/lift.hpp"
#include "world/metar.hpp"
#include "world/runways.hpp"
#include "world/winds_aloft.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <future>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace glideslope::world {

// A METAR with where its station is.
struct SurfaceReport {
    Metar metar;
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double elevation_m = 0.0;
};

// aviationweather.gov's JSON METAR response: every report in it, read from its
// raw text, with its station's position. Throws JsonError or MetarError.
std::vector<SurfaceReport> parse_aviationweather(std::string_view json);

// The latest METAR for a station - an ICAO location indicator, four letters or
// digits, in either case - from aviationweather.gov. Throws MetarError for a
// station that is not one or has no METAR, ServiceUnavailable if nothing
// answered or a server error was all it answered, DemError for any other
// answer that is not a report, or the parse errors above. `retry_wait` is
// fetch_with_retries' wait; a test shortens it.
SurfaceReport fetch_metar(const std::string& station, const Fetch& fetch,
                          std::chrono::milliseconds retry_wait = std::chrono::milliseconds(2000));

// The International Standard Atmosphere's temperature at a height, degrees C.
double isa_temperature_c(double height_m);

// The conditions a report describes, at its station: the wind it reports, the
// temperature offset from the standard atmosphere that makes the air at the
// station's elevation the temperature it reports, and its QNH. A variable or
// unreported wind is calm; an unreported temperature or pressure standard.
sim::Conditions surface_conditions(const SurfaceReport& report);

// **How far a METAR's present weather reaches**: 8 km. Weather reported
// without "VC" is at the aerodrome; "VC" is its vicinity, between about 8
// and 16 km (ICAO Annex 3, appendix 3; the US's FMH-1 puts the
// vicinity at 5 to 10 statute miles). A runway is wet by a report's
// weather only within this of its station (with_air_motion,
// frontend::wet_runways), and dry beyond it.
inline constexpr double metar_radius_m = 8000.0;

// **The runway condition a report's weather implies** (sim/runway_condition.hpp):
// wet, code 5, while precipitation falls at the station - rain, drizzle,
// snow, snow grains, ice pellets, hail, small hail or unknown precipitation,
// of any intensity, not in the vicinity - or has fallen and stopped since the
// last routine report (recent weather: RERA, REDZ, RESN, ...); and dry, 6,
// otherwise. A METAR's weather does not say how deep anything lies, so it
// never gives more than code 5's "1/8 in or less": that is the runway-state
// group's (below).
int runway_condition_of(const Metar& metar);

// **The runway condition code a runway's reported state gives**, AC 25-32
// table 2 (the FAA's Runway Condition Assessment Matrix) read from WMO-No.
// 306's code tables, at the report's temperature:
// - SNOCLO, the aerodrome closed by snow: 1, the most slippery code flown
//   (code 0, nil, is not one here);
// - CLRD, cleared: 6;
// - extent 1 or 2, 25 per cent of the runway or less covered: 6, as AC 25-32
//   6.1 calls such a runway dry, whatever lies on the rest;
// - deposit 0 clear and dry: 6; 1 damp, 3 rime or frost: 5; 7 ice: 1;
// - 2 wet, 4 dry snow, 5 wet snow, 6 slush: 5 at 3 mm (1/8 in) deep or less,
//   or with no depth that matters (//); deeper, 2 for water and slush, 3 for
//   snow; and 1 for depth 99, the runway not operational;
// - 8 compacted snow: 4 at -15 C and colder, 3 warmer or with no
//   temperature; 9 frozen ruts or ridges, which the matrix does not name, as
//   compacted snow (an estimate: it is compacted snow, frozen hard);
// - and then a braking action reported (0366's 91 poor to 95 good) as the
//   matrix's braking-action column has it - 95 good 5, 94 medium to good 4,
//   93 medium 3, 92 medium to poor 2, 91 poor 1 - where that is more
//   slippery, or alone where no deposit is reported. A measured friction
//   coefficient (00 to 90) is not read: the matrix correlates codes with
//   braking action, not with a measured coefficient.
// Absent for a state that says nothing of the surface: no deposit and no
// braking action.
std::optional<int> runway_condition_of(const Metar::RunwayState& state,
                                       std::optional<double> temperature_c);

// **Whether a runway state names a runway end**: the end itself or its other
// end (24R is 06L's runway). A designator without a letter names the runway
// of that number without one, or its left parallel - the older eight
// figures' convention, a right parallel's number having 50 added. All
// runways - 88, 99, SNOCLO - name every one.
bool names(const Metar::RunwayState& state, const RunwayEnd& end);

// world::on_runway and world::ContaminatedRunway are world/runways.hpp's,
// where the copilot sees them.
// The most a report lays: two ends of eight runways.
inline constexpr std::size_t most_contaminated_runways = 16;
// **The longest runway an end may be**, metres: what a client's reader of
// net::RunwayConditions allows. The longest in the world is under 6 km.
inline constexpr double longest_runway_laid_m = 20000.0;

// **The runways a report's states lie on**: for each state with a code more
// slippery than dry, each end of `station_runways` it names (world::names),
// in order, at most most_contaminated_runways of them. An end no client
// would read - off the Earth, heading outside 0 to 360 degrees, of no length
// or longer than longest_runway_laid_m, or not a number - is left out, so that a
// server never says what every client refuses.
std::vector<ContaminatedRunway> contaminated_runways(const Metar& metar,
                                                     const std::vector<RunwayEnd>& station_runways);

// What a flight's weather is made of.
struct WeatherReport {
    SurfaceReport surface;
    // **The runways its runway-state groups lie on** (contaminated_runways):
    // laid once the station's runways are had, which is not at once
    // (frontend::StationRunways); until then, and with none, a state
    // reported lies on no runway, and the runways are as the weather alone
    // says.
    std::vector<ContaminatedRunway> contaminated;
    std::optional<WindsAloft> aloft;
    // The turbulence's severity, 0 none to 7 (world/air_motion.hpp), or, when
    // not given, what the METAR's gusts imply.
    std::optional<int> turbulence_severity;
    // What the gusts' and turbulence's pattern is drawn from: the same report
    // and seed give the same air everywhere (world/air_motion.hpp).
    std::uint64_t air_seed = 0;
    // Microbursts, placed by whoever sets the weather: the server, a lesson, a
    // test.
    std::vector<Microburst> microbursts;
};

// **A report's gust factor**, knots: the spread of its METAR's gusts over
// its mean wind, or, where its turbulence is given a severity, the spread
// that severity is read from (gust_spread_of_severity) where that is more -
// moderate turbulence 15 kt. 0 in calm, steady air. What the AI adds half of
// to its climb speed (sim::in_gusts), as a pilot adds half the gust factor
// to the approach (FAA-H-8083-3C, chapter 9).
double gust_factor_kt(const WeatherReport& report);

// The seed a station's report at a time gives its air: from the station and
// the observation's day, hour and minute, so every machine that has the report
// has the seed.
std::uint64_t air_seed_of(const Metar& metar);

// The conditions a report describes at a height above mean sea level.
//
// **The wind as a boundary layer.** 10 m above the station - where a METAR's
// wind is measured - the METAR's; below that, falling logarithmically to
// nothing at a roughness length of 3 cm, open country's; above it, through the
// forecast's winds 80, 120 and 180 m above the ground where it has them,
// logarithmically in height between each; and from the highest of those to the
// forecast's lowest pressure level above it, linearly, and its levels above.
// The forecast's own 10 m wind is not used: the METAR measured it.
//
// The temperature is the METAR's up to 10 m, the forecast's from its lowest
// level above that, and linear in height between, as an offset from the
// standard atmosphere. The pressure is the METAR's everywhere.
sim::Conditions conditions_at(const WeatherReport& report, double height_msl_m);

// What a report's rising and sinking air is made in, worked out from it once:
// the convective layer over its station (world/lift.hpp), from its surface
// temperature and wind and the forecast's temperatures above - none without a
// forecast, whose temperatures alone can say whether there is one; the wind that
// carries the thermals, the layer's halfway up; and the wind and the air's
// buoyancy frequency the terrain's waves are made in - the wind 1,000 m above
// the station, and the frequency between 1,000 and 4,000 m above it, from the
// temperatures' lapse there against the dry adiabatic.
struct Lift {
    Convection convection;
    double drift_north_mps = 0.0;
    double drift_east_mps = 0.0;
    double wind_north_mps = 0.0;
    double wind_east_mps = 0.0;
    double buoyancy_frequency = 0.0; // radians a second
};
Lift lift_of(const WeatherReport& report);

// The ground's height above sea level at a place, for the air over it.
using GroundAt = std::function<double(double latitude_deg, double longitude_deg)>;

// The air's motion a report describes at a place and time beyond its mean
// wind - reported wind shear, microbursts, gusts and turbulence
// (world/air_motion.hpp), thermals and the terrain's lift (world/lift.hpp) -
// added to `mean`, the conditions there. A report of wind shear makes, within
// 8 km of the station, a 15 kt headwind on the named runway's approach - or
// along the surface wind, for all runways - between 60 and 600 m above the
// ground, rising from none at 60 m to all of it at 300 m. The gusts are the
// METAR's spread over its mean wind, along its wind, in full up to 10 m above
// the station and fading to none 600 m above that. The turbulence is Dryden's
// at its severity, over the height above the station. Both patterns are
// carried by the surface wind, from the station. The thermals are `lift`'s,
// over the ground beneath - or the station's elevation, without `ground` -
// carried by its drift. The terrain's lift is made from `ground`, and without
// it there is none. JSBSim's own turbulence is left off: the air is all here.
//
// **The runway is the report's only near its station**: beyond
// metar_radius_m, dry. Within it, as the report's weather says
// (runway_condition_of), or more slippery where a runway-state group names
// one of `report.contaminated` the place is on (on_runway).
sim::Conditions with_air_motion(const WeatherReport& report, const Lift& lift,
                                const GroundAt& ground, sim::Conditions mean,
                                double latitude_deg, double longitude_deg,
                                double height_msl_m, double time_s);

// A station's weather now: its latest METAR, and Open-Meteo's winds aloft over
// it for the hour `time` ("YYYY-MM-DDTHH:00", UTC). Throws as fetch_metar and
// fetch_winds_aloft do, with a service that did not answer - and only that -
// as ServiceUnavailable beginning "the weather could not be had: ".
// Open-Meteo's data must be credited wherever it is shown: open_meteo_credit.
WeatherReport fetch_weather(const std::string& station, const std::string& time,
                            const Fetch& fetch,
                            std::chrono::milliseconds retry_wait = std::chrono::milliseconds(2000));

// **fetch_weather on a thread of its own, given up when let go.** Letting it
// go before it is done gives the fetch up - its transfer abandoned, its wait
// between tries cut short (FetchesGivenUp) - and waits only for that, so that
// a flight quit during a refresh ends at once rather than when the weather
// service's retries run out.
class WeatherFetch {
public:
    WeatherFetch(std::string station, std::string time, Fetch fetch,
                 std::chrono::milliseconds retry_wait = std::chrono::milliseconds(2000));
    ~WeatherFetch();
    WeatherFetch(const WeatherFetch&) = delete;
    WeatherFetch& operator=(const WeatherFetch&) = delete;

    // Whether get() would return without waiting.
    bool done() const;
    // What fetch_weather returned, or throws what it threw. Once.
    WeatherReport get();

private:
    std::atomic<bool> give_up_{false};
    std::future<WeatherReport> report_;
};

// What CC BY 4.0 and Open-Meteo's terms ask to be shown beside its data.
inline constexpr const char* open_meteo_credit = "Weather data by Open-Meteo.com";

// **How long a runway stays wet after the rain has stopped**, seconds: half
// an hour. An estimate, not a source's: pavement in sun dries in half an
// hour to an hour and a half, and overcast and cool for hours longer (no
// study of runways' drying was found). A report's recent weather (RERA)
// already keeps it wet until the next routine report, up to an hour.
inline constexpr double still_wet_after_rain_s = 1800.0;

// A flight's weather: its report, and each new one blended in over an interval
// rather than stepped to - every value moving linearly from what the old report
// gave to what the new one gives, and the turbulence changing halfway.
//
// **A runway the old report wetted stays wet** (code 5, near the station)
// for still_wet_after_rain_s after the new report takes over at the blend's
// halfway, however dry the new report is.
class ReportedWeather : public sim::Weather {
public:
    // `geoid` converts the aircraft's height above the ellipsoid to height above
    // mean sea level; without one they are taken as the same. `ground` gives
    // the terrain the air rises and sinks over; without it, there is none.
    ReportedWeather(WeatherReport report, const Geoid* geoid, double blend_seconds,
                    GroundAt ground = {});

    // A new report, blending in from simulation time `now_s`.
    void update(WeatherReport report, double now_s);
    // The same, over `blend_seconds` from now on: a client blends a change
    // over what its server said it blends over (net::Weather).
    void update(WeatherReport report, double now_s, double blend_seconds) {
        blend_seconds_ = blend_seconds;
        update(std::move(report), now_s);
    }

    double blend_seconds() const {
        return blend_seconds_;
    }

    // **Whether a report flown has a runway's state**, the latest or the
    // one it blends from: what wants the station's runways laid.
    bool has_runway_states() const;
    // Whether the runways have been laid on what is flown now - by
    // lay_runways or lay - since it was made or last updated.
    bool runways_laid() const { return runways_laid_; }
    // **Lays both reports' states on the station's runways** - `all`, every
    // runway, of which each report's station's are taken - once they are had.
    void lay_runways(const std::vector<RunwayEnd>& all);
    // **Lays what a server says it laid** (net::RunwayConditions): the latest
    // report's runways, the one it blends from's, and until when the runways
    // near the station stay wet.
    void lay(std::vector<ContaminatedRunway> now, std::vector<ContaminatedRunway> before,
             double wet_until_s);
    // Until when the runways near the station stay wet from the rain before
    // (still_wet_after_rain_s); -infinity, not at all.
    double wet_until_s() const { return wet_until_s_; }

    // The report now: the latest, even while it is blending in.
    const WeatherReport& report() const {
        return current_;
    }
    // **What it blends from**, and from when: the report before the latest,
    // until the blend that took it over ends - after which it is flown no
    // more, and this is null. A client joining mid-blend is told it first, so
    // that it blends as the server does (frontend::weather_said).
    const WeatherReport* blending_from(double now_s) const {
        return previous_ && blend_seconds_ > 0.0 && now_s < changed_at_s_ + blend_seconds_
                   ? &*previous_
                   : nullptr;
    }

    sim::Conditions at(double latitude_deg, double longitude_deg, double height_m,
                       double time_s) override;

private:
    std::optional<WeatherReport> previous_;
    WeatherReport current_;
    Lift previous_lift_;
    Lift current_lift_;
    GroundAt ground_;
    double changed_at_s_ = 0.0;
    // Until when the runways near the station stay wet from rain the report
    // before said (still_wet_after_rain_s); -infinity, never.
    double wet_until_s_ = -std::numeric_limits<double>::infinity();
    bool runways_laid_ = false;
    const Geoid* geoid_;
    double blend_seconds_;
};

// **How wide the blend between two stations' weathers is**, metres of
// difference in distance: 20 km. A place is in the nearer station's weather
// alone where the next is farther by this or more, and between two stations
// that is everywhere but a band 20 km wide across the line halfway between
// them - 10 km either side of it, along the line joining them. An estimate,
// not a source's: wider than a METAR's vicinity (16 km, metar_radius_m's
// note), so that a band never ends inside one station's vicinity of the
// other, and narrower than the 40 to 100 km between most reporting
// aerodromes, so that each station's weather is its own around it. No
// simulator found publishes its own: FlightGear's interpolates by inverse
// distance everywhere, MSFS's is said to have a transition about each
// station (2026-11-11 searches, docs/status).
inline constexpr double station_blend_m = 20000.0;

// **The weight each station's weather has at a place**: each station
// `1 - (d - d_nearest) / station_blend_m`, none below nought, then all
// divided by their sum - the nearer station's weather alone outside the
// band, and a share that moves continuously with the place inside it. A
// flight moving `s` metres changes each difference in distance by at most
// 2 s, so no station's share moves by more than about 2 s / station_blend_m
// in a step. In the order of `stations`, latitude and longitude in degrees;
// empty for none.
std::vector<double> station_weights(const std::vector<std::pair<double, double>>& stations,
                                    double latitude_deg, double longitude_deg);

// **A flight's weather from several stations** (REQUIREMENTS.md 6.3): each a
// ReportedWeather of its own - its report, blended in time as it changes,
// its runways - flown by its share at the place (station_weights). Every
// value but two is the shares' weighted sum; the runway's condition and the
// turbulence's severity, codes that cannot be mixed, are the station with
// the greatest share's. Each station's gusts and turbulence keep their own
// pattern from their own seed and clock, so two ends that have the same
// stations fly the same air.
class StationsWeather : public sim::Weather {
public:
    explicit StationsWeather(std::vector<std::shared_ptr<ReportedWeather>> stations)
        : stations_(std::move(stations)) {}

    const std::vector<std::shared_ptr<ReportedWeather>>& stations() const { return stations_; }
    // **The station whose weather is most of it at a place**: what draws the
    // sky there. Its index in stations(); 0 with none.
    std::size_t nearest(double latitude_deg, double longitude_deg) const;

    sim::Conditions at(double latitude_deg, double longitude_deg, double height_m,
                       double time_s) override;

private:
    std::vector<std::pair<double, double>> places() const;
    std::vector<std::shared_ptr<ReportedWeather>> stations_;
};

// **The most stations a flight's weather is made of**: what a server finds
// around its aircraft (stations_around), and what a `WEATHER` may number.
inline constexpr std::size_t most_stations = 32;

// **The weather of the stations a server has found, in numbered slots**, as
// the server flies it and every client is told it (net::Weather::station):
// each slot empty or one station's ReportedWeather, the whole flown as a
// StationsWeather.
//
// **A slot's station sending a new report** blends it in over its blend, as
// ReportedWeather does. **A slot taking another station, or emptied, or
// filled** changes which stations there are: the stations before and after
// are each flown whole (StationsWeather), and the air blends from the one to
// the other over the blend from `set_changed_at_s` - so a station found or
// let go never steps the wind. Changes at one `set_changed_at_s` are one
// change. The first stations there are, from none, are flown whole at once,
// as a first report is.
class StationSlots : public sim::Weather {
public:
    explicit StationSlots(const Geoid* geoid, GroundAt ground = {})
        : geoid_(geoid), ground_(std::move(ground)), now_(std::make_shared<StationsWeather>(
                                                         std::vector<std::shared_ptr<ReportedWeather>>{})) {}

    // **What slot `slot` flies**: `report`, or nothing. `changed_at_s` and
    // `blend_s` as ReportedWeather::update's; `set_changed_at_s`, when the
    // stations changed, if this changes them. Throws std::out_of_range for a
    // slot of most_stations or more.
    void set(std::size_t slot, std::optional<WeatherReport> report, double changed_at_s,
             double blend_s, double set_changed_at_s);

    // The slots, each null when empty; as many as the highest set.
    const std::vector<std::shared_ptr<ReportedWeather>>& slots() const { return slots_; }
    // Whether any slot has a station.
    bool any() const;
    // **The station whose weather is most of the air at a place**: what
    // draws the sky there. Null with none.
    std::shared_ptr<ReportedWeather> nearest(double latitude_deg, double longitude_deg) const;
    // When the stations last changed, and over how long they blend.
    double set_changed_at_s() const { return set_changed_at_s_; }

    sim::Conditions at(double latitude_deg, double longitude_deg, double height_m,
                       double time_s) override;

private:
    const Geoid* geoid_;
    GroundAt ground_;
    std::vector<std::shared_ptr<ReportedWeather>> slots_;
    std::shared_ptr<StationsWeather> now_;
    std::shared_ptr<StationsWeather> before_;
    double set_changed_at_s_ = -std::numeric_limits<double>::infinity();
    double set_blend_s_ = 0.0;
};

// **How far around an aircraft its stations are looked for**, metres: 200 km
// each way, a box 400 km across. About 6,500 stations report METARs
// (MADIS's count), over about 149 million km^2 of land - one for every
// 150 km or so, evenly spread; they are not, so a box reaching 200 km finds
// a station for most places over land, and the nearest few of them are
// what the air is made of (stations_around).
inline constexpr double stations_radius_m = 200000.0;
// **How many of the nearest stations an aircraft's air is made of**: six.
// Only those within station_blend_m of the nearest have any share, and six
// is more than lie within 20 km of each other but at the busiest airfields.
inline constexpr std::size_t stations_per_aircraft = 6;

// **The METARs reported around a place**: aviationweather.gov's answer for
// the box stations_radius_m each way of it (`bbox=`), each with its
// station's place. Throws as fetch_metar does; none reported is empty, not
// an error.
std::vector<SurfaceReport> fetch_metars_around(double latitude_deg, double longitude_deg,
                                               const Fetch& fetch,
                                               std::chrono::milliseconds retry_wait =
                                                   std::chrono::milliseconds(2000));

// **The stations whose weather is flown**: for each place (an aircraft),
// the stations_per_aircraft nearest of `reports` within stations_radius_m of
// it, each station once, in order of the first place they are nearest to -
// at most most_stations of them.
std::vector<SurfaceReport> stations_around(const std::vector<SurfaceReport>& reports,
                                           const std::vector<std::pair<double, double>>& places);

// **The weather now of the stations around places** (aircraft): the METARs
// around each (fetch_metars_around; a place within 1 km of one already
// looked around is not looked around again), the stations of them
// (stations_around), and Open-Meteo's winds aloft over each for the hour
// `time`. Throws as fetch_weather does.
std::vector<WeatherReport> fetch_stations_weather(
    const std::vector<std::pair<double, double>>& places, const std::string& time,
    const Fetch& fetch, std::chrono::milliseconds retry_wait = std::chrono::milliseconds(2000));

// **fetch_stations_weather on a thread of its own, given up when let go**,
// as WeatherFetch is.
class StationsFetch {
public:
    StationsFetch(std::vector<std::pair<double, double>> places, std::string time, Fetch fetch,
                  std::chrono::milliseconds retry_wait = std::chrono::milliseconds(2000));
    ~StationsFetch();
    StationsFetch(const StationsFetch&) = delete;
    StationsFetch& operator=(const StationsFetch&) = delete;

    // Whether get() would return without waiting.
    bool done() const;
    // What fetch_stations_weather returned, or throws what it threw. Once.
    std::vector<WeatherReport> get();

private:
    std::atomic<bool> give_up_{false};
    std::future<std::vector<WeatherReport>> reports_;
};

} // namespace glideslope::world
