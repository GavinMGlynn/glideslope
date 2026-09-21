#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/checklist.hpp"
#include "sim/checklist_run.hpp"
#include "sim/test_pilot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

using glideslope::sim::Checklist;
using glideslope::sim::ChecklistItem;
using glideslope::sim::ChecklistRun;
using glideslope::sim::ItemProgress;
using glideslope::sim::Phase;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// The phases this flight covers: the ground and the air of a take-off, and
// the landing list, which is carried along unflown so that an item the
// aeroplane never satisfies can be seen not to tick.
const std::vector<Phase>& flown_phases() {
    static const std::vector<Phase> phases{Phase::taxi, Phase::take_off,
                                           Phase::climb, Phase::landing};
    return phases;
}

struct Watched {
    std::vector<ItemProgress> got;   // what the run recorded
    std::vector<std::int64_t> first; // the first tick each was really done, or -1
    std::vector<std::int64_t> last;  // the last tick each was really done, or -1
};

// **The Cessna's take-off, flown by the book**: brakes off, the throttle
// opened over three seconds, the nose raised at 55 knots and the climb held
// at 80. With `flaps_down` the flaps are left fully down instead of up,
// which is the fault the climb's "flaps up" item must not forgive.
std::map<Phase, Watched> fly_the_cessna(bool flaps_down) {
    const auto entry = glideslope::sim::find_aircraft(data(), "c172p");
    const auto lists = glideslope::sim::find_checklists(data(), "c172p");
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 0.0;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 0.0;
    ic.engine_running = true;
    aircraft.initialize(ic);

    std::map<Phase, ChecklistRun> runs;
    std::map<Phase, Watched> watched;
    for (const Phase phase : flown_phases()) {
        ChecklistRun run(lists);
        run.show(phase);
        runs.emplace(phase, std::move(run));
        Watched w;
        const std::size_t items = lists.at(phase).items.size();
        w.first.assign(items, -1);
        w.last.assign(items, -1);
        watched.emplace(phase, std::move(w));
    }

    glideslope::sim::TestPilot pilot(aircraft);
    std::optional<glideslope::sim::TestPilot> flying;
    glideslope::sim::Controls c;
    c.flaps = flaps_down ? 1.0 : 0.0;
    const double heading = aircraft.property("attitude/psi-deg");
    double pitch = 0.0;
    for (std::int64_t tick = 0; tick < 100 * steps_per_second; ++tick) {
        const double kcas = aircraft.property("velocities/vc-kts");
        c.throttle = std::min(static_cast<double>(tick) / (3.0 * steps_per_second), 1.0);
        c.rudder = pilot.steer_to(heading);
        if (kcas >= 55.0 && !flying) {
            flying.emplace(aircraft);
            pitch = aircraft.property("attitude/theta-deg");
        }
        if (flying) {
            // Up to ten degrees to unstick, then whatever holds 80 knots.
            pitch = std::min(pitch + 3.0 / steps_per_second, 10.0);
            c.elevator = aircraft.property("position/h-agl-ft") > 100.0
                             ? flying->pitch_for_speed(80.0)
                             : flying->pitch_to(pitch);
            c.aileron = flying->roll_to(0.0);
        }
        aircraft.set_controls(c);
        aircraft.step();

        for (const Phase phase : flown_phases()) {
            runs.at(phase).update(aircraft, tick);
            const Checklist& list = lists.at(phase);
            Watched& w = watched.at(phase);
            for (std::size_t i = 0; i < list.items.size(); ++i) {
                const ChecklistItem& item = list.items[i];
                if (item.pilots()) {
                    continue;
                }
                if (item.done(aircraft.property(item.property))) {
                    if (w.first[i] < 0) {
                        w.first[i] = tick;
                    }
                    w.last[i] = tick;
                }
            }
        }
    }
    for (const Phase phase : flown_phases()) {
        watched.at(phase).got = runs.at(phase).progress();
    }
    return watched;
}

} // namespace

// **The item's own verification**: flown by the book, every item the
// aircraft can see ticks at the tick its state first shows it done.
GLIDESLOPE_TEST(every_checklist_item_the_cessna_can_see_ticks_when_its_state_first_shows_it_done) {
    const auto flown = fly_the_cessna(false);
    const auto lists = glideslope::sim::find_checklists(data(), "c172p");

    std::size_t walked = 0;
    std::size_t ticked = 0;
    std::size_t pilots = 0;
    for (const Phase phase : flown_phases()) {
        const Checklist& list = lists.at(phase);
        const Watched& w = flown.at(phase);
        check(w.got.size() == list.items.size(),
              glideslope::sim::phase_name(phase) + " kept one state per item");
        for (std::size_t i = 0; i < list.items.size(); ++i) {
            const ChecklistItem& item = list.items[i];
            const std::string where =
                glideslope::sim::phase_name(phase) + " item " + std::to_string(i) +
                " (" + item.text + ")";
            ++walked;
            if (item.pilots()) {
                // Nobody but the pilot can tick this, and nobody did.
                check(!w.got[i].ticked, where + " is the pilot's and did not tick itself");
                ++pilots;
                continue;
            }
            if (w.first[i] < 0) {
                check(!w.got[i].ticked,
                      where + " was never done, so it must not be ticked");
                continue;
            }
            check(w.got[i].ticked, where + " was done and must be ticked");
            check(w.got[i].at_tick == w.first[i],
                  where + " ticked at " + std::to_string(w.got[i].at_tick) +
                      ", but was first done at " + std::to_string(w.first[i]));
            ++ticked;
        }
    }
    // The space this walked, stated: the four lists carried through the
    // flight, every item of each. The other five phases are not flown here
    // and are named in flown_phases() above with their reason.
    std::size_t carried = 0;
    for (const Phase phase : flown_phases()) {
        carried += lists.at(phase).items.size();
    }
    check(walked == carried, "every item of the four lists carried was walked: " +
                                 std::to_string(walked) + " of " +
                                 std::to_string(carried));
    check(ticked > 0, "the aeroplane ticked items off as it flew");
    check(pilots > 0, "and left the pilot's own alone");

    // **Flown by the book, the three lists it flies are finished.** Taxi,
    // take-off and climb are what this take-off covers, and an item of theirs
    // the aeroplane can see and did not tick would mean either the flight was
    // not by the book or the list asks for something the book does not.
    // Landing is deliberately not flown, and is what the flaps test uses.
    for (const Phase phase : {Phase::taxi, Phase::take_off, Phase::climb}) {
        const Checklist& list = lists.at(phase);
        const Watched& w = flown.at(phase);
        for (std::size_t i = 0; i < list.items.size(); ++i) {
            if (list.items[i].pilots()) {
                continue;
            }
            check(w.got[i].ticked,
                  glideslope::sim::phase_name(phase) + "'s \"" + list.items[i].text +
                      "\" was not ticked by a take-off flown by the book");
        }
    }
}

// **A checklist records that a thing was done, not that it is still true.**
// The taxi list wants the throttle back at a walking pace, which is so at the
// start and not so at full power; having ticked, it must stay ticked.
GLIDESLOPE_TEST(an_item_once_ticked_stays_ticked_though_the_aeroplane_moves_on) {
    const auto flown = fly_the_cessna(false);
    const auto lists = glideslope::sim::find_checklists(data(), "c172p");
    const Checklist& taxi = lists.at(Phase::taxi);
    const Watched& w = flown.at(Phase::taxi);

    std::size_t went_false_again = 0;
    for (std::size_t i = 0; i < taxi.items.size(); ++i) {
        if (taxi.items[i].pilots() || w.first[i] < 0) {
            continue;
        }
        // It was done, and by the end of the flight it was not.
        if (w.last[i] < 100 * steps_per_second - 1) {
            ++went_false_again;
            check(w.got[i].ticked,
                  "\"" + taxi.items[i].text + "\" was done at tick " +
                      std::to_string(w.first[i]) + ", stopped being so at " +
                      std::to_string(w.last[i]) + ", and must still be ticked");
        }
    }
    check(went_false_again > 0,
          "the taxi list has an item the take-off undoes, which is what this "
          "pins; none was found");
}

// **An item the aeroplane never satisfies is not ticked, and is outstanding.**
// Flown with the flaps left up, the landing list's flaps item cannot tick,
// and is what a screen flags and a debrief asks about.
GLIDESLOPE_TEST(flown_with_the_flaps_left_up_the_landing_flaps_item_stays_unticked_and_is_flagged) {
    const auto lists = glideslope::sim::find_checklists(data(), "c172p");
    const Checklist& landing = lists.at(Phase::landing);

    // The item this is about: the one that wants flap travel, which the
    // whole flight is flown without.
    std::optional<std::size_t> flaps;
    for (std::size_t i = 0; i < landing.items.size(); ++i) {
        if (landing.items[i].property == "fcs/flap-pos-deg") {
            flaps = i;
        }
    }
    check(flaps.has_value(),
          "the Cessna's landing list has an item about the flaps' travel");

    const auto flown = fly_the_cessna(false);
    const Watched& w = flown.at(Phase::landing);
    check(w.first[*flaps] < 0, "the flaps were never where the landing list wants them");
    check(!w.got[*flaps].ticked,
          "\"" + landing.items[*flaps].text + "\" must not tick with the flaps up");
    check(w.got[*flaps].at_tick == -1, "and must have no tick recorded");

    // Flagged: it is one of the outstanding items of that list.
    ChecklistRun run(lists);
    run.show(Phase::landing);
    check(!run.complete(), "a list nothing has ticked is not complete");
    const auto left = run.outstanding();
    check(std::find(left.begin(), left.end(), *flaps) != left.end(),
          "the flaps item is flagged as outstanding");
    check(left.size() == landing.items.size(),
          "and so is every other item nobody has done");
}

// **The pilot's own tick only when the pilot says so**, and only the
// pilot's: an item the aeroplane can see is not the pilot's to claim.
GLIDESLOPE_TEST(the_pilot_ticks_their_own_items_and_only_their_own) {
    const auto lists = glideslope::sim::find_checklists(data(), "c172p");
    ChecklistRun run(lists);
    run.show(Phase::before_start);
    const Checklist& list = lists.at(Phase::before_start);

    std::optional<std::size_t> theirs;
    std::optional<std::size_t> the_aeroplanes;
    for (std::size_t i = 0; i < list.items.size(); ++i) {
        if (list.items[i].pilots() && !theirs) {
            theirs = i;
        }
        if (!list.items[i].pilots() && !the_aeroplanes) {
            the_aeroplanes = i;
        }
    }
    check(theirs.has_value(), "the before-start list has an item of the pilot's");
    check(the_aeroplanes.has_value(), "and one the aeroplane can see");

    run.confirm(*theirs, 7);
    check(run.progress()[*theirs].ticked, "the pilot's item ticked when they said so");
    check(run.progress()[*theirs].at_tick == 7, "at the tick they said it");

    run.confirm(*the_aeroplanes, 9);
    check(!run.progress()[*the_aeroplanes].ticked,
          "an item the aeroplane can see is not the pilot's to tick");

    // Out of range is ignored rather than reaching past the end of the list.
    run.confirm(list.items.size() + 5, 11);
    check(run.progress().size() == list.items.size(), "the list did not grow");
}
