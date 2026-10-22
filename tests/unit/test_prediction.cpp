#include "harness.hpp"

#include "frontend/client/pass.hpp"
#include "frontend/same_air.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/controller.hpp"
#include "sim/crash.hpp"
#include "sim/figures.hpp"
#include "sim/learnt.hpp"
#include "sim/pacing.hpp"
#include "sim/prediction.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

using glideslope::sim::Aircraft;
using glideslope::sim::AircraftSnapshot;
using glideslope::sim::Controls;
using glideslope::sim::InitialConditions;
using glideslope::sim::Prediction;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// A flat, dry world, so that the two aircraft are compared against each
// other and not against a terrain fetch.
std::shared_ptr<glideslope::sim::Terrain> flat_ground() {
    return std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return false; });
}

void set_up(Aircraft& a, double airspeed_kts = 110.0) {
    a.set_terrain(flat_ground());
    InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.terrain_elevation_ft = 0.0;
    ic.altitude_ft = 6000.0;
    ic.heading_deg = 90.0;
    ic.airspeed_kts = airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    a.initialize(ic);
}

// **A pilot doing something**, so that prediction has something to get
// wrong: a turn, a climb and a throttle change rather than straight and
// level, which any two instances would agree on.
Controls flying(int frame) {
    const double t = static_cast<double>(frame) / steps_per_second;
    Controls c;
    c.throttle = 0.7 + 0.2 * std::sin(t * 0.7);
    c.mixture = 1.0;
    c.aileron = 0.25 * std::sin(t * 1.3);
    c.elevator = 0.05 * std::sin(t * 0.9);
    c.rudder = 0.05 * std::sin(t * 0.5);
    return c;
}

struct Flight {
    double worst_correction_m = 0.0;
    double worst_prediction_m = 0.0;
    std::size_t reconciliations = 0;
    std::size_t snapped = 0;
    std::size_t worst_replayed = 0;
    // Steps where the speedbrake lever a side's flight model was given was not
    // the one in the input that side flew: the client's, against what it
    // sent, and the server's, against what it was sent.
    std::size_t client_lever_off = 0;
    std::size_t server_lever_off = 0;
};

// **Client and server, with the wire between them.** Both fly the same
// JSBSim at 120 Hz. The client flies each input as it is made; the server
// sees it `one_way` frames later, and its state comes back `one_way` frames
// after that, which is the round trip.
//
// `model`, begun at `airspeed_kts`, is flown on `inputs`; the client predicts
// with `predicted` instead if it is given - a client that flies other inputs
// than it sends, made so that a test can be seen to catch one.
Flight fly(int frames, int one_way, int snapshot_every, bool by_motion = false,
           const std::string& model = "c172p", double airspeed_kts = 110.0,
           const std::function<Controls(int)>& inputs = flying,
           const std::function<Controls(int)>& predicted = {}) {
    Aircraft server_aircraft(data() / "jsbsim", model);
    Aircraft client_aircraft(data() / "jsbsim", model);
    set_up(server_aircraft, airspeed_kts);
    set_up(client_aircraft, airspeed_kts);
    Prediction client(client_aircraft);

    struct Posted {
        int arrives_at_frame = 0;
        AircraftSnapshot state;
        glideslope::sim::Motion motion;
        std::uint32_t last_applied = 0;
    };
    std::deque<Posted> post;

    Flight out;
    for (int frame = 0; frame < frames; ++frame) {
        // The client flies its own input at once: that is the whole point of
        // predicting.
        client.step(static_cast<std::uint32_t>(frame + 1),
                    predicted ? predicted(frame) : inputs(frame));
        // **The lever the prediction's model was given is the one sent**,
        // read back from the model itself (Aircraft::set_controls).
        if (client_aircraft.property("fcs/speedbrake-cmd-norm") != inputs(frame).speedbrake) {
            ++out.client_lever_off;
        }

        // The server applies the input that left the client `one_way` frames
        // ago, which is the only one it has.
        const int theirs = frame - one_way;
        if (theirs >= 0) {
            server_aircraft.set_controls(inputs(theirs));
            server_aircraft.step();
            if (server_aircraft.property("fcs/speedbrake-cmd-norm") !=
                inputs(theirs).speedbrake) {
                ++out.server_lever_off;
            }
            if (theirs % snapshot_every == 0) {
                post.push_back({frame + one_way,
                                by_motion ? AircraftSnapshot{} : server_aircraft.capture(),
                                server_aircraft.motion(),
                                static_cast<std::uint32_t>(theirs + 1)});
            }
        }

        // And the server's word arrives a further `one_way` frames later.
        while (!post.empty() && post.front().arrives_at_frame <= frame) {
            const Prediction::Correction c =
                // One step into each input: the server flies each for one,
                // and had flown as many steps as the input's number.
                by_motion ? client.reconcile(post.front().motion, post.front().last_applied, 1,
                                             post.front().last_applied)
                          : client.reconcile(post.front().state, post.front().last_applied);
            out.worst_correction_m = std::max(out.worst_correction_m, c.moved_m);
            out.worst_replayed = std::max(out.worst_replayed, c.replayed);
            if (c.snapped) {
                ++out.snapped;
            }
            ++out.reconciliations;
            post.pop_front();
        }
    }
    out.worst_prediction_m = out.worst_correction_m;
    return out;
}

} // namespace

// **Prediction error and correction size stay within stated bounds**, which
// is the item's own verification, at 100 ms and 200 ms of simulated latency.
//
// **What is being measured.** The client flies its own inputs at once; the
// server flies the same inputs, late, and sends back where it got to and
// which input it had reached. The client puts its aircraft back to that and
// flies forward again through everything the server had not yet seen. How far
// the aircraft moves in doing that is the correction - and it is also the
// prediction error, because it is exactly the distance between where the
// client had the aeroplane and where the server says it was.
//
// **The bound is stated for one machine, and says so.** Both instances here
// are the same build on the same computer flying the same inputs in the same
// order, so what is left is the difference between flying a state forward
// continuously and putting it back and flying it again. Across platforms it
// will be larger; `REQUIREMENTS.md` 8.3 is where that is measured, and this
// is not that test.
GLIDESLOPE_TEST(prediction_and_correction_stay_within_their_bounds_at_100_and_200_ms) {
    // **Ten centimetres on one machine.** Measured on Linux with GCC the
    // worst is 1.5 mm at 100 ms and 3.5 mm at 200 ms - it grows with the
    // latency because more inputs are flown again and each replay is another
    // chance for the floating point to take a different path. The bound is
    // set thirty times the worst seen rather than at half a metre, so that it
    // would actually catch a reconciliation that had begun to go wrong; a
    // platform that cannot meet it is a finding worth seeing rather than a
    // number to widen.
    constexpr double bound_m = 0.1;
    std::size_t walked = 0;
    for (const int latency_ms : {100, 200}) {
        const int one_way = latency_ms * steps_per_second / 2000;
        const Flight f = fly(6 * steps_per_second, one_way, steps_per_second / 20);
        check(f.reconciliations > 50,
              "the server was heard from " + std::to_string(f.reconciliations) +
                  " times at " + std::to_string(latency_ms) + " ms");
        check(f.worst_replayed >= static_cast<std::size_t>(one_way),
              "and the client had at least the round trip's inputs to fly again: " +
                  std::to_string(f.worst_replayed));
        check(f.snapped == 0, "no correction was too large to hide at " +
                                  std::to_string(latency_ms) + " ms: " +
                                  std::to_string(f.snapped) + " were snapped");
        check(f.worst_correction_m <= bound_m,
              "at " + std::to_string(latency_ms) + " ms the worst correction was " +
                  std::to_string(f.worst_correction_m) + " m, over the " +
                  std::to_string(bound_m) + " m bound");
        std::printf("  %3d ms: %zu reconciliations, worst correction %.4f m, "
                    "replayed up to %zu inputs\n",
                    latency_ms, f.reconciliations, f.worst_correction_m,
                    f.worst_replayed);
        ++walked;
    }
    check(walked == 2, "both latencies were flown");
}

// **Reconciled from its motion alone**, which is what a state update can
// carry, prediction stays within its bound too - at 100 and 200 ms. The
// client's own engines and actuators, flown on the same inputs, are left as
// they are; only where the aeroplane is, how it points, and how fast it goes
// and turns are put right.
GLIDESLOPE_TEST(prediction_reconciled_from_motion_alone_stays_within_its_bound_at_100_and_200_ms) {
    constexpr double bound_m = 0.1;
    std::size_t walked = 0;
    for (const int latency_ms : {100, 200}) {
        const int one_way = latency_ms * steps_per_second / 2000;
        const Flight f = fly(6 * steps_per_second, one_way, steps_per_second / 20, true);
        std::printf("  %3d ms by motion: %zu reconciliations, worst correction %.4f m, "
                    "replayed up to %zu inputs\n",
                    latency_ms, f.reconciliations, f.worst_correction_m, f.worst_replayed);
        check(f.reconciliations > 50, "the server was heard from " +
                                          std::to_string(f.reconciliations) + " times");
        check(f.snapped == 0, "no correction was snapped at " + std::to_string(latency_ms) +
                                  " ms: " + std::to_string(f.snapped));
        check(f.worst_correction_m <= bound_m,
              "at " + std::to_string(latency_ms) + " ms the worst correction by motion was " +
                  std::to_string(f.worst_correction_m) + " m, over the " +
                  std::to_string(bound_m) + " m bound");
        ++walked;
    }
    check(walked == 2, "both latencies were flown");
}

// **Inputs thirty times a second, arriving when the network lets them**, as
// over a real one: the server flies each from when it arrives until the next
// does - anything from no steps to eleven, not four - and says how far into
// the newest it had got, and how many steps it had flown in all. Placed on the
// client's clock by those, the server's word corrects the client by
// millimetres; replayed from each input's end, as before the server said, by
// metres (PROJECT_STATUS.md).
//
// **Built, not drawn.** Input `i` (from one) is late by `(5 (i - 1)) mod 8`
// steps - the first on time, as the first the server hears of a client is in
// effect - so every lateness from none to seven comes up. With `lossy`, every
// seventh input from the fourth is lost - the server never applies it, and
// flies the one before it on until the next arrives - and every ninth update
// from the fifth is lost on its way back. With `part_way`, the client's
// prediction begins two steps into its first input, as one begun from the
// first update does. What each run covered is counted and returned.
namespace {

struct Jittered {
    double worst_m = 0.0;
    std::size_t heard = 0;          // once settled
    double settling_m = 0.0;        // the worst before
    std::size_t latenesses = 0;     // of the eight, how many came up
    std::size_t caught_into = 0;    // of steps one to seven into an input
    std::size_t inputs_lost = 0;
    std::size_t updates_lost = 0;
};

Jittered fly_jittered(int latency_ms, bool jitter, bool lossy, bool part_way) {
    constexpr int steps_per_input = steps_per_second / 30;
    constexpr std::uint32_t most_late = 8;
    const int one_way = latency_ms * steps_per_second / 2000;
    Aircraft server_aircraft(data() / "jsbsim", "c172p");
    Aircraft client_aircraft(data() / "jsbsim", "c172p");
    set_up(server_aircraft);
    set_up(client_aircraft);
    const auto controls_of = [](std::uint32_t sequence) {
        return flying(static_cast<int>(sequence) * steps_per_input);
    };
    const auto late = [jitter](std::uint32_t sequence) {
        return jitter ? (5 * (sequence - 1)) % most_late : 0;
    };
    const auto lost = [lossy](std::uint32_t sequence) { return lossy && sequence % 7 == 4; };
    const auto arrives = [one_way, late](std::uint32_t sequence) {
        const int sent = static_cast<int>(sequence - 1) * steps_per_input;
        return sent + one_way + static_cast<int>(late(sequence));
    };

    struct Posted {
        int arrives_at = 0;
        glideslope::sim::Motion motion;
        std::uint32_t applied = 0;
        std::size_t into = 0;
        std::uint64_t server_steps = 0;
    };
    std::deque<Posted> post;
    std::uint32_t applied = 0;
    std::size_t into = 0;
    std::uint32_t next = 1;
    std::uint64_t server_steps = 0;
    std::size_t posted = 0;
    std::vector<bool> lateness(most_late, false);
    std::vector<bool> caught(most_late, false);
    Jittered out;
    // The steps flown before the prediction began, on the first input.
    const int before = part_way ? 2 : 0;
    for (int frame = 0; frame < before; ++frame) {
        client_aircraft.set_controls(controls_of(1));
        client_aircraft.step();
    }
    Prediction client(client_aircraft);
    const int frames = 8 * steps_per_second;
    for (int frame = 0; frame < frames; ++frame) {
        // The client sends an input every fourth step and flies it at once.
        const auto sequence = static_cast<std::uint32_t>(frame / steps_per_input + 1);
        if (frame >= before) {
            client.step(sequence, controls_of(sequence));
        }

        // The server applies the newest input that has arrived, and flies
        // it, from the first input's arrival - when the aircraft is the
        // client's, as it was when the client flew it.
        while (arrives(next) <= frame) {
            if (lost(next)) {
                ++out.inputs_lost;
            } else {
                lateness[late(next)] = true;
                applied = next;
                into = 0;
            }
            ++next;
        }
        if (applied > 0) {
            server_aircraft.set_controls(controls_of(applied));
            server_aircraft.step();
            ++into;
            ++server_steps;
            if (frame % 5 == 0) {
                if (into < caught.size()) {
                    caught[into] = true;
                }
                if (lossy && posted % 9 == 4) {
                    ++out.updates_lost;
                } else {
                    post.push_back(
                        {frame + one_way, server_aircraft.motion(), applied, into, server_steps});
                }
                ++posted;
            }
        }
        while (!post.empty() && post.front().arrives_at <= frame) {
            // Counted once the clocks' difference is known, as the network
            // checks count it (sim::offset_settled): before, the correction
            // is the difference being learnt, and is so named.
            const bool settled = client.settled();
            const Prediction::Correction c = client.reconcile(
                post.front().motion, post.front().applied, post.front().into,
                post.front().server_steps);
            if (settled) {
                out.worst_m = std::max(out.worst_m, c.moved_m);
                ++out.heard;
            } else {
                out.settling_m = std::max(out.settling_m, c.moved_m);
            }
            post.pop_front();
        }
    }
    out.latenesses = static_cast<std::size_t>(std::count(lateness.begin(), lateness.end(), true));
    out.caught_into = static_cast<std::size_t>(std::count(caught.begin() + 1, caught.end(), true));
    return out;
}

} // namespace

// **Through 100 and 200 ms with jitter and loss, the client is put right by
// under a metre** - the tail's own verification, on every machine, because
// nothing here depends on the machine keeping time. Measured on Linux with
// GCC: PROJECT_STATUS.md.
GLIDESLOPE_TEST(a_client_put_right_from_as_far_into_its_input_as_the_server_had_flown_is_off_by_centimetres) {
    constexpr double bound_m = 1.0;
    std::size_t walked = 0;
    for (const int latency_ms : {100, 200}) {
        for (const bool lossy : {false, true}) {
            const Jittered f = fly_jittered(latency_ms, true, lossy, false);
            const std::string what = std::to_string(latency_ms) + " ms with jitter" +
                                     (lossy ? " and loss" : "");
            std::printf("  %s: %zu reconciliations, %zu inputs and %zu updates lost, worst "
                        "correction %.4f m (%.3f m while the clocks' difference settled)\n",
                        what.c_str(), f.heard, f.inputs_lost, f.updates_lost, f.worst_m,
                        f.settling_m);
            check(f.latenesses == 8, what + ": every lateness from none to seven steps came "
                                            "up, not " + std::to_string(f.latenesses));
            check(f.caught_into == 7, what + ": the server was caught at every step into an "
                                             "input from one to seven, not " +
                                             std::to_string(f.caught_into));
            check(lossy ? f.inputs_lost > 20 && f.updates_lost > 15
                        : f.inputs_lost == 0 && f.updates_lost == 0,
                  what + ": " + std::to_string(f.inputs_lost) + " inputs and " +
                      std::to_string(f.updates_lost) + " updates lost");
            check(f.heard > 100, what + ": the server was heard from " +
                                     std::to_string(f.heard) + " times");
            check(f.worst_m <= bound_m, what + ": the worst correction was " +
                                            std::to_string(f.worst_m) + " m, over the " +
                                            std::to_string(bound_m) + " m bound");
            ++walked;
        }
    }
    check(walked == 4, "both latencies were flown, with loss and without");
}

// **A prediction begun part-way through an input** - as one begun from the
// first update is - does not take the step it began on for the step that
// input began on. Taken so, the clocks' difference came out two steps short
// and was held for two seconds of updates: a metre off, every update, with no
// jitter at all.
GLIDESLOPE_TEST(a_prediction_begun_part_way_through_an_input_is_not_put_off_by_it) {
    const Jittered f = fly_jittered(200, false, false, true);
    std::printf("  begun two steps in: %zu reconciliations, worst correction %.4f m\n", f.heard,
                f.worst_m);
    check(f.heard > 100, "the server was heard from " + std::to_string(f.heard) + " times");
    check(f.worst_m <= 0.1, "the worst correction was " + std::to_string(f.worst_m) +
                                " m, over the 0.1 m bound");
}

// **A client that ignores the server drifts, and reconciling puts it back.**
// Without this, a reconciliation that did nothing at all would pass the test
// above, because doing nothing moves the aeroplane no distance.
GLIDESLOPE_TEST(a_client_that_flew_different_inputs_is_put_back_where_the_server_says) {
    Aircraft server_aircraft(data() / "jsbsim", "c172p");
    Aircraft client_aircraft(data() / "jsbsim", "c172p");
    set_up(server_aircraft);
    set_up(client_aircraft);
    Prediction client(client_aircraft);

    // The client holds the stick over; the server never saw that input.
    Controls hard;
    hard.throttle = 1.0;
    hard.mixture = 1.0;
    hard.aileron = 1.0;
    hard.elevator = -0.3;
    for (int i = 0; i < steps_per_second * 3; ++i) {
        client.step(static_cast<std::uint32_t>(i + 1), hard);
    }
    Controls level;
    level.throttle = 0.7;
    level.mixture = 1.0;
    for (int i = 0; i < steps_per_second * 3; ++i) {
        server_aircraft.set_controls(level);
        server_aircraft.step();
    }

    const double apart_before = glideslope::sim::how_far_apart_m(
        client_aircraft.state(), server_aircraft.state());
    check(apart_before > 50.0, "they had flown far apart: " +
                                   std::to_string(apart_before) + " m");

    // The server has applied everything; nothing is left to fly again.
    const Prediction::Correction c = client.reconcile(
        server_aircraft.capture(), static_cast<std::uint32_t>(steps_per_second * 3));
    check(client.unacknowledged() == 0, "nothing was left unacknowledged");
    check(c.replayed == 0, "and nothing had to be flown again");
    check(c.snapped, "a correction that large is snapped, not hidden");
    check(c.moved_m > 50.0, "the aeroplane was moved " + std::to_string(c.moved_m) +
                                " m to where the server says");

    const double apart_after = glideslope::sim::how_far_apart_m(
        client_aircraft.state(), server_aircraft.state());
    check(apart_after < 0.01, "and is now where the server says, within " +
                                  std::to_string(apart_after) + " m");
    std::printf("  drifted %.0f m, snapped back to within %.4f m\n", apart_before,
                apart_after);
}

// **And from the server's motion alone**, which is what goes over the wire:
// the same drift, put right as far as where it is and how it moves - which is
// all a state update carries.
GLIDESLOPE_TEST(a_client_that_flew_different_inputs_is_put_back_by_the_servers_motion_alone) {
    Aircraft server_aircraft(data() / "jsbsim", "c172p");
    Aircraft client_aircraft(data() / "jsbsim", "c172p");
    set_up(server_aircraft);
    set_up(client_aircraft);
    Prediction client(client_aircraft);

    // The client holds the stick over; the server never saw that input.
    Controls hard;
    hard.throttle = 1.0;
    hard.mixture = 1.0;
    hard.aileron = 1.0;
    hard.elevator = -0.3;
    for (int i = 0; i < steps_per_second * 3; ++i) {
        client.step(static_cast<std::uint32_t>(i + 1), hard);
    }
    Controls level;
    level.throttle = 0.7;
    level.mixture = 1.0;
    for (int i = 0; i < steps_per_second * 3; ++i) {
        server_aircraft.set_controls(level);
        server_aircraft.step();
    }

    const double apart_before = glideslope::sim::how_far_apart_m(
        client_aircraft.state(), server_aircraft.state());
    check(apart_before > 50.0, "they had flown far apart: " +
                                   std::to_string(apart_before) + " m");

    // The server has applied everything; nothing is left to fly again.
    const Prediction::Correction c = client.reconcile(
        server_aircraft.motion(), static_cast<std::uint32_t>(steps_per_second * 3), 1,
        steps_per_second * 3);
    check(client.unacknowledged() == 0, "nothing was left unacknowledged");
    check(c.replayed == 0, "and nothing had to be flown again");
    check(c.snapped, "a correction that large is snapped, not hidden");
    check(c.moved_m > 50.0, "the aeroplane was moved " + std::to_string(c.moved_m) +
                                " m to where the server says");

    const double apart_after = glideslope::sim::how_far_apart_m(
        client_aircraft.state(), server_aircraft.state());
    check(apart_after < 0.01, "and is now where the server says, within " +
                                  std::to_string(apart_after) + " m");
    std::printf("  by motion: drifted %.0f m, snapped back to within %.4f m\n", apart_before,
                apart_after);
}

// **Another aircraft taken over is flown on to now from the word that gave
// it**, however long the client took to hear it. Client and server fly the
// client's Cessna at 200 ms until the clocks' difference is known; the
// server's other Cessna, flying level half a kilometre away, is taken over
// at three seconds and flown by the client's inputs from then on, and its
// word is heard a whole second late - a pass of the frame loop that long, as
// a slow machine's is. Put where that word says and not flown on, it is that
// second and the round trip behind: tens of metres. Flown on through the
// inputs since, it is where the server has it once those inputs arrive.
GLIDESLOPE_TEST(an_aircraft_taken_over_is_flown_on_to_now_from_the_word_that_gave_it) {
    constexpr int steps_per_input = steps_per_second / 30;
    constexpr int one_way = 12; // 100 ms each way
    constexpr int taken_at = 3 * steps_per_second;
    constexpr int heard_late = steps_per_second;
    Aircraft own_server(data() / "jsbsim", "c172p");
    Aircraft own_client(data() / "jsbsim", "c172p");
    Aircraft other(data() / "jsbsim", "c172p");
    set_up(own_server);
    set_up(own_client);
    other.set_terrain(flat_ground());
    {
        InitialConditions ic;
        ic.latitude_deg = -33.905;
        ic.longitude_deg = 151.2;
        ic.terrain_elevation_ft = 0.0;
        ic.altitude_ft = 6000.0;
        ic.heading_deg = 90.0;
        ic.airspeed_kts = 110.0;
        ic.engine_running = true;
        ic.gear = 0.0;
        other.initialize(ic);
    }
    Controls level;
    level.throttle = 0.8;
    level.mixture = 1.0;
    const auto controls_of = [](std::uint32_t sequence) {
        return flying(static_cast<int>(sequence) * steps_per_input);
    };

    struct Posted {
        int arrives_at = 0;
        glideslope::sim::Motion motion;
        std::uint32_t applied = 0;
        std::size_t into = 0;
        std::uint64_t server_steps = 0;
    };
    std::deque<Posted> post;
    Prediction client(own_client);
    std::uint32_t applied = 0;
    std::uint32_t next = 1;
    std::size_t into = 0;
    std::uint64_t server_steps = 0;
    std::optional<Posted> taking;
    std::optional<glideslope::sim::AircraftState> adopted;
    std::size_t flown_on = 0;
    int adopted_at = -1;
    const int frames = taken_at + one_way + heard_late + one_way + 1;
    for (int frame = 0; frame < frames; ++frame) {
        const auto sequence = static_cast<std::uint32_t>(frame / steps_per_input + 1);
        client.step(sequence, controls_of(sequence));
        while (static_cast<int>(next - 1) * steps_per_input + one_way <= frame) {
            applied = next;
            into = 0;
            ++next;
        }
        if (applied > 0) {
            // Taken over: from here the other flies the client's inputs, and
            // the word that says so is on its way.
            const bool taken = frame >= taken_at;
            if (frame == taken_at) {
                taking = Posted{frame + one_way + heard_late, other.motion(), applied, into,
                                server_steps};
            }
            own_server.set_controls(controls_of(applied));
            own_server.step();
            other.set_controls(taken ? controls_of(applied) : level);
            other.step();
            ++into;
            ++server_steps;
            if (!taken && frame % 5 == 0) {
                post.push_back(
                    {frame + one_way, own_server.motion(), applied, into, server_steps});
            }
        }
        while (!post.empty() && post.front().arrives_at <= frame) {
            (void)client.reconcile(post.front().motion, post.front().applied,
                                   post.front().into, post.front().server_steps);
            post.pop_front();
        }
        if (taking && taking->arrives_at == frame) {
            check(client.settled(), "the clocks' difference was known at the take-over");
            flown_on = client.adopt(taking->motion, taking->server_steps);
            adopted = own_client.state();
            adopted_at = frame;
        }
    }
    // The server has the other where the client had it once the inputs the
    // client had flown have arrived: a one-way trip on.
    check(adopted.has_value(), "the take-over was heard");
    check(frames - 1 == adopted_at + one_way, "the server was flown a one-way trip past it");
    const double apart_m = glideslope::sim::how_far_apart_m(*adopted, other.state());
    std::printf("  heard %d ms late: flown on through %zu steps, %.3f m from where the server "
                "has it\n",
                heard_late * 1000 / steps_per_second, flown_on, apart_m);
    check(flown_on >= static_cast<std::size_t>(heard_late),
          "it was flown on through the second it was heard late, not " +
              std::to_string(flown_on) + " steps");
    check(apart_m < 1.0, "the aircraft taken over was " + std::to_string(apart_m) +
                             " m from where the server has it, over the 1 m bound");
}

namespace {

struct LongFrame {
    Prediction::Correction last;
    glideslope::sim::AircraftState client;
    std::size_t words = 0;
    bool settled = false;
};

// **A frame a second long, heard all at once.** Client and server 100 ms
// apart each way; the client sends an input every four steps and hears each
// word as it arrives, until half a second in, when a frame takes a second:
// nothing is sent, flown or heard, and then the input read at its end is
// sent, the second's 120 steps are flown on it, and every word that arrived
// meanwhile is heard - as the window client does (Online::hear). With
// `newest_only`, the older words give only the clocks' difference
// (Prediction::hear_clock) and the newest puts it right; otherwise each is
// reconciled in turn, as it was.
LongFrame fly_a_long_frame(bool newest_only) {
    constexpr int one_way = 12;
    constexpr int steps_per_input = 4;
    constexpr int long_at = steps_per_second / 2;
    constexpr int long_for = steps_per_second;
    Aircraft server(data() / "jsbsim", "c172p");
    Aircraft own(data() / "jsbsim", "c172p");
    set_up(server);
    set_up(own);
    Prediction client(own);
    struct Sent {
        int arrives_at = 0;
        std::uint32_t sequence = 0;
        Controls controls;
    };
    struct Posted {
        int arrives_at = 0;
        glideslope::sim::Motion motion;
        std::uint32_t applied = 0;
        std::size_t into = 0;
        std::uint64_t server_steps = 0;
    };
    std::deque<Sent> up;
    std::deque<Posted> down;
    std::uint32_t sequence = 0;
    Controls stick;
    std::uint32_t applied = 0;
    Controls applied_controls;
    std::size_t into = 0;
    std::uint64_t server_steps = 0;
    const auto send = [&](int frame) {
        ++sequence;
        stick = flying(static_cast<int>(sequence) * steps_per_input);
        up.push_back({frame + one_way, sequence, stick});
    };
    LongFrame out;
    for (int frame = 0; frame <= long_at + long_for; ++frame) {
        // The server, on its own clock: every step, whatever the client does.
        while (!up.empty() && up.front().arrives_at <= frame) {
            applied = up.front().sequence;
            applied_controls = up.front().controls;
            into = 0;
            up.pop_front();
        }
        if (applied > 0) {
            server.set_controls(applied_controls);
            server.step();
            ++into;
            ++server_steps;
            if (server_steps % 5 == 0) {
                down.push_back({frame + one_way, server.motion(), applied, into, server_steps});
            }
        }
        if (frame < long_at) {
            if (frame % steps_per_input == 0) {
                send(frame);
            }
            client.step(sequence, stick);
            while (!down.empty() && down.front().arrives_at <= frame) {
                (void)client.reconcile(down.front().motion, down.front().applied,
                                       down.front().into, down.front().server_steps);
                down.pop_front();
            }
        } else if (frame == long_at + long_for) {
            send(frame);
            for (int i = 0; i < long_for; ++i) {
                client.step(sequence, stick);
            }
            while (!down.empty() && down.front().arrives_at <= frame) {
                const Posted& p = down.front();
                ++out.words;
                if (newest_only && down.size() > 1 && down[1].arrives_at <= frame) {
                    client.hear_clock(p.applied, p.into, p.server_steps);
                } else {
                    out.last = client.reconcile(p.motion, p.applied, p.into, p.server_steps);
                }
                down.pop_front();
            }
        }
    }
    out.client = own.state();
    out.settled = client.settled();
    return out;
}

} // namespace

// **A second's frame heard all at once is put right once, from the newest
// word, by a little, and knows the clocks' difference as well as when every
// word put it right.** Put right from each in turn, every one replayed the
// inputs since - 24 words, each up to a second of steps - which in a
// sanitized build made the next frame longer still (PROJECT_STATUS.md,
// 2026-09-30). Settled only by the words heard in that frame, so a word left
// out of the clocks' difference shows.
GLIDESLOPE_TEST(a_seconds_frame_heard_at_once_is_put_right_once_from_its_newest_word) {
    const LongFrame once = fly_a_long_frame(true);
    const LongFrame each = fly_a_long_frame(false);
    const double apart_m = glideslope::sim::how_far_apart_m(once.client, each.client);
    std::printf("  %zu words in the frame: put right once by %.3f m, replaying %zu steps, "
                "at step %lld; %.6f m from where putting it right from each left it\n",
                once.words, once.last.moved_m, once.last.replayed,
                once.last.at_step ? static_cast<long long>(*once.last.at_step) : -1LL, apart_m);
    check(once.words >= 20, "the frame heard " + std::to_string(once.words) +
                                " words, not the second's 24 it was built to");
    check(once.settled && each.settled,
          "the clocks' difference was known after the frame, from its words");
    check(once.last.at_step.has_value() && once.last.at_step == each.last.at_step,
          "the newest word was placed at the step putting it right from each placed it");
    check(once.last.moved_m < 2.0,
          "put right by " + std::to_string(once.last.moved_m) + " m, over the 2 m bound");
    // Not to the bit: a word puts back the motion alone, and the engine and
    // actuators flown through each replay in turn are not quite where one
    // replay leaves them - 0.2 mm here.
    check(apart_m < 0.01, "left " + std::to_string(apart_m) +
                              " m from where putting it right from each word left it, over "
                              "the centimetre bound");
}

// **Three seconds of inputs are still held**: a frame on a slow machine and
// the round trip came to two, and the prediction once let go of what was
// past 240 steps - two seconds at 120 Hz - so a word about one of them could
// not be replayed from.
GLIDESLOPE_TEST(three_seconds_of_unacknowledged_inputs_are_held) {
    Aircraft own(data() / "jsbsim", "c172p");
    set_up(own);
    Prediction client(own);
    constexpr int three_seconds = 3 * steps_per_second;
    for (int step = 0; step < three_seconds; ++step) {
        client.step(static_cast<std::uint32_t>(step / 4 + 1), flying(step));
    }
    check(client.unacknowledged() == static_cast<std::size_t>(three_seconds),
          "held " + std::to_string(client.unacknowledged()) + " of the " +
              std::to_string(three_seconds) + " steps flown");
}

// **And no more than four seconds are held**: flown five seconds with nothing
// acknowledged, the oldest second is let go, and what is held is the four
// seconds sim::most_unacknowledged says.
GLIDESLOPE_TEST(no_more_than_four_seconds_of_unacknowledged_inputs_are_held) {
    Aircraft own(data() / "jsbsim", "c172p");
    set_up(own);
    Prediction client(own);
    constexpr int five_seconds = 5 * steps_per_second;
    for (int step = 0; step < five_seconds; ++step) {
        client.step(static_cast<std::uint32_t>(step / 4 + 1), flying(step));
    }
    check(client.unacknowledged() == 4 * steps_per_second,
          "held " + std::to_string(client.unacknowledged()) + " of the " +
              std::to_string(five_seconds) + " steps flown, not four seconds' 480");
}

namespace {

struct Paced {
    double worst_learning_m = 0.0; // put right before the clocks' difference was known
    double worst_known_m = 0.0;    // and after
    std::size_t learning = 0;
    std::size_t known = 0;
    std::size_t known_after_long = 0; // known, and heard after the long frame
    std::int64_t second_sent_after_us = 0;
    std::size_t long_frames = 0; // flown
};

// **A client paced by real time, as the window client is**, against a server
// on its own clock. Time is in microseconds, and a step is due every 120th of
// a second on both. The client's passes come at the frame lengths given, and
// each is the window client's, through the same glideslope::client::
// fly_a_pass that orders its parts there, the parts this model's: an input
// sent if a thirtieth of a second has gone since the last (Online::fly),
// what has arrived taken in (Online::listen), the ticks flown on what fly
// says, the input sent flown from the next (Online::flown), and what was
// taken in heard, the older words for the clocks' difference and the newest
// putting it right (Online::hear). The server applies an input from its first step after it
// arrives, and says where it is every fifth step; each way takes
// `one_way_us`. A long frame is one of `long_us` or more.
Paced fly_paced(const std::vector<std::int64_t>& frames_us, std::int64_t one_way_us,
                std::int64_t long_us) {
    constexpr std::int64_t second_us = 1000000;
    constexpr std::int64_t inputs_every_us = second_us / 30;
    const auto due_by = [](std::int64_t t_us) {
        return static_cast<std::uint64_t>(t_us * steps_per_second / second_us);
    };
    const auto time_of_step = [](std::uint64_t n) {
        return static_cast<std::int64_t>((n * second_us + steps_per_second - 1) /
                                         steps_per_second);
    };
    Aircraft server(data() / "jsbsim", "c172p");
    Aircraft own(data() / "jsbsim", "c172p");
    set_up(server);
    set_up(own);
    Prediction client(own);
    struct Sent {
        std::int64_t arrives_us = 0;
        std::uint32_t sequence = 0;
        Controls controls;
    };
    struct Posted {
        std::int64_t arrives_us = 0;
        glideslope::sim::Motion motion;
        std::uint32_t applied = 0;
        std::size_t into = 0;
        std::uint64_t server_steps = 0;
    };
    std::deque<Sent> up;
    std::deque<Posted> down;
    std::uint32_t applied = 0;
    Controls applied_controls = flying(0);
    std::size_t into = 0;
    std::uint64_t server_steps = 0;
    std::uint64_t client_steps = 0;
    std::uint32_t sequence = 0;
    Controls stick = flying(0);
    std::uint32_t flown_sequence = 0;
    Controls flown_stick = stick;
    std::int64_t sent_at_us = 0;
    std::int64_t now_us = 0;
    bool long_seen = false;
    Paced out;
    // The server, every step due by `t_us`, whatever the client did.
    const auto serve_until = [&](std::int64_t t_us) {
        while (server_steps < due_by(t_us)) {
            const std::int64_t at_us = time_of_step(server_steps + 1);
            while (!up.empty() && up.front().arrives_us < at_us) {
                applied = up.front().sequence;
                applied_controls = up.front().controls;
                into = 0;
                up.pop_front();
            }
            server.set_controls(applied_controls);
            server.step();
            ++into;
            ++server_steps;
            if (server_steps % 5 == 0) {
                down.push_back({at_us + one_way_us, server.motion(), applied, into, server_steps});
            }
        }
    };
    for (const std::int64_t frame_us : frames_us) {
        now_us += frame_us;
        serve_until(now_us);
        if (frame_us >= long_us) {
            long_seen = true;
            ++out.long_frames;
        }
        const auto send = [&] {
            sent_at_us = now_us;
            ++sequence;
            stick = flying(static_cast<int>(due_by(now_us)));
            up.push_back({now_us + one_way_us, sequence, stick});
            if (sequence == 2) {
                out.second_sent_after_us = frame_us;
            }
        };
        // The window client's own pass, its parts this model's.
        struct Link {
            std::function<Controls()> fly_;
            std::function<void()> listen_;
            std::function<void()> flown_;
            std::function<void()> hear_;
            Controls fly(double, const Controls&) { return fly_(); }
            void listen(double) { listen_(); }
            void ticks_flown() {}
            void flown() { flown_(); }
            void hear() { hear_(); }
        };
        std::vector<Posted> arrived;
        Link link{
            // Sent if due: the first, with nothing flown before it, flown at
            // once (Online::fly).
            [&] {
                if (sequence == 0) {
                    send();
                    flown_sequence = sequence;
                    flown_stick = stick;
                } else if (now_us - sent_at_us >= inputs_every_us) {
                    send();
                }
                return flown_stick;
            },
            // What has arrived by the time the clock was read.
            [&] {
                while (!down.empty() && down.front().arrives_us <= now_us) {
                    arrived.push_back(down.front());
                    down.pop_front();
                }
            },
            [&] {
                flown_sequence = sequence;
                flown_stick = stick;
            },
            // The older words for the clocks' difference, the newest
            // putting it right.
            [&] {
                for (std::size_t i = 0; i < arrived.size(); ++i) {
                    const Posted& p = arrived[i];
                    if (i + 1 < arrived.size()) {
                        client.hear_clock(p.applied, p.into, p.server_steps);
                        continue;
                    }
                    const bool known = client.settled();
                    const Prediction::Correction c =
                        client.reconcile(p.motion, p.applied, p.into, p.server_steps);
                    if (known) {
                        out.worst_known_m = std::max(out.worst_known_m, c.moved_m);
                        ++out.known;
                        if (long_seen) {
                            ++out.known_after_long;
                        }
                    } else {
                        out.worst_learning_m = std::max(out.worst_learning_m, c.moved_m);
                        ++out.learning;
                    }
                }
            }};
        const auto due = static_cast<std::int64_t>(due_by(now_us) - client_steps);
        glideslope::client::fly_a_pass(link, static_cast<double>(now_us) / 1e6, stick, due,
                                       [&](const Controls& controls) {
                                           client.step(flown_sequence, controls);
                                           ++client_steps;
                                       });
    }
    return out;
}

} // namespace

// **A long frame puts the client right by little, whether or not the clocks'
// difference is known yet** - CI's failures (PROJECT_STATUS.md, 2026-10-02):
// the window client put right 22.6 to 31.0 m around frames of 142 to 730 ms.
// Its passes sent the input first and then flew every step the frame was
// owed on it, so an input sent at the end of a long frame was flown here from
// the frame's beginning, and the server - which cannot fly an input before
// it arrives - flew it a whole frame later: that input said the clocks
// differ by the frame more than they do. Once the difference is known, the
// least of two seconds' words leaves it out; but while it is being learnt it
// can be the least heard, and when an input sent after a quick frame came the
// difference fell by the long frame, and the aircraft was put right by the
// way flown in it - 7.7 m at 142 ms, 25 m at 428 and 43 m at 730 here. Flown
// on the input sent before, and the new one sent after, each input is flown
// here from the step it was sent at, as the server flies it.
//
// **The space**: five long frames (CI's 142, 167, 284 and 730 ms, and 428)
// by three places - the first and the second frame after the prediction
// began, where the first input whose beginning is known is the one sent at
// the long frame's end, and three seconds on, when the difference is known -
// by two networks, 0 and 5 ms each way: thirty flights, each counted.
GLIDESLOPE_TEST(a_long_frame_puts_a_client_right_by_little_whether_or_not_its_clocks_difference_is_known_yet) {
    // A tenth of the snap and more: what is left is the steps a word is
    // placed to, half a metre each at 120 knots.
    constexpr double bound_m = 3.0;
    constexpr std::int64_t quick_us = 16667;
    const std::vector<std::int64_t> long_ms{142, 167, 284, 428, 730};
    const std::vector<int> quick_before{1, 2, 3 * 60};
    const std::vector<std::int64_t> one_way_ms{0, 5};
    std::size_t long_frames_flown = 0;
    for (const std::int64_t long_frame_ms : long_ms) {
        for (const int before : quick_before) {
            for (const std::int64_t one_way : one_way_ms) {
                std::vector<std::int64_t> frames(static_cast<std::size_t>(before), quick_us);
                frames.push_back(long_frame_ms * 1000);
                frames.insert(frames.end(), 3 * 60, quick_us);
                const Paced p = fly_paced(frames, one_way * 1000, long_frame_ms * 1000);
                const bool learning = before < 60;
                const std::string what = std::to_string(long_frame_ms) + " ms " +
                                         std::to_string(before) + " frames in, " +
                                         std::to_string(one_way) + " ms each way";
                std::printf("  %s: put right %zu times learning the clocks' difference, the "
                            "worst %.3f m; %zu times knowing it, the worst %.3f m\n",
                            what.c_str(), p.learning, p.worst_learning_m, p.known,
                            p.worst_known_m);
                if (learning) {
                    check(p.second_sent_after_us == long_frame_ms * 1000,
                          what + ": the first input whose beginning is known was sent at the "
                                 "long frame's end");
                } else {
                    check(p.known_after_long > 0,
                          what + ": the clocks' difference was known before the long frame, "
                                 "and the client was put right after it");
                }
                check(p.learning > 0 && p.known >= 40,
                      what + ": put right " + std::to_string(p.learning) + " times learning and " +
                          std::to_string(p.known) + " knowing");
                check(p.worst_learning_m <= bound_m && p.worst_known_m <= bound_m,
                      what + ": put right by " +
                          std::to_string(std::max(p.worst_learning_m, p.worst_known_m)) +
                          " m, over the " + std::to_string(bound_m) + " m bound");
                long_frames_flown += p.long_frames;
            }
        }
    }
    const std::size_t space = long_ms.size() * quick_before.size() * one_way_ms.size();
    check(long_frames_flown == space, "flew " + std::to_string(long_frames_flown) +
                                          " long frames in the " + std::to_string(space) +
                                          " flights, not one in each");
}

namespace {

// Air that keeps the moment it was asked for, and is still.
class MomentsAsked : public glideslope::sim::Weather {
public:
    glideslope::sim::Conditions at(double, double, double, double time_s) override {
        moments.push_back(time_s);
        return {};
    }
    std::vector<double> moments;
};

} // namespace

// **Every step a client flies meets the air at the moment the server flies
// it**, a step flown again in a replay as much as one flown forward: a gust
// is of the session's time, and air read at one moment for every step of a
// frame put a client 1.6 m out in gusts (PROJECT_STATUS.md, 2026-10-06). The
// client flies 50 inputs of four steps each; the server's word says it
// applied input 30 at its step 1000, so this client's step s is the server's
// s + 884, flown at (s + 885) / 120 s. Every step of the replay that follows,
// 84 of them, and the next one forward are held to that; before the word,
// there is no such moment.
GLIDESLOPE_TEST(every_step_a_client_flies_meets_the_air_at_the_moment_the_server_flies_it) {
    Aircraft client_aircraft(data() / "jsbsim", "c172p");
    set_up(client_aircraft);
    Prediction client(client_aircraft);
    const auto asked = std::make_shared<MomentsAsked>();
    client_aircraft.set_weather(std::make_shared<glideslope::frontend::SessionClocked>(
        asked, [&client] { return client.session_time_s().value_or(-1.0); }));
    Controls c;
    c.throttle = 0.7;
    c.mixture = 1.0;
    for (std::uint32_t s = 0; s < 200; ++s) {
        client.step(s / 4 + 1, c);
    }
    check(!client.session_time_s(), "no moment before the clocks' difference is known");
    // Input 30 began at this client's step 116, and the server applied it at
    // its step 1000: an offset of 884.
    asked->moments.clear();
    const Prediction::Correction k = client.reconcile(client_aircraft.motion(), 30, 0, 1000);
    check(k.replayed == 200 - 116, "the steps from 116 on were flown again: " +
                                       std::to_string(k.replayed));
    check(asked->moments.size() == k.replayed, "each asked the air once");
    std::size_t held = 0;
    for (std::size_t i = 0; i < asked->moments.size(); ++i) {
        const double want = static_cast<double>(116 + i + 885) / 120.0;
        check(asked->moments[i] == want, "replayed step " + std::to_string(116 + i) +
                                             " met the air at " +
                                             std::to_string(asked->moments[i]) + " s, not " +
                                             std::to_string(want));
        ++held;
    }
    asked->moments.clear();
    client.step(51, c);
    check(asked->moments.size() == 1 && asked->moments[0] == (200.0 + 885.0) / 120.0,
          "and the next step forward meets it at its own moment");
    check(held == 84, "84 replayed steps were held to their moments");
}

// **A prediction stops the engine the server names, once, and starts it
// again when the word says none has stopped** - an aircraft flown again after
// a wreck - or names another. Stopped before it is put right, so that the
// inputs flown again are flown with it out; and only an engine it stopped for
// that word is started again: one stopped here by itself stays stopped. A
// twin, so that which engine is what is tested.
GLIDESLOPE_TEST(a_prediction_stops_its_engine_on_the_servers_word_and_starts_it_again_on_its_word) {
    Aircraft twin(data() / "jsbsim", glideslope::sim::find_aircraft(data(), "mosquito-fb6").model);
    set_up(twin);
    check(twin.engine_count() == 2, "the Mosquito has two engines");
    glideslope::sim::Prediction prediction(twin);
    check(!twin.any_engine_stopped(), "both run to begin with");
    check(!prediction.hear_engine_stopped(std::nullopt), "nothing is stopped for a word of none");
    check(prediction.hear_engine_stopped(1), "the word that the second has stopped stops one");
    check(twin.engine_running(0) && !twin.engine_running(1), "and it is the second");
    check(!prediction.hear_engine_stopped(1), "said again, nothing more is stopped");
    check(prediction.engines_stopped_for_the_server() == 1, "one stopped for the word");
    for (int i = 0; i < steps_per_second; ++i) {
        prediction.step(static_cast<std::uint32_t>(i + 1), flying(i));
    }
    check(!twin.engine_running(1), "it stays stopped while flown, its ignition off");
    check(prediction.hear_engine_stopped(0), "the first named instead is stopped");
    check(!twin.engine_running(0) && twin.engine_running(1),
          "and the second, stopped for the word before, started again");
    check(!prediction.hear_engine_stopped(std::nullopt), "the word of none stops nothing");
    check(!twin.any_engine_stopped(), "and starts the one it stopped again");
    check(!prediction.hear_engine_stopped(7), "an engine it does not have is not stopped");

    Aircraft dry(data() / "jsbsim", "c172p");
    set_up(dry);
    glideslope::sim::Prediction own(dry);
    dry.fail_engine(0, false);
    check(!own.hear_engine_stopped(0), "one stopped here already is not stopped again");
    (void)own.hear_engine_stopped(std::nullopt);
    check(dry.any_engine_stopped(), "and one stopped here by itself is not started for the word");
}

namespace {

// How far apart two aircraft's engines turn, the worst over every engine: in
// rpm for the propellers and rotors, in percentage points for the spools.
struct SpinApart {
    double rpm = 0.0;
    double spool = 0.0;
};

SpinApart spin_apart(const glideslope::sim::ReplayState& a, const glideslope::sim::ReplayState& b) {
    SpinApart out;
    for (std::size_t i = 0; i < a.thruster_rpm.size(); ++i) {
        out.rpm = std::max(out.rpm, std::abs(a.thruster_rpm[i] - b.thruster_rpm[i]));
        for (const auto& spool : {a.n1[i] - b.n1[i], a.n2[i] - b.n2[i]}) {
            // NaN where the engine has no such spool, on both.
            if (!std::isnan(spool)) {
                out.spool = std::max(out.spool, std::abs(spool));
            }
        }
    }
    return out;
}

// The surfaces a replay moved on again: what the levers move, read back.
constexpr std::array<const char*, 3> moved_by_levers = {
    "fcs/flap-pos-norm", "fcs/speedbrake-pos-norm", "gear/gear-pos-norm"};

} // namespace

// **An engine stopped on the server winds down in a prediction put right as
// it does flown once**, every engine of every aircraft. A word puts back the
// motion alone and the steps since are flown again; an engine's spin - a
// propeller's rpm, a turbine's spools - is not in the motion, and was left as
// it was at the newest step, so every replay wound a stopping engine down
// again: the Cessna's propeller, stopped by the server, took six seconds to
// stop there and under two in a client half a second ahead
// (PROJECT_STATUS.md, 2026-10-09). **And the flaps, the speedbrakes and the
// gear moving are where they are flown once**: their actuators, moved on by
// every replay the same way, are put back too (sim::ReplayState).
// **Built, not timed**: two of each aircraft, the same engine of each
// stopped at the same step; one flown once, the other predicted and put
// right ten times a second by words that agree with it exactly - its own
// motion half a second back - so that whatever is apart is what the replays
// did. The throttle moves, so the running engines' spin changes too; the
// flaps run out, the speedbrake lever is out for a second and the gear goes
// down.
//
// **What must hold**: the engines apart by no more than a tenth of how far
// they wound flown once (2 rpm, 0.1 points of a spool, at the least), and
// each surface within a hundredth of its travel. Measured (linux-release):
// see PROJECT_STATUS.md, 2026-10-09. With nothing put back the jets were 17
// to 27 points apart and the propellers 600 to 1500 rpm - all but the
// Mosquito's, whose stopped propeller windmills at what the air gives it
// within a replay, so it holds either way (1.6 rpm); held, not a witness.
// With the jets' spools put where the throttle settles them after every
// correction (Aircraft::set_motion), 7.6 points.
GLIDESLOPE_TEST(an_engine_stopped_on_the_server_winds_down_in_a_prediction_put_right_as_it_does_flown_once_in_every_aircraft) {
    constexpr int flown_steps = 2 * steps_per_second;
    constexpr int behind = steps_per_second / 2;
    constexpr int every = steps_per_second / 10;
    constexpr double share = 0.1;
    constexpr double rpm_floor = 2.0;
    constexpr double spool_floor = 0.1;
    constexpr double surface_bound = 0.01;
    const auto catalogue = glideslope::sim::read_catalogue(data());
    std::size_t flown = 0;
    std::size_t engines = 0;
    std::set<std::string> left_out;
    for (const auto& entry : catalogue) {
        int count = 1;
        for (int engine = 0; engine < count; ++engine) {
            Aircraft once(data() / "jsbsim", entry.model);
            Aircraft predicted(data() / "jsbsim", entry.model);
            count = once.engine_count();
            if (engine == 0) {
                engines += static_cast<std::size_t>(count);
            }
            set_up(once, entry.start_airspeed_kts);
            set_up(predicted, entry.start_airspeed_kts);
            once.fail_engine(engine, false);
            Prediction client(predicted);
            check(client.hear_engine_stopped(engine),
                  entry.id + ": the client stopped engine " + std::to_string(engine));
            const auto controls = [&entry](int step) {
                Controls c = flying(step);
                c.throttle =
                    std::clamp(entry.start_throttle + 0.2 * std::sin(step * 0.02), 0.0, 1.0);
                const double t = static_cast<double>(step) / steps_per_second;
                c.flaps = std::min(1.0, t / 1.5);
                c.speedbrake = t >= 0.25 && t < 1.25 ? 1.0 : 0.0;
                c.gear = t >= 0.5 ? 1.0 : 0.0;
                return c;
            };
            std::vector<glideslope::sim::Motion> after;
            std::size_t words = 0;
            // How it turned after its first step, against which how far it
            // wound in the flight is measured.
            glideslope::sim::ReplayState began;
            double surfaces_apart = 0.0;
            std::string worst_surface;
            for (int step = 0; step < flown_steps; ++step) {
                once.set_controls(controls(step));
                once.step();
                if (step == 0) {
                    began = once.replay_state();
                }
                client.step(static_cast<std::uint32_t>(step + 1), controls(step));
                after.push_back(predicted.motion());
                // The word about step k: its motion after it, the server's
                // steps then k + 1, one step into input k + 1.
                if (step >= behind && step % every == 0) {
                    const int k = step - behind;
                    (void)client.reconcile(after[static_cast<std::size_t>(k)],
                                           static_cast<std::uint32_t>(k + 1), 1,
                                           static_cast<std::uint64_t>(k + 1));
                    ++words;
                }
                for (const char* surface : moved_by_levers) {
                    // **Left out**: the Short S.23's flaps, wound by a motor
                    // through an integrator, whose state JSBSim keeps in a
                    // member with no setter (sim::ReplayState) - 0.13 of
                    // their travel apart here, a minute's travel end to end.
                    if (entry.id == "short_s23" && std::string(surface) == "fcs/flap-pos-norm") {
                        left_out.insert(entry.id + " " + surface);
                        continue;
                    }
                    const double apart =
                        std::abs(once.property(surface) - predicted.property(surface));
                    if (apart > surfaces_apart) {
                        surfaces_apart = apart;
                        worst_surface = surface;
                    }
                }
            }
            const SpinApart apart =
                spin_apart(once.replay_state(), predicted.replay_state());
            const SpinApart wound = spin_apart(began, once.replay_state());
            const double rpm_bound = std::max(rpm_floor, share * wound.rpm);
            const double spool_bound = std::max(spool_floor, share * wound.spool);
            std::printf("  %-12s engine %d, %zu words; flown once, wound %8.2f rpm and %6.3f "
                        "points; put right, apart by %7.3f rpm and %6.3f points, surfaces "
                        "%.4f\n",
                        entry.id.c_str(), engine, words, wound.rpm, wound.spool, apart.rpm,
                        apart.spool, surfaces_apart);
            const std::string which = entry.id + " engine " + std::to_string(engine);
            check(words == 15, which + ": put right " + std::to_string(words) + " times, not 15");
            check(apart.rpm <= rpm_bound,
                  which + ": put right, its engines turned " + std::to_string(apart.rpm) +
                      " rpm from flown once, over the " + std::to_string(rpm_bound) + " bound");
            check(apart.spool <= spool_bound,
                  which + ": put right, its spools were " + std::to_string(apart.spool) +
                      " points from flown once, over the " + std::to_string(spool_bound) +
                      " bound");
            check(surfaces_apart <= surface_bound,
                  which + ": put right, " + worst_surface + " was " +
                      std::to_string(surfaces_apart) + " from flown once, over " +
                      std::to_string(surface_bound));
            ++flown;
        }
    }
    check(left_out.size() == 1, "one surface was left out, the S.23's flaps, not " +
                                    std::to_string(left_out.size()));
    check(catalogue.size() == 16 && engines == 35 && flown == engines,
          "every engine of every aircraft was stopped: " + std::to_string(flown) + " of " +
              std::to_string(engines) + " engines, in " + std::to_string(catalogue.size()) +
              " aircraft, not the 35 in 16");
}

// **A client working the speedbrakes is predicted as the server flies them**,
// in every aircraft that has them: the lever run out and in again over six
// seconds, on top of the turn, the climb and the throttle the other tests
// fly, at 200 ms and put right from the motion alone, as a state update
// carries it. The lever is an input like any other, so the client and the
// server move the same surfaces, and the worst correction is held to a quarter
// over the same flight's with the lever left stowed: working it costs the
// prediction nothing it did not already have. **And the lever each side's
// flight model is given is the one sent, at every step**, read back from the
// model - which catches a client flying the lever stowed while sending it
// out in every one of the nine. The correction alone does not: put right
// every 50 ms, such a client was put right by no more than its own drift in
// five of them, because one round trip flown wrongly moves her millimetres;
// only the 737's shows it plainly (0.094 m against 0.037), and that is held.
GLIDESLOPE_TEST(a_client_working_the_speedbrakes_is_predicted_as_the_server_flies_them_in_every_aircraft_with_them) {
    const int one_way = 200 * steps_per_second / 2000;
    const auto catalogue = glideslope::sim::read_catalogue(data());
    std::size_t walked = 0;
    std::size_t with = 0;
    for (const auto& entry : catalogue) {
        if (!entry.speedbrakes) {
            continue;
        }
        ++with;
        const auto lever = [&entry](int frame, bool worked) {
            Controls c = flying(frame);
            c.throttle = entry.start_throttle;
            const double t = static_cast<double>(frame) / steps_per_second;
            // Out over the first two seconds, held, and in over the last two.
            c.speedbrake = worked ? std::clamp(std::min(t, 6.0 - t) / 2.0, 0.0, 1.0) : 0.0;
            return c;
        };
        const auto worked = [&lever](int frame) { return lever(frame, true); };
        const auto stowed = [&lever](int frame) { return lever(frame, false); };
        const auto flown = [&](const std::function<Controls(int)>& sent,
                               const std::function<Controls(int)>& predicted) {
            return fly(6 * steps_per_second, one_way, steps_per_second / 20, true, entry.model,
                       entry.start_airspeed_kts, sent, predicted);
        };
        const Flight still = flown(stowed, {});
        const Flight f = flown(worked, {});
        // **Both sides fly the lever sent, at every step**, and a client
        // that flew it stowed while sending it out is caught at every step it
        // was out.
        const Flight wrong = flown(worked, stowed);
        check(f.client_lever_off == 0 && f.server_lever_off == 0,
              entry.id + ": the lever flown was not the lever sent at " +
                  std::to_string(f.client_lever_off) + " of the client's steps and " +
                  std::to_string(f.server_lever_off) + " of the server's");
        check(wrong.client_lever_off > 0 && wrong.server_lever_off == 0,
              entry.id + ": a client flying the lever stowed while sending it out was not "
                         "caught: " +
                  std::to_string(wrong.client_lever_off) + " steps off");
        // And where it shows in the correction, the 737's, it is held there.
        if (entry.id == "737-300") {
            check(wrong.worst_correction_m > 2.0 * f.worst_correction_m,
                  "737-300: flown stowed while sent out, she was put right by " +
                      std::to_string(wrong.worst_correction_m) + " m, not more than twice " +
                      std::to_string(f.worst_correction_m) + " m");
        }
        // Since the actuators are put back for every replay (2026-10-09),
        // the lever costs nothing: the F-22's cost her 11 cm before, hidden
        // until then under her spools' error (sim::ReplayState).
        const double bound_m = 1.25 * still.worst_correction_m;
        std::printf("  %-11s at 200 ms: worst correction %.4f m with the lever worked (bound "
                    "%.4f), %.4f m stowed\n",
                    entry.id.c_str(), f.worst_correction_m, bound_m, still.worst_correction_m);
        check(f.reconciliations > 50, entry.id + " was heard from " +
                                          std::to_string(f.reconciliations) + " times");
        check(f.snapped == 0, entry.id + ": no correction was too large to hide");
        check(f.worst_correction_m <= bound_m,
              entry.id + ": the worst correction was " + std::to_string(f.worst_correction_m) +
                  " m, over the bound of " + std::to_string(bound_m) + " m");
        ++walked;
    }
    check(walked == with && with == 9,
          "the nine aircraft with speedbrakes were flown: " + std::to_string(walked) + " of " +
              std::to_string(with));
}

namespace {

// **A client and a server on clocks the test sets**: the server's runs at
// `rate` of real time, as one behind real time does, and the client paces
// its own flying by the clocks' difference its prediction would measure
// (sim::Pacing). The client sends an input every thirtieth of a second, begun
// at the step it has flown to; the server applies each `delay_s` later, at
// the step its clock has reached, and says so in a word every twenty-fifth of
// a simulated second, heard `delay_s` after that. The pace is steered by the
// least of the last ten words' differences (sim::pace_window), once there
// are twenty-five (sim::offset_settled), as the client does.
struct AtPace {
    // How fast it flew over the last ten seconds, as a fraction of real time.
    double pace = 1.0;
    // The most the difference moved from the one held over the last ten
    // seconds, in steps.
    std::int64_t worst_late_off = 0;
    // Where the difference was at the end against where it began, in steps.
    std::int64_t drifted = 0;
    // The furthest the pace was from the server's from the jump on.
    double worst_pace_off_after_jump = 0.0;
    // The difference from the one held at the end, in steps.
    std::int64_t end_off = 0;
    // The furthest the difference went from the one held after the server's
    // pace changed, and when it last was more than two steps from it or the
    // pace more than 0.01 from the server's, in seconds after the change.
    std::int64_t worst_off_after_change = 0;
    double followed_after_s = 0.0;
};

// `jump` steps more on the server's clock from `jump_at_s` on: a server that
// had fallen behind catching up all at once, as a starved one does four steps
// a look.
// And `rate_after` of real time from `change_at_s` on, if it is given: a
// server slowed all of a sudden, its clock running on from where it was.
AtPace fly_at_pace(double rate, double seconds, bool paced, std::int64_t jump = 0,
                   double jump_at_s = 0.0, double rate_after = -1.0,
                   double change_at_s = 0.0) {
    constexpr double delay_s = 0.1;
    constexpr double frame_s = 1.0 / 60.0;
    glideslope::sim::Pacing pacing;
    struct Sent {
        double at_s;
        std::int64_t began;
    };
    std::deque<Sent> sent;
    std::deque<std::int64_t> offsets;
    std::int64_t applied_at = 0;
    std::int64_t applied_began = -1;
    double next_send_s = 0.0;
    double next_word_s = 0.0; // on the server's clock
    std::optional<std::int64_t> first;
    AtPace out;
    double late_flown_s = 0.0, late_t = 0.0, last_flown_s = 0.0, last_t = 1.0;
    const bool changes = rate_after > 0.0;
    const auto server_step = [=](double real_s) {
        const double server_s = changes && real_s >= change_at_s
                                    ? change_at_s * rate + (real_s - change_at_s) * rate_after
                                    : real_s * rate;
        return static_cast<std::int64_t>(server_s * 120.0) +
               (jump != 0 && real_s >= jump_at_s ? jump : 0);
    };
    const double final_rate = changes ? rate_after : rate;
    for (int frame = 0; static_cast<double>(frame) * frame_s < seconds; ++frame) {
        const double t = static_cast<double>(frame) * frame_s;
        const double flown_s = paced ? pacing.at(t) : t;
        if (t < seconds - 10.0) {
            late_flown_s = flown_s;
            late_t = t;
        }
        last_flown_s = flown_s;
        last_t = t;
        const auto flown = static_cast<std::int64_t>(flown_s * 120.0);
        if (t >= next_send_s) {
            sent.push_back({t, flown});
            next_send_s += 1.0 / 30.0;
        }
        // Applied on the server once arrived, at the step its clock is at.
        while (!sent.empty() && sent.front().at_s + delay_s <= t) {
            applied_at = server_step(sent.front().at_s + delay_s);
            applied_began = sent.front().began;
            sent.pop_front();
        }
        // A word every twenty-fifth of the server's second, heard a delay
        // later.
        const double server_s_heard =
            static_cast<double>(server_step(t - delay_s)) / 120.0;
        while (applied_began >= 0 && server_s_heard >= next_word_s) {
            next_word_s += 1.0 / 25.0;
            offsets.push_back(applied_at - applied_began);
            while (offsets.size() > glideslope::sim::offset_window) {
                offsets.pop_front();
            }
            if (offsets.size() < glideslope::sim::offset_settled) {
                continue;
            }
            const std::int64_t least = *std::min_element(
                offsets.end() - static_cast<std::ptrdiff_t>(std::min(offsets.size(), glideslope::sim::pace_window)),
                offsets.end());
            if (!first) {
                first = least;
            }
            out.drifted = least - *first;
            pacing.heard(least, t);
            if (changes && t >= change_at_s) {
                if (std::llabs(pacing.off()) > std::llabs(out.worst_off_after_change)) {
                    out.worst_off_after_change = pacing.off();
                }
                if (std::llabs(pacing.off()) > 2 || std::abs(pacing.pace() - rate_after) > 0.01) {
                    out.followed_after_s = t - change_at_s;
                }
            }
            if (jump != 0 && t >= jump_at_s) {
                out.worst_pace_off_after_jump =
                    std::max(out.worst_pace_off_after_jump, std::abs(pacing.pace() - rate));
            }
            if (t >= seconds - 10.0 && std::llabs(pacing.off()) > std::llabs(out.worst_late_off)) {
                out.worst_late_off = pacing.off();
            }
        }
    }
    out.pace = (last_flown_s - late_flown_s) / (last_t - late_t);
    out.end_off = pacing.off();
    (void)final_rate;
    return out;
}

} // namespace

// **A client paced by its clocks' difference flies at the server's pace and
// holds the difference**, whatever the server's pace: every server rate from
// a fifth of real time to real time, in tenths, with no fitted rate to start
// from - the loop alone. Over the last ten of 60 s it flew within 0.002 of
// real time of the server's pace, and the difference stayed within two
// steps of the one held. Unpaced against a server at 0.6, the difference ran
// away by the server's lag - which is what put a client off by metres.
GLIDESLOPE_TEST(a_client_paced_by_its_clocks_difference_flies_at_the_servers_pace_and_holds_it) {
    std::size_t rates = 0;
    for (int tenths = 2; tenths <= 10; ++tenths) {
        const double rate = tenths / 10.0;
        const AtPace p = fly_at_pace(rate, 60.0, true);
        check(std::abs(p.pace - rate) <= 0.002,
              "against a server at " + std::to_string(rate) + " of real time it flew at " +
                  std::to_string(p.pace));
        check(std::llabs(p.worst_late_off) <= 2,
              "against a server at " + std::to_string(rate) +
                  " of real time the clocks' difference moved " +
                  std::to_string(p.worst_late_off) +
                  " steps from the one held in the last ten seconds");
        ++rates;
    }
    // **A server catching up at once is not chased** - its clock jumped,
    // its pace did not change. Held to the difference from before, a client
    // flew at 1.25 to take back 36 steps the server had caught up, and was
    // off by 1.4 m all the while (CI's Windows clang-cl, job 112958941087).
    for (const std::int64_t jump : {36, -36, 120}) {
        const AtPace p = fly_at_pace(1.0, 60.0, true, jump, 30.0);
        check(p.worst_pace_off_after_jump <= 0.05,
              "the server's clock jumping " + std::to_string(jump) +
                  " steps, the client's pace went " +
                  std::to_string(p.worst_pace_off_after_jump) + " from the server's");
        // And what it is held to went with the jump: the difference from it
        // small again, and held there over the last ten seconds.
        check(std::llabs(p.end_off) <= 2 && std::llabs(p.worst_late_off) <= 2,
              "the server's clock jumping " + std::to_string(jump) +
                  " steps, the difference ended " + std::to_string(p.end_off) +
                  " steps from the one held, " + std::to_string(p.worst_late_off) +
                  " at worst in the last ten seconds");
        ++rates;
    }
    // **A server slowed all of a sudden is followed** - a pace, not a jump,
    // however abrupt: from real time to 0.3 of it, the difference falls 84
    // steps a second, more than the 0.5 a pace may part by (`most_parting`).
    // What is past that is taken as a jump and forgiven, the rest steered on;
    // as the pace comes down the parting falls under the half, and from
    // then all of it is steered. Within 20 s the pace is within 0.01 of the
    // server's and the difference within two steps of the one held, and it
    // never went more than 80 steps from it: 17.1 s and 66 steps to 0.3, 10.9 s
    // and 37 to 0.6. **Seen to fail** with `most_parting` 0.05, nearly all of
    // the slowing swallowed as a jump: followed only 33.9 s after.
    for (const double after : {0.3, 0.6}) {
        const AtPace p = fly_at_pace(1.0, 60.0, true, 0, 0.0, after, 20.0);
        check(p.followed_after_s <= 20.0,
              "slowed from real time to " + std::to_string(after) + ", it followed only " +
                  std::to_string(p.followed_after_s) + " s after");
        check(std::llabs(p.worst_off_after_change) <= 80,
              "slowed from real time to " + std::to_string(after) +
                  ", the difference went " + std::to_string(p.worst_off_after_change) +
                  " steps from the one held");
        check(std::abs(p.pace - after) <= 0.002,
              "slowed to " + std::to_string(after) + ", it flew at " + std::to_string(p.pace));
        ++rates;
    }
    check(rates == 14, "nine server rates, three jumps and two slowings were flown against: " +
                           std::to_string(rates));
    const AtPace unpaced = fly_at_pace(0.6, 60.0, false);
    check(unpaced.drifted < -1000,
          "unpaced against a server at 0.6 the clocks' difference fell by " +
              std::to_string(-unpaced.drifted) + " steps, not over a thousand");
}

namespace {

// Flies `inputs` inputs of four steps each, the first numbered `first`, and
// then says of each from the second on that the server applied it `offset`
// steps after this client began it - a word each, as the clocks' difference.
void words_at(Prediction& p, std::uint32_t first, std::uint32_t inputs, std::int64_t offset) {
    Controls c;
    c.throttle = 0.7;
    c.mixture = 1.0;
    const std::uint64_t began_at = p.steps();
    for (std::uint32_t i = 0; i < inputs; ++i) {
        for (int s = 0; s < 4; ++s) {
            p.step(first + i, c);
        }
    }
    for (std::uint32_t i = 1; i < inputs; ++i) {
        const auto began = static_cast<std::int64_t>(began_at + 4 * i);
        p.hear_clock(first + i, 0, static_cast<std::uint64_t>(began + offset));
    }
}

} // namespace

// **What both clients do with a word, in every state the pacing can be in**
// (sim::Pacing::follow - client::Flight::pace_by_clocks and the command
// line's Predicting both call it and nothing else): before the clocks'
// difference is known, with no rate it flies at this machine's pace and with
// one at that rate, never above real time; once known it is held, from the
// rate; a difference fallen since flies it slower; and a prediction begun
// again (`hold`, as a take-over's or take-back's adopt does) is held anew
// from its own first word, at the pace flown then, the rate no longer taken.
// Six states, asserted.
GLIDESLOPE_TEST(a_client_paces_itself_by_its_words_before_and_after_its_clocks_difference_is_known_and_after_starting_again) {
    std::size_t states = 0;
    Aircraft a(data() / "jsbsim", "c172p");
    set_up(a);
    glideslope::sim::Pacing pacing;
    {
        Prediction p(a);
        words_at(p, 1, 10, 500);
        check(!p.settled(), "nine words do not settle it");
        pacing.follow(p, 1.0, std::nullopt);
        check(pacing.pace() == 1.0 && !pacing.holding(),
              "unknown, with no rate: this machine's pace, not held");
        ++states;
        pacing.follow(p, 1.1, 0.6);
        check(pacing.pace() == 0.6 && !pacing.holding(), "unknown, with a rate: at it");
        ++states;
        pacing.follow(p, 1.2, 1.7);
        check(pacing.pace() == 1.0 && !pacing.holding(), "and never above real time");
        ++states;
        pacing.follow(p, 1.25, 0.6);
        words_at(p, 10, 30, 500);
        check(p.settled(), "thirty words more settle it");
        pacing.follow(p, 2.0, 0.9);
        check(pacing.holding() && pacing.off() == 0 && pacing.pace() == 0.9,
              "known: held, from the rate");
        ++states;
        // Every newest word twelve steps less: the server fell behind.
        words_at(p, 40, 12, 488);
        pacing.follow(p, 2.5, 0.9);
        check(pacing.off() == -12 && pacing.pace() < 0.9,
              "fallen twelve steps: " + std::to_string(pacing.off()) + ", flown slower, at " +
                  std::to_string(pacing.pace()));
        ++states;
    }
    const double was = pacing.pace();
    pacing.hold();
    Prediction again(a);
    words_at(again, 1, 30, 9000);
    pacing.follow(again, 3.0, 0.3);
    check(pacing.holding() && pacing.off() == 0 && pacing.pace() == was,
          "begun again: held anew at the pace flown, not the rate");
    ++states;
    check(states == 6, "six states were paced in: " + std::to_string(states));
}

namespace {

// **A predicting client and the server fed the same STOVL inputs**: the
// F-35B at her vertical-landing loading, level at 150 ft and 200 KCAS over
// flat ground in calm air, flown `seconds` on `inputs` (frame, her trimmed
// controls). The server flies each input 100 ms late through its controller
// (sim::Controller, the server's own), and its motion comes back 100 ms
// after that every fifth step; the client predicts on the same inputs and is
// put right by it (sim::Prediction). `stages` is asked after each side's
// step what it has reached, by name.
struct StovlAgreement {
    double worst_m = 0.0;
    double settling_m = 0.0;
    std::size_t heard = 0;
    std::set<std::string> seen;
};

StovlAgreement stovl_agreement(
    const std::function<Controls(int, const Controls&)>& inputs, int seconds,
    const std::function<void(const Aircraft&, const std::string&, std::set<std::string>&)>&
        stages) {
    using glideslope::sim::apply;
    using glideslope::sim::Controller;
    using glideslope::sim::Motion;
    using glideslope::sim::PublishedFigures;
    using glideslope::sim::read_published_figures;
    using glideslope::sim::trimmed_controls;
    const PublishedFigures figures = read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/f35b.xml");
    const auto start = [&](Aircraft& a) {
        a.load(figures.loadings.at("vertical_landing").loading);
        a.set_terrain(flat_ground());
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.heading_deg = 70.0;
        ic.altitude_ft = 150.0;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = 200.0;
        ic.gear = 0.0;
        ic.trim = true;
        a.initialize(ic);
        a.freeze_fuel(true);
    };
    Aircraft server_aircraft(data() / "jsbsim", "f35b");
    Aircraft client_aircraft(data() / "jsbsim", "f35b");
    start(server_aircraft);
    start(client_aircraft);
    const Controls trimmed = trimmed_controls(server_aircraft);
    Controller server(server_aircraft, trimmed);
    Prediction client(client_aircraft);
    const int one_way = 100 * steps_per_second / 2000;
    const int every = 5;
    struct Posted {
        int arrives_at = 0;
        Motion motion;
        std::uint32_t applied = 0;
        std::uint64_t server_steps = 0;
    };
    std::deque<Posted> post;
    std::uint64_t server_steps = 0;
    StovlAgreement out;
    const int frames = seconds * steps_per_second;
    for (int frame = 0; frame < frames; ++frame) {
        client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame, trimmed));
        stages(client_aircraft, "client", out.seen);
        const int theirs = frame - one_way;
        if (theirs >= 0) {
            server.set_pilot(inputs(theirs, trimmed));
            apply(server_aircraft, server);
            server_aircraft.step();
            ++server_steps;
            stages(server_aircraft, "server", out.seen);
            if (theirs % every == 0) {
                post.push_back({frame + one_way, server_aircraft.motion(),
                                static_cast<std::uint32_t>(theirs + 1), server_steps});
            }
        }
        while (!post.empty() && post.front().arrives_at <= frame) {
            const bool settled = client.settled();
            const Prediction::Correction c = client.reconcile(
                post.front().motion, post.front().applied, 1, post.front().server_steps);
            if (settled) {
                out.worst_m = std::max(out.worst_m, c.moved_m);
                ++out.heard;
            } else {
                out.settling_m = std::max(out.settling_m, c.moved_m);
            }
            post.pop_front();
        }
    }
    std::string stages_seen;
    for (const std::string& s : out.seen) {
        stages_seen += " " + s + ";";
    }
    std::printf("  %zu words heard settled, worst correction %.4f m (settling %.4f m);%s\n",
                out.heard, out.worst_m, out.settling_m, stages_seen.c_str());
    return out;
}

bool stovl_hovering(const Aircraft& a) {
    return a.property("fcs/stovl-pos-norm") >= 1.0 && a.property("fcs/nozzle-pos-deg") >= 89.5 &&
           a.state().height_above_ground_ft > 100.0;
}

} // namespace

// **A predicting client and the server agree through her STOVL mode**
// (stovl_agreement, above): the pilot's selector to STOVL at one second, the
// stick held forward from 170 s, flown to 260 s. Both must convert, hover
// and land - each stage seen on both - and once the clocks' difference is
// known no correction may move the client more than the motion test's 0.1
// m. A client that flew her controls raw never converted, and was put right
// by metres at every word.
GLIDESLOPE_TEST(a_predicting_client_and_the_server_fed_the_same_stovl_inputs_agree_through_a_conversion_a_hover_and_a_landing) {
    constexpr double bound_m = 0.1;
    const StovlAgreement r = stovl_agreement(
        [](int frame, const Controls& trimmed) {
            Controls c = trimmed;
            c.stovl = frame >= steps_per_second ? 1.0 : 0.0;
            c.elevator = frame >= 170 * steps_per_second ? -1.0 : 0.0;
            return c;
        },
        260, [](const Aircraft& a, const std::string& side, std::set<std::string>& seen) {
            if (a.property("fcs/stovl-pos-norm") >= 1.0) {
                seen.insert(side + " converted");
            }
            if (stovl_hovering(a) && std::hypot(a.property("velocities/v-north-fps"),
                                                a.property("velocities/v-east-fps")) < 5.0) {
                seen.insert(side + " hovering");
            }
            if (a.property("gear/wow") > 0.5) {
                seen.insert(side + " down");
            }
        });
    check(r.seen.size() == 6, "every stage on both sides, not all");
    check(r.heard > 1000, "the server was heard " + std::to_string(r.heard) + " times");
    check(r.worst_m <= bound_m, "the worst correction was " + std::to_string(r.worst_m) +
                                    " m, over the " + std::to_string(bound_m) + " m bound");
}

// **And while her pilot moves her in STOVL mode** (Transition::Inceptors),
// every way the table has: the selector to STOVL at one second; hovering,
// the lever from where it was to half way between its detent and its
// forward stop at 170 s, to its detent at 190, to its aft stop at 205 and
// its detent at 225; the stick full right from 240 to 255 s and full left
// from 265 to 280; the right pedal from 290 to 298 s and the left from 305
// to 313; the lever at its forward stop from 320 s - past her translational
// rate command's 30 kt, jet-borne - at its aft stop from 345, and at its
// detent from 395; the stick forward from 450 s to land, flown to 540 s.
// Both sides must be seen, in STOVL mode, going ahead, back, right and left
// at more than 10 kt, turning right and left at more than 5 degrees a
// second, jet-borne past 40 kt, and down - sixteen stages, asserted - and
// no correction once settled may move the client more than 0.1 m.
GLIDESLOPE_TEST(a_predicting_client_and_the_server_agree_while_her_pilot_moves_the_f35b_in_the_hover) {
    constexpr double bound_m = 0.1;
    const StovlAgreement r = stovl_agreement(
        [](int frame, const Controls& trimmed) {
            const double s = static_cast<double>(frame) / steps_per_second;
            Controls c = trimmed;
            c.stovl = s >= 1.0 ? 1.0 : 0.0;
            // Her model's detent (fcs/stovl-lever-detent), and half way to
            // the stop past it.
            const double detent = 0.25;
            if (s >= 170.0) {
                c.throttle = s < 190.0   ? detent + 0.5 * (1.0 - detent)
                             : s < 205.0 ? detent
                             : s < 225.0 ? 0.0
                             : s < 320.0 ? detent
                             : s < 345.0 ? 1.0
                             : s < 395.0 ? 0.0
                                         : detent;
            }
            c.aileron = s >= 240.0 && s < 255.0 ? 1.0 : s >= 265.0 && s < 280.0 ? -1.0 : 0.0;
            c.rudder = s >= 290.0 && s < 298.0 ? 1.0 : s >= 305.0 && s < 313.0 ? -1.0 : 0.0;
            c.elevator = s >= 450.0 ? -1.0 : 0.0;
            return c;
        },
        540, [](const Aircraft& a, const std::string& side, std::set<std::string>& seen) {
            const double kt = 3600.0 / 6076.12;
            const double n = a.property("velocities/v-north-fps") * kt;
            const double e = a.property("velocities/v-east-fps") * kt;
            if (a.property("fcs/stovl-pos-norm") >= 1.0 && std::hypot(n, e) > 40.0) {
                seen.insert(side + " jet-borne");
            }
            if (stovl_hovering(a)) {
                const double psi = a.state().heading_deg * 3.14159265358979323846 / 180.0;
                const double ahead = n * std::cos(psi) + e * std::sin(psi);
                const double right = -n * std::sin(psi) + e * std::cos(psi);
                const double turn = a.state().r_radps * 180.0 / 3.14159265358979323846;
                if (ahead > 10.0) {
                    seen.insert(side + " ahead");
                }
                if (ahead < -10.0) {
                    seen.insert(side + " back");
                }
                if (right > 10.0) {
                    seen.insert(side + " right");
                }
                if (right < -10.0) {
                    seen.insert(side + " left");
                }
                if (turn > 5.0) {
                    seen.insert(side + " turning right");
                }
                if (turn < -5.0) {
                    seen.insert(side + " turning left");
                }
            }
            if (a.property("gear/wow") > 0.5) {
                seen.insert(side + " down");
            }
        });
    const Aircraft probe(data() / "jsbsim", "f35b");
    check(std::abs(probe.property("fcs/stovl-lever-detent") - 0.25) < 1e-9,
          "her lever's detent is the 0.25 flown here");
    check(r.seen.size() == 16, std::to_string(r.seen.size()) + " of the 16 stages seen");
    check(r.heard > 1000, "the server was heard " + std::to_string(r.heard) + " times");
    check(r.worst_m <= bound_m, "the worst correction was " + std::to_string(r.worst_m) +
                                    " m, over the " + std::to_string(bound_m) + " m bound");
}

// **A prediction put right at every word burns the fuel the server does**:
// the F-35B trimmed level at 20,000 ft and 300 kt, her fuel burning, flown a
// minute by the server and by a client predicting her on the same inputs,
// the server's motion coming back every fifth step 100 ms each way - each
// word flying again the steps since, 25 of them - as stovl_agreement's
// does. Her weight must agree within a pound: both burned 89.9 lb,
// measured (linux-release and linux-debug, 2026-10-14). Without the tanks
// in the replay's state (sim::ReplayState) each step's fuel was burned
// again by every word that flew it again, and the prediction burned 305.6
// lb to the server's 89.9.
GLIDESLOPE_TEST(a_prediction_put_right_at_every_word_burns_the_fuel_the_server_does) {
    using glideslope::sim::apply;
    using glideslope::sim::Controller;
    using glideslope::sim::Motion;
    const auto start = [&](Aircraft& a) {
        a.set_terrain(flat_ground());
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.heading_deg = 270.0;
        ic.altitude_ft = 20000.0;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = 300.0;
        ic.gear = 0.0;
        ic.engine_running = true;
        ic.trim = true;
        a.initialize(ic);
    };
    Aircraft server_aircraft(data() / "jsbsim", "f35b");
    Aircraft client_aircraft(data() / "jsbsim", "f35b");
    start(server_aircraft);
    start(client_aircraft);
    // Trimmed level, as she is flown: left alone at full power she dives,
    // and into the ground inside the minute from 20,000 ft at 850 kt.
    const Controls trimmed = glideslope::sim::trimmed_controls(server_aircraft);
    Controller server(server_aircraft, trimmed);
    Prediction client(client_aircraft);
    const int one_way = 100 * steps_per_second / 2000;
    const int every = 5;
    struct Posted {
        int arrives_at = 0;
        Motion motion;
        std::uint32_t applied = 0;
        std::uint64_t server_steps = 0;
    };
    std::deque<Posted> post;
    std::uint64_t server_steps = 0;
    std::size_t heard = 0;
    const double full_lbs = server_aircraft.property("inertia/weight-lbs");
    const int frames = 60 * steps_per_second;
    for (int frame = 0; frame < frames + one_way; ++frame) {
        if (frame < frames) {
            client.step(static_cast<std::uint32_t>(frame + 1), trimmed);
        }
        const int theirs = frame - one_way;
        if (theirs >= 0 && theirs < frames) {
            server.set_pilot(trimmed);
            apply(server_aircraft, server);
            server_aircraft.step();
            ++server_steps;
            if (theirs % every == 0) {
                post.push_back({frame + one_way, server_aircraft.motion(),
                                static_cast<std::uint32_t>(theirs + 1), server_steps});
            }
        }
        while (!post.empty() && post.front().arrives_at <= frame) {
            (void)client.reconcile(post.front().motion, post.front().applied, 1,
                                   post.front().server_steps);
            ++heard;
            post.pop_front();
        }
    }
    const double server_lbs = server_aircraft.property("inertia/weight-lbs");
    const double client_lbs = client_aircraft.property("inertia/weight-lbs");
    std::printf("  %zu words heard; burned %.1f lb on the server, %.1f lb in the prediction; "
                "%.0f ft above the ground at the end\n",
                heard, full_lbs - server_lbs, full_lbs - client_lbs,
                server_aircraft.state().height_above_ground_ft);
    check(heard > 1000, "the server was heard " + std::to_string(heard) + " times");
    // Flown clear of the ground, which the stick left alone dives her
    // towards.
    check(server_aircraft.state().height_above_ground_ft > 15000.0,
          "she came down to " + std::to_string(server_aircraft.state().height_above_ground_ft) +
              " ft above the ground");
    check(full_lbs - server_lbs > 50.0,
          "the server burned " + std::to_string(full_lbs - server_lbs) + " lb, not over 50");
    check(std::abs(client_lbs - server_lbs) < 1.0,
          "the prediction weighs " + std::to_string(client_lbs - server_lbs) +
              " lb more than the server, not within a pound");
}

// **Put back to its own state, a prediction flies again to exactly where it
// was**, through her STOVL mode: the F-35B flown as above, and at each of
// four moments - slowing wing-borne for the conversion (60 s), jet-borne
// (115 s), hovering (165 s) and letting down (185 s) - its whole state is
// taken, a second more is flown, and it is put back to that state and flown
// through the same second again. Where it ends must be within a tenth of a
// millimetre of where it was - JSBSim's own state taken and put back is not
// bitwise, and moved it 0.3 to 11 micrometres - with the lift fan's and the
// nozzle's commands within a thousandth: the transition law, its autopilot,
// its hover law and their integrals are put back with it
// (Controller::PilotsLaws). Left as the newest step had them, she was flown
// again to 0.5 mm to 23 cm from where she had been, the fan's command 0.02
// off.
GLIDESLOPE_TEST(a_stovl_prediction_put_back_to_its_own_state_flies_again_to_exactly_where_it_was) {
    using glideslope::sim::PublishedFigures;
    using glideslope::sim::read_published_figures;
    using glideslope::sim::trimmed_controls;
    const PublishedFigures figures = read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/f35b.xml");
    Aircraft a(data() / "jsbsim", "f35b");
    a.load(figures.loadings.at("vertical_landing").loading);
    a.set_terrain(flat_ground());
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    ic.altitude_ft = 150.0;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 200.0;
    ic.gear = 0.0;
    ic.trim = true;
    a.initialize(ic);
    a.freeze_fuel(true);
    const Controls trimmed = trimmed_controls(a);
    const auto inputs = [&](int frame) {
        Controls c = trimmed;
        c.stovl = frame >= steps_per_second ? 1.0 : 0.0;
        c.elevator = frame >= 170 * steps_per_second ? -1.0 : 0.0;
        return c;
    };
    Prediction client(a);
    check(client.laws() != nullptr, "her prediction flies her through a controller");
    const std::vector<int> moments{60, 115, 165, 185};
    std::size_t checked = 0;
    int frame = 0;
    for (const int at_s : moments) {
        for (; frame < at_s * steps_per_second; ++frame) {
            client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
        }
        const AircraftSnapshot then = a.capture();
        const auto then_sequence = static_cast<std::uint32_t>(frame);
        for (int i = 0; i < steps_per_second; ++i, ++frame) {
            client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
        }
        const double nozzle_was = a.property("fcs/nozzle-cmd-deg");
        const double fan_was = a.property("fcs/lift-fan-cmd-norm");
        const Prediction::Correction c = client.reconcile(then, then_sequence);
        std::printf("  at %d s: flew %zu again, moved %.9f m, nozzle %.4f, fan %.6f\n", at_s,
                    c.replayed, c.moved_m, a.property("fcs/nozzle-cmd-deg"),
                    a.property("fcs/lift-fan-cmd-norm"));
        check(c.replayed == static_cast<std::size_t>(steps_per_second),
              "a second flown again at " + std::to_string(at_s) + " s");
        check(c.moved_m < 1e-4, "at " + std::to_string(at_s) + " s it moved " +
                                    std::to_string(c.moved_m) + " m flown again");
        check(std::abs(a.property("fcs/nozzle-cmd-deg") - nozzle_was) < 1e-3 &&
                  std::abs(a.property("fcs/lift-fan-cmd-norm") - fan_was) < 1e-3,
              "and her lift system's commands are the same at " + std::to_string(at_s) + " s");
        ++checked;
    }
    check(checked == moments.size() && checked == 4, "all four moments were checked");
    check(client.laws()->transition() != nullptr, "she was still in STOVL mode");
}

// **Each step held is kept as the newest replay flew it**
// (Prediction::fly_held_again): her controller with it. Three F-35Bs on the
// same inputs, the selector to STOVL at one second: the prediction `a`
// flies 61 s; a server's word on 60 s - from an aircraft flown from 2 ft
// higher - puts it right and it flies the second again; a second word, on
// 60.5 s, is where a twin `c`, flown to 60.5 s and put right by the same
// first word, was. That second word says what `a`'s own replay flew, so
// put back to it with her controller as that replay had it, `a` flies to
// where it already is. Kept as its first pass flew it - from 2 ft lower -
// the controller put back was not the one the motion came with.
GLIDESLOPE_TEST(a_prediction_put_right_twice_flies_the_second_word_from_the_controller_its_first_replay_left) {
    using glideslope::sim::PublishedFigures;
    using glideslope::sim::read_published_figures;
    using glideslope::sim::trimmed_controls;
    const PublishedFigures figures = read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/f35b.xml");
    const auto start = [&](Aircraft& x, double altitude_ft) {
        x.load(figures.loadings.at("vertical_landing").loading);
        x.set_terrain(flat_ground());
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.heading_deg = 70.0;
        ic.altitude_ft = altitude_ft;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = 200.0;
        ic.gear = 0.0;
        ic.trim = true;
        x.initialize(ic);
        x.freeze_fuel(true);
    };
    Aircraft a(data() / "jsbsim", "f35b");
    Aircraft c(data() / "jsbsim", "f35b");
    Aircraft s(data() / "jsbsim", "f35b");
    start(a, 150.0);
    start(c, 150.0);
    start(s, 152.0);
    const Controls trimmed = trimmed_controls(a);
    const auto inputs = [&](int frame) {
        Controls k = trimmed;
        k.stovl = frame >= steps_per_second ? 1.0 : 0.0;
        return k;
    };
    Prediction pa(a);
    Prediction pc(c);
    Prediction ps(s);
    const int first = 60 * steps_per_second;
    const int second = first + steps_per_second / 2;
    const int last = first + steps_per_second;
    for (int frame = 0; frame < last; ++frame) {
        pa.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
        if (frame < second) {
            pc.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
        }
        if (frame < first) {
            ps.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
        }
    }
    const AircraftSnapshot first_word = s.capture();
    const Prediction::Correction once = pa.reconcile(first_word, static_cast<std::uint32_t>(first));
    (void)pc.reconcile(first_word, static_cast<std::uint32_t>(first));
    const AircraftSnapshot second_word = c.capture();
    const Prediction::Correction twice =
        pa.reconcile(second_word, static_cast<std::uint32_t>(second));
    std::printf("  the first word moved her %.6f m, flying %zu again; the second %.9f m, "
                "flying %zu again\n",
                once.moved_m, once.replayed, twice.moved_m, twice.replayed);
    check(pa.laws() != nullptr && pa.laws()->transition() != nullptr, "she was in STOVL mode");
    check(once.moved_m > 0.1, "the first word moved her " + std::to_string(once.moved_m) +
                                  " m, not over 0.1: it says nothing new");
    check(once.replayed == static_cast<std::size_t>(steps_per_second) &&
              twice.replayed == static_cast<std::size_t>(steps_per_second / 2),
          "a second, then half of one, flown again");
    check(twice.moved_m < 1e-4, "the second word moved her " + std::to_string(twice.moved_m) +
                                    " m flown again from her own replay");
}

// **And moved by her pilot in the hover** (Transition::Inceptors): flown as
// `a_predicting_client_and_the_server_agree_while_her_pilot_moves_the_f35b_in_the_hover`
// flies her, put back half a second before the lever first moves (169.5 s:
// the lever not yet hers, which the put-back must keep - Controller::
// PilotsLaws's lever_at_selection), going ahead (180 s), going right (225
// s) and turning (250 s), and flown again within a tenth of a millimetre,
// her lift system's commands within a thousandth.
GLIDESLOPE_TEST(a_stovl_prediction_moved_by_her_pilot_in_the_hover_put_back_flies_again_to_exactly_where_it_was) {
    using glideslope::sim::PublishedFigures;
    using glideslope::sim::read_published_figures;
    using glideslope::sim::trimmed_controls;
    const PublishedFigures figures = read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/f35b.xml");
    Aircraft a(data() / "jsbsim", "f35b");
    a.load(figures.loadings.at("vertical_landing").loading);
    a.set_terrain(flat_ground());
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 70.0;
    ic.altitude_ft = 150.0;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 200.0;
    ic.gear = 0.0;
    ic.trim = true;
    a.initialize(ic);
    a.freeze_fuel(true);
    const Controls trimmed = trimmed_controls(a);
    const auto inputs = [&](int frame) {
        Controls c = trimmed;
        const double s = static_cast<double>(frame) / steps_per_second;
        c.stovl = s >= 1.0 ? 1.0 : 0.0;
        if (s >= 170.0) {
            c.throttle = s < 195.0 ? 0.25 + 0.5 * 0.75 : 0.25;
        }
        c.aileron = s >= 215.0 && s < 235.0 ? 1.0 : 0.0;
        c.rudder = s >= 245.0 && s < 255.0 ? 1.0 : 0.0;
        return c;
    };
    Prediction client(a);
    check(client.laws() != nullptr, "her prediction flies her through a controller");
    const std::vector<double> moments{169.5, 180.0, 225.0, 250.0};
    std::size_t checked = 0;
    int frame = 0;
    for (const double at_s : moments) {
        for (; frame < static_cast<int>(at_s * steps_per_second); ++frame) {
            client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
        }
        const AircraftSnapshot then = a.capture();
        const auto then_sequence = static_cast<std::uint32_t>(frame);
        for (int i = 0; i < steps_per_second; ++i, ++frame) {
            client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
        }
        const double nozzle_was = a.property("fcs/nozzle-cmd-deg");
        const double fan_was = a.property("fcs/lift-fan-cmd-norm");
        const Prediction::Correction c = client.reconcile(then, then_sequence);
        std::printf("  at %.1f s: flew %zu again, moved %.9f m, nozzle %.4f, fan %.6f\n", at_s,
                    c.replayed, c.moved_m, a.property("fcs/nozzle-cmd-deg"),
                    a.property("fcs/lift-fan-cmd-norm"));
        check(c.replayed == static_cast<std::size_t>(steps_per_second),
              "a second flown again at " + std::to_string(at_s) + " s");
        check(c.moved_m < 1e-4, "at " + std::to_string(at_s) + " s it moved " +
                                    std::to_string(c.moved_m) + " m flown again");
        check(std::abs(a.property("fcs/nozzle-cmd-deg") - nozzle_was) < 1e-3 &&
                  std::abs(a.property("fcs/lift-fan-cmd-norm") - fan_was) < 1e-3,
              "and her lift system's commands are the same at " + std::to_string(at_s) + " s");
        ++checked;
    }
    check(checked == moments.size() && checked == 4, "all four moments were checked");
    check(client.laws()->transition() != nullptr, "she was still in STOVL mode");
}



// **A wreck the server holds is held by the prediction too, nothing in her
// goes to a NaN, and she flies again from where the server starts her.** An
// F-35B at her model's own weight, 600 ft and 300 KCAS over flat ground, the
// stick full forward, is flown on a server - an aircraft stepped and judged
// as the server steps and judges it - and in a prediction on the same
// inputs, four steps an input, each server word heard three inputs (100 ms)
// late, as through the test's relay. **As the server does**: once its judge
// calls her a wreck it steps her no further for five seconds (`wreck_s`),
// though it still takes each input - its word's `last_applied` goes on
// rising, so the inputs the prediction holds stay a word's delay long - and
// then flies her again from her start (2,200 ft, 300 KCAS, level), the
// word saying she is no longer a wreck. Meanwhile the pilot selects STOVL
// mode, the stick still forward, so that in the prediction her transition
// law flies her; after the wreck, untouched (0.6, the stick centred,
// STOVL mode off). What holds: every number in her property tree finite at
// every step; while a wreck, within a millimetre of where the server holds
// her; at every word, no more than 16 steps flown again (the three inputs'
// delay and the one in flight); and from the word that she flies again,
// every step within 5 cm of the server's at the same step, and within 5 mm
// in the fifth second (2.5 cm and 1.4 mm, measured). The 5 cm is what is
// not the motion: her engine and actuators, carried here through the wreck
// where the server started her afresh, which no word puts right, and the
// difference flies out as both settle. **Seen red** without the wreck
// heard: 7.0 m from where the server holds her. Not a NaN here: with
// `last_applied` rising, as the server's does, each word flies 12 steps
// again at most. Held at the wreck's input instead - every input since
// flown again at every word, through the ground - her whole state went to
// a NaN at the 125th word, which is the hold's other half.
GLIDESLOPE_TEST(a_prediction_told_its_aircraft_is_a_wreck_holds_her_where_the_server_does_and_never_goes_to_a_nan) {
    using glideslope::sim::AircraftState;
    using glideslope::sim::Motion;
    using glideslope::sim::steps_per_second;
    InitialConditions ic;
    ic.latitude_deg = -33.9461;
    ic.longitude_deg = 151.1772;
    ic.heading_deg = 270.0;
    ic.altitude_ft = 600.0;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 300.0;
    ic.gear = 0.0;
    ic.engine_running = true;
    InitialConditions again = ic;
    again.altitude_ft = 2200.0;
    Aircraft server(data() / "jsbsim", "f35b");
    Aircraft client(data() / "jsbsim", "f35b");
    server.set_terrain(flat_ground());
    client.set_terrain(flat_ground());
    server.initialize(ic);
    client.initialize(ic);
    Prediction p(client);
    glideslope::sim::GroundJudge judge(false);
    struct Word {
        Motion motion;
        std::uint32_t applied = 0;
        std::size_t into = 0;
        std::uint64_t steps = 0;
        bool wrecked = false;
    };
    constexpr std::size_t delay_inputs = 3;
    constexpr std::size_t most_replayed = (delay_inputs + 1) * 4;
    std::deque<Word> on_the_way;
    std::vector<AircraftState> server_at; // the server's state after each step
    std::optional<std::uint64_t> wrecked_at;
    std::optional<std::uint64_t> again_at;
    std::uint64_t steps = 0;
    std::uint32_t sequence = 0;
    int not_finite = 0;
    std::string first_not_finite;
    double furthest_held_m = 0.0;
    double furthest_again_m = 0.0;
    double furthest_last_m = 0.0;
    std::size_t longest_replay = 0;
    int words_again = 0;
    const std::vector<std::string> names = client.number_names();
    const std::uint64_t wreck_steps = 5 * steps_per_second;
    while (!again_at || steps < *again_at + 5 * steps_per_second) {
        ++sequence;
        Controls k;
        k.throttle = 0.6;
        if (!again_at) {
            k.elevator = -1.0;
            k.stovl = wrecked_at ? 1.0 : 0.0;
        }
        for (int i = 0; i < 4; ++i) {
            // The server's step: frozen while a wreck, flown again after.
            if (wrecked_at && !again_at && steps >= *wrecked_at + wreck_steps) {
                server.initialize(again);
                judge.reset();
                again_at = steps;
            }
            const bool held = wrecked_at && !again_at;
            if (!held) {
                server.set_controls(k);
                server.step();
                if (!wrecked_at && judge.judge(server)) {
                    wrecked_at = steps + 1;
                }
            }
            ++steps;
            server_at.push_back(server.state());
            // The client's step, held against the server's at that step
            // once she flies again.
            p.step(sequence, k);
            const std::vector<double> numbers = client.numbers();
            for (std::size_t n = 0; n < numbers.size(); ++n) {
                if (!std::isfinite(numbers[n])) {
                    if (not_finite == 0) {
                        first_not_finite = names[n] + " at step " + std::to_string(steps);
                    }
                    ++not_finite;
                    break;
                }
            }
            if (words_again > 0) {
                const double apart_m =
                    glideslope::sim::how_far_apart_m(server_at.back(), client.state());
                furthest_again_m = std::max(furthest_again_m, apart_m);
                if (steps + steps_per_second >= *again_at + 5 * steps_per_second) {
                    furthest_last_m = std::max(furthest_last_m, apart_m);
                }
            }
        }
        // Four steps into the input it applied: the clocks' difference nothing.
        on_the_way.push_back({server.motion(), sequence, 4, steps, wrecked_at && !again_at});
        if (on_the_way.size() > delay_inputs) {
            const Word w = on_the_way.front();
            on_the_way.pop_front();
            p.hear_wrecked(w.wrecked);
            const Prediction::Correction c = p.reconcile(w.motion, w.applied, w.into, w.steps);
            longest_replay = std::max(longest_replay, c.replayed);
            if (w.wrecked) {
                furthest_held_m = std::max(furthest_held_m, glideslope::sim::how_far_apart_m(
                                                                server_at[w.steps - 1],
                                                                client.state()));
            } else if (again_at && w.steps > *again_at) {
                ++words_again;
            }
        }
        check(steps < 120 * steps_per_second, "the flight never ended");
    }
    check(wrecked_at.has_value(), "she was never a wreck");
    std::printf("  a wreck at step %llu, flown again at %llu; %llu steps in all: %d with a number "
                "not finite%s; held at most %.6f m from the server's wreck; at most %zu steps "
                "flown again at a word (bound %zu); flying again, at most %.6f m from the "
                "server over %d words, %.6f m in the last second\n",
                static_cast<unsigned long long>(*wrecked_at),
                static_cast<unsigned long long>(*again_at), static_cast<unsigned long long>(steps),
                not_finite, not_finite > 0 ? (", first " + first_not_finite).c_str() : "",
                furthest_held_m, longest_replay, most_replayed, furthest_again_m, words_again,
                furthest_last_m);
    check(not_finite == 0, std::to_string(not_finite) + " steps with a number not finite, the "
                                                        "first " + first_not_finite);
    check(furthest_held_m < 0.001, "held " + std::to_string(furthest_held_m) +
                                       " m from where the server holds her");
    check(longest_replay <= most_replayed,
          std::to_string(longest_replay) + " steps flown again at one word");
    check(words_again >= 100, std::to_string(words_again) + " words heard flying again, not 100");
    check(furthest_again_m < 0.05, "flying again she was " + std::to_string(furthest_again_m) +
                                       " m from the server");
    check(furthest_last_m < 0.005, "flying again, in the last second she was " +
                                       std::to_string(furthest_last_m) + " m from the server");
}

// **A prediction that came down on water ahead of the server's word flies
// again when the server does, and never goes to a NaN.** The CI failure of
// run 39883628869: a client predicting a Cessna whose engine the server had
// stopped glided into Sydney Harbour, was a wreck on water, flew again - and
// at the second wreck and flight its JSBSim asserted at FGTable.cpp:618 on a
// NaN. Built here: a Cessna at her model's own weight, 300 ft and 90 KCAS over
// water, her engine stopped on the server and heard stopped by the
// prediction, the stick a third forward, flown on a server as the server
// steps and judges her and in a prediction on the same inputs, four steps an
// input, each word heard three inputs (100 ms) late. The prediction, ahead,
// meets the water before it hears the wreck. Five seconds a wreck, then
// started again at 2,200 ft as the server starts her, its word saying she is
// no longer one and her engine runs; flown on, untouched, for ten seconds -
// **twice**, as in the run. What holds: no number in her property tree a
// NaN at any step (her engine's air-fuel ratio is JSBSim's infinity with no
// fuel flowing, which is not one); through the ten seconds after each start,
// at every word (290 or more each, counted), not held on the water's surface
// (`ditched`); and at every step of the last second of each flight within
// 5 cm of the server's at the same step (6.5 mm, measured). **Seen red**
// without the hold-down released (Aircraft::set_motion): held on the water
// at all 593 words of both flights, flying on her velocity alone, 3.7 cm
// off in the last seconds. No NaN came of it here, nor in the run itself
// flown locally (docs/status, 2026-10-23, a prediction come down on water).
GLIDESLOPE_TEST(a_prediction_come_down_on_water_flies_again_when_the_server_does_and_never_goes_to_a_nan) {
    using glideslope::sim::AircraftState;
    using glideslope::sim::Motion;
    using glideslope::sim::steps_per_second;
    const auto water = std::make_shared<glideslope::sim::FunctionTerrain>(
        [](double, double) { return 0.0; }, [](double, double) { return true; });
    InitialConditions ic;
    ic.latitude_deg = -33.85;
    ic.longitude_deg = 151.24;
    ic.heading_deg = 270.0;
    ic.altitude_ft = 300.0;
    ic.terrain_elevation_ft = 0.0;
    ic.airspeed_kts = 90.0;
    ic.engine_running = true;
    InitialConditions again = ic;
    again.altitude_ft = 2200.0;
    Aircraft server(data() / "jsbsim", "c172p");
    Aircraft client(data() / "jsbsim", "c172p");
    server.set_terrain(water);
    client.set_terrain(water);
    server.initialize(ic);
    client.initialize(ic);
    server.fail_engine(0, false);
    Prediction p(client);
    glideslope::sim::GroundJudge judge(false);
    struct Word {
        Motion motion;
        std::uint32_t applied = 0;
        std::size_t into = 0;
        std::uint64_t steps = 0;
        bool wrecked = false;
        bool engine_stopped = false;
    };
    constexpr std::size_t delay_inputs = 3;
    constexpr int flights = 2;
    std::deque<Word> on_the_way;
    std::vector<AircraftState> server_at; // the server's state after each step
    std::optional<std::uint64_t> wrecked_at;
    std::optional<std::uint64_t> again_at;
    int wrecks = 0;
    int flown_again = 0;
    std::uint64_t steps = 0;
    std::uint32_t sequence = 0;
    int not_finite = 0;
    std::string first_not_finite;
    int ditched_flying = 0;
    int words_flying = 0;
    double furthest_last_m = 0.0;
    int last_second_steps = 0;
    bool client_ditched_first = false;
    const std::vector<std::string> names = client.number_names();
    const std::uint64_t wreck_steps = 5 * steps_per_second;
    const std::uint64_t flight_steps = 10 * steps_per_second;
    while (flown_again < flights || steps < *again_at + flight_steps) {
        ++sequence;
        Controls k;
        k.throttle = 0.6;
        // Untouched for ten seconds after each start again, and otherwise
        // into the water, until she has been flown again `flights` times.
        const bool settling = again_at && steps < *again_at + flight_steps;
        k.elevator = flown_again < flights && !settling && !wrecked_at ? -0.35 : 0.0;
        for (int i = 0; i < 4; ++i) {
            // The server's step: frozen while a wreck, flown again after.
            if (wrecked_at && steps >= *wrecked_at + wreck_steps) {
                server.initialize(again);
                judge.reset();
                again_at = steps;
                wrecked_at.reset();
                ++flown_again;
            }
            if (!wrecked_at) {
                server.set_controls(k);
                server.step();
                if (judge.judge(server)) {
                    wrecked_at = steps + 1;
                    ++wrecks;
                }
            }
            ++steps;
            server_at.push_back(server.state());
            p.step(sequence, k);
            if (client.state().ditched && !p.wrecked() && flown_again == 0) {
                client_ditched_first = true;
            }
            const std::vector<double> numbers = client.numbers();
            for (std::size_t n = 0; n < numbers.size(); ++n) {
                if (std::isnan(numbers[n])) {
                    if (not_finite == 0) {
                        first_not_finite = names[n] + " at step " + std::to_string(steps);
                    }
                    ++not_finite;
                    break;
                }
            }
            if (again_at && !wrecked_at && steps + steps_per_second >= *again_at + flight_steps &&
                steps < *again_at + flight_steps) {
                furthest_last_m = std::max(furthest_last_m, glideslope::sim::how_far_apart_m(
                                                                server_at.back(), client.state()));
                ++last_second_steps;
            }
        }
        on_the_way.push_back({server.motion(), sequence, 4, steps, wrecked_at.has_value(),
                              !server.engine_running(0)});
        if (on_the_way.size() > delay_inputs) {
            const Word w = on_the_way.front();
            on_the_way.pop_front();
            (void)p.hear_engine_stopped(w.engine_stopped ? std::optional<int>(0) : std::nullopt);
            p.hear_wrecked(w.wrecked);
            (void)p.reconcile(w.motion, w.applied, w.into, w.steps);
            if (!w.wrecked && again_at && w.steps > *again_at + 8 &&
                w.steps < *again_at + flight_steps) {
                ++words_flying;
                if (client.state().ditched) {
                    ++ditched_flying;
                }
            }
        }
        check(steps < 120 * steps_per_second, "the flight never ended");
        if (steps >= 120 * steps_per_second) {
            break;
        }
    }
    std::printf("  %d wrecks, flown again %d times, %llu steps in all; the prediction on the "
                "water before the server's word: %s; %d steps with a number not finite%s; "
                "%d of %d words flying again with her held on the water; in the last second of "
                "each flight at most %.6f m from the server over %d steps\n",
                wrecks, flown_again, static_cast<unsigned long long>(steps),
                client_ditched_first ? "yes" : "no", not_finite,
                not_finite > 0 ? (", first " + first_not_finite).c_str() : "", ditched_flying,
                words_flying, furthest_last_m, last_second_steps);
    check(wrecks == flights && flown_again == flights,
          std::to_string(wrecks) + " wrecks and " + std::to_string(flown_again) +
              " flights again, not " + std::to_string(flights) + " of each");
    check(client_ditched_first, "the prediction never came down on the water before the "
                                "server's word - the situation was not built");
    check(not_finite == 0, std::to_string(not_finite) + " steps with a number not finite, the "
                                                        "first " + first_not_finite);
    check(words_flying >= flights * 290, std::to_string(words_flying) + " words heard flying again");
    check(ditched_flying == 0, std::to_string(ditched_flying) +
                                   " words flying again with the prediction held on the water");
    check(last_second_steps == flights * steps_per_second,
          std::to_string(last_second_steps) + " steps compared in the last seconds");
    check(furthest_last_m < 0.05, "flying again, in the last second she was " +
                                      std::to_string(furthest_last_m) + " m from the server");
}

// **A prediction whose STOVL law was seeded off is put right to the server's
// height asked at its next word, and then agrees.** Each end seeds the law's
// height from her height above the ground as it sees her at the selector,
// and the stick moves both alike from there, so a prediction off at that
// moment held the difference for as long as the law flew her - through a
// server a steady 108 mm at the median (PROJECT_STATUS.md, 2026-10-15). Built
// here, not hoped for: the F-35B at her model's own 45,400 lb, too heavy to
// hover and so held wing-borne, trimmed level at 1,000 ft and 200 KCAS over
// flat ground; the client's begun 2 m above the server's, and again 2 m
// below - both signs, counted - and told nothing until both have selected
// STOVL mode, at one second, so its law is seeded off by 6.6 ft; then the
// stick half forward, as the multi-process test's pilot holds it, for a
// minute. The server's motion comes back every fifth step 100 ms each way,
// with the height asked rounded as the wire rounds it (an `f32`), as
// stovl_agreement's does. Every word heard has a law flying her on both
// sides - asserted, so none is passed for want of one - and from the first,
// each correction once the clocks' difference is known must be within a
// millimetre, and at the end her height asked in the prediction the
// server's within a thousandth of a foot (the server's flown on to the same
// step). Measured 0.0000 m and 0.0000 ft for both signs. Seen red without
// the height told: the heights asked 6.56 ft apart at the end, and
// corrections up to 5.2 mm, the law pulling her back to its own height
// between every word.
GLIDESLOPE_TEST(a_prediction_whose_stovl_law_was_seeded_off_is_put_right_to_the_servers_height_asked_and_then_agrees) {
    using glideslope::sim::apply;
    using glideslope::sim::Controller;
    using glideslope::sim::Motion;
    std::size_t offsets_flown = 0;
    for (const double off_m : {2.0, -2.0}) {
        const auto start = [&](Aircraft& a, double feet) {
            a.set_terrain(flat_ground());
            InitialConditions ic;
            ic.latitude_deg = -33.9461;
            ic.longitude_deg = 151.1772;
            ic.heading_deg = 270.0;
            ic.altitude_ft = feet;
            ic.terrain_elevation_ft = 0.0;
            ic.airspeed_kts = 200.0;
            ic.gear = 0.0;
            ic.engine_running = true;
            ic.trim = true;
            a.initialize(ic);
            a.freeze_fuel(true);
        };
        Aircraft server_aircraft(data() / "jsbsim", "f35b");
        Aircraft client_aircraft(data() / "jsbsim", "f35b");
        start(server_aircraft, 1000.0);
        start(client_aircraft, 1000.0 + off_m / 0.3048);
        const Controls trimmed = glideslope::sim::trimmed_controls(server_aircraft);
        Controller server(server_aircraft, trimmed);
        Prediction client(client_aircraft);
        check(client.laws() != nullptr, "her prediction flies her through a controller");
        const int one_way = 100 * steps_per_second / 2000;
        const int every = 5;
        const int selected_from = steps_per_second;
        const auto inputs = [&](int frame) {
            Controls c = trimmed;
            if (frame >= selected_from) {
                c.stovl = 1.0;
                c.elevator = -0.5;
            }
            return c;
        };
        struct Posted {
            int arrives_at = 0;
            Motion motion;
            std::uint32_t applied = 0;
            std::uint64_t server_steps = 0;
            std::optional<double> height_asked_ft;
        };
        std::deque<Posted> post;
        std::uint64_t server_steps = 0;
        std::size_t heard = 0;
        std::size_t both_flown = 0;
        std::size_t judged = 0;
        double worst_asked_ft = 0.0;
        double worst_m = 0.0;
        double seeded_off_ft = 0.0;
        const int frames = 61 * steps_per_second;
        for (int frame = 0; frame < frames; ++frame) {
            client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
            if (frame == selected_from && client.laws()->transition()) {
                seeded_off_ft = client.laws()->stovl_height_asked_ft();
            }
            const int theirs = frame - one_way;
            if (theirs >= 0) {
                server.set_pilot(inputs(theirs));
                apply(server_aircraft, server);
                server_aircraft.step();
                ++server_steps;
                if (theirs == selected_from && server.transition()) {
                    seeded_off_ft -= server.stovl_height_asked_ft();
                }
                // **Told nothing until both have selected**, a word's worth
                // after: the prediction's law is seeded from its own height.
                if (theirs % every == 0 && theirs > selected_from + every) {
                    std::optional<double> asked;
                    if (server.transition()) {
                        asked = static_cast<double>(
                            static_cast<float>(server.stovl_height_asked_ft()));
                    }
                    post.push_back({frame + one_way, server_aircraft.motion(),
                                    static_cast<std::uint32_t>(theirs + 1), server_steps, asked});
                }
            }
            while (!post.empty() && post.front().arrives_at <= frame) {
                const Posted& p = post.front();
                const bool settled = client.settled();
                const Prediction::Correction c =
                    client.reconcile(p.motion, p.applied, 1, p.server_steps, p.height_asked_ft);
                ++heard;
                if (p.height_asked_ft && client.laws()->transition()) {
                    ++both_flown;
                }
                if (settled) {
                    worst_m = std::max(worst_m, c.moved_m);
                    ++judged;
                }
                post.pop_front();
            }
        }
        // The two laws' heights asked at the end, each at its own newest
        // step: the server's is `one_way` steps behind, at the stick's 3
        // ft/s.
        const double sink_fps = 0.5 * Controller::stick_sink_fps;
        const double behind_ft =
            sink_fps * static_cast<double>(one_way) / static_cast<double>(steps_per_second);
        const double apart_ft = client.laws()->stovl_height_asked_ft() -
                                (server.stovl_height_asked_ft() - behind_ft);
        worst_asked_ft = std::abs(apart_ft);
        std::printf("  begun %+.0f m off: seeded %+.2f ft off; %zu words heard, %zu with a law "
                    "on both sides; at the end the heights asked %.4f ft apart; worst "
                    "correction once known %.4f m over %zu words; %.0f ft above the ground\n",
                    off_m, seeded_off_ft, heard, both_flown, apart_ft, worst_m, judged,
                    server_aircraft.state().height_above_ground_ft);
        check(std::abs(seeded_off_ft - off_m / 0.3048) < 0.5,
              "the prediction's law was seeded " + std::to_string(seeded_off_ft) +
                  " ft off the server's, not the " + std::to_string(off_m / 0.3048) + " built");
        check(heard > 1400 && both_flown == heard,
              "every one of the " + std::to_string(heard) + " words heard had a law flying her "
              "on both sides, not " + std::to_string(both_flown));
        check(judged > 1400, "judged " + std::to_string(judged) + " words once known");
        check(worst_m < 0.001, "a correction once known moved her " + std::to_string(worst_m) +
                                   " m, over a millimetre");
        check(worst_asked_ft < 0.001, "the heights asked ended " + std::to_string(apart_ft) +
                                          " ft apart, not within a thousandth of a foot");
        check(server_aircraft.state().height_above_ground_ft > 500.0,
              "she was let down by the stick, not dived: " +
                  std::to_string(server_aircraft.state().height_above_ground_ft) + " ft");
        ++offsets_flown;
    }
    check(offsets_flown == 2, "both signs flown: " + std::to_string(offsets_flown));
}

// **A prediction whose STOVL law was made a step or two apart from the
// server's is put right by the law's carried state, and then agrees.** Her
// selector placed a step off by the clocks' difference - a step late on a
// busy server, or two on Windows' 15.6 ms timers - and each side's law is
// made on its own step, its wing-borne autopilot seeding its pitch integral
// from her as it finds her then; put back to the same motion at every word,
// both integrate alike from there, so the difference stays. Built: the
// F-35B at her model's 45,400 lb, untrimmed at 2,200 ft and 300 KCAS, flown
// untouched (0.6) and diving as a server holds a player's, then STOVL mode,
// the stick full forward and the lever at 0.7, as the multi-process test's
// pilot flies her; the server makes its law a step early, on the same step,
// a step late and two late, from 5 s and from 9 s into the dive - eight
// cases, counted. The server's word every fifth step 100 ms each way,
// carrying the height asked and the pitch integral rounded as the wire
// rounds them. Over a minute once the clocks' difference is known: the
// prediction error - the server's word against where the prediction had
// flown her by that step - under a millimetre at the median, and her pitch
// within half a degree of the server's at every word (COMPLETION_PLAN's
// bound). Measured: the median at most 0.2 mm, the worst 0.5 mm, and the
// pitch 0.020 degrees. **Seen red** with the pitch integral not carried:
// from 5 s a step early, the first case, the median 8.3 mm and the pitch
// 1.14 degrees off (in the search that found it, a step late 21 mm and 2.9).
GLIDESLOPE_TEST(a_prediction_whose_stovl_law_was_made_a_step_or_two_apart_from_the_servers_is_put_right_by_what_the_law_carries) {
    using glideslope::sim::apply;
    using glideslope::sim::Controller;
    using glideslope::sim::Motion;
    using glideslope::sim::Transition;
    std::size_t cases = 0;
    for (const int selected_s : {5, 9}) {
        for (const int late : {-1, 0, 1, 2}) {
            const auto start = [](Aircraft& a) {
                a.set_terrain(flat_ground());
                InitialConditions ic;
                ic.latitude_deg = -33.9461;
                ic.longitude_deg = 151.1772;
                ic.heading_deg = 270.0;
                ic.altitude_ft = 2200.0;
                ic.terrain_elevation_ft = 0.0;
                ic.airspeed_kts = 300.0;
                ic.gear = 0.0;
                ic.engine_running = true;
                ic.trim = false;
                a.initialize(ic);
                a.freeze_fuel(true);
            };
            Aircraft server_aircraft(data() / "jsbsim", "f35b");
            Aircraft client_aircraft(data() / "jsbsim", "f35b");
            start(server_aircraft);
            start(client_aircraft);
            Controls untouched;
            untouched.throttle = 0.6;
            Controller server(server_aircraft, untouched);
            Prediction client(client_aircraft);
            const int one_way = 100 * steps_per_second / 2000;
            const int every = 5;
            const int selected = selected_s * steps_per_second;
            const auto inputs = [&](int frame) {
                Controls c = untouched;
                if (frame >= selected) {
                    c.stovl = 1.0;
                    c.elevator = -1.0;
                    c.throttle = 0.7;
                }
                return c;
            };
            // The input the server flies at its step `theirs`: the selector
            // reaching it `late` steps after the client began it (early when
            // negative).
            const auto flown = [&](int theirs) {
                if (late > 0 && theirs >= selected && theirs < selected + late) {
                    return selected - 1;
                }
                if (late < 0 && theirs >= selected + late && theirs < selected) {
                    return selected;
                }
                return theirs;
            };
            struct Posted {
                int arrives_at = 0;
                Motion motion;
                std::uint32_t applied = 0;
                std::uint64_t server_steps = 0;
                std::optional<double> height_ft;
                Transition::Carried carried;
                double pitch_deg = 0.0;
            };
            std::deque<Posted> post;
            std::uint64_t server_steps = 0;
            struct Flown {
                std::array<double, 3> where;
                double pitch_deg;
            };
            std::vector<Flown> flown_here;
            std::vector<double> errors_m;
            double worst_pitch_deg = 0.0;
            const int frames = selected + 61 * steps_per_second;
            for (int frame = 0; frame < frames; ++frame) {
                client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
                const Motion m = client_aircraft.motion();
                flown_here.push_back({{m.location_ecef_m[0], m.location_ecef_m[1],
                                       m.location_ecef_m[2]},
                                      client_aircraft.state().pitch_deg});
                const int theirs = frame - one_way;
                if (theirs < 0) {
                    continue;
                }
                const int k = flown(theirs);
                server.set_pilot(inputs(k));
                apply(server_aircraft, server);
                server_aircraft.step();
                ++server_steps;
                if (theirs % every == 0) {
                    Posted p;
                    p.arrives_at = frame + one_way;
                    p.motion = server_aircraft.motion();
                    p.applied = static_cast<std::uint32_t>(k + 1);
                    p.server_steps = server_steps;
                    p.pitch_deg = server_aircraft.state().pitch_deg;
                    if (server.transition()) {
                        p.height_ft = static_cast<double>(
                            static_cast<float>(server.stovl_height_asked_ft()));
                        const Transition::Carried c = server.transition()->carried();
                        if (c.pitch_integral_deg) {
                            p.carried.pitch_integral_deg = static_cast<double>(
                                static_cast<float>(*c.pitch_integral_deg));
                        }
                    }
                    post.push_back(p);
                }
                while (!post.empty() && post.front().arrives_at <= frame) {
                    const Posted& p = post.front();
                    const bool settled = client.settled();
                    const Prediction::Correction c = client.reconcile(
                        p.motion, p.applied, 1, p.server_steps, p.height_ft, p.carried);
                    if (settled && p.height_ft && c.at_step && *c.at_step > 0 &&
                        *c.at_step <= flown_here.size()) {
                        const Flown& f = flown_here[*c.at_step - 1];
                        errors_m.push_back(std::hypot(f.where[0] - p.motion.location_ecef_m[0],
                                                      f.where[1] - p.motion.location_ecef_m[1],
                                                      f.where[2] - p.motion.location_ecef_m[2]));
                        worst_pitch_deg = std::max(worst_pitch_deg, std::abs(f.pitch_deg - p.pitch_deg));
                    }
                    post.pop_front();
                }
            }
            std::sort(errors_m.begin(), errors_m.end());
            check(errors_m.size() > 1400, "words compared once known, law flying: " +
                                              std::to_string(errors_m.size()));
            const double median_m = errors_m[errors_m.size() / 2];
            std::printf("  selected at %d s, the server's law made %+d steps late: the median "
                        "%.4f m, the worst %.4f m, the pitch %.3f degrees off at worst, over %zu "
                        "words\n",
                        selected_s, late, median_m, errors_m.back(), worst_pitch_deg,
                        errors_m.size());
            check(median_m <= 0.001, "the median error was " + std::to_string(median_m) +
                                         " m, over a millimetre");
            check(worst_pitch_deg <= 0.5, "her pitch was " + std::to_string(worst_pitch_deg) +
                                              " degrees off the server's, over half");
            ++cases;
        }
    }
    check(cases == 8, "two moments and four placings: " + std::to_string(cases));
}

// **A law made on a prediction's first step is seeded as the server seeds
// its own** (Prediction::held_by_server), and made again in a replay as it
// was made first. Two F-35Bs where a server starts a player's, 2,200 ft and
// 300 KCAS: the server's trimmed and held a second on what the trim found
// (held_until_moved), the client's a model just loaded, put there untrimmed.
// Her first input selects STOVL mode. The server's controller, made on that
// input as the server makes it, seeds her law from what she was held on;
// the prediction, told those controls, seeds its own from them - and not
// told, from the model just loaded, which is the situation this pins. Then
// it flies a second on, and a word placed by nothing - no clocks' difference
// yet, no input applied - puts her back to her first step and flies it
// again: the law is made again, from the controls she had as that step
// began, not from the newest step's.
GLIDESLOPE_TEST(a_prediction_told_what_the_server_held_her_on_seeds_a_law_made_on_her_first_input_as_the_server_does) {
    using glideslope::sim::Controller;
    using glideslope::sim::held_until_moved;
    const auto start = [](Aircraft& a, bool trim) {
        a.set_terrain(flat_ground());
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.terrain_elevation_ft = 0.0;
        ic.altitude_ft = 2200.0;
        ic.heading_deg = 270.0;
        ic.airspeed_kts = 300.0;
        ic.engine_running = true;
        ic.gear = 0.0;
        ic.trim = trim;
        ic.trim_lateral = trim;
        a.initialize(ic);
    };
    Aircraft server(data() / "jsbsim", "f35b");
    start(server, true);
    check(server.trimmed(), "the server's F-35B is trimmed, as a player's is started");
    const Controls held = held_until_moved(server);
    for (int i = 0; i < steps_per_second; ++i) {
        server.set_controls(held);
        server.step();
    }
    Controls first;
    first.stovl = 1.0;
    first.throttle = 0.7;
    first.elevator = -1.0;
    Controller servers_laws(server, first);
    servers_laws.set_pilot(first);
    (void)glideslope::sim::apply(server, servers_laws);
    check(servers_laws.transition() != nullptr, "the server's law is made on her first input");
    const double servers = servers_laws.transition()->throttle_integral_made_with();
    std::printf("  held on throttle %.6f; the server's law made with %.17g\n", held.throttle,
                servers);
    check(servers == held.throttle && servers > 0.1,
          "the server's law is made with the throttle she was held on");

    std::size_t cases = 0;
    for (const bool told : {false, true}) {
        Aircraft client(data() / "jsbsim", "f35b");
        start(client, false);
        const glideslope::sim::Motion at_first = client.motion();
        Prediction p(client);
        if (told) {
            p.held_by_server(held);
        }
        p.step(1, first);
        check(p.laws() != nullptr && p.laws()->transition() != nullptr,
              "the prediction's law is made on her first input");
        const double made = p.laws()->transition()->throttle_integral_made_with();
        for (int i = 0; i < steps_per_second; ++i) {
            p.step(1, first);
        }
        (void)p.reconcile(at_first, 0, 0, 0);
        const double again = p.laws()->transition()->throttle_integral_made_with();
        std::printf("  %s: made with %.17g, made again with %.17g\n",
                    told ? "told" : "not told", made, again);
        if (told) {
            check(made == servers, "told, the prediction's law is made with the server's "
                                   "throttle integral");
            check(again == servers, "and made again in a replay with the same");
        } else {
            check(made != servers, "not told, the prediction's law is made with another "
                                   "throttle integral - the situation this pins");
        }
        ++cases;
    }
    check(cases == 2, "told and not told: " + std::to_string(cases));
}


namespace {

// **What her pilot's inceptors set in her STOVL law** (Controller,
// Transition), read from a controller whose law flies her: the height the
// stick's fore-and-aft asks, the speed the lever asks, the bank the stick's
// sideways holds and the turn the pedals ask jet-borne, and what the hover
// law is asked in the hover.
struct StickSets {
    glideslope::sim::Transition::Stage stage{};
    double height_ft = 0.0;
    double speed_kt = 0.0;
    double bank_deg = 0.0;
    double pedals_radps = 0.0;
    double right_kt = 0.0;
    double turn_degps = 0.0;
};

std::optional<StickSets> stick_sets(const glideslope::sim::Controller& c) {
    const glideslope::sim::Transition* t = c.transition();
    if (t == nullptr) {
        return std::nullopt;
    }
    StickSets s;
    s.stage = t->stage();
    s.height_ft = c.stovl_height_asked_ft();
    s.speed_kt = t->speed_asked_kt();
    s.bank_deg = t->bank_held_deg();
    s.pedals_radps = t->pedals_turn_radps();
    if (t->hover_asked()) {
        s.right_kt = t->hover_asked()->right_kt;
        s.turn_degps = t->hover_asked()->turn_degps;
    }
    return s;
}

// The four stages of her law and the four things her pilot moves in STOVL
// mode: the stick fore-and-aft and sideways, the pedals, and the lever.
constexpr std::size_t stick_stages = 4;
constexpr std::size_t stick_axes = 4;
const std::array<const char*, stick_stages> stage_names{"wing-borne", "converting", "jet-borne",
                                                         "hovering"};
const std::array<const char*, stick_axes> axis_names{"the stick fore-and-aft",
                                                      "the stick sideways", "the pedals",
                                                      "the lever"};

// **What one step of an axis set in a stage**: what the law reads from it
// there - for the fore-and-aft the step's change of the height asked; for
// the lever the step's change of the speed asked; sideways, jet-borne the
// step's change of the bank held and in the hover the ground speed to the
// side asked; the pedals, the turn of her heading asked - jet-borne, less
// as she goes faster. None for an axis that stage does not
// read: wing-borne and converting forward, her autopilot flies her heading
// and speed, and only the height is the stick's.
std::optional<double> what_it_set(std::size_t stage, std::size_t axis, const StickSets& before,
                                  const StickSets& after) {
    const bool jet = stage == 2;
    const bool hover = stage == 3;
    switch (axis) {
    case 0: return after.height_ft - before.height_ft;
    case 1:
        if (jet) {
            return after.bank_deg - before.bank_deg;
        }
        if (hover) {
            return after.right_kt;
        }
        return std::nullopt;
    case 2:
        if (jet) {
            return after.pedals_radps;
        }
        if (hover) {
            return after.turn_degps;
        }
        return std::nullopt;
    default:
        if (jet || hover) {
            return after.speed_kt - before.speed_kt;
        }
        return std::nullopt;
    }
}

// **Each (stage, axis) held against the server's**: the steps compared -
// the axis off centre and both sides in that stage through the step - and
// of those, the steps the server's own law moved by it, and the steps the
// two disagreed on.
struct StickCell {
    std::size_t compared = 0;
    std::size_t moved = 0;
    std::size_t apart = 0;
    // The most the two were apart, as a share of what the server's set.
    double worst_share = 0.0;
};
using StickCells = std::array<std::array<StickCell, stick_axes>, stick_stages>;

// **Each side's law's stage, step by step**: the server's steps flown, those
// on which the prediction's step on the same input left its law in another
// stage - or with a law where the server's had none, or none where it had
// one - and the server's law's changes of stage.
struct StickStages {
    std::size_t steps = 0;
    std::size_t apart = 0;
    std::size_t changes = 0;
};

// **A predicting client and the server fed the same STOVL inputs, what the
// stick sets compared at every step**: the F-35B at `loading` (her model's
// own, empty), level at `feet` and 200 KCAS over flat ground in calm air,
// flown `seconds` on `inputs` as stovl_agreement's are - the server 100 ms
// behind, its word back 100 ms after every fifth step - and put right, as
// a real server's word puts her, with the height her law was asked and
// what else the law carries (net::OwnMotion). Each side's law is read
// before and after each step it flies on an input, and the client's step
// on that input - flown forward, from where its last correction left her -
// held against the server's. Two agree when they differ by no more than 2%
// of what the server's set, or 1e-9: a tenth of what a prediction flown on
// 0.8 of the stick is off by. Measured (linux-release, 2026-10-22), every
// stage and inceptor compared agrees to the last bit, or the lever's speed
// to 2.4e-12 of it, but the pedals' turn jet-borne, which falls off with her
// speed along the heading her law holds - the heading not told - and was up
// to 0.63% apart.
void stick_agreement(const std::string& loading, double feet,
                     const std::function<Controls(int, const Controls&)>& inputs, int seconds,
                     StickCells& cells, StickStages& stages) {
    using glideslope::sim::apply;
    using glideslope::sim::Controller;
    using glideslope::sim::Motion;
    using glideslope::sim::Transition;
    const glideslope::sim::PublishedFigures figures = glideslope::sim::read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/f35b.xml");
    const auto start = [&](Aircraft& a) {
        if (!loading.empty()) {
            a.load(figures.loadings.at(loading).loading);
        }
        a.set_terrain(flat_ground());
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.heading_deg = 70.0;
        ic.altitude_ft = feet;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = 200.0;
        ic.gear = 0.0;
        ic.trim = true;
        a.initialize(ic);
        a.freeze_fuel(true);
    };
    Aircraft server_aircraft(data() / "jsbsim", "f35b");
    Aircraft client_aircraft(data() / "jsbsim", "f35b");
    start(server_aircraft);
    start(client_aircraft);
    const Controls trimmed = glideslope::sim::trimmed_controls(server_aircraft);
    const double detent = server_aircraft.property("fcs/stovl-lever-detent");
    Controller server(server_aircraft, trimmed);
    Prediction client(client_aircraft);
    const int one_way = 100 * steps_per_second / 2000;
    const int every = 5;
    struct Posted {
        int arrives_at = 0;
        Motion motion;
        std::uint32_t applied = 0;
        std::uint64_t server_steps = 0;
        std::optional<double> height_asked_ft;
        Transition::Carried carried;
    };
    std::deque<Posted> post;
    std::uint64_t server_steps = 0;
    // The client's step on each input, before and after, until the
    // server's on the same input is flown.
    using Flew = std::pair<std::optional<StickSets>, std::optional<StickSets>>;
    std::deque<Flew> client_flew;
    const int frames = seconds * steps_per_second;
    const auto off_centre = [&](const Controls& c, std::size_t axis) {
        switch (axis) {
        case 0: return std::abs(c.elevator) > 1e-6;
        case 1: return std::abs(c.aileron) > 1e-6;
        case 2: return std::abs(c.rudder) > 1e-6;
        default:
            return std::abs(c.throttle - trimmed.throttle) > Controller::lever_release &&
                   std::abs(c.throttle - detent) >= Controller::lever_detent_width;
        }
    };
    for (int frame = 0; frame < frames; ++frame) {
        const auto client_before = client.laws() ? stick_sets(*client.laws()) : std::nullopt;
        client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame, trimmed));
        client_flew.emplace_back(client_before,
                                 client.laws() ? stick_sets(*client.laws()) : std::nullopt);
        const int theirs = frame - one_way;
        if (theirs >= 0) {
            const Controls input = inputs(theirs, trimmed);
            const auto before = stick_sets(server);
            server.set_pilot(input);
            apply(server_aircraft, server);
            server_aircraft.step();
            ++server_steps;
            const auto after = stick_sets(server);
            const Flew ours = client_flew.front();
            client_flew.pop_front();
            ++stages.steps;
            if (after.has_value() != ours.second.has_value() ||
                (after && after->stage != ours.second->stage)) {
                ++stages.apart;
            }
            if (before && after && before->stage != after->stage) {
                ++stages.changes;
            }
            if (before && after && ours.first && ours.second && before->stage == after->stage &&
                ours.first->stage == before->stage && ours.second->stage == before->stage) {
                const auto stage = static_cast<std::size_t>(before->stage);
                for (std::size_t axis = 0; axis < stick_axes; ++axis) {
                    const auto theirs_set = what_it_set(stage, axis, *before, *after);
                    if (!off_centre(input, axis) || !theirs_set) {
                        continue;
                    }
                    const double ours_set = *what_it_set(stage, axis, *ours.first, *ours.second);
                    StickCell& cell = cells[stage][axis];
                    ++cell.compared;
                    if (std::abs(*theirs_set) > 1e-9) {
                        ++cell.moved;
                    }
                    const double apart = std::abs(ours_set - *theirs_set);
                    if (std::abs(*theirs_set) > 1e-9) {
                        cell.worst_share =
                            std::max(cell.worst_share, apart / std::abs(*theirs_set));
                    }
                    if (apart > 1e-9 + 0.02 * std::abs(*theirs_set)) {
                        ++cell.apart;
                    }
                }
            }
            if (theirs % every == 0) {
                Posted p{frame + one_way, server_aircraft.motion(),
                         static_cast<std::uint32_t>(theirs + 1), server_steps, std::nullopt, {}};
                if (server.transition()) {
                    p.height_asked_ft = std::max(0.0, server.stovl_height_asked_ft());
                    p.carried = server.transition()->carried();
                }
                post.push_back(p);
            }
        }
        while (!post.empty() && post.front().arrives_at <= frame) {
            (void)client.reconcile(post.front().motion, post.front().applied, 1,
                                   post.front().server_steps, post.front().height_asked_ft,
                                   post.front().carried);
            post.pop_front();
        }
    }
}

} // namespace

// **What her pilot's stick sets in a STOVL prediction is what it sets on the
// server** (stick_agreement, above), at every step, in every stage of her
// law and on every inceptor that stage reads. The height the server tells
// puts a prediction's height asked right at every word, so a prediction
// flown on 0.8 of the stick the client sent flew to a median of 2 mm through
// a server and passed every test (PROJECT_STATUS.md, 2026-10-15 and
// 2026-10-16): this holds what the stick sets, not where she went.
//
// Two flights. Heavier than she can hover (her model's own loading, at 1,000
// ft), STOVL mode is flown wing-borne, the stick forward from 3 s to 20 s
// and back from 22 s to 30 s. At her vertical-landing loading (at 150 ft)
// she converts, slows jet-borne to a hover and is moved in it: the selector
// at 1 s; the stick back from 1.5 s to 8 s through her conversion;
// jet-borne, slowing, the stick sideways right and left, the pedals right
// and left, and fore-and-aft back and forward; in the hover from 170 s the
// lever forward, to its detent and aft, the stick fore-and-aft, right and
// left and the pedals right and left; the lever at its forward stop from
// 320 s, jet-borne again, the stick right and left, the pedals and the
// stick fore-and-aft there, and the lever aft from 345 s.
//
// **Coverage asserted**: of the sixteen stages and inceptors, ten are read -
// the fore-and-aft in all four stages, and jet-borne and in the hover the
// stick sideways, the pedals and the lever - and each of the ten must be
// compared for at least a second with the server's law moved by it. The six
// not read are named (what_it_set). Every step compared must agree.
//
// **And both laws change stage on the same step**: on every step the server
// flies, the prediction's law on the same input is in the server's law's
// stage, and the server's changes stage the three times the second flight
// asks - jet-borne, hovering and jet-borne again. Until 2026-10-22 the
// prediction's was jet-borne from 1.5 s and the server's converting until 9
// s, so the fore-and-aft converting was left out: a replay flown from before
// the selector flew those steps with the fan's doors commanded open, as the
// newest step had left them (Aircraft::replay_state keeps the lift system's
// commands since). Seen red with them not kept: 1,104 steps in another
// stage.
GLIDESLOPE_TEST(a_stovl_prediction_sets_with_her_stick_what_the_server_sets_in_every_stage_on_every_inceptor) {
    StickCells cells{};
    StickStages stages;
    stick_agreement(
        "", 1000.0,
        [](int frame, const Controls& trimmed) {
            const double s = static_cast<double>(frame) / steps_per_second;
            Controls c = trimmed;
            c.stovl = s >= 1.0 ? 1.0 : 0.0;
            c.elevator = s >= 3.0 && s < 20.0 ? -0.5 : s >= 22.0 && s < 30.0 ? 0.5 : 0.0;
            return c;
        },
        32, cells, stages);
    stick_agreement(
        "vertical_landing", 150.0,
        [](int frame, const Controls& trimmed) {
            const double s = static_cast<double>(frame) / steps_per_second;
            const auto during = [&](double from, double to) { return s >= from && s < to; };
            Controls c = trimmed;
            c.stovl = s >= 1.0 ? 1.0 : 0.0;
            const double detent = 0.25;
            if (s >= 170.0) {
                c.throttle = s < 180.0   ? detent + 0.5 * (1.0 - detent)
                             : s < 190.0 ? detent
                             : s < 200.0 ? 0.5 * detent
                             : s < 320.0 ? detent
                             : s < 345.0 ? 1.0
                             : s < 360.0 ? 0.0
                                         : detent;
            }
            c.elevator = during(1.5, 8.0)       ? 0.5
                         : during(60.0, 64.0)   ? 0.5
                         : during(64.0, 68.0)   ? -0.5
                         : during(210.0, 214.0) ? 0.5
                         : during(214.0, 218.0) ? -0.5
                         : during(336.0, 339.0) ? 0.5
                         : during(339.0, 342.0) ? -0.5
                                                : 0.0;
            c.aileron = during(40.0, 44.0)     ? 0.5
                        : during(44.0, 48.0)   ? -0.5
                        : during(230.0, 240.0) ? 0.6
                        : during(245.0, 255.0) ? -0.6
                        : during(326.0, 328.0) ? 0.5
                        : during(328.0, 330.0) ? -0.5
                                               : 0.0;
            c.rudder = during(50.0, 53.0)     ? 0.5
                       : during(53.0, 56.0)   ? -0.5
                       : during(260.0, 265.0) ? 0.5
                       : during(268.0, 273.0) ? -0.5
                       : during(331.0, 333.0) ? 0.5
                       : during(333.0, 335.0) ? -0.5
                                              : 0.0;
            return c;
        },
        370, cells, stages);
    std::printf("  her law's stage: %zu of the server's steps, %zu in another stage on the "
                "prediction, %zu changes of stage on the server\n",
                stages.steps, stages.apart, stages.changes);
    check(stages.changes == 3, "the server's law changed stage " +
                                   std::to_string(stages.changes) + " times, not 3");
    check(stages.apart == 0, "the prediction's law was in another stage on " +
                                 std::to_string(stages.apart) + " steps");
    for (std::size_t stage = 0; stage < stick_stages; ++stage) {
        for (std::size_t axis = 0; axis < stick_axes; ++axis) {
            const StickCell& cell = cells[stage][axis];
            std::printf("  %-10s %-22s compared %5zu steps, moved %5zu, apart %5zu (at most "
                        "%.2g of it)%s\n",
                        stage_names[stage], axis_names[axis], cell.compared, cell.moved,
                        cell.apart, cell.worst_share,
                        axis != 0 && stage < 2 ? " - not read there" : "");
        }
    }
    std::size_t read = 0;
    std::size_t not_read = 0;
    for (std::size_t stage = 0; stage < stick_stages; ++stage) {
        for (std::size_t axis = 0; axis < stick_axes; ++axis) {
            const StickCell& cell = cells[stage][axis];
            if (axis != 0 && stage < 2) {
                ++not_read;
                continue;
            }
            ++read;
            check(cell.moved >= static_cast<std::size_t>(steps_per_second),
                  std::string(axis_names[axis]) + " " + stage_names[stage] +
                      " moved the server's law on " + std::to_string(cell.moved) +
                      " steps compared, under a second's");
            check(cell.apart == 0, std::string(axis_names[axis]) + " " + stage_names[stage] +
                                       " set something else in the prediction on " +
                                       std::to_string(cell.apart) + " of " +
                                       std::to_string(cell.compared) + " steps");
        }
    }
    std::printf("  %zu of the 16 stages and inceptors compared, %zu not read\n", read, not_read);
    check(read == 10 && read + not_read == stick_stages * stick_axes,
          "the ten compared and the six not read are the sixteen");
}

// **The guard of a prediction trips on every input not finite, and names
// it** (sim::Prediction::lost; docs/status, 2026-10-23). Each of the
// controls set to a NaN and to an infinity in turn, flown as a step; each of
// the server's motion's thirteen numbers, likewise, in the word it is put
// right by and in the motion it adopts; the STOVL law's height asked and
// pitch integral the word carries; the levers the server holds her on; and a
// number of the server's snapshot. Each trips it before JSBSim is given the
// number - the aircraft is not stepped - and is named, and a prediction lost
// flies no step after. The space walked is counted and asserted.
// **Left out**: the motion a step *leaves* not finite. Its check is the same
// function as the server's motion's, walked here for every number, but no
// step of a debug JSBSim can be made to leave a NaN without first looking a
// table up at it and asserting (FGTable.cpp:618) - which is the limit the
// guard is stated with.
GLIDESLOPE_TEST(every_input_a_prediction_is_given_not_finite_trips_its_guard_and_is_named) {
    Aircraft a(data() / "jsbsim", "c172p");
    set_up(a);
    constexpr std::array<const char*, Controls::control_count> names = {
        "elevator",        "aileron",         "rudder",           "throttle",
        "mixture",         "flaps",           "left brake",       "right brake",
        "pitch trim",      "propeller",       "gear",             "supercharger",
        "speedbrake",      "throttle offset 0", "throttle offset 1", "cooling flaps 0",
        "cooling flaps 1", "reverse",         "stovl",            "roll trim",
        "yaw trim"};
    static_assert(names.back() != nullptr, "every control needs its name");
    const std::array<double, 2> bad = {std::nan(""), HUGE_VAL};
    std::size_t walked = 0;
    std::size_t tripped = 0;
    const auto tripped_on = [&](const Prediction& p, const std::string& what,
                                const std::string& case_name) {
        ++walked;
        if (p.lost() && p.lost()->what == what) {
            ++tripped;
        } else {
            check(false, case_name + ": the guard said '" +
                             (p.lost() ? p.lost()->what : std::string("nothing")) +
                             "', not '" + what + "'");
        }
    };
    // Every control.
    for (std::size_t i = 0; i < Controls::control_count; ++i) {
        for (const double v : bad) {
            Prediction p(a);
            auto list = Controls{}.as_list();
            list[i] = v;
            const double was_s = a.state().sim_time_s;
            p.step(1, Controls::from_list(list));
            tripped_on(p, std::string("the input's ") + names[i], names[i]);
            check(a.state().sim_time_s == was_s, std::string(names[i]) + ": not stepped");
            p.step(1, Controls{});
            check(a.state().sim_time_s == was_s,
                  std::string(names[i]) + ": a prediction lost flies no step after");
        }
    }
    const std::size_t controls_walked = walked;
    check(controls_walked == 2 * Controls::control_count,
          "every control, NaN and infinite: " + std::to_string(controls_walked));
    // Every number of the server's motion, in reconcile and in adopt.
    const glideslope::sim::Motion good = a.motion();
    std::size_t motion_numbers = 0;
    const auto each_number = [&](const std::function<void(glideslope::sim::Motion&,
                                                          const std::string&)>& fn) {
        motion_numbers = 0;
        for (const double v : bad) {
            for (std::size_t i = 0; i < 3; ++i) {
                glideslope::sim::Motion m = good;
                m.location_ecef_m[i] = v;
                fn(m, "location_ecef_m[" + std::to_string(i) + "]");
                m = good;
                m.uvw_mps[i] = v;
                fn(m, "uvw_mps[" + std::to_string(i) + "]");
                m = good;
                m.pqr_radps[i] = v;
                fn(m, "pqr_radps[" + std::to_string(i) + "]");
                motion_numbers += 3;
            }
            for (std::size_t i = 0; i < 4; ++i) {
                glideslope::sim::Motion m = good;
                m.attitude_local[i] = v;
                fn(m, "attitude_local[" + std::to_string(i) + "]");
                ++motion_numbers;
            }
        }
    };
    each_number([&](glideslope::sim::Motion& m, const std::string& n) {
        Prediction p(a);
        (void)p.reconcile(m, 0, 0, 0, std::nullopt, {});
        tripped_on(p, "the server's word's " + n, "reconcile " + n);
    });
    check(motion_numbers == 2 * 13, "every number of a motion, NaN and infinite: " +
                                        std::to_string(motion_numbers));
    each_number([&](glideslope::sim::Motion& m, const std::string& n) {
        Prediction p(a);
        (void)p.adopt(m, 0);
        tripped_on(p, "the server's word's " + n, "adopt " + n);
    });
    // The STOVL law's numbers the word carries.
    for (const double v : bad) {
        {
            Prediction p(a);
            (void)p.reconcile(good, 0, 0, 0, v, {});
            tripped_on(p, "the server's word's stovl_height_asked_ft", "height asked");
        }
        {
            Prediction p(a);
            glideslope::sim::Transition::Carried carried;
            carried.pitch_integral_deg = v;
            (void)p.reconcile(good, 0, 0, 0, std::nullopt, carried);
            tripped_on(p, "the server's word's stovl pitch_integral_deg", "pitch integral");
        }
        {
            Prediction p(a);
            Controls held;
            held.throttle = v;
            p.held_by_server(held);
            tripped_on(p, "the server's held throttle", "held by the server");
        }
        {
            Prediction p(a);
            p.held_by_server(Controls{}, v);
            tripped_on(p, "the server's held trim", "held trim");
        }
        {
            Prediction p(a);
            AircraftSnapshot s = a.capture();
            s.uvw_fps[1] = v;
            (void)p.reconcile(s, 0);
            tripped_on(p, "the server's snapshot's uvw_fps[1]", "snapshot motion");
        }
        {
            Prediction p(a);
            AircraftSnapshot s = a.capture();
            check(!s.properties.empty(), "a snapshot has properties");
            s.properties.front().second = v;
            (void)p.reconcile(s, 0);
            tripped_on(p, "the server's snapshot's " + s.properties.front().first,
                       "snapshot property");
        }
    }
    const std::size_t space = 2 * Controls::control_count + 2 * 2 * 13 + 2 * 6;
    check(walked == space && tripped == space,
          "the space walked " + std::to_string(walked) + " of " + std::to_string(space) +
              ", tripped and named " + std::to_string(tripped));
}

namespace {

// **A prediction and the server, her law made a step or two apart, flown
// through every stage of it** with what a real server's word carries - the
// height asked and the wing-borne autopilot's pitch integral, rounded as the
// wire rounds them - and nothing more: the F-35B at her vertical-landing
// loading, trimmed level at 150 ft and 200 KCAS heading 070 over flat
// ground, STOVL mode from `selected_frame`, the server's selector placed
// `late` steps after the client's (early when negative). She converts,
// slows jet-borne - the stick back, then right and left, the pedals, the
// stick forward - and hovers from about 109 s, the stick right, the pedals
// left and the stick back there; 130 s from the selector. The server's word
// every fifth step, 100 ms each way. Once the prediction has settled, each
// word against where the prediction had flown her by that step: the median
// and the worst error, and her pitch's worst by the stage the server's law
// was in, and the words in each.
struct LawApart {
    double median_m = 0.0;
    double worst_m = 0.0;
    std::array<double, 4> worst_pitch_deg{};
    std::array<std::size_t, 4> words{};
};
LawApart law_apart(int selected_frame, int late) {
    using glideslope::sim::apply;
    using glideslope::sim::Controller;
    using glideslope::sim::Motion;
    using glideslope::sim::Transition;
    const glideslope::sim::PublishedFigures figures = glideslope::sim::read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/f35b.xml");
    const auto start = [&](Aircraft& a) {
        a.load(figures.loadings.at("vertical_landing").loading);
        a.set_terrain(flat_ground());
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.heading_deg = 70.0;
        ic.altitude_ft = 150.0;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = 200.0;
        ic.gear = 0.0;
        ic.trim = true;
        a.initialize(ic);
        a.freeze_fuel(true);
    };
    Aircraft server_aircraft(data() / "jsbsim", "f35b");
    Aircraft client_aircraft(data() / "jsbsim", "f35b");
    start(server_aircraft);
    start(client_aircraft);
    const Controls trimmed = glideslope::sim::trimmed_controls(server_aircraft);
    Controller server(server_aircraft, trimmed);
    Prediction client(client_aircraft);
    client.held_by_server(trimmed);
    const int one_way = 100 * steps_per_second / 2000;
    const int every = 5;
    const auto inputs = [&](int frame) {
        Controls c = trimmed;
        const double s = static_cast<double>(frame - selected_frame) / steps_per_second;
        if (frame >= selected_frame) {
            c.stovl = 1.0;
            const auto during = [&](double from, double to) { return s >= from && s < to; };
            c.aileron = during(30, 34)     ? 0.5
                        : during(40, 44)   ? -0.5
                        : during(118, 122) ? 0.5
                                           : 0.0;
            c.rudder = during(50, 53) ? 0.5 : during(120, 125) ? -0.5 : 0.0;
            c.elevator = during(1, 6)       ? 0.3
                         : during(60, 63)   ? -0.3
                         : during(126, 128) ? 0.5
                                            : 0.0;
        }
        return c;
    };
    const auto flown = [&](int theirs) {
        if (late > 0 && theirs >= selected_frame && theirs < selected_frame + late) {
            return selected_frame - 1;
        }
        if (late < 0 && theirs >= selected_frame + late && theirs < selected_frame) {
            return selected_frame;
        }
        return theirs;
    };
    struct Posted {
        int arrives_at = 0;
        Motion motion;
        std::uint32_t applied = 0;
        std::uint64_t server_steps = 0;
        std::optional<double> height_ft;
        Transition::Carried carried;
        double pitch_deg = 0.0;
        int stage = -1;
    };
    std::deque<Posted> post;
    std::uint64_t server_steps = 0;
    struct Flown {
        std::array<double, 3> where;
        double pitch_deg;
    };
    std::vector<Flown> flown_here;
    std::vector<double> errors_m;
    LawApart out;
    const int frames = selected_frame + 130 * steps_per_second;
    for (int frame = 0; frame < frames; ++frame) {
        client.step(static_cast<std::uint32_t>(frame + 1), inputs(frame));
        const Motion m = client_aircraft.motion();
        flown_here.push_back(
            {{m.location_ecef_m[0], m.location_ecef_m[1], m.location_ecef_m[2]},
             client_aircraft.state().pitch_deg});
        const int theirs = frame - one_way;
        if (theirs < 0) {
            continue;
        }
        const int k = flown(theirs);
        server.set_pilot(inputs(k));
        apply(server_aircraft, server);
        server_aircraft.step();
        ++server_steps;
        if (theirs % every == 0) {
            Posted p;
            p.arrives_at = frame + one_way;
            p.motion = server_aircraft.motion();
            p.applied = static_cast<std::uint32_t>(k + 1);
            p.server_steps = server_steps;
            p.pitch_deg = server_aircraft.state().pitch_deg;
            if (const Transition* t = server.transition()) {
                p.height_ft =
                    static_cast<double>(static_cast<float>(server.stovl_height_asked_ft()));
                const Transition::Carried c = t->carried();
                if (c.pitch_integral_deg) {
                    p.carried.pitch_integral_deg =
                        static_cast<double>(static_cast<float>(*c.pitch_integral_deg));
                }
                p.stage = static_cast<int>(t->stage());
            }
            post.push_back(p);
        }
        while (!post.empty() && post.front().arrives_at <= frame) {
            const Posted& p = post.front();
            const bool settled = client.settled();
            const Prediction::Correction c =
                client.reconcile(p.motion, p.applied, 1, p.server_steps, p.height_ft, p.carried);
            if (settled && p.stage >= 0 && c.at_step && *c.at_step > 0 &&
                *c.at_step <= flown_here.size()) {
                const Flown& f = flown_here[*c.at_step - 1];
                errors_m.push_back(std::hypot(f.where[0] - p.motion.location_ecef_m[0],
                                              f.where[1] - p.motion.location_ecef_m[1],
                                              f.where[2] - p.motion.location_ecef_m[2]));
                const auto st = static_cast<std::size_t>(p.stage);
                out.worst_pitch_deg[st] =
                    std::max(out.worst_pitch_deg[st], std::abs(f.pitch_deg - p.pitch_deg));
                ++out.words[st];
            }
            post.pop_front();
        }
    }
    std::sort(errors_m.begin(), errors_m.end());
    out.median_m = errors_m.empty() ? 0.0 : errors_m[errors_m.size() / 2];
    out.worst_m = errors_m.empty() ? 0.0 : errors_m.back();
    return out;
}
} // namespace

// **Converting, jet-borne and hovering, a prediction whose STOVL law was
// made a step or two apart from the server's agrees with it on what the
// law carries now** - the height asked and the wing-borne autopilot's pitch
// integral (net::OwnMotion) - and the converting, jet-borne and hover laws'
// own integrals, each side's own, need no telling: each is seeded at its
// stage's change from the controls the law before it left, which by then
// agree, and both laws change stage on the same step (Aircraft::replay_state
// keeps the lift system's commands). Flown as law_apart says, selected at
// 0.5 s and at 3 s, the server's law made a step early, on the step, a step
// late and two late: eight cases, counted, each with at least 150 words
// compared converting, 2,000 jet-borne and 500 hovering. Bounds: the median
// under a millimetre, and her pitch within half a degree of the server's at
// every word in every stage (COMPLETION_PLAN's bound). Measured
// (linux-release, 2026-10-22): the median at most 0.4 mm, the worst 6 mm,
// and the pitch within 0.012 degrees. A search that told every number of the
// transition and hover laws besides - 39 - moved the pitch to 0.007 degrees
// and the median under 0.05 mm: nothing a bound here could tell apart.
// **Seen red** with the lift system's commands not kept by a replay: the
// first case's median 16.8 mm, her pitch 0.377 degrees off jet-borne. (Not
// red with the pitch integral not carried: from 200 KCAS she converts at
// once, and the wing-borne autopilot it belongs to has no time to wander -
// the wing-borne test above pins that.)
GLIDESLOPE_TEST(a_prediction_whose_stovl_law_was_made_a_step_or_two_apart_agrees_with_the_server_converting_jet_borne_and_hovering) {
    std::size_t cases = 0;
    for (const int selected_frame : {steps_per_second / 2, 3 * steps_per_second}) {
        for (const int late : {-1, 0, 1, 2}) {
            const LawApart a = law_apart(selected_frame, late);
            std::printf("  selected at %.1f s, the server's law made %+d steps late: the median "
                        "%.4f m, the worst %.4f m; her pitch off at most %.3f converting, %.3f "
                        "jet-borne, %.3f hovering, over %zu, %zu and %zu words\n",
                        static_cast<double>(selected_frame) / steps_per_second, late,
                        a.median_m, a.worst_m, a.worst_pitch_deg[1], a.worst_pitch_deg[2],
                        a.worst_pitch_deg[3], a.words[1], a.words[2], a.words[3]);
            check(a.words[1] >= 150 && a.words[2] >= 2000 && a.words[3] >= 500,
                  "too few words compared in a stage: " + std::to_string(a.words[1]) + ", " +
                      std::to_string(a.words[2]) + ", " + std::to_string(a.words[3]));
            check(a.median_m <= 0.001,
                  "the median error was " + std::to_string(a.median_m) + " m, over a millimetre");
            for (std::size_t stage = 0; stage < a.worst_pitch_deg.size(); ++stage) {
                check(a.worst_pitch_deg[stage] <= 0.5,
                      "her pitch was " + std::to_string(a.worst_pitch_deg[stage]) +
                          " degrees off the server's in stage " + std::to_string(stage) +
                          ", over half");
            }
            ++cases;
        }
    }
    check(cases == 8, "two moments and four placings: " + std::to_string(cases));
}

namespace {

// **A client joining her while her STOVL law flies her**, from `join_s`
// into a flight: the F-35B at her vertical-landing loading, trimmed level at
// 150 ft and 200 KCAS heading 070 over flat ground, her pilot's selector at
// 1 s; `banked`, the stick full right from 30 s to 32 s and centred - her
// law holding the 20 degrees of bank it left, jet-borne, turning - or else
// slowing level to a hover from about 109 s. The joining client's model is loaded and put where the
// server's first word to it says, and predicts her from there on the same
// inputs - the selector on, the stick centred - the server's word every
// fifth step, 100 ms each way, telling the law's state with the first
// (`told`) or not. From that first word on, each word against where the
// prediction had flown her by that step, for `seconds`: the median and
// worst error, and the worst of her pitch and her roll.
struct Joined {
    double median_m = 0.0;
    double worst_m = 0.0;
    double worst_pitch_deg = 0.0;
    double worst_roll_deg = 0.0;
    double bank_held_deg = 0.0;
    std::size_t words = 0;
    int stage = -1;
};

Joined join_her(double join_s, bool banked, bool told, int seconds) {
    using glideslope::sim::apply;
    using glideslope::sim::Controller;
    using glideslope::sim::Motion;
    using glideslope::sim::Transition;
    const glideslope::sim::PublishedFigures figures = glideslope::sim::read_published_figures(
        std::string(GLIDESLOPE_TEST_FIGURES_DIR) + "/f35b.xml");
    const auto start = [&](Aircraft& a) {
        a.load(figures.loadings.at("vertical_landing").loading);
        a.set_terrain(flat_ground());
        InitialConditions ic;
        ic.latitude_deg = -33.9461;
        ic.longitude_deg = 151.1772;
        ic.heading_deg = 70.0;
        ic.altitude_ft = 150.0;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = 200.0;
        ic.gear = 0.0;
        ic.trim = true;
        a.initialize(ic);
        a.freeze_fuel(true);
    };
    Aircraft server_aircraft(data() / "jsbsim", "f35b");
    start(server_aircraft);
    const Controls trimmed = glideslope::sim::trimmed_controls(server_aircraft);
    Controller server(server_aircraft, trimmed);
    const auto inputs = [&](int frame) {
        const double s = static_cast<double>(frame) / steps_per_second;
        Controls c = trimmed;
        c.stovl = s >= 1.0 ? 1.0 : 0.0;
        c.aileron = banked && s >= 30.0 && s < 32.0 ? 1.0 : 0.0;
        return c;
    };
    const int join = static_cast<int>(join_s * steps_per_second);
    for (int frame = 0; frame < join; ++frame) {
        server.set_pilot(inputs(frame));
        apply(server_aircraft, server);
        server_aircraft.step();
    }
    // The joining client: its model loaded, put where the server says.
    Aircraft client_aircraft(data() / "jsbsim", "f35b");
    start(client_aircraft);
    client_aircraft.set_motion(server_aircraft.motion());
    Prediction client(client_aircraft);
    client.held_by_server(server.pilots_laws().applied);
    const int one_way = 100 * steps_per_second / 2000;
    const int every = 5;
    struct Posted {
        int arrives_at = 0;
        Motion motion;
        std::uint32_t applied = 0;
        std::uint64_t server_steps = 0;
        std::optional<double> height_ft;
        Transition::Carried carried;
        std::optional<Controller::LawState> law;
        double pitch_deg = 0.0;
        double roll_deg = 0.0;
    };
    std::deque<Posted> post;
    std::uint64_t server_steps = static_cast<std::uint64_t>(join);
    struct Flown {
        std::array<double, 3> where;
        double pitch_deg;
        double roll_deg;
    };
    std::vector<Flown> flown_here;
    std::vector<double> errors_m;
    Joined out;
    out.bank_held_deg = server.transition() ? server.transition()->bank_held_deg() : 0.0;
    out.stage = server.transition() ? static_cast<int>(server.transition()->stage()) : -1;
    bool first = true;
    const int frames = (seconds + 1) * steps_per_second;
    for (int f = 0; f < frames; ++f) {
        client.step(static_cast<std::uint32_t>(f + 1), inputs(join + f));
        const Motion m = client_aircraft.motion();
        flown_here.push_back(
            {{m.location_ecef_m[0], m.location_ecef_m[1], m.location_ecef_m[2]},
             client_aircraft.state().pitch_deg, client_aircraft.state().roll_deg});
        const int theirs = f - one_way;
        if (theirs >= 0) {
            server.set_pilot(inputs(join + theirs));
            apply(server_aircraft, server);
            server_aircraft.step();
            ++server_steps;
            if (theirs % every == 0) {
                Posted p;
                p.arrives_at = f + one_way;
                p.motion = server_aircraft.motion();
                p.applied = static_cast<std::uint32_t>(theirs + 1);
                p.server_steps = server_steps;
                p.pitch_deg = server_aircraft.state().pitch_deg;
                p.roll_deg = server_aircraft.state().roll_deg;
                if (const Transition* t = server.transition()) {
                    p.height_ft = static_cast<double>(
                        static_cast<float>(server.stovl_height_asked_ft()));
                    const Transition::Carried c = t->carried();
                    if (c.pitch_integral_deg) {
                        p.carried.pitch_integral_deg =
                            static_cast<double>(static_cast<float>(*c.pitch_integral_deg));
                    }
                    if (first && told) {
                        p.law = server.law_state();
                    }
                }
                first = false;
                post.push_back(p);
            }
        }
        while (!post.empty() && post.front().arrives_at <= f) {
            const Posted& p = post.front();
            const Prediction::Correction c = client.reconcile(
                p.motion, p.applied, 1, p.server_steps, p.height_ft, p.carried, p.law);
            if (c.at_step && *c.at_step > 0 && *c.at_step <= flown_here.size()) {
                const Flown& was = flown_here[*c.at_step - 1];
                errors_m.push_back(std::hypot(was.where[0] - p.motion.location_ecef_m[0],
                                              was.where[1] - p.motion.location_ecef_m[1],
                                              was.where[2] - p.motion.location_ecef_m[2]));
                out.worst_pitch_deg =
                    std::max(out.worst_pitch_deg, std::abs(was.pitch_deg - p.pitch_deg));
                out.worst_roll_deg =
                    std::max(out.worst_roll_deg, std::abs(was.roll_deg - p.roll_deg));
            }
            post.pop_front();
        }
    }
    std::sort(errors_m.begin(), errors_m.end());
    out.words = errors_m.size();
    out.median_m = errors_m.empty() ? 0.0 : errors_m[errors_m.size() / 2];
    out.worst_m = errors_m.empty() ? 0.0 : errors_m.back();
    return out;
}

} // namespace

// **A client joining an F-35B while her STOVL law flies her predicts her
// from the server's law state** (Controller::LawState, told with the first
// word): mid-bank jet-borne, joined 40 s in, her law holding her pilot's 20
// degrees; and hovering, joined 125 s in. From the first word on, for 20 s:
// the median within a millimetre, and her pitch and roll within half a
// degree of the server's at every word (the STOVL bounds of the tests
// above). Measured (linux-release, 2026-10-22): mid-bank the median 0.0 mm,
// the worst 41 mm on the first word, her pitch 0.008 and roll 0.014 degrees;
// hovering the median 0.0 mm, the worst 51 mm, her pitch 0.005 degrees.
// **Seen red** without the law told - a prediction begun on a model just
// loaded, the fan's doors shut, its law made wing-borne and level: mid-bank
// the median 74 mm and her pitch 0.88 and roll 0.96 degrees off; hovering
// her pitch 0.97 degrees off. And red with each part left out: the law told
// but her lift system not put where the server's stands, mid-bank the median
// 2.7 mm; the bank held told as nought, her roll 0.89 degrees off.
GLIDESLOPE_TEST(a_client_joining_an_f35b_mid_bank_or_hovering_predicts_her_from_the_servers_law_state) {
    std::size_t cases = 0;
    for (const bool banked : {true, false}) {
        const double join_s = banked ? 40.0 : 125.0;
        for (const bool told : {true, false}) {
            const Joined j = join_her(join_s, banked, told, 20);
            std::printf("  joined at %.0f s (her law's stage %d, bank held %.1f degrees), %s: the "
                        "median %.4f m, the worst %.4f m, her pitch %.3f and roll %.3f degrees "
                        "off at worst, over %zu words\n",
                        join_s, j.stage, j.bank_held_deg, told ? "told" : "not told",
                        j.median_m, j.worst_m, j.worst_pitch_deg, j.worst_roll_deg, j.words);
            if (!told) {
                continue;
            }
            check(j.stage == (banked ? 2 : 3),
                  "her law was in stage " + std::to_string(j.stage) + " when she was joined");
            check(!banked || j.bank_held_deg >= 19.9,
                  "her law held " + std::to_string(j.bank_held_deg) + " degrees of bank");
            check(j.words >= 450, "words compared: " + std::to_string(j.words));
            check(j.median_m <= 0.001,
                  "the median error was " + std::to_string(j.median_m) + " m, over a millimetre");
            check(j.worst_pitch_deg <= 0.5 && j.worst_roll_deg <= 0.5,
                  "her pitch was " + std::to_string(j.worst_pitch_deg) + " and roll " +
                      std::to_string(j.worst_roll_deg) + " degrees off the server's");
            ++cases;
        }
    }
    check(cases == 2, "mid-bank and hovering, told: " + std::to_string(cases));
}
