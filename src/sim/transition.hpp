#pragma once

// **The transition law**: an aeroplane with a lift system (LiftSystem in
// sim/aircraft.hpp, the F-35B's) flown between wing-borne flight and a hover,
// either way, at a height above the ground.
//
// The sequence is the one the F-35B's first transitions were flown in, as
// Lockheed Martin's Code One magazine tells them ("F-35 Lightning II Flight
// Test Update 1", 2010; docs/ASSETS.md): on 17 March 2010 the lead STOVL
// pilot "initiates conversion to STOVL mode at 200 knots. He then slows the
// aircraft to sixty knots and flies a decelerating approach to a zero
// airspeed hover at 150 feet above the runway"; on 7 January he engaged the
// lift fan at 210 knots, slowed to 180 "before accelerating and converting
// back to conventional flight mode". The speed she converts at either way is
// her model's, `fcs/stovl-convert-kcas` (tools/make_f35b.py); how fast she
// slows and gathers speed is not published, and is this law's estimate.
//
// **Its stages**, each flown by what flies it best:
//
//   wing-borne   the autopilot (sim/autopilot.hpp) on the height and a
//                speed: no faster than her conversion speed when a hover is
//                asked, the speed asked when wing-borne flight is;
//   converting   slowing, the same autopilot, the lift system converting
//                under it, the nozzle aft and the fan's share holding the
//                attitude it asks; gathering speed, the jet-borne law below
//                as the fan stops, the thrust all aft and her attitude raised
//                toward her wing-borne one as the fan's lift goes, and the
//                autopilot once it has;
//   jet-borne    her own law, the lift system converted: the height on the
//                throttle, as the hover law's; the speed along her heading on
//                the thrust's angle - the nozzle and the fan's vane box
//                swung down together, past straight down to slow her - with
//                the speed asked moving at a set rate toward a hover or
//                past her conversion speed; the attitude on the stabilator
//                and the fan's share together, from the attitude she had as
//                she converted down to level as the speed goes, or up from
//                level to a low attitude gathering speed; the wings on the
//                ailerons and the posts; the heading on the nozzle's swivel
//                and the ball on the rudder;
//   hovering     the hover law (sim/hover.hpp), taken over in the air with
//                nothing stepped, over the point she stopped at.
//
// Asked for a hover she goes forward through them, and asked for wing-borne
// flight back, from wherever she is.

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/hover.hpp"

#include <memory>
#include <optional>

namespace glideslope::sim {

class Transition {
public:
    enum class Stage { wing_borne, converting, jet_borne, hovering };

    // What it is asked: a hover, or wing-borne flight at `kcas`; at
    // `height_ft` above the ground beneath her either way.
    struct Target {
        bool hover = true;
        double height_ft = 150.0;
        double kcas = 200.0;
    };

    // Taken over from whatever is flying her now with `flying` - wing-borne
    // when her lift system is not converted, hovering when she stands on the
    // ground or hovers, the nozzle down, and jet-borne otherwise. Throws
    // std::logic_error for an aircraft without a lift system.
    Transition(const Aircraft& aircraft, const Hover::Command& flying);

    // One 120 Hz step's commands.
    Hover::Command step(const Target& target);

    Stage stage() const {
        return stage_;
    }
    // The hover law flying her, while she hovers; else none.
    const Hover* hover() const {
        return hover_.get();
    }
    // The speed the jet-borne law asks of her now, knots along her heading.
    double speed_asked_kt() const {
        return speed_asked_kt_;
    }

private:
    void become_wing_borne(const Hover::Command& flying);
    void become_jet_borne(const Hover::Command& flying);
    void become_hovering(const Hover::Command& flying);
    Hover::Command fly_wing_borne(const Target& target, bool converted);
    Hover::Command fly_jet_borne(const Target& target);

    const Aircraft& a_;
    double convert_kcas_ = 0.0;
    double stovl_throttle_ = 0.0;
    Stage stage_ = Stage::wing_borne;
    // Converting back to wing-borne flight, from the moment she reached her
    // conversion speed, whatever her speed does after.
    bool converting_back_ = false;
    double heading_deg_ = 0.0;
    std::unique_ptr<Autopilot> autopilot_;
    std::unique_ptr<Hover> hover_;
    double hover_base_ft_ = 0.0;
    // The jet-borne law's integrals and what it asks.
    double speed_asked_kt_ = 0.0;
    double pitch_from_deg_ = 0.0;
    double speed_from_kt_ = 0.0;
    double throttle_integral_ = 0.0;
    double angle_integral_deg_ = 0.0;
    double elevator_integral_ = 0.0;
    double fan_integral_ = 0.0;
    double post_integral_ = 0.0;
    Hover::Command last_;
};

} // namespace glideslope::sim
