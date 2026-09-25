#pragma once

// The messages that go through the reliable layer.
//
// **Six things must each arrive, exactly once, in order**: the lobby, the
// session, the weather, an aircraft's definition, the terrain dataset and a
// controller swap. They go as **seven kinds of message**, because the
// weather is two of them - see below. `net/reliable.hpp` makes delivery true
// of an opaque body; this says what those bodies are.
//
// **Every message begins with one byte saying which kind it is**, and the
// rest is that kind's own fields in the order given here. `docs/TRANSPORT.md`
// writes them out byte for byte, and a test holds the document and this code
// to each other.
//
// **These structures are the wire's, not the simulation's.** `src/net/`
// includes nothing but the standard library and, for the handshake's keys,
// libsodium - no socket, no terrain tile and no flight model, which is what
// lets the whole transport be tested without a network.
// Turning a `world::WeatherReport` into a `Weather` is the server's job and
// turning it back is the client's; neither belongs here.
//
// **The weather is sent as what it was made from, not as what it became.**
// A METAR is a line of text with twenty optional fields; re-encoding them is
// twenty chances to disagree. The raw report goes on the wire and the
// receiver parses it with the same code the sender did, so the two cannot
// read it differently. The forecast above it has no such text, so its levels
// are sent as numbers.
//
// **The weather is two messages, because it does not fit in one datagram.**
// A datagram is 1232 bytes and the forecast alone is most of that: the
// nineteen pressure levels this project fetches are 760 bytes alone, and a
// report with them, a METAR and a single microburst comes to 1,235 - over
// the 1,218 left after the envelope and the reliable header. So the report
// goes as `WEATHER` and the forecast above it as `WEATHER_ALOFT`, and each
// fits with room. Nothing here fragments, so a message that does not fit
// cannot be sent at all: `UdpSocket::send` refuses it and the reliable layer
// would retransmit it for ever.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "net/protocol.hpp"

namespace glideslope::net {

// Which message this is. The value is the first byte of every reliable body.
enum class Message : std::uint8_t {
    lobby = 1,
    session = 2,
    weather = 3,
    aircraft = 4,
    terrain_dataset = 5,
    controller_swap = 6,
    weather_aloft = 7,
};

// Whether `kind` is one this version knows.
bool known_message(std::uint8_t kind);

// Who is flying an aircraft. A slot with `nobody` is open.
enum class Controller : std::uint8_t {
    nobody = 0,
    person = 1,
    ai = 2,
};

bool known_controller(std::uint8_t controller);

// **The lobby**: every slot the server has, and who is in it. Sent whole
// rather than as changes, because it is small and a whole one cannot be
// misapplied to a state the receiver did not have.
struct Lobby {
    struct Slot {
        std::uint8_t index = 0;
        Controller controller = Controller::nobody;
        std::string name; // the player's, or the AI pilot's; empty if nobody
    };
    // 1 to 4, as REQUIREMENTS.md has it.
    std::uint8_t players_allowed = 1;
    std::vector<Slot> slots;
};

// **The session**: what this one is and when it began.
struct Session {
    std::uint64_t id = 0;
    std::string name;
    // Milliseconds since the Unix epoch, UTC, so that two machines in
    // different places agree what time the session started.
    std::uint64_t began_unix_ms = 0;
    // The simulation's own clock, seconds since the session began.
    double simulation_time_s = 0.0;
};

// The forecast's wind and temperature at a pressure level.
struct AloftLevel {
    double pressure_hpa = 0.0;
    double height_m = 0.0; // above mean sea level
    double wind_north_mps = 0.0;
    double wind_east_mps = 0.0;
    double temperature_c = 0.0;
};

// The forecast's wind a fixed height above the ground.
struct NearGroundWind {
    double height_m = 0.0; // above the ground
    double wind_north_mps = 0.0;
    double wind_east_mps = 0.0;
};

// Sinking air placed by whoever set the weather.
struct Microburst {
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double radius_m = 0.0;
    double downdraught_mps = 0.0;
    double start_s = 0.0;
    double duration_s = 0.0;
};

// **The weather at the station**, as its source rather than its effect.
struct Weather {
    std::string metar; // the raw report, parsed by the receiver
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double elevation_m = 0.0;
    // 0 to 7, or absent to let the METAR's gusts decide.
    std::optional<std::uint8_t> turbulence_severity;
    // The same report and seed give the same air on every machine.
    std::uint64_t air_seed = 0;
    std::vector<Microburst> microbursts;
};

// **The forecast above the station.** Sent after a `WEATHER`, and only where
// there is a forecast: a station with none simply has no `WEATHER_ALOFT`,
// which is how its absence is said.
struct WeatherAloft {
    std::string time; // "2026-09-17T16:00", UTC
    std::vector<AloftLevel> levels;
    std::vector<NearGroundWind> near_ground;
};

// **An aircraft's definition**: which aeroplane an aircraft is, by the
// server's number for it - the number state updates carry. Not a slot: an AI
// aircraft has none, and a client must know what it is to draw it.
struct AircraftDefinition {
    std::uint8_t aircraft = 0;
    std::string id;    // the catalogue's, "c172p"
    std::string model; // the JSBSim model's directory
};

// **The terrain dataset** both ends must agree on, because the collision
// terrain decides where the ground is and a client predicting against a
// different one would drift against the server for a reason no measurement
// would explain.
struct TerrainDataset {
    std::string name;
    std::string version;
    // The dataset's pinned SHA-256, as 32 bytes.
    std::vector<std::uint8_t> sha256;
};

// **A controller swap**: an aircraft handed between a person and an AI pilot,
// by the server's number for it. A client asks for its own with one, and the
// server says it has happened with another, to every client.
struct ControllerSwap {
    std::uint8_t aircraft = 0;
    Controller to = Controller::nobody;
    // When it takes effect, on the simulation's clock.
    double at_simulation_time_s = 0.0;
};

// **The most of each variable-length thing a message may carry.** A reader
// that trusted a count could be told to hold four billion slots by six
// bytes; these are what this protocol will accept, and anything more breaks
// the reader rather than being trimmed.
//
// **Every one of these is chosen so that its message fits in one datagram**,
// and a test holds them to it. They are not generous guesses: the forecast
// this project fetches is nineteen pressure levels and four near-ground
// winds, and a METAR runs to about a hundred characters.
inline constexpr std::size_t most_slots = 4;
inline constexpr std::size_t most_levels = 24;
inline constexpr std::size_t most_near_ground = 4;
inline constexpr std::size_t most_microbursts = 16;
inline constexpr std::size_t most_metar_bytes = 256;
inline constexpr std::size_t most_name_bytes = 64;
inline constexpr std::size_t most_time_bytes = 32;
inline constexpr std::size_t sha256_bytes = 32;

// **The most a message body may be**: a datagram, less the envelope in front
// of it and the reliable layer's number and acknowledgement. Nothing
// fragments, so a body larger than this cannot be sent at all.
inline constexpr std::size_t envelope_and_reliable_header = 6 + 8;
inline constexpr std::size_t most_message_bytes = 1232 - envelope_and_reliable_header;

// **Writing.** Each returns a body beginning with its message byte, ready
// for `Reliable::send`. None can fail.
//
// **A number that is not one is refused on reading, not on writing.** A NaN
// or an infinity in any floating-point field makes the far end refuse the
// whole message - a NaN position would spread through the floating origin
// and the terrain query, and an infinite duration would never end - and
// `docs/TRANSPORT.md` says so where it says what a reader must refuse. So
// whoever fills one of these in, from a weather report with a field missing
// or from anywhere else, must not put one there: the message would be sent,
// repeated until acknowledged, and thrown away at the other end.
std::vector<std::uint8_t> write(const Lobby& m);
std::vector<std::uint8_t> write(const Session& m);
std::vector<std::uint8_t> write(const Weather& m);
std::vector<std::uint8_t> write(const WeatherAloft& m);
std::vector<std::uint8_t> write(const AircraftDefinition& m);
std::vector<std::uint8_t> write(const TerrainDataset& m);
std::vector<std::uint8_t> write(const ControllerSwap& m);

// Which kind a body is, or nothing if it is empty or a kind this version
// does not know.
std::optional<Message> kind_of(std::span<const std::uint8_t> body);

// **Reading.** Each takes a whole body, message byte and all, and answers
// whether it read as that kind with nothing left over - nothing missing,
// nothing trailing, and no count beyond the limits above. **`out` is
// untouched unless the answer is true**, so a caller cannot act on half a
// message it has been told not to trust.
bool read(std::span<const std::uint8_t> body, Lobby& out);
bool read(std::span<const std::uint8_t> body, Session& out);
bool read(std::span<const std::uint8_t> body, Weather& out);
bool read(std::span<const std::uint8_t> body, WeatherAloft& out);
bool read(std::span<const std::uint8_t> body, AircraftDefinition& out);
bool read(std::span<const std::uint8_t> body, TerrainDataset& out);
bool read(std::span<const std::uint8_t> body, ControllerSwap& out);

} // namespace glideslope::net
