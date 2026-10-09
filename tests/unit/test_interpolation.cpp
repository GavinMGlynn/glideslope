#include "harness.hpp"

#include "net/interpolation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

using glideslope::net::Interpolated;
using glideslope::net::RemoteState;
using glideslope::test::check;

namespace {

// **The aeroplane this is measured against**: a steady turn at speed, which
// is the case linear interpolation is worst at - a straight line between two
// points on a curve cuts the corner. 200 m/s is about Mach 0.6 at height,
// and 3 degrees a second is a rate-one turn.
constexpr double speed_mps = 200.0;
constexpr double turn_rate_deg_s = 3.0;
constexpr double climb_mps = 5.0;
constexpr double pi = 3.14159265358979323846;

// Where the aeroplane really is at `t`.
RemoteState truth(double t) {
    const double turn_rad_s = turn_rate_deg_s * pi / 180.0;
    const double radius_m = speed_mps / turn_rad_s;
    const double angle = turn_rad_s * t;
    RemoteState s;
    s.time_s = t;
    // A circle about a centre `radius_m` east of the start.
    s.north_m = radius_m * std::sin(angle);
    s.east_m = radius_m * (1.0 - std::cos(angle));
    s.down_m = -climb_mps * t;
    s.north_mps = speed_mps * std::cos(angle);
    s.east_mps = speed_mps * std::sin(angle);
    s.down_mps = -climb_mps;
    s.heading_deg = std::fmod(turn_rate_deg_s * t + 360.0, 360.0);
    s.pitch_deg = 2.0;
    s.roll_deg = 25.0;
    return s;
}

double how_far(const RemoteState& a, const RemoteState& b) {
    const double dn = a.north_m - b.north_m;
    const double de = a.east_m - b.east_m;
    const double dd = a.down_m - b.down_m;
    return std::sqrt(dn * dn + de * de + dd * dd);
}

// One run: snapshots at `rate_hz`, the ones named by `lose` never arriving,
// each arriving `jitter` late. Returns the worst distance between what was
// shown and where the aeroplane really was, over the whole run.
struct Run {
    double worst_m = 0.0;
    std::size_t shown = 0;
    std::size_t guessed = 0;
};

Run fly(double seconds, double rate_hz, std::uint32_t lose, int lose_width,
        const std::function<double(int)>& jitter) {
    struct Posted {
        double arrives_s;
        RemoteState state;
    };
    std::vector<Posted> post;
    const int snapshots = static_cast<int>(seconds * rate_hz);
    for (int i = 0; i < snapshots; ++i) {
        if (i < lose_width && (lose & (1u << i)) != 0) {
            continue; // this snapshot never arrives
        }
        const double sent = static_cast<double>(i) / rate_hz;
        post.push_back({sent + jitter(i), truth(sent)});
    }

    Interpolated shown_at;
    Run out;
    std::size_t next = 0;
    // **The moment the first snapshot covers.** Before it there is nothing to
    // interpolate between and nothing to carry on from, so the aircraft is
    // held at the oldest state known - which is a different thing from
    // interpolation error, and is held to its own test below. Measuring it
    // here would only measure how long the first packet took to arrive.
    const double covered_from_s = post.empty() ? 0.0 : post.front().state.time_s;
    // Rendered at 60 Hz, which is what a client would do.
    const int frames = static_cast<int>(seconds * 60.0);
    for (int f = 0; f < frames; ++f) {
        const double now = static_cast<double>(f) / 60.0;
        while (next < post.size() && post[next].arrives_s <= now) {
            shown_at.received(post[next].state);
            ++next;
        }
        const RemoteState got = shown_at.at(now);
        if (!shown_at.known()) {
            continue;
        }
        // It is shown where it was `shown_behind_s` ago, so that is what it
        // is held against.
        const double when = now - glideslope::net::shown_behind_s;
        if (when < covered_from_s) {
            continue; // before the first snapshot this run ever had
        }
        out.worst_m = std::max(out.worst_m, how_far(got, truth(when)));
        if (shown_at.extrapolating()) {
            ++out.guessed;
        }
        ++out.shown;
    }
    return out;
}

} // namespace

// **With every snapshot arriving, an aircraft is shown where it was.** This
// is the floor: whatever error there is here is linear interpolation cutting
// the corner of a turn, and nothing else.
GLIDESLOPE_TEST(with_every_snapshot_arriving_an_aircraft_is_shown_where_it_was) {
    const Run r = fly(4.0, 20.0, 0, 0, [](int) { return 0.0; });
    check(r.shown > 200, "it was shown " + std::to_string(r.shown) + " times");
    check(r.guessed == 0, "and never had to guess");
    // A rate-one turn at 200 m/s has a radius of 3,820 m; a straight line
    // across 50 ms of it cuts the corner by about 3 mm.
    check(r.worst_m < 0.05, "the worst error was " + std::to_string(r.worst_m) +
                                " m, which is more than 5 cm");
    std::printf("  no loss: worst %.4f m over %zu frames\n", r.worst_m, r.shown);
}

// **Under every pattern of loss, the error stays within its stated bound.**
// This is the item's own verification. Every pattern over twelve snapshots is
// tried - all 4,096 - at 20 Hz, so a pattern can take out 600 ms in a row and
// the aircraft is carried on from its velocity through it.
GLIDESLOPE_TEST(the_interpolation_error_stays_within_its_bound_under_every_pattern_of_loss) {
    // **The bound, stated**: two metres. An aeroplane carried on straight
    // while it is really turning at rate one leaves the turn by about 1.3 m
    // over the half second this project is prepared to guess for, and the
    // blend back adds no more than that again.
    constexpr double bound_m = 2.0;

    std::uint32_t walked = 0;
    double worst = 0.0;
    std::uint32_t worst_pattern = 0;
    std::size_t guessed = 0;
    for (std::uint32_t lose = 0; lose < 4096; ++lose) {
        const Run r = fly(2.0, 20.0, lose, 12, [](int) { return 0.0; });
        if (r.worst_m > worst) {
            worst = r.worst_m;
            worst_pattern = lose;
        }
        guessed += r.guessed;
        if (r.worst_m > bound_m) {
            glideslope::test::fail(
                "with loss pattern " + std::to_string(lose) + " the aircraft was " +
                std::to_string(r.worst_m) + " m from where it really was, which is "
                "more than the " + std::to_string(bound_m) + " m bound");
        }
        ++walked;
    }
    check(walked == 4096, "every pattern of loss was walked: " +
                              std::to_string(walked));
    check(guessed > 0, "and some of them had to be guessed through");
    std::printf("  4,096 loss patterns: worst %.3f m (pattern %u), bound %.1f m\n",
                worst, worst_pattern, bound_m);
}

// **And under jitter**, where snapshots arrive late and out of order.
GLIDESLOPE_TEST(the_interpolation_error_stays_within_its_bound_under_jitter) {
    constexpr double bound_m = 2.0;
    std::mt19937_64 random(20260922);
    double worst = 0.0;
    std::size_t walked = 0;
    for (int round = 0; round < 200; ++round) {
        // Up to 80 ms late, which at 20 Hz puts snapshots out of order.
        std::vector<double> late(64);
        for (double& d : late) {
            d = static_cast<double>(random() % 81) / 1000.0;
        }
        const Run r = fly(2.0, 20.0, 0, 0,
                          [&late](int i) { return late[static_cast<std::size_t>(i) %
                                                      late.size()]; });
        worst = std::max(worst, r.worst_m);
        if (r.worst_m > bound_m) {
            glideslope::test::fail("under jitter the aircraft was " +
                                   std::to_string(r.worst_m) +
                                   " m from where it really was");
        }
        ++walked;
    }
    check(walked == 200, "two hundred rounds of jitter were flown");
    std::printf("  jitter up to 80 ms: worst %.3f m, bound %.1f m\n", worst, bound_m);
}

// **A guess is held for a while and then no longer**, so that an aircraft
// nobody has heard from does not sail off across the sky.
GLIDESLOPE_TEST(an_aircraft_nobody_has_heard_from_is_not_carried_on_for_ever) {
    Interpolated shown;
    shown.received(truth(0.0));
    shown.received(truth(0.05));
    // Nothing more arrives. Where is it a long time later?
    const RemoteState soon = shown.at(0.15 + glideslope::net::shown_behind_s);
    const RemoteState late = shown.at(10.0);
    const double carried =
        how_far(late, shown.at(0.05 + glideslope::net::extrapolate_at_most_s +
                               glideslope::net::shown_behind_s));
    check(shown.extrapolating(), "it is guessing");
    check(carried < 1e-6, "and stopped carrying on after " +
                              std::to_string(glideslope::net::extrapolate_at_most_s) +
                              " s, rather than for ever");
    // Half a second of guessing at 200 m/s is 100 m and no more.
    const double went = how_far(late, truth(0.05));
    check(went < 120.0, "it went " + std::to_string(went) +
                            " m, which is further than half a second's guessing");
    std::printf("  a lost aircraft carried on %.0f m and then stopped\n", went);
    (void)soon;
}

// **Angles take the short way round.** An aircraft turning through north
// must not spin the wrong way round the compass to get there.
GLIDESLOPE_TEST(an_aircraft_turning_through_north_takes_the_short_way_round) {
    check(glideslope::net::shortest_turn_deg(350.0, 10.0) == 20.0,
          "350 to 10 is 20 degrees right");
    check(glideslope::net::shortest_turn_deg(10.0, 350.0) == -20.0,
          "and back is 20 degrees left");
    check(glideslope::net::shortest_turn_deg(0.0, 180.0) == 180.0, "half a turn");
    check(glideslope::net::shortest_turn_deg(0.0, 181.0) == -179.0,
          "and just past it goes the other way");

    Interpolated shown;
    RemoteState a = truth(0.0);
    a.heading_deg = 350.0;
    RemoteState b = truth(0.1);
    b.heading_deg = 10.0;
    shown.received(a);
    shown.received(b);
    const RemoteState half = shown.at(0.05 + glideslope::net::shown_behind_s);
    // Halfway from 350 to 10 the short way is 0, not 180.
    const double from_north = std::abs(glideslope::net::shortest_turn_deg(
        half.heading_deg, 0.0));
    check(from_north < 1.0, "halfway is heading " + std::to_string(half.heading_deg) +
                                ", which should be about north");
}

// **The client follows the server's clock at the server's own rate.** A
// server at 80%, 100% and 125% of real time, each with no jitter, 30 ms and
// 60 ms of it, over 100 ms of latency and one update in twenty lost - nine
// sessions of a minute each, updates twenty-five a second. What the client
// can know is the session's time less the least latency, and after the first
// two seconds (the window the rate is fitted over) its estimate must never
// run more than 20 ms ahead of that nor more than 50 ms behind. The client
// draws 100 ms behind the clock: a fifth of that ahead still draws between
// updates, and half of it behind only spends some of the margin. (Its frames
// come sixty a second, so an update is heard up to 17 ms after it arrived,
// which is most of what behind there is.) A clock that assumed real time,
// as the first one did, is 12 s ahead of an 80% server after a minute.
GLIDESLOPE_TEST(the_client_follows_the_servers_clock_at_the_servers_own_rate) {
    constexpr double least_latency_s = 0.100;
    std::size_t sessions = 0;
    double worst_ahead_s = 0.0;
    double worst_behind_s = 0.0;
    for (const double rate : {0.8, 1.0, 1.25}) {
        for (const double jitter_s : {0.0, 0.030, 0.060}) {
            std::mt19937_64 random(7);
            std::uniform_real_distribution<double> unit(0.0, 1.0);
            // Updates as the server sends them, then as they arrive, in
            // arrival order.
            struct Arrival {
                double local_s;
                double session_s;
            };
            std::vector<Arrival> arrivals;
            for (double session_s = 0.0; session_s < 60.0 * rate; session_s += 0.04) {
                if (unit(random) < 0.05) continue;
                const double sent_local_s = session_s / rate;
                arrivals.push_back({sent_local_s + least_latency_s + jitter_s * unit(random),
                                    session_s});
            }
            std::sort(arrivals.begin(), arrivals.end(),
                      [](const Arrival& a, const Arrival& b) { return a.local_s < b.local_s; });
            glideslope::net::SessionClock clock;
            std::size_t next = 0;
            std::size_t judged = 0;
            for (double local_s = 0.0; local_s < 60.0; local_s += 1.0 / 60.0) {
                for (; next < arrivals.size() && arrivals[next].local_s <= local_s; ++next) {
                    clock.heard(arrivals[next].session_s, local_s);
                }
                if (!clock.known() || local_s < arrivals.front().local_s + 2.0) continue;
                const double knowable_s = (local_s - least_latency_s) * rate;
                const double error_s = clock.now(local_s) - knowable_s;
                worst_ahead_s = std::max(worst_ahead_s, error_s);
                worst_behind_s = std::min(worst_behind_s, error_s);
                check(error_s <= 0.020,
                      "at " + std::to_string(rate) + " of real time with " +
                          std::to_string(jitter_s * 1000.0) + " ms of jitter the clock ran " +
                          std::to_string(error_s * 1000.0) + " ms ahead at " +
                          std::to_string(local_s) + " s");
                check(error_s >= -0.050,
                      "at " + std::to_string(rate) + " of real time with " +
                          std::to_string(jitter_s * 1000.0) + " ms of jitter the clock ran " +
                          std::to_string(-error_s * 1000.0) + " ms behind at " +
                          std::to_string(local_s) + " s");
                ++judged;
            }
            check(judged > 3000, "the clock was judged at " + std::to_string(judged) +
                                     " frames of a minute");
            check(std::abs(clock.rate() - rate) < 0.01,
                  "the rate fitted is " + std::to_string(clock.rate()) + ", not " +
                      std::to_string(rate));
            ++sessions;
        }
    }
    check(sessions == 9, "nine sessions, three rates by three jitters, not " +
                             std::to_string(sessions));
    std::printf("  9 sessions: at worst %.1f ms ahead and %.1f ms behind\n",
                worst_ahead_s * 1000.0, -worst_behind_s * 1000.0);
}

// **The velocity an aircraft is drawn moving at is its path's own**, in
// every state the path can be in: before anything straddles the moment,
// between two snapshots, carried on through a gap, held still past the
// guess, and taking up an offset after a guess. Each frame's is compared
// with how far the answer moves in the next hundred-thousandth of a second.
// The snapshots report velocities a tenth slower than the aircraft flies, so
// an answer that gave the reported velocity between two snapshots, as the
// client once carried aircraft on at, is 20 m/s out.
GLIDESLOPE_TEST(the_velocity_an_aircraft_is_drawn_moving_at_is_its_paths_own) {
    enum Regime { before, between, carried, held, taking_up, regimes };
    const char* const names[regimes] = {"before the first snapshot", "between two",
                                        "carried on", "held past the guess",
                                        "taking up an offset"};
    std::size_t seen[regimes] = {};
    // Snapshots at 20 Hz for four seconds, lost from 1 s to 1.3 s (a guess,
    // then a take-up) and from 2 s to 3 s (a guess held past its limit).
    Interpolated shown;
    std::vector<RemoteState> post;
    for (int i = 0; i < 80; ++i) {
        const double t = i / 20.0;
        if ((t >= 1.0 && t < 1.3) || (t >= 2.0 && t < 3.0)) continue;
        RemoteState s = truth(t);
        s.north_mps *= 0.9;
        s.east_mps *= 0.9;
        s.down_mps *= 0.9;
        post.push_back(s);
    }
    std::size_t next = 0;
    double newest_s = -1.0;
    double guess_ended_s = -10.0;
    bool was_guessing = false;
    double worst = 0.0;
    // Frames at 60 Hz, a little off the snapshots' times, so that no frame
    // falls exactly where one straight line meets the next.
    for (int k = 0; k < 250; ++k) {
        const double now = k / 60.0 + 0.0003;
        const double want = now - glideslope::net::shown_behind_s;
        for (; next < post.size() && post[next].time_s <= now; ++next) {
            shown.received(post[next]);
            newest_s = post[next].time_s;
        }
        if (!shown.known()) continue;
        const RemoteState here = shown.at(now);
        const std::array<double, 3> v = shown.path_velocity();
        Interpolated probe = shown;
        constexpr double h = 1e-5;
        const RemoteState then = probe.at(now + h);
        const double moved[3] = {(then.north_m - here.north_m) / h,
                                 (then.east_m - here.east_m) / h,
                                 (then.down_m - here.down_m) / h};
        double off = 0.0;
        for (std::size_t i = 0; i < 3; ++i) {
            off = std::max(off, std::abs(moved[i] - v[i]));
        }
        Regime r = between;
        if (shown.extrapolating()) {
            r = want - newest_s < glideslope::net::extrapolate_at_most_s ? carried : held;
        } else if (was_guessing || now - guess_ended_s < glideslope::net::blend_s) {
            if (was_guessing) guess_ended_s = now;
            r = taking_up;
        } else if (want <= post.front().time_s) {
            r = before;
        }
        was_guessing = shown.extrapolating();
        ++seen[r];
        worst = std::max(worst, off);
        check(off < 1e-3, std::string("at ") + std::to_string(now) + " s, " + names[r] +
                              ", the velocity said is " + std::to_string(off) +
                              " m/s from the path's own");
    }
    for (int r = 0; r < regimes; ++r) {
        check(seen[r] > 0, std::string("no frame was ") + names[r]);
    }
    std::printf("  frames %zu before, %zu between, %zu carried, %zu held, %zu taking up; "
                "worst %.2g m/s\n",
                seen[before], seen[between], seen[carried], seen[held], seen[taking_up], worst);
}

// **An aircraft drawn again after a pause is drawn where it is**, not where a
// guess from before the pause, taken up, would put it. Drawn while guessing
// through a gap, left undrawn for three seconds while updates kept coming,
// and then drawn again: it must be exactly where an aircraft never drawn
// before would be, and moving as that one moves. Taking up the old guess
// moved a client's own aircraft hundreds of metres in a quarter of a second
// (2026-09-26).
GLIDESLOPE_TEST(an_aircraft_drawn_again_after_a_pause_is_drawn_where_it_is) {
    Interpolated drawn;
    Interpolated fresh;
    // Updates twenty a second, each heard as it is sent, with a gap from
    // 1.0 s to 1.6 s that the frames guess through.
    std::vector<RemoteState> sent;
    for (int i = 0; i < 100; ++i) {
        const double t = i / 20.0;
        if (t >= 1.0 && t < 1.6) continue;
        sent.push_back(truth(t));
    }
    std::size_t next = 0;
    std::size_t guessed = 0;
    // Drawn sixty times a second up to 1.5 s, the last frames a guess.
    for (int k = 0; k <= 90; ++k) {
        const double now = k / 60.0 + 0.0003;
        for (; next < sent.size() && sent[next].time_s <= now; ++next) {
            drawn.received(sent[next]);
        }
        (void)drawn.at(now);
        if (drawn.extrapolating()) ++guessed;
    }
    check(drawn.extrapolating(), "the last frame before the pause was a guess");
    check(guessed > 0, "and some frames were guessed");
    // Three seconds on, with the updates since heard, drawn again.
    const double again = 4.5003;
    for (; next < sent.size() && sent[next].time_s <= again; ++next) {
        drawn.received(sent[next]);
    }
    for (const RemoteState& s : sent) {
        if (s.time_s <= again) fresh.received(s);
    }
    const RemoteState here = drawn.at(again);
    const std::array<double, 3> v = drawn.path_velocity();
    const RemoteState should = fresh.at(again);
    const std::array<double, 3> should_v = fresh.path_velocity();
    const double off = how_far(here, should);
    double off_v = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        off_v = std::max(off_v, std::abs(v[i] - should_v[i]));
    }
    check(off < 1e-9, "drawn again, it was " + std::to_string(off) +
                          " m from where it is");
    check(off_v < 1e-9, "and moving " + std::to_string(off_v) + " m/s off its path");
    std::printf("  %zu frames guessed before the pause; after it, %.3g m and %.3g m/s off\n",
                guessed, off, off_v);
}

// **An aircraft guessed across updates that all come late comes back from
// what was shown.** For a second every update arrives after the moment it
// is about has been drawn, as every one does to a client whose clock runs
// ahead - so the aircraft is guessed, frame after frame, each guess carried
// on from a newer update than the last - and then they arrive in time again.
// Every frame must be within the 2 m bound of where it was, and none may
// step more than half a metre beyond its own motion. Kept from the first
// update of the run, the guess came back from where that one's stopped,
// half a second of flight behind: 94.8 m here at 200 m/s, and 61.8 m in the
// 200 ms network check with its programs held to one core (2026-09-30).
GLIDESLOPE_TEST(an_aircraft_guessed_across_late_updates_comes_back_from_what_was_shown) {
    constexpr double bound_m = 2.0;
    struct Posted {
        double arrives_s;
        RemoteState state;
    };
    std::vector<Posted> post;
    for (int i = 0; i < 75; ++i) {
        const double t = i * 0.04;
        const bool late = t >= 1.0 && t < 2.0;
        post.push_back({t + (late ? 0.15 : 0.02), truth(t)});
    }
    std::sort(post.begin(), post.end(),
              [](const Posted& a, const Posted& b) { return a.arrives_s < b.arrives_s; });
    Interpolated shown;
    std::size_t next = 0;
    double worst = 0.0;
    double worst_step = 0.0;
    double longest_guess_s = 0.0;
    double guess_began_s = -1.0;
    std::size_t moved_on = 0;
    double guessed_from_s = -1.0;
    RemoteState last{};
    bool have_last = false;
    for (int k = 0; k < 180; ++k) {
        const double now = k / 60.0 + 0.0003;
        for (; next < post.size() && post[next].arrives_s <= now; ++next) {
            shown.received(post[next].state);
        }
        if (!shown.known()) continue;
        const RemoteState got = shown.at(now);
        const double want = now - glideslope::net::shown_behind_s;
        if (want < 0.0) continue;
        const double off = how_far(got, truth(want));
        worst = std::max(worst, off);
        check(off < bound_m, "at " + std::to_string(now) + " s it was " + std::to_string(off) +
                                 " m from where it was");
        if (shown.extrapolating()) {
            if (guess_began_s < 0.0) guess_began_s = now;
            longest_guess_s = std::max(longest_guess_s, now - guess_began_s);
            if (guessed_from_s >= 0.0 && shown.newest_s() != guessed_from_s) ++moved_on;
            guessed_from_s = shown.newest_s();
        } else {
            guess_began_s = -1.0;
            guessed_from_s = -1.0;
        }
        // A step: how far it moved beyond what 200 m/s covers in a frame.
        if (have_last) {
            const double step = how_far(got, last) - speed_mps / 60.0;
            worst_step = std::max(worst_step, step);
        }
        last = got;
        have_last = true;
    }
    // The situation, built: a guess carried across late updates for longer
    // than a guess is held.
    check(longest_guess_s > glideslope::net::extrapolate_at_most_s,
          "the longest run of guesses was " + std::to_string(longest_guess_s) + " s");
    check(moved_on >= 20, "a guess moved on to a newer update " + std::to_string(moved_on) +
                              " times, of the 25 that came late");
    check(worst_step < 0.5, "a frame stepped " + std::to_string(worst_step) +
                                " m beyond its own motion");
    std::printf("  guessed for %.2f s across %zu late updates: worst %.3f m, a step at most "
                "%.3f m beyond its motion\n",
                longest_guess_s, moved_on, worst, worst_step);
}

// **And from one guess to the next without a jump, through a manoeuvre.**
// The same second of late updates, from an aeroplane jinking - 100 m/s north
// and 60 m/s east or west, turning about every fifth of a second - so that
// a guess carried on from one update and the guess from the next are metres
// apart. Moving on to the newer one must be taken up, not jumped: no frame
// may move more than a metre beyond the fastest the aeroplane flies in
// a frame. Taking the newer update as it came, with no blend, jumped the
// aircraft by the difference between the guesses at once.
GLIDESLOPE_TEST(an_aircraft_guessed_through_a_manoeuvre_moves_between_guesses_without_a_jump) {
    constexpr double north_mps = 100.0;
    constexpr double east_mps = 60.0;
    constexpr double leg_s = 0.21;
    // Where the jinking aeroplane is at `t`, and how fast it is going.
    const auto jinking = [](double t) {
        RemoteState s;
        s.time_s = t;
        const int legs = static_cast<int>(t / leg_s);
        double east = 0.0;
        for (int i = 0; i < legs; ++i) east += (i % 2 == 0 ? east_mps : -east_mps) * leg_s;
        const double sign = legs % 2 == 0 ? 1.0 : -1.0;
        east += sign * east_mps * (t - legs * leg_s);
        s.north_m = north_mps * t;
        s.east_m = east;
        s.north_mps = north_mps;
        s.east_mps = sign * east_mps;
        return s;
    };
    struct Posted {
        double arrives_s;
        RemoteState state;
    };
    std::vector<Posted> post;
    for (int i = 0; i < 75; ++i) {
        const double t = i * 0.04;
        const bool late = t >= 1.0 && t < 2.0;
        post.push_back({t + (late ? 0.15 : 0.02), jinking(t)});
    }
    std::sort(post.begin(), post.end(),
              [](const Posted& a, const Posted& b) { return a.arrives_s < b.arrives_s; });
    const double fastest_per_frame =
        std::sqrt(north_mps * north_mps + east_mps * east_mps) / 60.0;
    Interpolated shown;
    std::size_t next = 0;
    double worst_step = 0.0;
    double widest_between_guesses = 0.0;
    std::size_t moved_on = 0;
    double guessed_from_s = -1.0;
    RemoteState last{};
    bool have_last = false;
    for (int k = 0; k < 180; ++k) {
        const double now = k / 60.0 + 0.0003;
        for (; next < post.size() && post[next].arrives_s <= now; ++next) {
            shown.received(post[next].state);
        }
        if (!shown.known()) continue;
        const RemoteState got = shown.at(now);
        const double want = now - glideslope::net::shown_behind_s;
        if (shown.extrapolating()) {
            if (guessed_from_s >= 0.0 && shown.newest_s() != guessed_from_s) {
                ++moved_on;
                // How far apart the guess it had and the one it moved on to
                // are at this moment: what a jump would have been.
                const RemoteState a = jinking(guessed_from_s);
                const RemoteState b = jinking(shown.newest_s());
                const double da = want - a.time_s;
                const double db = want - b.time_s;
                widest_between_guesses = std::max(
                    widest_between_guesses,
                    std::hypot(a.north_m + a.north_mps * da - b.north_m - b.north_mps * db,
                               a.east_m + a.east_mps * da - b.east_m - b.east_mps * db));
            }
            guessed_from_s = shown.newest_s();
        } else {
            guessed_from_s = -1.0;
        }
        if (have_last) {
            const double step = how_far(got, last) - fastest_per_frame;
            worst_step = std::max(worst_step, step);
            check(step < 1.0, "at " + std::to_string(now) + " s it stepped " +
                                  std::to_string(step) + " m beyond the fastest it flies");
        }
        last = got;
        have_last = true;
    }
    // The situation, built: guesses moved on from, and metres between them.
    check(moved_on >= 20, "a guess moved on to a newer update " + std::to_string(moved_on) +
                              " times, of the 25 that came late");
    check(widest_between_guesses > 3.0,
          "the guesses moved between were at most " + std::to_string(widest_between_guesses) +
              " m apart: no manoeuvre was guessed through");
    std::printf("  %zu guesses moved on from, as much as %.2f m apart; a step at most %.3f m "
                "beyond its motion\n",
                moved_on, widest_between_guesses, worst_step);
}

// **A wreck that flies again is drawn where it flies again, at once**, and
// not moved there from the wreck - even when it was being guessed at when
// the news came, and when an update from before it flew again arrives late.
// Blended, it would cross kilometres in a quarter of a second, and say it
// was moving at thousands of metres a second to anything that took its
// motion - a take-over hands an aircraft over moving as it was drawn.
GLIDESLOPE_TEST(a_wreck_that_flies_again_is_drawn_where_it_flies_again_moving_sanely) {
    Interpolated shown;
    // A wreck, still, heard until 1.0 s, the last two updates late - so it
    // is being guessed at - and then flying again 5 km away from 1.2 s,
    // straight north at 50 m/s.
    RemoteState wreck;
    wreck.wrecked = true;
    const auto flying = [](double t) {
        RemoteState s;
        s.time_s = t;
        s.north_m = 5000.0 + 50.0 * (t - 1.2);
        s.north_mps = 50.0;
        return s;
    };
    std::size_t guessed = 0;
    for (int k = 0; k <= 72; ++k) {
        const double now = k / 60.0 + 0.0003;
        for (double t = 0.0; t <= std::min(now - 0.02, 0.92); t += 0.04) {
            wreck.time_s = std::round(t / 0.04) * 0.04;
            shown.received(wreck);
        }
        (void)shown.at(now);
        if (shown.extrapolating()) ++guessed;
    }
    check(guessed > 0, "the wreck was being guessed at when it flew again");
    shown.received(flying(1.2));
    // An update from while it was a wreck, late.
    wreck.time_s = 0.96;
    shown.received(wreck);
    double worst_from_m = 0.0;
    double fastest_mps = 0.0;
    std::size_t drawn = 0;
    for (int k = 0; k < 30; ++k) {
        const double now = 1.2 + 0.02 + k / 60.0;
        for (double t = 1.24; t <= now - 0.02; t += 0.04) shown.received(flying(t));
        const RemoteState got = shown.at(now);
        const std::array<double, 3> v = shown.path_velocity();
        // Where it flies, from the moment it flew again: never nearer the
        // wreck than that.
        const double want = std::max(1.2, now - glideslope::net::shown_behind_s);
        worst_from_m = std::max(worst_from_m, how_far(got, flying(want)));
        fastest_mps = std::max(fastest_mps, std::hypot(v[0], v[1], v[2]));
        ++drawn;
    }
    check(worst_from_m < 1.0, "flying again, it was drawn " + std::to_string(worst_from_m) +
                                  " m from where it flew");
    check(fastest_mps < 60.0, "and said to be moving at " + std::to_string(fastest_mps) +
                                  " m/s, where it flies at 50");
    std::printf("  %zu frames after flying again: at worst %.3f m off and %.1f m/s\n", drawn,
                worst_from_m, fastest_mps);
}

namespace {

// Updates thirty a second over 100 ms of latency and 10 ms of jitter, heard
// as the window client hears them: a pass at a time, every one at the
// pass's own time. Every third pass is long - `long_s`, or up to it at
// random - and the rest 17 ms. The worst step of the clock drawn at: how far
// a frame's time is from where the frame before, moving on at the rate it
// gave, carries it.
struct ClockRun {
    double worst_step_s = 0.0;
    double worst_step_share = 0.0; // of its frame's length
    std::size_t frames = 0;
};
template <typename Drawn>
ClockRun run_passes(unsigned seed, double long_s, bool at_random, Drawn drawn) {
    std::mt19937 random(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    struct Arrival {
        double local_s;
        double session_s;
    };
    std::vector<Arrival> arrivals;
    for (int i = 0; i < 60 * 30; ++i) {
        const double session_s = i / 30.0;
        arrivals.push_back({session_s + 0.100 + 0.010 * unit(random), session_s});
    }
    std::sort(arrivals.begin(), arrivals.end(),
              [](const Arrival& a, const Arrival& b) { return a.local_s < b.local_s; });
    glideslope::net::SessionClock clock;
    ClockRun run;
    std::size_t next = 0;
    double local_s = 0.0;
    bool before = false;
    double before_local_s = 0.0;
    double before_s = 0.0;
    double before_rate = 1.0;
    for (int pass = 0; local_s < 60.0; ++pass) {
        local_s += pass % 3 == 0 ? long_s * (at_random ? unit(random) : 1.0) : 0.017;
        for (; next < arrivals.size() && arrivals[next].local_s <= local_s; ++next) {
            clock.heard(arrivals[next].session_s, local_s);
        }
        if (!clock.known()) {
            continue;
        }
        double rate = 1.0;
        const double shown_s = drawn(local_s, clock, rate);
        // Judged after the fit's first window, as a player sees it.
        if (before && local_s > 3.0) {
            const double dt = local_s - before_local_s;
            const double step = std::abs(shown_s - before_s - before_rate * dt);
            run.worst_step_s = std::max(run.worst_step_s, step);
            if (dt > 0.0) {
                run.worst_step_share = std::max(run.worst_step_share, step / dt);
            }
            ++run.frames;
        }
        before = true;
        before_local_s = local_s;
        before_s = shown_s;
        before_rate = rate;
    }
    return run;
}

} // namespace

// **The clock aircraft are drawn at does not step under long, uneven
// frames** (net::ShownClock). The session's fitted clock jumped by up to a
// pass when the window client's passes were long and uneven, because every
// update heard in a pass is heard at the pass's time: 88.9 ms in one frame
// with passes up to 400 ms, 165.5 ms up to 700 ms - 3.8 and 7.0 m at 42 m/s,
// the kind of CI's 5.4 m step (PROJECT_STATUS.md, 2026-10-09). The table:
// passes of 17 ms; 329 ms every third pass; up to 400 ms and up to 700 ms at
// random - five seeds each, twenty runs of a minute. The bound on a frame's
// step is what the slew can move in it, a tenth of the frame - at 42 m/s
// and 700 ms, 2.9 m; and the clock drawn at stays within the most a pass
// can put the fit out, 0.7 s, of it.
GLIDESLOPE_TEST(the_clock_aircraft_are_drawn_at_does_not_step_under_long_uneven_frames) {
    struct Case {
        double long_s;
        bool at_random;
    };
    const Case cases[] = {{0.017, false}, {0.329, false}, {0.400, true}, {0.700, true}};
    std::size_t runs = 0;
    for (const Case& c : cases) {
        double worst_s = 0.0;
        double worst_share = 0.0;
        for (unsigned seed = 0; seed < 5; ++seed) {
            glideslope::net::ShownClock shown;
            const ClockRun run = run_passes(
                seed, c.long_s, c.at_random,
                [&](double local_s, const glideslope::net::SessionClock& clock, double& rate) {
                    const double s = shown.at(local_s, clock);
                    rate = shown.rate();
                    check(std::abs(s - clock.now(local_s)) <= 0.7,
                          "the clock drawn at is more than 0.7 s from the session's");
                    return s;
                });
            check(run.frames > 100, "the clock was judged at " + std::to_string(run.frames) +
                                        " frames of a minute");
            worst_s = std::max(worst_s, run.worst_step_s);
            worst_share = std::max(worst_share, run.worst_step_share);
            ++runs;
        }
        check(worst_share <= glideslope::net::ShownClock::most_slew + 1e-9,
              "with passes " + std::string(c.at_random ? "up to " : "of ") +
                  std::to_string(c.long_s * 1000.0) + " ms the clock drawn at stepped " +
                  std::to_string(worst_s * 1000.0) + " ms in a frame, " +
                  std::to_string(worst_share * 100.0) + "% of it, past the slew's tenth");
        std::printf("  passes %s %.0f ms: the worst step %.2f ms (%.2f m at 42 m/s), %.1f%% "
                    "of its frame\n",
                    c.at_random ? "up to" : "of", c.long_s * 1000.0, worst_s * 1000.0,
                    worst_s * 42.0, worst_share * 100.0);
    }
    check(runs == 20, "four kinds of pass by five seeds, not " + std::to_string(runs));
}

// **A real change of the session's clock is followed**: one within
// ShownClock::snap_beyond_s settles to a millisecond within
// settles_within_s; one past it is taken at once, and what the fit does
// after it - it has heard nothing for two seconds - settles as well. A
// server at real time, updates thirty a second heard at 60 frames a second,
// and its latency changed at 20 s by 0.3 s either way (the fit takes it as
// an offset) - and by 2 s, as a server stalled and caught up.
GLIDESLOPE_TEST(a_real_change_of_the_sessions_clock_is_followed_within_its_stated_time) {
    std::size_t cases = 0;
    for (const double change_s : {0.3, -0.3, 2.0}) {
        glideslope::net::SessionClock clock;
        glideslope::net::ShownClock shown;
        double settled_at_s = -1.0;
        for (int frame = 0; frame < 60 * 60; ++frame) {
            const double local_s = frame / 60.0;
            const double latency_s = 0.1 + (local_s >= 20.0 ? change_s : 0.0);
            const double session_s = local_s - latency_s;
            if (frame % 2 == 0 && session_s > 0.0) {
                clock.heard(session_s, local_s);
            }
            if (!clock.known()) {
                continue;
            }
            const double off = std::abs(shown.at(local_s, clock) - clock.now(local_s));
            if (local_s >= 20.0) {
                if (off > 0.001) {
                    settled_at_s = -1.0;
                } else if (settled_at_s < 0.0) {
                    settled_at_s = local_s;
                }
            }
        }
        check(settled_at_s >= 20.0,
              "a change of " + std::to_string(change_s) + " s never settled to a millisecond");
        const double took_s = settled_at_s - 20.0;
        const bool snapped = std::abs(change_s) > glideslope::net::ShownClock::snap_beyond_s;
        const double within_s = glideslope::net::ShownClock::settles_within_s;
        check(took_s <= within_s, "a change of " + std::to_string(change_s) + " s took " +
                                      std::to_string(took_s) + " s to settle, past " +
                                      std::to_string(within_s));
        check(shown.snaps() == (snapped ? 1 : 0),
              "a change of " + std::to_string(change_s) + " s was taken at once " +
                  std::to_string(shown.snaps()) + " times");
        std::printf("  a change of %+.1f s settled to a millisecond in %.2f s\n", change_s,
                    took_s);
        ++cases;
    }
    check(cases == 3, "three changes, not " + std::to_string(cases));
}
