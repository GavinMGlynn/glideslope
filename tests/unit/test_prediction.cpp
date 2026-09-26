#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/prediction.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <memory>
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

void set_up(Aircraft& a) {
    a.set_terrain(flat_ground());
    InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.terrain_elevation_ft = 0.0;
    ic.altitude_ft = 6000.0;
    ic.heading_deg = 90.0;
    ic.airspeed_kts = 110.0;
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
};

// **Client and server, with the wire between them.** Both fly the same
// JSBSim at 120 Hz. The client flies each input as it is made; the server
// sees it `one_way` frames later, and its state comes back `one_way` frames
// after that, which is the round trip.
Flight fly(int frames, int one_way, int snapshot_every, bool by_motion = false) {
    Aircraft server_aircraft(data() / "jsbsim", "c172p");
    Aircraft client_aircraft(data() / "jsbsim", "c172p");
    set_up(server_aircraft);
    set_up(client_aircraft);
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
        client.step(static_cast<std::uint32_t>(frame + 1), flying(frame));

        // The server applies the input that left the client `one_way` frames
        // ago, which is the only one it has.
        const int theirs = frame - one_way;
        if (theirs >= 0) {
            server_aircraft.set_controls(flying(theirs));
            server_aircraft.step();
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
