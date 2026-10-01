#include "harness.hpp"

#include "frontend/client/pass.hpp"
#include "sim/aircraft.hpp"
#include "sim/prediction.hpp"
#include "sim/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
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
