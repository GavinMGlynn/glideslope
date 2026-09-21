#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/checklist.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using glideslope::sim::AircraftChecklists;
using glideslope::sim::Checklist;
using glideslope::sim::ChecklistError;
using glideslope::sim::ChecklistItem;
using glideslope::sim::Phase;
using glideslope::test::check;

namespace {

// The data directory the programs read: the one the flight models are in.
std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// The roster's size, stated so that an aircraft added without its checklists
// makes this say so rather than quietly testing one fewer.
constexpr std::size_t roster_size = 16;

// What a file with everything right looks like, for the refusals below to
// spoil one line of at a time.
std::string a_whole_file() {
    std::string text;
    for (const Phase phase : glideslope::sim::all_phases()) {
        text += "phase " + glideslope::sim::phase_name(phase) + "\n";
        text += "confirm Something the pilot can see\n";
    }
    return text;
}

// The message a file is refused with, or "" if it was accepted.
std::string refusal(const std::string& text) {
    try {
        glideslope::sim::parse_checklists("probe", text);
    } catch (const ChecklistError& e) {
        return e.what();
    }
    return {};
}

void refused(const std::string& text, const std::string& says) {
    const std::string got = refusal(text);
    if (got.empty()) {
        glideslope::test::fail("this was accepted and should not have been:\n" + text);
    }
    if (got.find(says) == std::string::npos) {
        glideslope::test::fail("refused with \"" + got + "\", which does not say \"" +
                               says + "\"");
    }
}

} // namespace

// **The item's own verification**: every aircraft in the roster has a
// checklist for each phase of flight, and every item either names a state of
// the aircraft that shows it done or is marked the pilot's to confirm.
GLIDESLOPE_TEST(every_aircraft_has_a_checklist_for_every_phase_of_flight) {
    const auto roster = glideslope::sim::read_catalogue(data());
    check(roster.size() == roster_size,
          "the roster is " + std::to_string(roster_size) + " aircraft, not " +
              std::to_string(roster.size()));

    const auto& phases = glideslope::sim::all_phases();
    check(phases.size() == 9,
          "there are nine phases of flight, not " + std::to_string(phases.size()));

    std::size_t walked = 0;
    std::size_t items = 0;
    for (const auto& entry : roster) {
        const AircraftChecklists lists = glideslope::sim::find_checklists(data(), entry.id);
        check(lists.phases.size() == phases.size(),
              entry.id + " has " + std::to_string(lists.phases.size()) +
                  " phases, not " + std::to_string(phases.size()));
        for (const Phase phase : phases) {
            const Checklist& list = lists.at(phase);
            check(!list.items.empty(),
                  entry.id + "'s " + glideslope::sim::phase_name(phase) +
                      " has nothing to do");
            items += list.items.size();
            ++walked;
        }
    }
    check(walked == roster_size * phases.size(),
          "every aircraft's every phase was walked: " + std::to_string(walked) +
              " of " + std::to_string(roster_size * phases.size()));
    check(items > 0, "the checklists have items in them");
}

// **A checklist may only name a state this aircraft actually has.** A
// property the model does not carry would be an item that could never tick,
// and nothing else would say so: `Aircraft::property` throws for one, so each
// model is loaded and asked about every property its own checklists name.
GLIDESLOPE_TEST(every_checklist_item_names_a_state_its_own_aircraft_has_or_is_the_pilots) {
    const auto roster = glideslope::sim::read_catalogue(data());
    check(roster.size() == roster_size,
          "the roster is " + std::to_string(roster_size) + " aircraft, not " +
              std::to_string(roster.size()));

    std::size_t checked = 0;
    std::size_t pilots = 0;
    std::vector<std::string> wrong;
    for (const auto& entry : roster) {
        const AircraftChecklists lists = glideslope::sim::find_checklists(data(), entry.id);
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        for (const Checklist& list : lists.phases) {
            for (const ChecklistItem& item : list.items) {
                if (item.pilots()) {
                    ++pilots;
                    continue;
                }
                try {
                    (void)aircraft.property(item.property);
                    ++checked;
                } catch (const std::out_of_range&) {
                    wrong.push_back(entry.id + "'s " +
                                    glideslope::sim::phase_name(list.phase) + ": " +
                                    entry.model + " has no " + item.property);
                }
            }
        }
    }
    // Name every one of them before failing, so a run says how much is wrong
    // rather than the first thing that is.
    if (!wrong.empty()) {
        std::string all;
        for (const std::string& one : wrong) {
            all += "\n  " + one;
        }
        glideslope::test::fail(std::to_string(wrong.size()) +
                               " items name a state their aircraft has not got:" + all);
    }
    check(checked > 0, "some items are checked against the aircraft");
    check(pilots > 0, "some items are the pilot's to confirm");
}

// **A band is a band, and a limit is a limit.** What `done` answers is the
// whole of what ticking an item will rest on, so both ends of a band count
// and a one-sided check is open at the other end.
GLIDESLOPE_TEST(an_item_is_done_when_the_aircrafts_state_is_inside_its_band) {
    const AircraftChecklists lists = glideslope::sim::parse_checklists(
        "probe", a_whole_file() + "\n" +
                     "check fcs/flap-cmd-norm <= 0.5 Flaps up\n"
                     "check velocities/vc-kts >= 55 Rotate\n"
                     "range fcs/flap-pos-deg 20 30 Flaps down\n");
    const Checklist& last = lists.at(Phase::after_landing);
    check(last.items.size() == 4, "the three items were read");

    const ChecklistItem& at_most = last.items[1];
    check(at_most.done(0.0) && at_most.done(0.5), "at or under the limit is done");
    check(!at_most.done(0.51), "over the limit is not done");

    const ChecklistItem& at_least = last.items[2];
    check(at_least.done(55.0) && at_least.done(1e9), "at or over the limit is done");
    check(!at_least.done(54.9), "under the limit is not done");

    const ChecklistItem& band = last.items[3];
    check(band.done(20.0) && band.done(25.0) && band.done(30.0),
          "inside the band, both ends counting, is done");
    check(!band.done(19.9) && !band.done(30.1), "outside the band is not done");

    check(last.items[0].pilots(), "a confirm item is the pilot's");
    check(!band.pilots(), "a checked item is not");
}

// **A file that is wrong is refused where it is wrong.** Each of these was
// watched: the message names the line and says what was expected.
GLIDESLOPE_TEST(a_checklist_file_that_is_wrong_is_refused_and_says_where) {
    refused("phase before-start\nconfirm A\nwaffle on\n", "no command \"waffle\"");
    refused("confirm A thing\n", "an item before any phase");
    refused("phase before-lunch\n", "no phase \"before-lunch\"");
    refused("phase before-start\nconfirm A\nphase climb\n",
            "\"taxi\" was expected here");
    refused("phase before-start\nconfirm A\n", "must give all 9 phases");
    refused("phase before-start\ncheck fcs/flap-cmd-norm == 1 Flaps\n",
            "must be <= or >=");
    refused("phase before-start\ncheck fcs/flap-cmd-norm <= lots Flaps\n",
            "must be a number");
    refused("phase before-start\nrange fcs/flap-pos-deg 30 20 Flaps\n",
            "must not be above its top");
    refused("phase before-start\nconfirm\n", "an item must say what to do");
    refused("phase before-start\ncheck fcs/flap-cmd-norm <= 0.5\n",
            "check PROPERTY OP VALUE TEXT");

    // A phase left empty is a phase not written, and is refused for saying
    // nothing rather than for being absent.
    std::string empty_taxi;
    for (const Phase phase : glideslope::sim::all_phases()) {
        empty_taxi += "phase " + glideslope::sim::phase_name(phase) + "\n";
        if (phase != Phase::taxi) {
            empty_taxi += "confirm Something\n";
        }
    }
    refused(empty_taxi, "nothing to do in \"taxi\"");

    // And the whole file, unspoilt, is accepted - otherwise the refusals
    // above would pass for the wrong reason.
    check(refusal(a_whole_file()).empty(), "a file with every phase is accepted");
}

// **Every phase is spelt once, and the nine are the nine.** `all_phases` is
// what every walk above trusts to be complete.
GLIDESLOPE_TEST(the_nine_phases_of_flight_each_have_one_name_and_answer_to_it) {
    const auto& phases = glideslope::sim::all_phases();
    check(phases.size() == 9, "there are nine phases of flight");

    std::set<std::string> names;
    for (const Phase phase : phases) {
        const std::string name = glideslope::sim::phase_name(phase);
        check(!name.empty(), "every phase has a name");
        Phase back{};
        check(glideslope::sim::phase_of(name, back), name + " answers to its name");
        check(back == phase, name + " answers to its own name and no other");
        names.insert(name);
    }
    check(names.size() == phases.size(), "the nine names are nine different names");

    Phase nowhere{};
    check(!glideslope::sim::phase_of("go-around", nowhere),
          "a name there is no phase of is refused");
}

// **A property being there does not mean the aeroplane has the thing.**
// JSBSim's FGFCS::bind ties `fcs/flap-pos-deg`, `fcs/flap-cmd-norm` and
// `fcs/speedbrake-pos-norm` for every aircraft whether or not its model has
// that channel (ext/jsbsim/src/models/FGFCS.cpp:725, 753, 758), so they
// answer on a Piper Cub, which has no flaps at all, and on a B-2, whose drag
// rudders answer the pedals. An item resting on one of those would never
// tick and nothing above would say so.
//
// What separates a real channel from a phantom is whether the aircraft's own
// flight model names the property - to drive it, or to read it back. So each
// aircraft's model files are read, and a control property its own model never
// mentions is refused.
//
// **Three are named here as exceptions, with their reason**: JSBSim ties
// `fcs/throttle-cmd-norm`, `fcs/mixture-cmd-norm` and `fcs/advance-cmd-norm`
// once per engine from the engine count (FGFCS.cpp:785-797), so a model that
// uses them need never spell them. Mixture and propeller pitch are refused
// on an aircraft whose engine is not a piston or turboprop, which is the one
// way those two could still be nonsense.
GLIDESLOPE_TEST(no_checklist_item_rests_on_a_control_its_aircrafts_model_has_not_got) {
    const auto roster = glideslope::sim::read_catalogue(data());
    check(roster.size() == roster_size,
          "the roster is " + std::to_string(roster_size) + " aircraft, not " +
              std::to_string(roster.size()));

    // Tied per engine by JSBSim, so a model that uses one need not name it.
    const std::set<std::string> tied_per_engine{
        "fcs/throttle-cmd-norm", "fcs/mixture-cmd-norm", "fcs/advance-cmd-norm"};
    // The two of those that only mean anything with a piston or turboprop.
    const std::set<std::string> needs_a_piston{"fcs/mixture-cmd-norm",
                                               "fcs/advance-cmd-norm"};

    std::size_t looked_at = 0;
    std::size_t against_the_model = 0;
    std::vector<std::string> wrong;
    for (const auto& entry : roster) {
        const std::filesystem::path model_dir =
            data() / "jsbsim" / "aircraft" / entry.model;
        // Everything the model is made of: its own file and its systems.
        std::string model_text;
        for (const auto& file :
             std::filesystem::recursive_directory_iterator(model_dir)) {
            if (!file.is_regular_file()) {
                continue;
            }
            std::ifstream in(file.path(), std::ios::binary);
            model_text.append((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
        }
        check(!model_text.empty(), entry.model + "'s flight model was read");

        // Whether it burns petrol: its engines, as the model names them.
        bool piston = false;
        for (std::size_t at = model_text.find("<engine file=");
             at != std::string::npos; at = model_text.find("<engine file=", at + 1)) {
            const std::size_t open = model_text.find('"', at);
            const std::size_t close = open == std::string::npos
                                          ? std::string::npos
                                          : model_text.find('"', open + 1);
            if (close == std::string::npos) {
                continue;
            }
            const std::string name = model_text.substr(open + 1, close - open - 1);
            std::ifstream in(data() / "jsbsim" / "engine" / (name + ".xml"),
                             std::ios::binary);
            const std::string engine((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
            if (engine.find("<piston_engine") != std::string::npos ||
                engine.find("<turboprop_engine") != std::string::npos) {
                piston = true;
            }
        }

        const AircraftChecklists lists = glideslope::sim::find_checklists(data(), entry.id);
        for (const Checklist& list : lists.phases) {
            for (const ChecklistItem& item : list.items) {
                if (item.pilots()) {
                    continue;
                }
                const bool a_control = item.property.starts_with("fcs/") ||
                                       item.property.starts_with("gear/") ||
                                       item.property.starts_with("hydro/");
                if (!a_control) {
                    continue; // a state JSBSim always computes
                }
                ++looked_at;
                const std::string where =
                    entry.id + "'s " + glideslope::sim::phase_name(list.phase) + ": ";
                if (tied_per_engine.count(item.property) != 0) {
                    if (needs_a_piston.count(item.property) != 0 && !piston) {
                        wrong.push_back(where + item.property +
                                        ", but " + entry.model +
                                        " has no piston or turboprop engine");
                    }
                    continue;
                }
                if (model_text.find(item.property) == std::string::npos) {
                    wrong.push_back(where + entry.model + "'s flight model never "
                                    "mentions " + item.property +
                                    ", so the item could not tick");
                } else {
                    ++against_the_model;
                }
            }
        }
    }
    if (!wrong.empty()) {
        std::string all;
        for (const std::string& one : wrong) {
            all += "\n  " + one;
        }
        glideslope::test::fail(std::to_string(wrong.size()) +
                               " items rest on a control their aircraft has not got:" +
                               all);
    }
    check(looked_at > 0, "some items name a control");
    check(against_the_model > 0,
          "some of them were held against the model's own files");
}

// **A band the aeroplane can never reach is an item that can never tick.**
// The checks above prove the property is real and that the aircraft's own
// model drives it. They do not prove the figure is one the aeroplane can get
// to: a flap band of 33 to 35 degrees on a type whose flaps stop at 32 would
// pass everything above and never tick once.
//
// A lever and what it moves are held differently, because they are different
// things:
//
//   a command (`...-cmd-norm`) **is** the lever, and its travel is known
//   without flying anything: 0 to 1, or -1 to 1 for the ones that go both
//   ways. A band outside that is one no pilot could ever set.
//
//   a position (`...-pos-deg`, `...-pos-norm`) is where the aeroplane has
//   got to, which only its own model knows. Its levers are worked through
//   their travel with the aeroplane standing still, and what the position
//   really covers is measured.
//
// **The aeroplane is not flown to find out.** An earlier version drove every
// lever to its stop at once - full throttle against full brakes, the trim
// hard over - and put a tail-wheel aeroplane on its nose, off the end of its
// own aerodynamic tables, which JSBSim ends the flight for. Only the
// configuration levers are moved here, and the brakes hold it where it is.
//
// **What is left out, and why.** A speed, a height or an engine's speed is a
// state of the flight rather than of a lever. What those can reach is the
// whole flight envelope, which this does not fly, so they are counted and
// left alone.
namespace {

// Whether this model's undercarriage retracts, as its own files say.
bool retractable(const std::string& model) {
    for (const auto& file : std::filesystem::recursive_directory_iterator(
             std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR) / "aircraft" / model)) {
        if (!file.is_regular_file()) {
            continue;
        }
        std::ifstream in(file.path(), std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        // A fixed-gear aeroplane has no undercarriage channel at all, so it
        // never names the command or the position; one that retracts drives
        // them. `<retractable>` on a leg is not enough on its own: the
        // Mosquito's legs do not carry it and its undercarriage still comes
        // up.
        if (text.find("gear/gear-cmd-norm") != std::string::npos ||
            text.find("gear/gear-pos-norm") != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

GLIDESLOPE_TEST(every_checklist_band_is_one_its_aeroplanes_controls_can_reach) {
    const auto roster = glideslope::sim::read_catalogue(data());
    check(roster.size() == roster_size,
          "the roster is " + std::to_string(roster_size) + " aircraft, not " +
              std::to_string(roster.size()));

    constexpr int steps_per_second = 120;
    // The commands that go both ways; every other one runs 0 to 1.
    const std::set<std::string> both_ways{
        "fcs/elevator-cmd-norm", "fcs/aileron-cmd-norm", "fcs/rudder-cmd-norm",
        "fcs/pitch-trim-cmd-norm", "fcs/roll-trim-cmd-norm", "fcs/yaw-trim-cmd-norm"};

    std::size_t levers = 0;
    std::size_t positions = 0;
    std::size_t of_the_flight = 0;
    std::vector<std::string> wrong;
    for (const auto& entry : roster) {
        const AircraftChecklists lists = glideslope::sim::find_checklists(data(), entry.id);
        std::set<std::string> measure;
        for (const Checklist& list : lists.phases) {
            for (const ChecklistItem& item : list.items) {
                if (item.pilots()) {
                    continue;
                }
                const bool a_control = item.property.starts_with("fcs/") ||
                                       item.property.starts_with("gear/") ||
                                       item.property.starts_with("hydro/");
                if (!a_control) {
                    ++of_the_flight;
                } else if (item.property.ends_with("-cmd-norm")) {
                    // The lever itself: its travel is what a pilot can set.
                    ++levers;
                    const double least = both_ways.count(item.property) != 0 ? -1.0 : 0.0;
                    if (item.high < least || item.low > 1.0) {
                        wrong.push_back(entry.id + "'s " +
                                        glideslope::sim::phase_name(list.phase) + ", \"" +
                                        item.text + "\": wants " + item.property +
                                        " between " + std::to_string(item.low) + " and " +
                                        std::to_string(item.high) +
                                        ", which is outside the lever's travel of " +
                                        std::to_string(least) + " to 1");
                    }
                } else if (item.property.starts_with("gear/gear-pos")) {
                    // **The undercarriage is not worked on the ground.**
                    // Retracting it while the aeroplane stands on it drops
                    // the aeroplane on its belly and ends the flight inside
                    // JSBSim's own tables. What it can do is in the model:
                    // a leg it says is retractable moves, and one it does
                    // not stays down.
                    ++positions;
                    const bool retracts = retractable(entry.model);
                    const double least = retracts ? 0.0 : 1.0;
                    if (item.high < least || item.low > 1.0) {
                        wrong.push_back(entry.id + "'s " +
                                        glideslope::sim::phase_name(list.phase) + ", \"" +
                                        item.text + "\": wants " + item.property +
                                        " between " + std::to_string(item.low) + " and " +
                                        std::to_string(item.high) + ", but its gear " +
                                        (retracts ? "moves between 0 and 1"
                                                  : "is fixed down at 1"));
                    }
                } else {
                    measure.insert(item.property);
                }
            }
        }
        if (measure.empty()) {
            continue;
        }

        glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
        // **A flying boat needs water under it**, or its hull tells the
        // truth about dry land and every hydrodynamic item looks unreachable.
        const bool water = entry.seaplane;
        aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
            [](double, double) { return 0.0; },
            [water](double, double) { return water; }));
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = entry.seaplane ? 6.0 : 0.0;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = 0.0;
        ic.engine_running = true;
        ic.gear = 1.0;
        aircraft.initialize(ic);

        std::map<std::string, std::pair<double, double>> range;
        const auto watch = [&] {
            for (const std::string& name : measure) {
                double value = 0.0;
                try {
                    value = aircraft.property(name);
                } catch (const std::out_of_range&) {
                    continue;
                }
                const auto at = range.find(name);
                if (at == range.end()) {
                    range.emplace(name, std::pair{value, value});
                } else {
                    at->second.first = std::min(at->second.first, value);
                    at->second.second = std::max(at->second.second, value);
                }
            }
        };

        // The configuration levers out and back, with time for the slow ones:
        // the S.23's flaps are wound out by a motor that takes a full minute.
        for (const double set : {0.0, 1.0, 0.0}) {
            glideslope::sim::Controls c;
            c.throttle = 0.0;
            c.mixture = 1.0;
            c.left_brake = 1.0;
            c.right_brake = 1.0;
            c.flaps = set;
            c.gear = 1.0; // never on the ground; see above
            c.speedbrake = set;
            c.supercharger = set;
            c.cooling_flaps = {set, set};
            for (int i = 0; i < 70 * steps_per_second; ++i) {
                aircraft.set_controls(c);
                aircraft.step();
                watch();
            }
        }

        for (const Checklist& list : lists.phases) {
            for (const ChecklistItem& item : list.items) {
                if (item.pilots() || measure.count(item.property) == 0) {
                    continue;
                }
                const auto at = range.find(item.property);
                if (at == range.end()) {
                    continue;
                }
                ++positions;
                const double low = at->second.first;
                const double high = at->second.second;
                if (item.high < low || item.low > high) {
                    wrong.push_back(
                        entry.id + "'s " + glideslope::sim::phase_name(list.phase) +
                        ", \"" + item.text + "\": wants " + item.property + " between " +
                        std::to_string(item.low) + " and " + std::to_string(item.high) +
                        ", but its levers only move it between " + std::to_string(low) +
                        " and " + std::to_string(high));
                }
            }
        }
    }
    if (!wrong.empty()) {
        std::string all;
        for (const std::string& one : wrong) {
            all += "\n  " + one;
        }
        glideslope::test::fail(std::to_string(wrong.size()) +
                               " items ask for something their aeroplane cannot "
                               "reach:" + all);
    }
    check(levers > 0, "some items name a lever, held to its travel");
    check(positions > 0, "some name where a control has got to, held to what it moves");
    check(of_the_flight > 0,
          "and some are states of the flight, which this leaves out");
}
