#pragma once

// **What the server holds a player's aircraft on until her first input, as
// her client is told it** (`WATCH` of her own, net::Watched), shared by both
// clients: the controls a prediction begins on (sim::Prediction::
// held_by_server), so that a law made on her first input - STOVL mode
// selected - is seeded in the prediction as the server seeds its own. The
// server holds her on these to the bit: it holds each control as a 16-bit
// fraction, as the levers are said (glideslope_server, `Fleet::give`).

#include "net/state.hpp"
#include "sim/aircraft.hpp"

namespace glideslope::frontend {

// The controls the levers say: the stick and pedals, the throttle, the
// flaps, and the gear and speedbrake lever where she has them.
inline sim::Controls held_as_told(const net::Watched& levers) {
    sim::Controls c;
    c.aileron = levers.aileron;
    c.elevator = levers.elevator;
    c.rudder = levers.rudder;
    c.throttle = levers.throttle;
    c.flaps = levers.flaps;
    if (levers.gear) {
        c.gear = *levers.gear;
    }
    if (levers.speedbrake) {
        c.speedbrake = *levers.speedbrake;
    }
    return c;
}

} // namespace glideslope::frontend
