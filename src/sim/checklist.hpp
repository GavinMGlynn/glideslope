#pragma once

// An aircraft's checklists, as data (assets/aircraft): one file each, named
// for the aircraft - `c172p.checklist` - beside the `.aircraft` file that
// names it, so a checklist is changed by changing its file and no code.
//
//   phase NAME                       the phase of flight the items below are
//                                    for, one of the nine FEATURES.md names:
//                                    before-start, taxi, take-off, climb,
//                                    cruise, descent, approach, landing,
//                                    after-landing
//   check PROPERTY OP VALUE TEXT...  an item the aircraft can see done: a
//                                    JSBSim property, `<=` or `>=`, and the
//                                    figure it is held to
//   range PROPERTY LOW HIGH TEXT...  the same, where what shows it done is a
//                                    band rather than a limit; both ends count
//   confirm TEXT...                  an item the simulation cannot see - a
//                                    walk-round, a look out, a briefing -
//                                    which is the pilot's to tick
//
// with `#` beginning a comment. Every phase must appear, once, and in the
// order above: a checklist with a phase missing is a checklist that would
// quietly teach less than it claims.
//
// **The words are this project's own**, written from the aircraft's handbook
// or pilot's notes, which `docs/ASSETS.md` records. They are not the
// handbook's text.
//
// **A property is not checked against the model here.** Whether an aircraft
// has `fcs/flap-pos-deg` is a question for the aircraft, and
// `Aircraft::property` answers it; a test loads each model and asks.

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::sim {

struct ChecklistError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// The nine phases of flight, in the order they are flown (docs/FEATURES.md,
// "Checklists for every aircraft").
enum class Phase {
    before_start,
    taxi,
    take_off,
    climb,
    cruise,
    descent,
    approach,
    landing,
    after_landing,
};

// Every phase, in order. The one place the nine are listed, so a test that
// walks them cannot walk eight.
const std::vector<Phase>& all_phases();

// A phase's name as the data spells it - "before-start" - and back. `phase_of`
// returns false for a name there is none of.
std::string phase_name(Phase phase);
bool phase_of(std::string_view name, Phase& out);

struct ChecklistItem {
    std::string text;     // what the pilot reads
    std::string property; // the JSBSim property, or empty: the pilot's to tick
    double low = 0.0;     // the band that shows it done, both ends counting
    double high = 0.0;

    // Nobody but the pilot can see this one done.
    bool pilots() const { return property.empty(); }
    // Whether the property's value shows the item done.
    bool done(double value) const { return value >= low && value <= high; }
};

struct Checklist {
    Phase phase = Phase::before_start;
    std::vector<ChecklistItem> items;
};

struct AircraftChecklists {
    std::string id; // the file's name, less .checklist
    std::vector<Checklist> phases;

    // The checklist for a phase. Every phase is present, so this never fails.
    const Checklist& at(Phase phase) const;
};

// One aircraft's file. Throws ChecklistError naming the line of anything it
// cannot read, and for a file with a phase missing, repeated, out of order or
// empty.
AircraftChecklists parse_checklists(const std::string& id, std::string_view text);

// Every aircraft's checklists in `data`/aircraft, by id. Throws
// ChecklistError for a file it cannot read.
std::vector<AircraftChecklists> read_checklists(const std::filesystem::path& data);

// One aircraft's, by id. Throws ChecklistError if there is none.
AircraftChecklists find_checklists(const std::filesystem::path& data,
                                   const std::string& id);

} // namespace glideslope::sim
