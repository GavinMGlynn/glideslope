#include "sim/autopilot.hpp"

#include "sim/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace glideslope::sim {

namespace {

constexpr double dt = 1.0 / static_cast<double>(steps_per_second);

// Heading to bank: a degree of bank for each degree off, 25 at most, moving
// at 5 degrees a second.
constexpr double bank_per_degree = 1.0;
constexpr double bank_rate_degps = 5.0;
// **But no more bank than the aeroplane can sustain.** A level turn needs
// its weight over the cosine of the bank in lift, and the induced drag that
// lift costs grows with its square, so a turn wants power a straight line
// does not. Near its ceiling a light aeroplane has little to spare: the
// altitude hold keeps the height by pitching up, the speed bleeds away, and
// at twenty-five degrees a Cessna 182 turning once round near its ceiling
// slowed from 82 knots to 60, onto the back of its drag curve.
//
// So **while the throttle can give no more, a turn may spend 3 knots of the
// aeroplane's energy and then no more**: from there the bank the heading may
// ask for is what holds the energy - its height and its airspeed together -
// where it is, found by an integral on how far the energy is from there and
// how fast it is going, never below 10 degrees. The energy is counted from
// when the wings leave level, so what the throttle made up before it reached
// its stop is not spent twice. When the throttle has more to give, or the
// wings come level, the limit goes back to 25 degrees at 5 degrees a second,
// so in every other flight nothing here binds.
//
// The energy is Lambregts' total energy (Vertical flight path and speed
// control autopilot design using total energy principles, AIAA 83-2239), and
// the rule is the sustained turn's: how much load factor - how much bank - an
// aeroplane holds without losing height or speed is set by the power it has
// in excess (Hurt, Aerodynamics for Naval Aviators, NAVWEPS 00-80T-80).
// Airliners' flight guidance limits bank by fixed schedules instead - the
// A320's reduced at low speed and with an engine out - and light aircraft
// autopilots by a fixed limit (22 degrees for Garmin's GFC 700); this finds
// the limit from what the aeroplane is doing, because nothing tells the
// autopilot its power.
constexpr double least_sustained_bank_deg = 10.0;
constexpr double turn_may_spend_fps = 3.0 * 1852.0 / 3600.0 / 0.3048;
constexpr double bank_per_energy_ft = 0.05;   // degrees a second, per foot
constexpr double bank_per_energy_fpm = 0.02;  // degrees a second, per ft/min
constexpr double banked_deg = 5.0;            // a turn, rather than wings level
constexpr double energy_filter_s = 1.0;
constexpr double throttle_stop = 0.999;
constexpr double g_fps2 = 32.174;
// And the bank a heading needs held - against the propeller's slipstream and
// torque - found by an integral within 10 degrees of it, 5 degrees at most.
constexpr double bank_integral_per_degree = 0.1;
constexpr double bank_integral_within_deg = 10.0;
constexpr double most_bank_integral_deg = 5.0;
// Bank to aileron, and the roll rate's damping; an aileron offset at engaging,
// fading over two seconds.
constexpr double aileron_per_degree = 0.04;
constexpr double aileron_per_degps = 0.02;
// The aileron the bank needs held, found by an integral: a tenth of the
// proportional gain a second, so it settles over about ten seconds, well
// slower than the roll; never more than a quarter of the travel.
constexpr double aileron_trim_per_degree = 0.004;
constexpr double most_aileron_trim = 0.25;
constexpr double offset_fade_s = 2.0;
// The ball to rudder.
constexpr double rudder_per_degree = 0.1;
// **The sideslip's integral** holds the rudder a steady sideslip needs, at
// 0.05 of travel a second per degree (Aircraft::rudder_integral_rate, the
// catalogue's `yaw-damper`). **Slow, the B-2A takes a fifth of that and the
// F-22A a tenth**: the integral lags the sideslip and feeds a swing that
// their rudders there cannot damp - in a 20 kt crosswind the B-2A swung 7.3
// degrees either way at 159 kt and the F-22A 7.8 at 220. At a fifth the B-2A
// holds within 0.6 from 154 kt (at a third, 0.66 at 159); the F-22A, a tenth
// with three times the yaw damper (below), within 0.3 from 135, where with
// either alone she still swung up to 190. Not every aircraft's: at a third
// within two degrees of sideslip (2026-10-02) it spun the S.23 in her stall
// lesson.
// **And a yaw damper: the rudder against the yaw rate, washed out over a
// second**, so a steady turn's rate asks nothing of it (the washout of
// Stevens, Lewis and Johnson, Aircraft Control and Simulation, 3rd ed.,
// chapter 4, the yaw damper). The sideslip alone gives the rudder no
// damping, only stiffness, and the rudder moves no faster than a hand (below):
// once a yaw swing asks the rudder for more than a travel a second, the
// rudder lags the sideslip by a quarter of a cycle and feeds the swing. A
// crosswind arriving at once, a 10 to 28 degree sideslip, set it going: in a
// 20 kt crosswind the Cub and the Cherokee at their approach speeds swung 35
// degrees either way every three seconds and never settled, and the Cessnas
// at their cruise 14 (every_light_aeroplane_holds_a_heading_in_a_20_kt_crosswind_and_in_calm_air_without_yawing).
// The Cub's rudder turns it 2.4 times as hard as the 172's, a travel for a
// travel, and the Cherokee's 1.7 times, so for them the swing came soonest.
// 0.03 of travel per degree a second was the least that settled all four;
// this is 0.05 (Aircraft::yaw_damper_per_degps, the catalogue's `yaw-damper`
// where an aircraft says otherwise), and 0.1 settles them as well. The F-22A
// takes 0.15; more does not help the B-2A slow: at 0.15 she swung 8.7
// degrees and at 0.3 13.9.
constexpr double yaw_washout_s = 1.0;
// Altitude to vertical speed: 3 ft/min for each foot off.
constexpr double fpm_per_foot = 3.0;
// Vertical speed to pitch, which moves at 3 degrees a second at most, within
// -10 and 15 degrees.
constexpr double pitch_per_fpm = 0.004;
constexpr double pitch_integral_per_fpm = 0.002;
constexpr double pitch_rate_degps = 3.0;
constexpr double least_pitch_deg = -10.0;
constexpr double most_pitch_deg = 15.0;
// How far short of the climb asked, feet a minute, with the nose at its
// highest, before the speed asked rises for the climb; and how far it may.
constexpr double nose_at_stop_short_fpm = 200.0;
constexpr double most_climb_speed_kts = 40.0;
// **The airspeed on the elevator** (AutopilotModes::speed_on_elevator): at
// full power, the wing held below its stall and the aeroplane asked to sink
// no more than 20 ft/min for each knot it is short of the speed, by the
// vertical speed's own gains. **The speed comes from the engine and from as
// little height as that sink allows**, not from the nose: half a degree of
// nose down for each knot short, which this was until 2026-10-09, dived for
// the speed - a Learjet 35A recovered at her stall warning lost 589 ft
// against her lesson's 350, the B-2A 414 against 300. Held to the sink, the
// B-2A loses 112 and the Learjet 410. **Where the speed will not come so,
// more sink is asked**: 50 ft/min more each second for each knot a second
// the speed gains slower than 0.3 a second, for as long as it does. A
// Mosquito at 20,000 ft with her flaps and gear down, handed over at the
// lesson's entry's end, otherwise settled short of her speed, sinking, and
// never ended the lesson; with it she loses 504 ft.
//
// **It is how a stall is recovered.** Asked for a vertical speed, an aeroplane
// mushing in a stall sinks faster than it is asked to, and the vertical speed
// loop answers by raising the nose - which holds it in the stall. A B-2A left
// thirty seconds at 96 knots, 12 degrees nose up and 30 degrees of alpha,
// sinking 4,100 ft/min, was flown that way from 19,700 ft into the ground.
// Flying the speed instead puts the nose down until the wing is unloaded and
// the speed comes, whatever the height is doing, which is the recovery the
// FAA teaches (Airplane Flying Handbook, FAA-H-8083-3C, chapter 5: reduce the
// angle of attack first, then level the wings, add power as needed, and
// return to the flight path wanted).
//
// **Short of the speed the nose may go below the pitch envelope's floor, as
// far as the flight path and no further than thirty degrees down**, and moves
// at 8 degrees a second rather than 3. Held at ten degrees down, a B-2A sinking
// at thirty degrees of flight path was still at twenty of alpha and came back
// up into it; the pitch that unloads the wing is below the flight path it is
// already on, so it is that path that bounds the nose, not a fixed attitude.
// The nose goes down at 8 degrees a second and comes up at the 3 every mode
// uses; the elevator still moves no faster than a hand.
//
// **The wing is kept a tenth below the angle it stalled at**, which the
// autopilot learns by watching where the lift stopped rising
// (`stall_alpha_deg_`), because a speed alone does not unload a wing that is
// diving fast enough: an A320 past its speed at 27 degrees of alpha was held
// there. A tenth, because what the autopilot learns is a little past what a
// steady stall gives: a Cessna 172P's was 17.2 degrees where the stall test
// measures 16.0, and held at 16.3 she mushed there, short of her speed.
//
// **And below the angle that would pull 1.6 g at the speed it is doing**,
// which is a schedule on the angle of attack, not on the load: the lift
// answers the angle at once, where the load the nose is pulled to lags it.
// The angle is the one now moved by what the lift coefficient is short of
// or over 1.6 g's, along the lift's mean slope up to its peak. An F-15C
// left thirty seconds in her stall, held at 34 degrees of alpha as her speed
// came, pulled 1.90 g without it, and 1.66 with it.
//
// **A pitch above the nose, asked of a stalled wing, is no longer asked**:
// the pitch command comes down to the nose at once while the wing is past
// its stall, where it used to walk down from wherever the entry left it. An
// A320 left thirty seconds in her stall was handed over with the command 16
// degrees above her nose and her elevator full nose-up, and kept it there two
// seconds while she dived from 156 to 166 knots; her wing came back through
// its lift's peak at 178 knots, at 2.18 g. The elevator still moves at a
// hand's pace, so nothing steps.
//
// **The pull-out is held under 1.6 g**, below the 2 g the airworthiness rules
// require with the flaps out (14 CFR 23.345, 25.345): past it the nose goes
// down 10 degrees below where it is for each g over. The aeroplane's response
// lags the command, so what is measured is a little more.
//
// **Once at its speed and unstalled, the nose goes down no faster than any
// mode's 3 degrees a second**: at 8 an F-15C recovered at her warning, 60
// knots past her speed in a zoom, was pushed to less than nothing and lost
// as much height again.
constexpr double sink_per_knot_short_fpm = 20.0;
constexpr double least_gain_kts_per_s = 0.3;
constexpr double sink_per_slow_gain = 50.0; // ft/min a second, for each knot a second slow
constexpr double stall_alpha_kept_below = 0.1;
constexpr double steepest_unload_deg = -30.0;
constexpr double unload_rate_degps = 8.0;
constexpr double pull_out_most_g = 1.6;
constexpr double pitch_per_g_over = 10.0;
constexpr double past_the_peak_deg = 0.3;
constexpr double above_the_nose_deg = 5.0;
constexpr double stall_trend_s = 0.25;
constexpr double configuration_moved = 0.5; // degrees of flap, or a twentieth of the gear
// Pitch to elevator, the pitch rate's damping, and the trim the integral finds.
constexpr double elevator_per_degree = 0.05;
constexpr double elevator_per_degps = 0.03;
constexpr double trim_rate = 0.02;
// **An upset**: banked past this - 45 degrees, AC 120-111's own definition of
// an upset's bank - the wings are brought level before the nose is raised -
// unload, roll, then pull, the nose-low recovery of the FAA's Airplane Upset
// Prevention and Recovery Training Aid (AC 120-111) - and the
// elevator's trim is not wound on a pitch it cannot have. Pulling in a spiral
// tightens it: a Cessna handed over at 65 degrees of bank, 49 nose down,
// pulled for 20 s with its trim winding up, rolled level only after it, and
// zoomed 400 ft through the height it was held to, to 52 kt.
constexpr double upset_bank_deg = 45.0;
// The bank asked comes back to level this fast in an upset, three times a
// turn's: the aileron it takes still moves no faster than a hand.
constexpr double upset_roll_rate_degps = 15.0;
// **No loop moves a control faster than a pilot's hand: its full travel in a
// second, 1/120 of it in a step.** In ordinary flight none of them comes near
// this and the limit never binds. It binds where the laws are reading a fast
// moving measurement - recovering from a spiral, the rudder's term follows
// sideslip swinging tens of degrees a second - and there it once moved the
// rudder from one stop to the other, 2.0 of travel, in a single frame.
constexpr double a_hands_pace = 1.0 / static_cast<double>(steps_per_second);
// Airspeed to throttle, which moves at a quarter of its travel a second -
// slower than a hand, because an engine does not care to be slammed.
constexpr double throttle_per_knot = 0.08;
constexpr double throttle_integral_per_knot = 0.02;
constexpr double throttle_rate = 0.25;
// **Handed over climbing faster than the climb asked, she is eased down to
// it** (Autopilot::ease_climb), at 25 ft/min a second, rather than stepped:
// a take-off hands a light aeroplane over at full throttle climbing 1,000
// to 1,400 ft/min, and a plan asks 700. Stepped, the nose came down at once
// and the speed ran on while the throttle, held at its stop until the speed
// was past the one asked and then coming back at a quarter of its travel a
// second, caught up: the 182S 2.1 kt over her 82.0. The rate is the
// throttle's integral's: dropping 600 ft/min is about a quarter of a 182S's
// power, and the integral moves the throttle 0.02 a second for each knot
// off, so a quarter takes about 12 s at a knot off - 50 ft/min a second -
// and half that rate keeps it inside a knot. Measured, the 182S 1.1 kt over
// at 25, 1.0 at 15, 1.6 at 50, 2.1 stepped.
constexpr double ease_climb_fpm_per_s = 25.0;

// **Asked for a height it cannot hold, the height goes and the airspeed
// stays.** With the throttle at its stop the altitude hold can only buy
// height with airspeed, and it used to go on buying: a Cessna 172P asked for
// a height above its ceiling was flown to 46 knots and sinking. So when the
// throttle has no more to give - it is at its stop, or no speed is held and
// the throttle is not the autopilot's to move - and the airspeed, five
// seconds ahead on its trend, would be within a knot of the least it may fly
// at, the climb the altitude hold may ask for is limited, and the limit
// is found by an integral on the airspeed: 30 ft/min a second for each knot
// above the least, and 500 ft/min for each knot it moves. From the climb the
// aeroplane has when it starts, the limit comes down as the speed falls
// towards the least and holds it there, climbing what the aeroplane can at
// that speed or descending, and while it binds the throttle opens to its
// stop; it lets go when it no longer binds. The best-climb speed is the floor
// because it is where an aeroplane climbs fastest: a height it cannot hold
// there it can hold at no speed, and slower is only nearer the stall.
//
// **What was tried first, and why not.** Holding the speed as soon as it fell
// near the least, with throttle still to give, held it in ordinary flight at
// the best-climb speed too: the throttle opened, a Cessna at 3,000 ft sped up
// to 82 knots and was 110 ft off its height. Holding it from 5 knots above
// the least chased the gusts in moderate turbulence, 80 knots in a 500 ft/min
// climb taken for a speed about to fail, and the climb held to nothing.
// Holding it from a knot above the least without the trend let an aeroplane
// slowing fast on little power through by 2.3 knots. And holding the speed
// without opening the throttle left the throttle and the elevator sharing it,
// the throttle at 0.8 and a Cherokee coming down 1,250 ft when it could have
// climbed.
//
// **The least is the best-climb speed, or 5 knots below a slower speed asked
// for.** A speed asked for below the best-climb speed is flown as asked - slow
// flight, an approach, a stall lesson's entry - and is a speed to hold, not a
// floor. So the least is 5 knots below it, or as far below it as the
// best-climb speed is above it where that is less, so that the least moves
// smoothly as the speed asked for passes the best-climb speed. The altitude
// hold still flies an aeroplane into the stall when it is asked for a speed
// below the stall, as a stall lesson enters one.
//
// **Only a light aeroplane's** (sim::Aircraft's `climb_floor_kts`). A light
// aeroplane's climb speed is its handbook's best rate of climb, Vy, the
// speed the whole rule stands on. The other classes' climb speeds are not
// that. The jets' - airliners, the business jet, fighters and the bomber - are
// the speeds their climb rates were measured at on the model, chosen, not
// found to be the best. The Mosquito's is the speed of one climb test at
// 10,400 ft in one supercharger gear, and the S.23's the speed of its sea-level
// climb at one boost. None is the speed below which no height can be held,
// and a jet is kept from slowing by its angle of attack and a minimum speed,
// which this autopilot does not model. So they have no floor, as before, and
// a test names each.
//
// **Only clean: flaps up, and the gear up where it retracts.** The best-climb
// speed is published clean, and it is the aeroplane's best climb only so; with
// the flaps and gear down it is neither the best climb nor a floor worth
// diving for. A Learjet in the stall lesson, gear and flaps down at 20,000 ft
// and unable to make the 250 knots it held at full thrust, dived 235 ft to
// hold its clean best-climb speed of 240 before the lesson had begun. Asked
// for a speed it cannot make, a clean aeroplane settles at the speed it can,
// which is above its best-climb speed by construction - the floor binds only
// below it - so a speed out of reach is never a reason to dive.
//
// **A turn may spend 3 knots.** The bank limit below lets a turn at full
// throttle spend 3 knots of the aeroplane's energy and then banks no more than
// it can sustain; the altitude hold keeps the height, so what is spent is
// speed. With the floor at the best-climb speed that spend became height
// instead - a Cessna 182 near its ceiling lost 32 ft in a turn, against the
// 20 ft band the turn is held to - and the floor acted before the bank limit
// had found the bank to sustain. So while the wings are banked the least is 3
// knots lower, the bank limit's own allowance, and it stays lower until the
// speed is back at the least without it, so that rolling out does not dive
// to buy back what the turn spent. The two rules then agree: the bank limit
// keeps a turn within 3 knots, and the floor catches only what gets past it.
//
// **Nor does it wait for the throttle when the throttle cannot be there in
// time.** Asked from cruise into her best-rate climb, a light aeroplane's
// throttle is cut to idle for the slower speed, and opens again at a quarter
// of its travel a second only as the speed comes down to it - four seconds
// to its stop, while she slows two knots a second with the nose up for the
// climb. The floor waited for the stop, and every light aeroplane sank 4.9
// to 6.3 knots through her climb speed, the J-3 Cub from 47.8 to 42.6. So
// the floor also takes over while the speed, on its trend, would reach the
// least before the throttle could reach its stop: the throttle opens fully
// at once, and the climb she has is held - not wound down on how fast she
// is still slowing, which took a Cessna down 800 ft/min at 80 knots - until
// she is within its knot of the least or no longer slowing; from there the
// limit finds the climb that keeps the speed, as above. It is the speed
// capture of a flight level change - the speed on the climb while the power
// comes up - for the one transition this autopilot otherwise flies on the
// throttle alone. Now each is within 1.3 knots of her climb speed.
//
// This is the underspeed protection of Lambregts' total energy control
// (AIAA 83-2239), which short of speed gives the elevator to the speed and
// the throttle all it has - as a floor alone rather than the whole scheme,
// because everywhere else this autopilot holds height on the elevator and
// speed on the throttle, and nothing here changes that.
constexpr double hold_speed_within_kts = 1.0;
constexpr double turn_may_spend_kts = 3.0;
constexpr double clean_flaps_deg = 0.5;
constexpr double below_asked_kts = 5.0;
constexpr double speed_trend_s = 5.0;
constexpr double speed_trend_filter_s = 1.0;
constexpr double climb_limit_per_knot = 30.0;        // ft/min a second
constexpr double climb_limit_per_knot_moved = 500.0; // ft/min
// Slowing faster than this, a knot a second, the floor may take over before
// the throttle is at its stop (below); slowed into a climb from cruise a
// light aeroplane slows two.
constexpr double slowing_kts_per_s = 1.0;

// Flaps up, and gear up where it retracts: the configuration the best-climb
// speed is published in, and the only one it is a floor for.
bool clean(const Aircraft& a) {
    if (a.has_property("fcs/flap-pos-deg") &&
        a.property("fcs/flap-pos-deg") > clean_flaps_deg) {
        return false;
    }
    return !a.gear_retracts() || a.property("gear/gear-pos-norm") <= 0.0;
}

double degrees(double radians) {
    return radians * 180.0 / std::numbers::pi;
}

double toward(double from, double to, double most) {
    return from + std::clamp(to - from, -most, most);
}

} // namespace

Autopilot::Autopilot(const Aircraft& aircraft, const Controls& controls)
    : a_(aircraft), last_(controls), last_kts_(aircraft.property("velocities/vc-kts")) {
    // Holding what the aircraft is doing now.
    modes_.heading_deg = a_.property("attitude/psi-deg");
    modes_.altitude_ft = a_.property("position/h-sl-ft");
    modes_.airspeed_kts = a_.property("velocities/vc-kts");
    // Each loop set to give the controls the aircraft has.
    bank_command_deg_ = a_.property("attitude/phi-deg");
    {
        const double v = a_.property("velocities/vt-fps");
        last_energy_ft_ = a_.property("position/h-sl-ft") + v * v / (2.0 * g_fps2);
        turn_energy_ft_ = last_energy_ft_;
    }
    // **Engaging steps nothing, whatever attitude it is handed.** The loop
    // starts commanding the attitude the aeroplane has, even when that is
    // outside the envelope it is allowed to ask for, and walks into the
    // envelope at its own pitch rate over the frames after. Seeding it to
    // the clamped value instead left the first frame asking for a pitch it
    // could not have and the elevator jumped by the difference: 0.21 of its
    // travel at nineteen degrees nose up, 0.80 out of a diving turn, where a
    // pilot's hand moves 0.017 in a frame.
    //
    // The integrals below are only a starting guess - what was roughly
    // holding the aeroplane. They used to carry a term cancelling the law's
    // own damping so that the first step landed exactly on the handed
    // control, and **that cancellation broke whenever the damping term was
    // larger than the integral's own limit**: a Mosquito handed over skidding
    // in its landing roll seeded a rudder integral of 9, kept 1 of it, and
    // slammed the rudder to its stop - the full travel, in one frame. The
    // first step in `fly()` now measures what the laws actually give and
    // carries the difference as an offset instead, which cannot break.
    pitch_command_deg_ = a_.property("attitude/theta-deg");
    // The vertical speed loop's integral is seeded on the first step, from
    // the climb asked then (`seed_climb_`).
    pitch_integral_deg_ = pitch_command_deg_;
    elevator_trim_ = controls.elevator;
    rudder_integral_ = controls.rudder;
    steady_yaw_rate_degps_ = degrees(a_.property("velocities/r-rad_sec"));
    throttle_integral_ = controls.throttle;
    if (a_.mixture_lever()) {
        leaner_.emplace(a_, controls.mixture);
    }
}

std::optional<double> Autopilot::height_flown_to_ft() const {
    if (!modes_.altitude_ft) {
        return std::nullopt;
    }
    double h = *modes_.altitude_ft;
    if (floor_ft_) {
        h = std::max(h, *floor_ft_);
    }
    if (ceiling_ft_) {
        h = std::min(h, *ceiling_ft_);
    }
    return h;
}

Controls Autopilot::fly() {
    Controls c = last_;

    // Altitude, to the vertical speed wanted.
    const double climb_fpm = a_.property("velocities/h-dot-fps") * 60.0;
    double climb_wanted = modes_.vertical_speed_fpm;
    if (const std::optional<double> to_ft = height_flown_to_ft()) {
        const double rate = std::abs(modes_.vertical_speed_fpm);
        climb_wanted = std::clamp(fpm_per_foot * (*to_ft - a_.property("position/h-sl-ft")),
                                  -rate, rate);
    } else {
        // A vertical speed held, no further than the limits: at either, held
        // there as a height would be.
        const double h = a_.property("position/h-sl-ft");
        const double rate = std::max(std::abs(modes_.vertical_speed_fpm), 700.0);
        if (ceiling_ft_) {
            climb_wanted = std::min(climb_wanted,
                                    std::clamp(fpm_per_foot * (*ceiling_ft_ - h), -rate, rate));
        }
        if (floor_ft_) {
            climb_wanted = std::max(climb_wanted,
                                    std::clamp(fpm_per_foot * (*floor_ft_ - h), -rate, rate));
        }
    }

    // **A climb she was handed steeper than the one asked comes down to it
    // at `ease_climb_fpm_per_s`** (ease_climb), not in a step.
    if (eased_climb_fpm_) {
        if (climb_wanted >= *eased_climb_fpm_) {
            eased_climb_fpm_.reset();
        } else {
            eased_climb_fpm_ =
                std::max(climb_wanted, *eased_climb_fpm_ - ease_climb_fpm_per_s * dt);
            climb_wanted = *eased_climb_fpm_;
        }
    }

    // **The vertical speed loop starts from the pitch she has, whatever
    // climb is asked first**: its integral is her pitch less the
    // proportional part of the climb asked over the climb she has, so its
    // first command is her pitch. It was her pitch plus that part of the
    // climb she had, which is the same only for a climb asked of none - an
    // autopilot engaged holding its height, as it is in a stall or a
    // descent, where nothing changes. Asked for any other climb first, the
    // nose was stepped by the proportional part of it: a light aeroplane
    // handed from her take-off at 1,025 ft/min, eased (below) from that
    // climb, was pitched 4 degrees up, and the 172P sagged 3.2 kt.
    if (seed_climb_) {
        seed_climb_ = false;
        pitch_integral_deg_ = pitch_command_deg_ - pitch_per_fpm * (climb_wanted - climb_fpm);
    }

    // The airspeed and its trend, kept current whatever the modes and the
    // configuration, so that nothing jumps when either changes.
    const double kts = a_.property("velocities/vc-kts");
    const double moved_kts = kts - last_kts_;
    last_kts_ = kts;
    kts_per_s_ += (moved_kts / dt - kts_per_s_) * dt / speed_trend_filter_s;

    // **The angle of attack the wing stalls at, as this aeroplane has flown
    // it**: the angle at the greatest lift coefficient seen since the flaps
    // or the gear last moved. Nothing tells the autopilot where a model's
    // lift peaks, so it watches: an aeroplane that has flown past its peak
    // has shown it, and one that has not has shown only angles it flew
    // without stalling. The stall recovery keeps the wing below it.
    {
        const double config = (a_.has_property("fcs/flap-pos-deg")
                                   ? a_.property("fcs/flap-pos-deg")
                                   : 0.0) +
                              10.0 * a_.property("gear/gear-pos-norm");
        const double alpha = a_.property("aero/alpha-deg");
        const double lift = a_.property("forces/fwz-aero-lbs") /
                            std::max(a_.property("aero/qbar-psf") *
                                         a_.property("metrics/Sw-sqft"),
                                     1.0);
        if (std::abs(config - lift_config_) > configuration_moved) {
            lift_config_ = config;
            most_lift_ = lift;
            stall_alpha_deg_ = alpha;
        } else if (lift > most_lift_) {
            most_lift_ = lift;
            stall_alpha_deg_ = alpha;
        }
        // **Seen to stall**: past that angle with the angle rising and the
        // lift falling, both on their trends over a quarter of a second -
        // the lift actually going over its peak, not an angle past one a
        // gust or a pull once set. Until the wing is back under the angle.
        alpha_trend_ += ((alpha - last_alpha_) / dt - alpha_trend_) * dt / stall_trend_s;
        lift_trend_ += ((lift - last_lift_) / dt - lift_trend_) * dt / stall_trend_s;
        last_alpha_ = alpha;
        last_lift_ = lift;
        if (alpha <= stall_alpha_deg_) {
            seen_to_stall_ = false;
        } else if (alpha > stall_alpha_deg_ + past_the_peak_deg && alpha_trend_ > 0.0 &&
                   lift_trend_ < 0.0) {
            seen_to_stall_ = true;
        }
    }

    // Held back, when the speed is short, to what keeps the airspeed at the
    // least it may fly at.
    const double climb_asked = climb_wanted;
    if (const std::optional<double> floor_kts = a_.climb_floor_kts()) {
        const double speed_now_fps = a_.property("velocities/vt-fps");
        if (!clean(a_)) {
            // **No floor with the flaps or the gear out, and none held over.**
            // A hold left standing would pin the throttle at its stop.
            holding_speed_ = false;
            turn_allowance_kts_ = 0.0;
        } else {
            double least_kts = *floor_kts;
            if (modes_.airspeed_kts && *modes_.airspeed_kts < least_kts) {
                least_kts = *modes_.airspeed_kts -
                            std::min(least_kts - *modes_.airspeed_kts, below_asked_kts);
            }
            // A turn may spend its allowance of speed (see the bank limit
            // below), and has it until the speed is back.
            if (std::abs(bank_command_deg_) >= banked_deg) {
                turn_allowance_kts_ = turn_may_spend_kts;
            } else if (kts >= least_kts) {
                turn_allowance_kts_ = 0.0;
            }
            least_kts -= turn_allowance_kts_;
            const double over_kts = kts - least_kts;
            // **Or the throttle cannot be at its stop in time**: slowing
            // towards the least, the speed would be there before the
            // throttle, at `throttle_rate`, could open fully - a light
            // aeroplane asked into her best-rate climb from cruise, which
            // wants all her power, with her throttle cut for the slower
            // speed (`early_hold_`, below). Only while she is slowing by
            // more than `slowing_kts_per_s`: level at her best-climb speed,
            // a wobble's tenths of a knot a second engaged it, opened the
            // throttle, and a Cessna at 3,000 ft held her height only to
            // 17 ft, and 23 in a turn.
            const bool too_late =
                modes_.airspeed_kts && kts_per_s_ < -slowing_kts_per_s && over_kts > 0.0 &&
                (throttle_stop - last_.throttle) / throttle_rate >= over_kts / -kts_per_s_;
            const bool at_stop = !modes_.airspeed_kts || last_.throttle >= throttle_stop;
            if (!holding_speed_ && (at_stop || too_late) &&
                over_kts + speed_trend_s * std::min(kts_per_s_, 0.0) <
                    hold_speed_within_kts) {
                holding_speed_ = true;
                early_hold_ = !at_stop;
                climb_limit_fpm_ = std::min(climb_fpm, climb_wanted);
            } else if (holding_speed_) {
                // **Engaged early, the climb she has is held while the
                // throttle opens**, until the speed is within the hold's
                // knot of the least, or no longer slowing: from there the
                // limit finds the climb that keeps it, and lets go once she
                // has the climb asked. Wound from the start, on how fast she was
                // still slowing, it took her down 800 ft/min at 80 kt.
                early_hold_ =
                    early_hold_ && over_kts > hold_speed_within_kts && kts_per_s_ < 0.0;
                if (!early_hold_) {
                    climb_limit_fpm_ += climb_limit_per_knot * over_kts * dt +
                                        climb_limit_per_knot_moved * moved_kts;
                }
            }
            if (holding_speed_) {
                // **Bounded, so it cannot wind up**: no more than the climb
                // asked for, and no steeper than the descent the pitch
                // envelope's least pitch gives at this speed - past that the
                // pitch cannot follow it. Not the vertical speed the altitude
                // hold captures heights at: short of power the descent that
                // keeps the speed is steeper than that, and bounded there a
                // Cherokee on a fifth of its throttle slowed to 63 knots.
                const double steepest_fpm =
                    -speed_now_fps * 60.0 * std::sin(-least_pitch_deg * std::numbers::pi / 180.0);
                climb_limit_fpm_ = std::clamp(climb_limit_fpm_, steepest_fpm,
                                              std::max(climb_wanted, steepest_fpm));
                if (climb_limit_fpm_ >= climb_wanted) {
                    holding_speed_ = false;
                } else {
                    climb_wanted = climb_limit_fpm_;
                }
            }
        }
    }

    // The aeroplane's energy, as the height it would have with its true
    // airspeed climbed away, and its rate, smoothed over a second.
    const double speed_fps = a_.property("velocities/vt-fps");
    const double energy_ft =
        a_.property("position/h-sl-ft") + speed_fps * speed_fps / (2.0 * g_fps2);
    energy_fpm_ += ((energy_ft - last_energy_ft_) / dt * 60.0 - energy_fpm_) * dt /
                   energy_filter_s;
    last_energy_ft_ = energy_ft;

    // Heading, through bank, to aileron.
    const double phi = a_.property("attitude/phi-deg");
    const bool upset = std::abs(phi) > upset_bank_deg;
    const double p = degrees(a_.property("velocities/p-rad_sec"));
    if (std::abs(bank_command_deg_) < banked_deg) {
        turn_energy_ft_ = energy_ft;
        spent_ = false;
    } else {
        // A descent asked for is energy the turn is not spending.
        turn_energy_ft_ += std::min(climb_asked, 0.0) / 60.0 * dt;
    }
    const double above_ft =
        energy_ft - (turn_energy_ft_ - speed_fps * turn_may_spend_fps / g_fps2);
    if (last_.throttle < throttle_stop || std::abs(bank_command_deg_) < banked_deg) {
        sustained_bank_deg_ =
            toward(sustained_bank_deg_, most_bank_deg, bank_rate_degps * dt);
    } else {
        spent_ = spent_ || above_ft <= 0.0;
        if (spent_) {
            sustained_bank_deg_ = std::clamp(
                sustained_bank_deg_ +
                    (bank_per_energy_ft * above_ft + bank_per_energy_fpm * energy_fpm_) * dt,
                least_sustained_bank_deg, most_bank_deg);
        }
    }
    double bank_wanted = 0.0;
    const std::optional<double> heading_deg = away_deg_ ? away_deg_ : modes_.heading_deg;
    if (!away_deg_ && modes_.bank_deg && !modes_.heading_deg) {
        // A bank asked for, and no heading: flown, with none of the
        // heading's integral.
        bank_integral_deg_ = 0.0;
        bank_wanted = std::clamp(*modes_.bank_deg, -sustained_bank_deg_, sustained_bank_deg_);
    } else if (heading_deg) {
        const double off = std::remainder(*heading_deg - a_.property("attitude/psi-deg"), 360.0);
        if (std::abs(off) < bank_integral_within_deg) {
            bank_integral_deg_ =
                std::clamp(bank_integral_deg_ + bank_integral_per_degree * off * dt,
                           -most_bank_integral_deg, most_bank_integral_deg);
        }
        bank_wanted = std::clamp(bank_per_degree * off + bank_integral_deg_,
                                 -sustained_bank_deg_, sustained_bank_deg_);
    }
    // In an upset, wings level first, and briskly: no turn is asked until
    // they are, and the bank asked comes back at the upset's roll rate.
    if (upset) {
        bank_wanted = 0.0;
    }
    bank_command_deg_ = toward(bank_command_deg_, bank_wanted,
                               (upset ? upset_roll_rate_degps : bank_rate_degps) * dt);
    // **The bank it is asked for, not one near it**: an integral finds the
    // aileron the bank needs held. Proportional alone, it settled where the
    // aileron's error balanced the aeroplane's own roll: a Cessna 182 asked
    // for 25 degrees banked 30 to the left and 25 to the right, and near its
    // ceiling the extra bank's drag cost it the height.
    // **Only while the bank is at its limit**, which is a limit and must
    // hold, and once the bank commanded has caught up with the bank wanted -
    // no longer walking towards it at the roll rate. Rolling in, the bank
    // lags the command by design, and an integral wound up on that lag
    // overbanked every turn by 4 degrees, and the 747, 787 and A380 to 29.6
    // to 29.8 (every_aircraft_turns_ninety_degrees_without_overbanking_or_overshooting). Below the limit the heading loop
    // closes round the bank, and its own integral trims the roll: this one,
    // left on there, chased turbulence (a climb in moderate turbulence
    // settled in 38 s against 30), a stall's wing drop (a flying boat not
    // recovered from its stall), and an orbit's changing bank in wind (196 m
    // off the circle against 160). Otherwise it fades out at the rate it was
    // found.
    if (std::abs(bank_command_deg_) >= banked_deg &&
        std::abs(std::abs(bank_wanted) - sustained_bank_deg_) < 0.01 &&
        std::abs(bank_wanted - bank_command_deg_) < 0.01) {
        aileron_trim_ = std::clamp(
            aileron_trim_ + aileron_trim_per_degree * (bank_command_deg_ - phi) * dt,
            -most_aileron_trim, most_aileron_trim);
    } else {
        aileron_trim_ = toward(aileron_trim_, 0.0, most_aileron_trim / 10.0 * dt);
    }
    c.aileron = aileron_per_degree * (bank_command_deg_ - phi) - aileron_per_degps * p +
                aileron_trim_;

    // The ball, to rudder.
    const double beta = a_.property("aero/beta-deg");
    rudder_integral_ =
        std::clamp(rudder_integral_ - a_.rudder_integral_rate() * beta * dt, -1.0, 1.0);
    // And the yaw damper: the yaw rate, its steady part washed out.
    const double r_degps = degrees(a_.property("velocities/r-rad_sec"));
    steady_yaw_rate_degps_ += (r_degps - steady_yaw_rate_degps_) * dt / yaw_washout_s;
    c.rudder = -rudder_per_degree * beta + rudder_integral_ +
               a_.yaw_damper_per_degps() * (r_degps - steady_yaw_rate_degps_);

    // Vertical speed, through pitch, to elevator.
    const double climb_off = climb_wanted - climb_fpm;
    // **The envelope bounds what is asked for, not where the loop starts.**
    // Clamping the command itself would snap an aeroplane handed over
    // outside the envelope straight to its edge in one frame, which is a
    // jolt; clamping the target lets the command walk there at the pitch
    // rate, which is the autopilot taking over rather than grabbing.
    const bool on_speed = modes_.speed_on_elevator && modes_.airspeed_kts.has_value();
    // **Entering a stall, the envelope's top bounds the flight path, not the
    // nose** (AutopilotModes::hold_height_to_the_stall): the nose may rise
    // above it by the angle of attack the wing has - **until the wing has
    // gone past the angle its lift peaked at**, by three tenths of a degree,
    // and from then until the mode is let go the top is the nose's again.
    // The entry is to the stall, not into it: held level past the peak, a
    // 737-300 left thirty seconds was flown into a stall she was never
    // recovered from, and an F-35B and a Short S.23 pulled 2.9 g getting out.
    // Let go a degree past it, the A320 left thirty seconds pulled 2.13 g,
    // two degrees 2.22; three tenths, 1.90. **And never more than 5 degrees
    // above the nose**: a wing whose lift is flat at its peak, as the
    // 172P's, mushes without ever going past it by three tenths, and asked
    // for the flight path's top the pitch command wound up to 30 degrees
    // with her nose at 7 - and the recovery, engaged from that command,
    // zoomed her up past 20 degrees of pitch. A nose that follows, as a
    // fighter's does, is not held back by it.
    if (!modes_.hold_height_to_the_stall) {
        past_the_peak_ = false;
    } else if (a_.property("aero/alpha-deg") > stall_alpha_deg_ + past_the_peak_deg) {
        past_the_peak_ = true;
    }
    const double top_deg =
        modes_.hold_height_to_the_stall && !past_the_peak_
            ? std::max(most_pitch_deg,
                       std::min(most_pitch_deg + std::max(a_.property("aero/alpha-deg"), 0.0),
                                a_.property("attitude/theta-deg") + above_the_nose_deg))
            : most_pitch_deg;
    if (on_speed) {
        const double short_kts = *modes_.airspeed_kts - kts;
        // The flight path as the wing sees it: the pitch less the angle of
        // attack, which is the path itself with the wings level.
        const double theta_deg = a_.property("attitude/theta-deg");
        const double alpha_deg = a_.property("aero/alpha-deg");
        const double path_deg = theta_deg - alpha_deg;
        const bool stalled = alpha_deg >= stall_alpha_deg_;
        if (stalled) {
            pitch_command_deg_ = std::min(pitch_command_deg_, theta_deg);
        }
        if (!was_on_speed_) {
            // Engaged from the pitch commanded now, which is the pitch that
            // was holding the aeroplane: short of the speed, the law asks for
            // less at once, and the command walks there at the pitch rate.
            speed_integral_deg_ = pitch_command_deg_;
            sink_integral_fpm_ = 0.0;
        }
        // Short of the speed and gaining it slower than it should, more sink
        // is asked, as long as that lasts; at the speed, none.
        sink_integral_fpm_ =
            short_kts > 0.0
                ? std::max(sink_integral_fpm_ +
                               sink_per_slow_gain * (least_gain_kts_per_s - kts_per_s_) * dt,
                           0.0)
                : 0.0;
        // **The wing is kept below the angle it stalled at**: the pitch may
        // be no more than the pitch now less the angle of attack past that
        // angle. A speed alone does not
        // unload a wing: an A320 diving out of a stall at 156 knots, past
        // the speed it was asked for, was held at 27 degrees of alpha with
        // the elevator full up, still stalled and sinking 10,000 ft/min.
        const double wing_limit_deg = theta_deg - (alpha_deg - stall_alpha_deg_);
        // The angle the wing is held below: a tenth under its stall, and
        // under the angle that pulls the pull-out's load at this speed.
        const double qs = std::max(a_.property("aero/qbar-psf") * a_.property("metrics/Sw-sqft"),
                                   1.0);
        const double lift_now = a_.property("forces/fwz-aero-lbs") / qs;
        const double lift_at_most_g = pull_out_most_g * a_.property("inertia/weight-lbs") / qs;
        const double lift_per_degree =
            std::max(most_lift_, 0.1) / std::max(stall_alpha_deg_, 1.0);
        const double held_below_deg =
            std::min(stall_alpha_deg_ * (1.0 - stall_alpha_kept_below),
                     alpha_deg + (lift_at_most_g - lift_now) / lift_per_degree);
        const double held_limit_deg = theta_deg - (alpha_deg - held_below_deg);
        // The floor is lowered only while the aeroplane is short of its
        // speed or its wing is stalled - while it is recovering; otherwise
        // the envelope is the one every mode keeps.
        const double floor_deg =
            short_kts > 0.0 || stalled
                ? std::max(std::min({least_pitch_deg, path_deg, wing_limit_deg}),
                           steepest_unload_deg)
                : least_pitch_deg;
        const double sink_off_fpm =
            -sink_per_knot_short_fpm * std::max(short_kts, 0.0) - sink_integral_fpm_ - climb_fpm;
        const double pitch_law = speed_integral_deg_ + pitch_per_fpm * sink_off_fpm;
        double pitch_wanted = std::clamp(std::min(pitch_law, held_limit_deg), floor_deg,
                                         most_pitch_deg);
        // **The pull-out is gentle**: the nose comes up no faster than any
        // mode raises it, and not at all past the load the aeroplane is
        // allowed with its flaps down - it waits there for the speed.
        const double load_g = a_.property("accelerations/Nz");
        if (load_g >= pull_out_most_g) {
            pitch_wanted =
                std::max(std::min({pitch_wanted, pitch_command_deg_,
                                   theta_deg - pitch_per_g_over * (load_g - pull_out_most_g)}),
                         steepest_unload_deg);
        }
        const double down_rate_degps =
            short_kts > 0.0 || stalled ? unload_rate_degps : pitch_rate_degps;
        double pitch_next = std::clamp(pitch_wanted, pitch_command_deg_ - down_rate_degps * dt,
                                       pitch_command_deg_ + pitch_rate_degps * dt);
        // Rolled past the upset's bank, here too the nose is not raised -
        // an approach, a glide or a stall recovery as much as a cruise: a
        // pull while banked past 45 degrees tightens the turn, not the
        // descent, and a stall recovery with a wing down unloads first.
        if (upset) {
            pitch_next = std::min(pitch_next, pitch_command_deg_);
        }
        // **The integral winds only while the law's pitch is the pitch
        // given**: not while the command is still walking there, and not
        // while the law asks for more than the envelope allows at either end,
        // where winding would leave it to unwind before the nose could move.
        if (pitch_next == pitch_wanted && pitch_law == pitch_wanted) {
            speed_integral_deg_ += pitch_integral_per_fpm * sink_off_fpm * dt;
        }
        pitch_command_deg_ = pitch_next;
        // The vertical speed loop follows, so that letting go of the speed
        // takes the pitch on from where it is.
        pitch_integral_deg_ = pitch_command_deg_ - pitch_per_fpm * climb_off;
    } else {
        const double pitch_wanted = std::clamp(
            pitch_integral_deg_ + pitch_per_fpm * climb_off, least_pitch_deg, top_deg);
        double pitch_next = toward(pitch_command_deg_, pitch_wanted, pitch_rate_degps * dt);
        // **Rolled past the upset's bank, the wings come level before the
        // nose comes up** (sim/autopilot.hpp): the nose is not raised.
        if (upset) {
            pitch_next = std::min(pitch_next, pitch_command_deg_);
        }
        // The integral winds only while the pitch asked for is the pitch given.
        if (pitch_next == pitch_wanted) {
            pitch_integral_deg_ += pitch_integral_per_fpm * climb_off * dt;
        }
        pitch_command_deg_ = pitch_next;
    }
    // **The nose at its highest and the climb still short**: the pitch can
    // give no more of the climb, so the speed must - see the throttle.
    nose_at_stop_ = !on_speed && pitch_command_deg_ >= top_deg &&
                    climb_off > nose_at_stop_short_fpm;
    was_on_speed_ = on_speed;
    const double theta_off = pitch_command_deg_ - a_.property("attitude/theta-deg");
    const double q = degrees(a_.property("velocities/q-rad_sec"));
    // **Nor is it wound nose-up while the wing is past the angle its lift
    // peaked at** - a pitch a stalled wing cannot have, as an upset's is.
    // Holding the height into a stall, the integral wound the trim to its
    // nose-up stop, and the stall recovery inherited it: an A320 left thirty
    // seconds in her stall was handed over with the elevator +0.9 nose-up
    // and kept it there through a dive to 168 kt, pulling 1.95 g as her wing
    // came back through its peak; the Mosquito 2.22. With it, 1.81 and 1.71.
    //
    // **Only once the wing has been seen to stall** (`seen_to_stall_`): the
    // angle past the peak, rising, with the lift falling. Read off the angle
    // alone, a peak learnt low - a gust's or a pull's - would have refused
    // the trim in ordinary slow flight later.
    const bool past_its_peak = seen_to_stall_ &&
                               a_.property("aero/alpha-deg") > stall_alpha_deg_ + past_the_peak_deg;
    if (past_its_peak && theta_off > 0.0) {
        ++trim_held_steps_;
    }
    if (!upset && !(past_its_peak && theta_off > 0.0)) {
        elevator_trim_ = std::clamp(elevator_trim_ + trim_rate * theta_off * dt, -1.0, 1.0);
    }
    c.elevator =
        elevator_trim_ + elevator_per_degree * theta_off - elevator_per_degps * q;

    // **What the laws differ from the controls they were handed, on the very
    // first step, is an offset that fades over two seconds.** `last_` still
    // holds those handed controls here, because it is only replaced at the
    // end of this function. Measuring the offset rather than deriving it is
    // what makes it exact: there is no term to get wrong, and no limit for it
    // to fall foul of. The throttle is not in this - its law is already rate
    // limited from `last_.throttle`, so it cannot step.
    if (engaging_) {
        engaging_ = false;
        aileron_offset_ = last_.aileron - c.aileron;
        elevator_offset_ = last_.elevator - c.elevator;
        rudder_offset_ = last_.rudder - c.rudder;
    }
    c.aileron = std::clamp(c.aileron + aileron_offset_ * fade_, -1.0, 1.0);
    c.elevator = std::clamp(c.elevator + elevator_offset_ * fade_, -1.0, 1.0);
    c.rudder = std::clamp(c.rudder + rudder_offset_ * fade_, -1.0, 1.0);
    fade_ *= std::exp(-dt / offset_fade_s);
    c.aileron = toward(last_.aileron, c.aileron, a_hands_pace);
    c.elevator = toward(last_.elevator, c.elevator, a_hands_pace);
    c.rudder = toward(last_.rudder, c.rudder, a_hands_pace);

    // **The recovery takes the flaps up to a go-around's**, where the
    // aeroplane's figures give one (Aircraft::go_around_flaps), at a hand's
    // pace, and never lowers them. A Cherokee at full power and full rich at
    // 4,700 ft with her 40 degrees of landing flap, at the stall lesson's
    // recovery speed, sinks 190 ft/min for as long as she is flown, so she is
    // never level and never recovered; her handbooks climb out at 25 degrees
    // (assets/figures/pa28.xml). An aeroplane whose figures give
    // none keeps the flaps it was handed, as every one did before.
    if (on_speed) {
        if (const std::optional<double> go_around = a_.go_around_flaps()) {
            c.flaps = toward(last_.flaps, std::min(last_.flaps, *go_around), a_hands_pace);
        }
    }

    // Airspeed, to throttle.
    if (modes_.airspeed_kts) {
        // **With the nose at its highest and the climb short, the speed
        // asked for rises**, a knot a second up to `most_climb_speed_kts`
        // over it, while she holds it: the throttle opens to the faster
        // speed, and the wing climbs on less incidence. It falls back as
        // fast once the nose comes down or the climb comes. Only from the
        // speed asked, not on the way down to it: a stall demonstrated by
        // asking for ten knots under the stall is still a stall.
        // **And never past the fastest it may hold** (`limit_speed`): with
        // none given it is not raised at all.
        const double may_raise_kts =
            fastest_kts_ ? std::clamp(*fastest_kts_ - *modes_.airspeed_kts, 0.0,
                                      most_climb_speed_kts)
                         : 0.0;
        const double held_kts = *modes_.airspeed_kts + climb_speed_kts_;
        if (nose_at_stop_ && std::abs(held_kts - kts) < 5.0) {
            climb_speed_kts_ = std::min(climb_speed_kts_ + dt, may_raise_kts);
        } else {
            climb_speed_kts_ = std::max(climb_speed_kts_ - dt, 0.0);
        }
        climb_speed_kts_ = std::min(climb_speed_kts_, may_raise_kts);
        const double speed_off =
            *modes_.airspeed_kts + climb_speed_kts_ - a_.property("velocities/vc-kts");
        // **Short of speed for the height asked, the throttle opens.** While
        // the climb is held back for the speed, the aeroplane wants all the
        // power it has, whatever the airspeed loop makes of a speed that is
        // where it was asked to be; the integral follows, so letting go of
        // the speed takes the throttle on from where it is.
        const double wanted = holding_speed_ || on_speed
                                  ? 1.0
                                  : throttle_integral_ + throttle_per_knot * speed_off;

        const double next =
            std::clamp(toward(last_.throttle, wanted, throttle_rate * dt), 0.0, 1.0);
        if (holding_speed_ || on_speed) {
            throttle_integral_ = next;
        } else if (next == wanted) {
            throttle_integral_ += throttle_integral_per_knot * speed_off * dt;
        }
        c.throttle = next;
    }

    if (leaner_) {
        c.mixture = leaner_->lean(c.throttle);
    }

    last_ = c;
    return c;
}

} // namespace glideslope::sim
