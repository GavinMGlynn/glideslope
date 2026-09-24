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
                by_motion ? client.reconcile(post.front().motion, post.front().last_applied)
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
        server_aircraft.motion(), static_cast<std::uint32_t>(steps_per_second * 3));
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
