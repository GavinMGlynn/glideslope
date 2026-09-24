#pragma once

// An autopilot that takes off.
//
// The other half of `sim::Lander`, and the piece whose absence the client
// still announces: "the AI cannot take off; --on-ground is flown by the
// pilot". It flies four stages, in order:
//
//   roll     the throttle opened, the nose held on the centreline by rudder
//            and, while the rudder is still soft, by differential brake
//   rotate   from the lift-off speed: the nose raised to a take-off attitude
//   climb    the wheels off, the flaps raised, the best climb speed held
//   done     through the height it was asked to reach
//
// **A tail-wheel aeroplane swings, and a twin swings harder.** Directional
// control on the ground is the whole difficulty of a take-off and is why the
// rudder is helped by the brakes until it bites.
//
// Nothing here draws, and nothing here reads the DEM: the ground it leaves is
// the runway's own elevation.

#include "sim/aircraft.hpp"
#include "sim/lander.hpp"

#include <filesystem>
#include <string>

namespace glideslope::sim {

// How this aeroplane is taken off, from its own published figures.
struct DepartureSpeeds {
    double rotate_kts = 55.0; // the nose comes up here
    double climb_kts = 75.0;  // the best climb speed, published or measured
    // **What the take-off itself climbs away at.** For an aeroplane that
    // publishes a take-off roll it is `climb_kts`; for a jet, whose best
    // climb speed is an en-route one of 260 knots or more, it is V2 and ten
    // knots - 1.2 times the stall at the take-off flap, and ten - which is
    // what a jet climbs out at before it cleans up and accelerates.
    double initial_climb_kts = 75.0;
    double flap = 0.0;        // the take-off flap setting, 0 to 1
    // **A flying boat's running attitude on the water**, degrees, held from
    // the start of the run until the rotation speed, and three more from
    // there until the hull is clear - as Arthur Gouge flew the Short S.23's
    // take-off tests and as its published water take-off figure records.
    // Zero for a landplane, which rolls with the stick where it sits.
    double running_pitch_deg = 0.0;
    // Whether `rotate_kts` is the aeroplane's own published lift-off speed or
    // was worked from its published stall. The caller may want to say so.
    bool rotate_is_published = false;
};

// The speeds for an aircraft, from `data`/figures/MODEL.xml.
//
// The best climb speed is the speed its published climb rate was measured at.
// The lift-off speed is the one its published take-off roll was measured at
// where it has one; where it has not - the Cub publishes no take-off roll -
// it is a seventh above the published stall, which is the usual relation, and
// `rotate_is_published` says which it was. **An aeroplane with a published
// take-off field length** rotates from its stall at that field length's flap,
// and takes off with that flap, where it has a stall speed there; a jet's
// flaps-up stall is its landing stall's for want of any other, and 1.15 times
// that with the flaps up is below the speed a clean airliner flies at. Throws std::runtime_error where
// the aircraft publishes neither a climb speed nor anything to work a
// rotation speed from.
DepartureSpeeds departure_speeds(const std::filesystem::path& data,
                                 const std::string& model);

class Departure {
public:
    enum class Stage { roll, rotate, climb, done };

    // `to_ft` is the height above the runway at which the take-off is over.
    Departure(const Aircraft& aircraft, const Runway& runway,
              const DepartureSpeeds& speeds, double to_ft = 500.0);

    Controls fly();
    Stage stage() const { return stage_; }

    // Where the aeroplane is with respect to the runway, as the last `fly`
    // saw it: metres down the runway from the threshold, right of the
    // centreline, and above the threshold's elevation.
    double along_m() const { return along_m_; }
    double across_m() const { return across_m_; }
    double above_m() const { return above_m_; }

    // How far down the runway the wheels left it, in metres, or 0 before.
    double unstuck_along_m() const { return unstuck_along_m_; }

private:
    const Aircraft& a_;
    Runway runway_;
    DepartureSpeeds speeds_;
    double to_ft_ = 500.0;
    Stage stage_ = Stage::roll;

    double along_m_ = 0.0;
    double across_m_ = 0.0;
    double above_m_ = 0.0;
    double unstuck_along_m_ = 0.0;

    double throttle_ = 0.0;
    double pitch_trim_ = 0.0;
    double rotate_pitch_ = 0.0;
    bool unstuck_ = false;

    void measure();
};

} // namespace glideslope::sim
