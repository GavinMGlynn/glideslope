#pragma once

// **What the server sends back**: where every aircraft is, and how far it has
// got through this client's inputs.
//
// **This is the answer to the inputs, and it is why the server is
// authoritative.** A client sends inputs and predicts its own aircraft from
// them; the server flies every aircraft for real and says, at 20 to 30 Hz,
// where they all are. The client reconciles its prediction against its own
// aircraft's line and interpolates everybody else's.
//
// **It is not reliable and must not be.** A state update is worth nothing
// once a newer one exists, so repeating a lost one would deliver stale
// positions late. Each is sent once and rides inside a `SEALED` datagram as
// `Inside::state`; the sealing's replay window throws away an old one that
// arrives late.
//
// **Positions are Earth-centred, Earth-fixed and double precision**, which is
// the project's rule and not a choice made here: the whole world is in play,
// so there is no session origin to be near, and a float's 24 bits of mantissa
// would give half-metre steps at Earth's radius. Velocities and angles are
// floats, because a float holds a velocity in metres a second and an angle in
// degrees to far better than anything can measure them, and 20 aircraft
// twenty times a second is worth the bytes.
//
// **The count is bounded so that a packet always fits one datagram**, and a
// test fills one to its limits and holds it to that.

#include "net/messages.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace glideslope::net {

// Four players and sixteen AI aircraft is what a server may run: `--players`
// is 1 to 4 and `--ai` is 0 to 16.
inline constexpr std::size_t most_aircraft_in_a_state = 20;

// What `your_aircraft` holds when this client has none.
inline constexpr std::uint8_t no_aircraft = 255;

// The kind byte, the clock, the input sequence, this client's own aircraft
// and the count; then an index, a controller, a condition, three doubles and
// six floats per aircraft. Named so that nothing has to count bytes twice.
inline constexpr std::size_t state_header_bytes = 1 + 8 + 4 + 1 + 1;
inline constexpr std::size_t state_per_aircraft_bytes = 1 + 1 + 1 + 3 * 8 + 6 * 4;

// **Whether an aircraft is flying or a wreck.** Collisions are the server's
// to decide (sim/crash.hpp), and every client is told: a wreck stays where it
// hit for a few seconds, then flies again from the start.
enum class Condition : std::uint8_t {
    flying = 0,
    wrecked = 1,
};
bool known_condition(std::uint8_t value);

// One aircraft, as the wire carries it.
struct AircraftState {
    // The server's own number for this aircraft, steady for as long as it
    // flies. Not a slot: an AI aircraft has no slot.
    std::uint8_t index = 0;
    Controller controller = Controller::nobody;
    Condition condition = Condition::flying;
    // Earth-centred, Earth-fixed, metres.
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    // The same frame, metres a second.
    float vx_mps = 0.0F;
    float vy_mps = 0.0F;
    float vz_mps = 0.0F;
    // Local, degrees.
    float heading_deg = 0.0F;
    float pitch_deg = 0.0F;
    float roll_deg = 0.0F;

    bool operator==(const AircraftState&) const = default;
};

// **The client's own aircraft's motion**, which only its own state update
// carries - the rest of the packet says where every aircraft is for drawing
// it; this is what the client's prediction is put right by (sim::Motion,
// sim::Prediction). Metres, the quaternion from north-east-down to the body,
// metres a second and radians a second along the body's axes.
struct OwnMotion {
    double x_m = 0.0; // Earth-centred, Earth-fixed
    double y_m = 0.0;
    double z_m = 0.0;
    std::array<float, 4> attitude{};
    std::array<float, 3> uvw_mps{};
    std::array<float, 3> pqr_radps{};

    bool operator==(const OwnMotion&) const = default;
};

// The flag and the motion after it: three doubles and ten floats.
inline constexpr std::size_t own_motion_bytes = 1 + 3 * 8 + 10 * 4;

// **The controls of the aircraft this client is watching** (`WATCH`), which
// only its own state update carries: riding along in another aircraft shows
// what its pilot, a person's or the AI's, is doing with the controls. There is
// no room for every aircraft's in a full packet, so one client is told of the
// one it watches. Each control is a 16-bit fraction, as inputs are
// (net/inputs.hpp): the aileron and rudder (right positive) and elevator (back
// positive) from -1 to 1, the throttle and flaps from 0 to 1, and the gear -
// down at 1 - or none where it does not retract.
struct Watched {
    std::uint8_t aircraft = 0;
    double aileron = 0.0;
    double elevator = 0.0;
    double rudder = 0.0;
    double throttle = 0.0;
    double flaps = 0.0;
    std::optional<double> gear;

    bool operator==(const Watched&) const = default;
};

// The flag, the aircraft's number and six controls.
inline constexpr std::size_t watched_bytes = 1 + 1 + 6 * 2;
// What the gear is written as where it does not retract.
inline constexpr std::int16_t gear_fixed = -32768;

struct StatePacket {
    // The simulation's clock, seconds since the session began. What the
    // client interpolates against.
    double simulation_time_s = 0.0;
    // The newest input sequence from *this* client that the server has
    // applied. A client reconciles everything it has predicted after this.
    std::uint32_t last_input_applied = 0;
    // **Which of the aircraft below is this client's own**, by index, or
    // `no_aircraft`. A client cannot reconcile its prediction without knowing
    // which line is its own, and it is here rather than in a `SESSION`
    // message because this packet is already sealed to one client and already
    // carries one thing meant only for them - `last_input_applied`.
    std::uint8_t your_aircraft = no_aircraft;
    std::vector<AircraftState> aircraft;
    // This client's own aircraft's motion, when it has one.
    std::optional<OwnMotion> yours;
    // The controls of the aircraft this client is watching, when it is.
    std::optional<Watched> watched;

    bool operator==(const StatePacket&) const = default;
};

// The bytes of a state packet, `Inside::state` first. Nothing if it holds
// more than `most_aircraft_in_a_state` aircraft, or a number that is not one.
std::optional<std::vector<std::uint8_t>> write_state(const StatePacket& state);

// A state packet out of a sealed body's plaintext. Nothing if it is not one:
// too short, too long, the wrong kind, too many aircraft, anything left over
// at the end, a controller or a condition this version does not know, or a
// non-finite
// number.
std::optional<StatePacket> read_state(std::span<const std::uint8_t> body);

// How many bytes a packet carrying `count` aircraft takes.
std::size_t state_bytes(std::size_t count, bool with_yours = false,
                        bool with_watched = false);

} // namespace glideslope::net
