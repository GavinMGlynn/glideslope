#pragma once

// **What the server holds a player's aircraft on until her first input, as
// her client is told it** (`WATCH` of her own, net::Watched), shared by both
// clients: the controls a prediction begins on (sim::Prediction::
// held_by_server), so that a law made on her first input - STOVL mode
// selected - is seeded in the prediction as the server seeds its own. The
// server holds her on these controls to the bit: it holds each as a 16-bit
// fraction, as the levers are said (glideslope_server, `Fleet::give`). Her
// held trim, which no control sets, is not: the server holds it as her trim
// found it, and tells it as the nearest 16-bit fraction, so her prediction's
// is within half of 1/32767 of the server's.

#include "net/state.hpp"
#include "sim/aircraft.hpp"

namespace glideslope::frontend {

// **Her controls put where the levers say**: the stick and pedals, the
// pitch, roll and yaw trim, the throttle, the flaps, the mixture and propeller
// levers, and the gear and speedbrake lever where she has them - every control the server holds her on until her first input
// (glideslope_server's held_until_moved) - leaving the rest of `c` as it is.
// A client's inputs begin from these and move from there, so that her first
// input keeps the trim she was held on: begun from its own pitch trim,
// aileron and rudder, as both clients' were until 2026-10-21, a client's
// first input put her trim at nought.
inline void keep_held(const net::Watched& levers, sim::Controls& c) {
    c.aileron = levers.aileron;
    c.elevator = levers.elevator;
    c.rudder = levers.rudder;
    c.pitch_trim = levers.pitch_trim;
    c.roll_trim = levers.roll_trim;
    c.yaw_trim = levers.yaw_trim;
    c.throttle = levers.throttle;
    c.flaps = levers.flaps;
    c.mixture = levers.mixture;
    c.propeller = levers.propeller;
    if (levers.gear) {
        c.gear = *levers.gear;
    }
    if (levers.speedbrake) {
        c.speedbrake = *levers.speedbrake;
    }
}

// **Where her controls are, as her levers say them** (`WATCH`): what the
// server tells a client riding along in the aircraft numbered `index`, her
// own client among them - each as her flight model has it, the pitch trim
// among them, and the roll and yaw trim (since protocol 0F).
inline net::Watched levers_of(const sim::Aircraft& m, std::uint8_t index) {
    net::Watched w;
    w.aircraft = index;
    w.aileron = m.property("fcs/aileron-cmd-norm");
    w.elevator = -m.property("fcs/elevator-cmd-norm");
    w.rudder = m.property("fcs/rudder-cmd-norm");
    w.throttle = m.property("fcs/throttle-cmd-norm[0]");
    w.flaps = m.property("fcs/flap-cmd-norm");
    if (m.gear_retracts()) {
        w.gear = m.property("gear/gear-cmd-norm");
    }
    if (m.speedbrakes()) {
        w.speedbrake = m.property("fcs/speedbrake-cmd-norm");
    }
    // **The pitch trim negated, the roll and yaw trim not**, as
    // sim::Aircraft::set_controls puts them: a model sums its pitch trim
    // with fcs/elevator-cmd-norm (the C172P's "Pitch Trim Sum"), positive
    // stick forward, where sim::Controls' elevator and pitch trim are
    // positive back - so both are put negated, and read back so; it sums
    // its roll and yaw trim with fcs/aileron-cmd-norm and
    // fcs/rudder-cmd-norm, which are put as they are, right positive. Each
    // read back as it was put, every aircraft
    // (every_aircraft_a_players_first_input_sends_is_what_the_server_held_her_on,
    // which counts aircraft held on a pitch trim and on a roll trim, so that
    // a sign wrong on either fails it).
    w.pitch_trim = -m.property("fcs/pitch-trim-cmd-norm");
    w.roll_trim = m.property("fcs/roll-trim-cmd-norm");
    w.yaw_trim = m.property("fcs/yaw-trim-cmd-norm");
    // And her held trim, where her model has one: no control sets it.
    w.held_trim = m.held_trim();
    // Her mixture and propeller levers, as an airborne start set them.
    if (m.has_property("fcs/mixture-cmd-norm[0]")) {
        w.mixture = m.property("fcs/mixture-cmd-norm[0]");
    }
    if (m.has_property("fcs/advance-cmd-norm[0]")) {
        w.propeller = m.property("fcs/advance-cmd-norm[0]");
    }
    return w;
}

// The controls the levers say, on a model's own for the rest.
inline sim::Controls held_as_told(const net::Watched& levers) {
    sim::Controls c;
    keep_held(levers, c);
    return c;
}

} // namespace glideslope::frontend
