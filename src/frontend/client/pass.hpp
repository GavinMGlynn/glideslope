#pragma once

// One pass of the frame loop on a server, in the order its parts must come.
//
// **Why it is one function**: the order is the point, and it is easy to get
// wrong. The client with the window (main.cpp) flies its passes through
// this, and the prediction's unit test (tests/unit/test_prediction.cpp)
// flies its model client through the same function, so that a change to the
// order here is a change the test sees.

#include "sim/aircraft.hpp"

#include <cstdint>

namespace glideslope::client {

// `link` is the session as one pass sees it (Online, bound to a flight):
//
//   sim::Controls fly(double local_s, const sim::Controls& stick)
//       sends the stick if an input is due, and says what the ticks are
//       flown on;
//   void listen(double local_s)   takes in what has arrived by now;
//   void ticks_flown()            told once the ticks are flown;
//   void flown()                  the input sent this pass flies from here;
//   void hear()                   hears what `listen` took in.
//
// `tick(controls)` flies one of the `due` ticks.
//
// **The order, and what each place in it answers** (PROJECT_STATUS.md,
// 2026-10-02):
//
// 1. The stick is sent as the clock is read, and the ticks - the frame gone
//    by, before the stick was read - are flown on the input sent before,
//    which is what the server flew over the same time. Flown on the stick
//    sent this pass, an input sent at the end of a long frame was flown here
//    from that frame's beginning and on the server a frame later, and while
//    the clocks' difference was being learnt the aircraft was put right by
//    the way flown in the frame: 25 m at 428 ms in the unit test.
// 2. What has arrived is taken in before the ticks, as they fly to the time
//    the clock was read: taken in after them, a word that came while they
//    were flown was about a moment past the last of them, and the
//    prediction was put forward to it with nothing to replay.
// 3. The input sent this pass is flown from the next tick.
// 4. What was taken in is heard after the ticks: at the end of a long frame
//    the newest word is about a moment the ticks had not yet reached
//    before them (PROJECT_STATUS.md, 2026-09-30).
template <typename Link, typename Tick>
void fly_a_pass(Link& link, double local_s, const sim::Controls& stick, std::int64_t due,
                Tick tick) {
    const sim::Controls flown = link.fly(local_s, stick);
    link.listen(local_s);
    for (std::int64_t i = 0; i < due; ++i) {
        tick(flown);
    }
    link.ticks_flown();
    link.flown();
    link.hear();
}

} // namespace glideslope::client
