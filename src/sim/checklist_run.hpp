#pragma once

// A checklist being worked through, on one aircraft, as it flies.
//
// **An item ticks itself at the first tick its state shows it done**, and
// stays ticked: a checklist records that a thing was done, not that it is
// still true. Flaps set for take-off and then raised on the climb does not
// untick the take-off list.
//
// An item the simulation cannot see is the pilot's, and ticks only when the
// pilot says so. An item still unticked when the phase is left is
// outstanding - what a screen flags, and what the debrief will ask about.
//
// This is the simulation's own: it reads the aircraft and keeps a state, and
// draws nothing.

#include "sim/aircraft.hpp"
#include "sim/checklist.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace glideslope::sim {

struct ItemProgress {
    bool ticked = false;
    // The tick it was first seen done at, once it has been.
    std::int64_t at_tick = -1;
};

class ChecklistRun {
public:
    explicit ChecklistRun(AircraftChecklists lists);

    // Which phase's list is being worked through.
    void show(Phase phase);
    Phase showing() const { return showing_; }
    const Checklist& list() const { return lists_.at(showing_); }

    // Read the aircraft and tick what it now shows done. `tick` is the
    // simulation's step count, which is what "the tick it was first shown
    // done at" means.
    //
    // **A property the aircraft has not got leaves its item alone** rather
    // than throwing: a checklist is data, and data that names a state this
    // aircraft has not got is a fault for a test to catch, not a reason to
    // end a flight.
    void update(const Aircraft& aircraft, std::int64_t tick);

    // The pilot ticks one of their own. An item that is not the pilot's is
    // not theirs to tick, and this leaves it alone.
    void confirm(std::size_t item, std::int64_t tick);

    // Where every item of the showing phase has got to, in its order.
    const std::vector<ItemProgress>& progress() const;

    // The items of the showing phase still not ticked, in their order: what
    // a screen flags and a debrief asks about.
    std::vector<std::size_t> outstanding() const;

    // Every item of the showing phase is ticked.
    bool complete() const { return outstanding().empty(); }

private:
    AircraftChecklists lists_;
    Phase showing_ = Phase::before_start;
    // One entry per phase, in `all_phases()` order, each as long as its list.
    std::vector<std::vector<ItemProgress>> progress_;

    std::size_t at(Phase phase) const;
};

} // namespace glideslope::sim
