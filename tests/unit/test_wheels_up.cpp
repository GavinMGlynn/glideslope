#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;
constexpr double seconds_flown = 180.0;

// **The friction an airframe scraping a runway is held by**, as stated in
// tools/ground.py and written into every airframe contact. It is what decides
// how far a wheels-up landing slides, so the distance is checked against it
// rather than against a number picked to fit.
constexpr double scrape_friction = 0.4;
constexpr double gravity_fps2 = 32.174;
constexpr double feet_per_metre = 3.280839895013123;
constexpr double fps_per_knot = 1.6878098571011957;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// **Whether the wheels can be raised at all.** This is the model's own
// <retractable> flag on a contact, not the gear lever: JSBSim binds
// fcs/gear-cmd-norm and gear/gear-pos-norm for every aeroplane whether or not
// anything moves, so the Mosquito's lever drives an indicator while all seven
// of its contacts stay exactly where they are. What decides whether an
// aeroplane can land on its belly is whether a contact retracts.
bool wheels_retract(const std::string& model) {
    std::ifstream in(data() / "jsbsim" / "aircraft" / model / (model + ".xml"),
                     std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    return text.find("<retractable>1</retractable>") != std::string::npos;
}

struct Slide {
    bool retracts = false;  // the wheels can be raised at all
    bool stopped = false;
    double lowest_ft = 0.0; // the lowest the centre of gravity got, above ground
    double slid_m = 0.0;
    double seconds = 0.0;
    double from_kts = 0.0;
};

// **Put down on a runway with the wheels up.** Started just above the ground
// below its approach speed with the gear commanded up and the engines closed,
// and flown until it stops or until it is plainly through the runway.
Slide put_down(const glideslope::sim::CatalogueEntry& entry) {
    glideslope::sim::Aircraft aircraft(data() / "jsbsim", entry.model);
    aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; }));

    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.terrain_elevation_ft = 0.0;
    ic.altitude_ft = 8.0;
    ic.heading_deg = 0.0;
    ic.airspeed_kts = 0.6 * entry.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0; // up, and this is what the whole test is about
    aircraft.initialize(ic);

    Slide out;
    out.from_kts = ic.airspeed_kts;
    // An aeroplane whose wheels do not retract keeps them wherever they were
    // put: this is the model itself saying so, not a list kept in the test.
    out.retracts = wheels_retract(entry.model);

    const double from_lat = aircraft.state().latitude_deg;
    out.lowest_ft = aircraft.property("position/h-agl-ft");
    glideslope::sim::Controls c;
    c.throttle = 0.0;
    c.gear = 0.0;
    c.mixture = 1.0;
    const int steps = static_cast<int>(seconds_flown * steps_per_second);
    for (int tick = 0; tick < steps; ++tick) {
        aircraft.set_controls(c);
        aircraft.step();
        out.lowest_ft = std::min(out.lowest_ft, aircraft.property("position/h-agl-ft"));
        if (out.lowest_ft < -20.0) {
            break; // through the runway, not resting on it
        }
        if (std::abs(aircraft.property("velocities/vg-fps")) < 1.0 && tick > 2 * steps_per_second) {
            out.stopped = true;
            out.seconds = static_cast<double>(tick) / steps_per_second;
            break;
        }
    }
    out.slid_m = std::abs(aircraft.state().latitude_deg - from_lat) * 111132.0;
    return out;
}

// How far a slide at `scrape_friction` takes to stop from `knots`, in metres.
double stated_distance_m(double knots) {
    const double fps = knots * fps_per_knot;
    return fps * fps / (2.0 * scrape_friction * gravity_fps2) / feet_per_metre;
}

} // namespace

// **Every aircraft landed on a runway with its wheels up comes to rest on its
// airframe, its centre of gravity above the ground, in a distance the stated
// friction gives.** JSBSim gives a retracted wheel no force, so an aeroplane
// whose only contacts are its undercarriage falls straight through the
// runway; what it should come down on is its belly, measured from its own
// visual mesh by tools/ground.py.
//
// **Every aircraft in the catalogue is flown.** Two kinds are held to
// different things, and both are named here rather than left out:
//
//   - The ten whose wheels retract must rest on their airframe and stop.
//   - The five whose wheels do not retract - the Cessna 172P and 182, the
//     Cub, the Cherokee and the Mosquito - have no wheels to raise, so they
//     land on them and free-roll. Nothing brakes them, so they are not asked
//     to stop; they are asked not to go through the runway. (The real
//     Mosquito's undercarriage did retract; this model's does not, and that
//     is the model's business, not this test's.)
//
// **The flying boat is the one aircraft left out**: the Short S.23 has no
// undercarriage at all, so it has no wheels to put up and a runway is not
// where it lands.
GLIDESLOPE_TEST(every_aircraft_put_down_with_its_wheels_up_rests_on_its_airframe) {
    // **Every aeroplane now has an airframe to rest on.** The F-35A was the
    // last without one - no F-35A flight model carrying the points exists,
    // and FGAddon has no F-35A to measure a mesh from - and it has since
    // become the F-35B, which FGAddon does have. This list is empty so that
    // an aeroplane losing its airframe again cannot pass unnoticed.
    const std::vector<std::string> awaiting_a_model{};

    const auto roster = glideslope::sim::read_catalogue(data());
    check(roster.size() == 16, "the roster is sixteen aircraft, not " +
                                   std::to_string(roster.size()));

    std::size_t retracting = 0, fixed = 0, afloat = 0;
    std::vector<std::string> fell_through, never_stopped, slid_too_far;
    for (const auto& entry : roster) {
        if (entry.seaplane) {
            ++afloat;
            continue;
        }
        const Slide s = put_down(entry);
        const bool through = s.lowest_ft < -5.0;
        const double stated = stated_distance_m(s.from_kts);
        std::printf("  %-14s %-4s %-24s lowest %7.2f ft, slid %6.0f m "
                    "(%.0f m at %.1f friction from %.0f kt)%s\n",
                    entry.id.c_str(), s.retracts ? "up" : "down",
                    through      ? "went through the runway"
                    : s.stopped  ? "came to rest"
                                 : "still moving at 180 s",
                    s.lowest_ft, s.slid_m, stated, scrape_friction, s.from_kts,
                    s.retracts ? "" : "  [wheels do not retract]");
        std::fflush(stdout);

        const bool excused = std::find(awaiting_a_model.begin(), awaiting_a_model.end(),
                                       entry.id) != awaiting_a_model.end();
        if (s.retracts) {
            ++retracting;
            if (through != excused) {
                fell_through.push_back(entry.id + (through ? " goes through the runway"
                                                           : " no longer does, so the"
                                                             " list above is stale"));
            }
            if (!through && !s.stopped) {
                never_stopped.push_back(entry.id);
            }
            // The slide is the stated friction's, within a factor of three:
            // lift carries some of the weight at first, and drag takes some
            // of the speed, so it is not asked to be the textbook figure.
            if (s.stopped && s.slid_m > 3.0 * stated) {
                slid_too_far.push_back(entry.id + " slid " + std::to_string(s.slid_m) +
                                       " m where the stated friction gives " +
                                       std::to_string(stated));
            }
        } else {
            ++fixed;
            if (through) {
                fell_through.push_back(entry.id + " has fixed wheels and still went"
                                                  " through the runway");
            }
        }
    }

    auto report = [](const std::vector<std::string>& bad, const std::string& what) {
        if (!bad.empty()) {
            std::string all;
            for (const std::string& one : bad) {
                all += "\n  " + one;
            }
            glideslope::test::fail(what + ":" + all);
        }
    };
    report(fell_through, "the airframe is not where this test expects it");
    report(never_stopped, "these rest on their airframe but never come to rest");
    report(slid_too_far, "these slid further than the stated friction allows");

    // **The space this walked, stated.** Sixteen aircraft: ten that retract,
    // five that cannot, and one flying boat.
    check(retracting == 10, "ten aircraft raise their wheels, not " + std::to_string(retracting));
    check(fixed == 5, "five cannot, not " + std::to_string(fixed));
    check(afloat == 1, "one is a flying boat and was left out, not " + std::to_string(afloat));
    check(retracting + fixed + afloat == roster.size(), "every aircraft was accounted for");
}
