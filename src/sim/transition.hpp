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
// back to conventional flight mode". Everything it reads of her is her
// model's (tools/make_f35b.py): the speed she converts at either way
// (`fcs/stovl-convert-kcas`), the least height she converts at
// (`fcs/stovl-convert-least-ft`), the nozzle's and the fan's vane box's
// travel, and - estimated, as nothing published gives them - how fast she
// slows and gathers speed and the attitude she gathers it at. The attitude
// she converts back to is reckoned from what she weighs and her wing's lift
// slope at her conversion speed.
//
// **Its stages**, each flown by what flies it best:
//
//   wing-borne   the autopilot (sim/autopilot.hpp) on the height and a
//                speed: her conversion speed, at no less than her least
//                conversion height, when a hover is asked;
//   converting   the fan's doors opening or closing, which way decided by the
//                ask alone. Opening from wing-borne flight, the same
//                autopilot, the nozzle aft and the fan's share holding the
//                attitude it asks; closing from jet-borne flight, the
//                jet-borne law below, the thrust all aft and her attitude
//                raised toward her wing-borne one as the fan's lift goes;
//   jet-borne    her own law, the lift system converted: the height on the
//                throttle, as the hover law's; the speed along her heading on
//                the thrust's angle - the nozzle and the fan's vane box
//                swung down together, past straight down to slow her - with
//                the speed asked moving at her rate toward a hover or past
//                her conversion speed; the attitude on the stabilator and
//                the fan's share together, from the attitude she had when
//                last asked down to level as the speed goes, or up toward her
//                gathering attitude by her conversion speed; the wings on the
//                ailerons and the posts; the heading on the nozzle's swivel
//                and the ball on the rudder;
//   hovering     the hover law (sim/hover.hpp), taken over in the air with
//                nothing stepped, over the point she stopped at.
//
// Asked for a hover she goes forward through them, and asked for wing-borne
// flight back, from wherever she is; asked the other way part-way, she turns
// back from where she is, the jet-borne law's integrals and the attitude and
// speed it reckons from taken afresh. **Wing-borne flight is flown no slower
// than her conversion speed**: asked for less, she gathers speed to it,
// converts back there and is held there - the slower part of the ask is
// refused. Asked for 150 KCAS, the autopilot slowing her wing-borne held her
// nose at its 15-degree limit with the throttle closed and she sank from
// 155 ft into the ground (PROJECT_STATUS, 2026-10-13): what flies her slower
// wing-borne is the autopilot's to answer, not this law's.

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
    //
    // **A hover may be over a point** (`over`):
    //
    //   wing-borne   she is steered at it while it is more than
    //                `steer_within_ft` away;
    //   jet-borne    she is not steered - her law flies her speed along her
    //                heading and nothing across it, so a turn there would
    //                leave her drifting sideways - and the speed asked is no
    //                more than she stops in, at her own rate, by the
    //                distance to it along her heading, so that she slows to
    //                a stop over it;
    //   hovering     the hover law holds over it, not over where she
    //                stopped.
    //
    // Without one she hovers where she stops, as before.
    //
    // **And hovering, she lets down to land** when `land` is asked (the hover
    // law's own landing, Hover::Target::land).
    struct Point {
        double latitude_deg = 0.0;
        double longitude_deg = 0.0;
    };
    //
    // **Flown by a pilot** (`pilot`), her unified control laws' inceptors,
    // each from -1 to 1, in STOVL mode:
    //
    //   lever    the left-hand inceptor's travel from its detent, aft to
    //            forward. Under her translational rate command's speed
    //            (`fcs/stovl-trc-kt`) it asks a ground speed along her
    //            heading in proportion - aft of the detent backward, to
    //            `fcs/stovl-trc-aft-kt`; above
    //            it, an acceleration - forward at her gathering rate, aft at
    //            her slowing rate, the detent holding the speed - up to her
    //            conversion speed. Either is reached at those rates. None
    //            is a lever the pilot has not moved: she slows to a hover,
    //            as without a pilot.
    //   side     the stick's sideways travel: in the hover a ground speed
    //            to the right, in proportion, to `fcs/stovl-trc-side-kt`;
    //   pedals   the pedals: in the hover a turn to the right, in
    //            proportion, to `fcs/stovl-turn-degps`.
    //
    // Jet-borne the stick's sideways and the pedals are not flown yet: her
    // law holds her heading and her wings level there.
    struct Inceptors {
        std::optional<double> lever;
        double side = 0.0;
        double pedals = 0.0;
    };
    struct Target {
        bool hover = true;
        double height_ft = 150.0;
        double kcas = 200.0;
        bool land = false;
        std::optional<Point> over;
        std::optional<Inceptors> pilot;
    };
    static constexpr double steer_within_ft = 2000.0;

    // **How far ahead of a point she must be asked for a hover over it** to
    // stop over it, from `kcas` wing-borne: the distance she stops in at her
    // own rate from her conversion speed, half again, and half a minute at
    // her conversion speed for the fan's doors to open - feet.
    static double stops_within_ft(const Aircraft& aircraft, double kcas);

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
        return hover_ ? &*hover_ : nullptr;
    }
    // The speed the jet-borne law asks of her now, knots along her heading.
    double speed_asked_kt() const {
        return speed_asked_kt_;
    }
    // **What a prediction cannot work out for itself** of this law, from the
    // server's (net::OwnMotion, sim::Prediction::reconcile): seeded on each
    // side from her as that side sees her when the law is made, a step apart
    // when the selector reached the server a step off, and kept apart by
    // both being put back to the same motion. Measured, of the law's state
    // only the wing-borne autopilot's pitch integral needed telling
    // (PROJECT_STATUS.md, 2026-10-16); none while no autopilot flies her -
    // converting, jet-borne or hovering, whose laws' integrals are not
    // carried.
    struct Carried {
        std::optional<double> pitch_integral_deg;
        bool operator==(const Carried&) const = default;
    };
    Carried carried() const;
    // Each part carried put on this law where it has that part to take it.
    void carry(const Carried& carried);

    // **The attitude she flies wing-borne at her conversion speed**, level,
    // for what she weighs now: her weight over her wing's area, her lift
    // slope and the dynamic pressure of her conversion speed, degrees.
    double wing_borne_pitch_deg() const;

private:
    void become_jet_borne(const Hover::Command& flying);
    void become_hovering(const Hover::Command& flying);
    // The jet-borne law's integrals from `flying`, and the speed and
    // attitude it reckons from where she is now.
    void seed_jet_borne(const Hover::Command& flying, bool from_hover);
    // The least height she converts at, or the height asked if higher.
    double conversion_height_ft(const Target& target) const;
    // Turned toward `target`'s point while it is far off; and the distance
    // to it along her heading, ft, if there is one.
    void steer_at(const Target& target);
    std::optional<double> ahead_of_point_ft(const Target& target) const;
    Hover::Command fly_wing_borne(const Target& target, bool converted);
    Hover::Command fly_jet_borne(const Target& target, bool back);
    // The speed asked moved as the pilot's lever asks it.
    void fly_lever(double lever);

    const Aircraft& a_;
    // Read from her model.
    double convert_kcas_ = 0.0;
    double least_ft_ = 0.0;
    double stovl_throttle_ = 0.0;
    double nozzle_most_deg_ = 0.0;
    double fan_least_deg_ = 0.0;
    double fan_most_deg_ = 0.0;
    double slow_kt_per_s_ = 0.0;
    double gather_kt_per_s_ = 0.0;
    double gathering_pitch_deg_ = 0.0;
    double lift_slope_per_rad_ = 0.0;
    double trc_kt_ = 0.0;
    double trc_aft_kt_ = 0.0;
    double trc_side_kt_ = 0.0;
    double turn_degps_ = 0.0;

    Stage stage_ = Stage::wing_borne;
    std::optional<bool> last_asked_hover_;
    double heading_deg_ = 0.0;
    std::optional<Autopilot> autopilot_;
    std::optional<Hover> hover_;
    double hover_base_ft_ = 0.0;
    // The jet-borne law's integrals and what it asks.
    double speed_asked_kt_ = 0.0;
    double pitch_from_deg_ = 0.0;
    double speed_from_kt_ = 0.0;
    double back_pitch_from_deg_ = 0.0;
    double throttle_integral_ = 0.0;
    double angle_integral_deg_ = 0.0;
    double elevator_integral_ = 0.0;
    double fan_integral_ = 0.0;
    double post_integral_ = 0.0;
    Hover::Command last_;
};

} // namespace glideslope::sim
