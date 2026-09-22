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

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace glideslope::net {

// Four players and sixteen AI aircraft is what a server may run: `--players`
// is 1 to 4 and `--ai` is 0 to 16.
inline constexpr std::size_t most_aircraft_in_a_state = 20;

// One aircraft, as the wire carries it.
struct AircraftState {
    // The server's own number for this aircraft, steady for as long as it
    // flies. Not a slot: an AI aircraft has no slot.
    std::uint8_t index = 0;
    Controller controller = Controller::nobody;
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

struct StatePacket {
    // The simulation's clock, seconds since the session began. What the
    // client interpolates against.
    double simulation_time_s = 0.0;
    // The newest input sequence from *this* client that the server has
    // applied. A client reconciles everything it has predicted after this.
    std::uint32_t last_input_applied = 0;
    std::vector<AircraftState> aircraft;

    bool operator==(const StatePacket&) const = default;
};

// The bytes of a state packet, `Inside::state` first. Nothing if it holds
// more than `most_aircraft_in_a_state` aircraft, or a number that is not one.
std::optional<std::vector<std::uint8_t>> write_state(const StatePacket& state);

// A state packet out of a sealed body's plaintext. Nothing if it is not one:
// too short, too long, the wrong kind, too many aircraft, anything left over
// at the end, a controller this version does not know, or a non-finite
// number.
std::optional<StatePacket> read_state(std::span<const std::uint8_t> body);

// How many bytes a packet carrying `count` aircraft takes.
std::size_t state_bytes(std::size_t count);

} // namespace glideslope::net
