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
// and the count; then an index, a controller, a condition, the engine
// stopped, three doubles and six floats per aircraft. Named so that nothing
// has to count bytes twice.
inline constexpr std::size_t state_header_bytes = 1 + 8 + 4 + 1 + 1;
inline constexpr std::size_t state_per_aircraft_bytes = 1 + 1 + 1 + 1 + 3 * 8 + 6 * 4;

// What `stopped_engine` holds when no engine has stopped.
inline constexpr std::uint8_t no_engine = 255;

// **Whether an aircraft is flying or a wreck.** Collisions are the server's
// to decide (sim/crash.hpp), and every client is told: a wreck stays where it
// hit for a few seconds, then flies again from the start. **And whether its
// engines run** (2026-09-30): flying with an engine stopped is said, so that
// a player's copilot, on the player's machine, knows to glide.
enum class Condition : std::uint8_t {
    flying = 0,
    wrecked = 1,
    engine_stopped = 2,
};
bool known_condition(std::uint8_t value);

// One aircraft, as the wire carries it.
struct AircraftState {
    // The server's own number for this aircraft, steady for as long as it
    // flies. Not a slot: an AI aircraft has no slot.
    std::uint8_t index = 0;
    Controller controller = Controller::nobody;
    Condition condition = Condition::flying;
    // **Which engine has stopped** (version 04): the first, by its number
    // from 0, when `condition` is `engine_stopped`, and `no_engine` exactly
    // when it is not - so that a predicting client stops the one the server
    // did, not a guess.
    std::uint8_t stopped_engine = no_engine;
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
//
// **And how far into its newest input the server had flown it**: the server
// flies an input from when it arrives, so an input the network made late is
// flown late and for however long it is until the next one - not for as long
// as it was flown on the client's screen. With the clock, it says at which
// step of the server's the input began, and so how far the server's clock is
// from the client's: what the client places this motion on its own clock by,
// to fly forward again from there (sim::Prediction). Saturates at 65,535,
// which is nine minutes of one input.
//
// What `OwnMotion::stovl_height_asked_ft` is when no law flies her lift
// system; any other value below nought is refused.
inline constexpr float no_stovl_law = -1.0F;

struct OwnMotion {
    double x_m = 0.0; // Earth-centred, Earth-fixed
    double y_m = 0.0;
    double z_m = 0.0;
    std::array<float, 4> attitude{};
    std::array<float, 3> uvw_mps{};
    std::array<float, 3> pqr_radps{};
    // Steps of 1/120 s flown on `last_input_applied` when this was taken.
    std::uint16_t steps_into_input = 0;
    // **The height above the ground her STOVL transition law is asked to
    // hold**, ft, when this was taken - or `no_stovl_law` when no law flies
    // her lift system (since version 13). Each end seeds it from her height
    // as it sees her at the selector and the stick moves both alike from
    // there, so a prediction that was off then held the difference for as
    // long as the law flew her: told this, it is put right with the motion
    // (sim::Prediction).
    float stovl_height_asked_ft = no_stovl_law;
    // **And her wing-borne autopilot's pitch integral**, degrees, while that
    // law flies her with an autopilot (sim::Transition::Carried); none
    // otherwise, and always none with no law. Seeded on each side when the
    // law is made, a step apart when her selector reached the server a step
    // off, and kept apart by the same motion put back at every word: 21 mm
    // at the median, nearly 3 degrees of pitch (PROJECT_STATUS.md,
    // 2026-10-16). Since version 13.
    std::optional<float> stovl_pitch_integral_deg;

    bool operator==(const OwnMotion&) const = default;
};

// The height asked of her STOVL law, or none where no law flies her, as a
// prediction takes it (sim::Prediction::reconcile).
inline std::optional<double> stovl_height_asked(const OwnMotion& m) {
    if (m.stovl_height_asked_ft == no_stovl_law) {
        return std::nullopt;
    }
    return static_cast<double>(m.stovl_height_asked_ft);
}

// The pitch integral, as a prediction takes it.
inline std::optional<double> stovl_pitch_integral(const OwnMotion& m) {
    if (!m.stovl_pitch_integral_deg) {
        return std::nullopt;
    }
    return static_cast<double>(*m.stovl_pitch_integral_deg);
}

// The flag and the motion after it: three doubles, ten floats, the steps,
// the height asked, and the pitch integral's flag and value.
inline constexpr std::size_t own_motion_bytes = 1 + 3 * 8 + 10 * 4 + 2 + 4 + 1 + 4;

// **The controls of the aircraft this client is watching** (`WATCH`), which
// only its own state update carries: riding along in another aircraft shows
// what its pilot, a person's or the AI's, is doing with the controls. There is
// no room for every aircraft's in a full packet, so one client is told of the
// one it watches. Each control is a 16-bit fraction, as inputs are
// (net/inputs.hpp): the aileron and rudder (right positive) and elevator (back
// positive) from -1 to 1, the throttle and flaps from 0 to 1, the gear -
// down at 1 - or none where it does not retract, the speedbrake lever -
// out at 1 - or none where it has no speedbrakes (since version 06), and the
// pitch trim, nose up positive, from -1 to 1 (since version 0F): what a
// player's client keeps of the trim the server held her on until her first
// input (frontend::keep_held).
struct Watched {
    std::uint8_t aircraft = 0;
    double aileron = 0.0;
    double elevator = 0.0;
    double rudder = 0.0;
    double throttle = 0.0;
    double flaps = 0.0;
    std::optional<double> gear;
    std::optional<double> speedbrake;
    double pitch_trim = 0.0;

    bool operator==(const Watched&) const = default;
};

// The flag, the aircraft's number and eight controls.
inline constexpr std::size_t watched_bytes = 1 + 1 + 8 * 2;
// What the gear is written as where it does not retract, and the speedbrake
// lever where there are no speedbrakes.
inline constexpr std::int16_t gear_fixed = -32768;
inline constexpr std::int16_t no_speedbrakes = gear_fixed;

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
