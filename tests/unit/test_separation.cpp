#include "harness.hpp"

#include "sim/aircraft.hpp"
#include "sim/autopilot.hpp"
#include "sim/controller.hpp"
#include "sim/plan.hpp"
#include "sim/separation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

using glideslope::sim::HeightLimit;
using glideslope::sim::Separation;
using glideslope::sim::Traffic;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;
constexpr double apart_ft = Separation::minimum_ft + Separation::margin_ft;
constexpr double metres_per_degree = 6371000.0 * std::numbers::pi / 180.0;

// An aircraft at Sydney, `north_m` north of the point the cases are built
// round, at `altitude_ft`, flying north at `north_kts` (south if negative).
Traffic at(double north_m, double altitude_ft, double north_kts, bool gives_way,
           std::optional<double> held_ft) {
    Traffic t;
    t.latitude_deg = -33.9 + north_m / metres_per_degree;
    t.longitude_deg = 151.2;
    t.altitude_ft = altitude_ft;
    t.north_fps = north_kts * 1852.0 / 3600.0 / 0.3048;
    t.gives_way = gives_way;
    t.held_ft = held_ft;
    return t;
}

bool none(const HeightLimit& l) {
    return !l.floor_ft && !l.ceiling_ft;
}

bool about(std::optional<double> x, double want) {
    return x && std::abs(*x - want) < 1.0;
}

} // namespace

// **Every rule the monitor keeps, each built and checked** - which side an
// aircraft is held to, when, of whom, and when not: ten rules, each built in
// a block of its own.
GLIDESLOPE_TEST(the_monitor_holds_an_aircraft_that_gives_way_off_the_heights_of_those_it_gives_way_to_and_no_other) {
    // Below another, near, flying to a height above it: held below it.
    {
        const auto l = glideslope::sim::separate(
            {at(0, 3000, 100, true, 3000), at(-1000, 2000, 100, true, 4000)});
        check(none(l[0]), "the first gives way to nobody after it");
        check(about(l[1].ceiling_ft, 3000 - apart_ft) && !l[1].floor_ft &&
                  l[1].clear_of == std::size_t{0},
              "climbing through the first's height near it, the second is held below it");
    }
    // Above another, near: held above it.
    {
        const auto l = glideslope::sim::separate(
            {at(0, 3000, 100, true, 3000), at(-1000, 4000, 100, true, 2000)});
        check(about(l[1].floor_ft, 3000 + apart_ft) && !l[1].ceiling_ft,
              "descending through the first's height near it, the second is held above it");
    }
    // Far apart, and parting: nothing.
    {
        const auto l = glideslope::sim::separate(
            {at(0, 3000, 100, true, 3000), at(-20000, 2000, -100, true, 4000)});
        check(none(l[1]), "20 km apart and parting, nothing is held");
    }
    // Far apart, but meeting head on within the lookahead: held already.
    {
        const auto l = glideslope::sim::separate(
            {at(0, 3000, 100, true, 3000), at(12000, 2000, -100, true, 4000)});
        check(about(l[1].ceiling_ft, 3000 - apart_ft),
              "12 km apart and closing at 200 kt, the second is held below the first already");
    }
    // One that does not give way - a person's, one taking off - is given way
    // to by an AI aircraft before it in the order, and is given nothing.
    {
        const auto l = glideslope::sim::separate(
            {at(-1000, 2000, 100, true, 4000), at(0, 3000, 100, false, std::nullopt)});
        check(about(l[0].ceiling_ft, 3000 - apart_ft) && l[0].clear_of == std::size_t{1},
              "an AI aircraft gives way to a person's after it in the order");
        check(none(l[1]), "a person's aircraft is given no limit");
    }
    // Level with the other: the side it is going to.
    {
        const auto l = glideslope::sim::separate(
            {at(0, 3000, 100, true, 3000), at(-1000, 3020, 100, true, 2000),
             at(20000, 3000, 100, true, 3000), at(19000, 2990, 100, true, 4000)});
        check(about(l[1].ceiling_ft, 3000 - apart_ft),
              "level with another and going down, it is held below");
        check(about(l[3].floor_ft, 3000 + apart_ft),
              "level with another and going up, it is held above");
    }
    // A ceiling into the ground: the floor instead.
    {
        std::vector<Traffic> t{at(0, 1500, 100, true, 1500), at(-1000, 1200, 100, true, 1200)};
        t[1].ground_ft = 500.0;
        const auto l = glideslope::sim::separate(t);
        check(about(l[1].floor_ft, 1500 + apart_ft) && !l[1].ceiling_ft,
              "held below another it would be within 500 ft of the ground, so above it instead");
    }
    // Another climbing with no height to fly to: the heights it will climb
    // through in the lookahead are kept clear of too.
    {
        std::vector<Traffic> t{at(0, 3000, 100, false, std::nullopt),
                               at(-1000, 4500, 100, true, 4500)};
        t[0].climb_fpm = 600.0;
        const auto l = glideslope::sim::separate(t);
        check(about(l[1].floor_ft, 3000 + 600.0 * Separation::lookahead_s / 60.0 + apart_ft),
              "above one climbing, it is held above where that one climbs to in 90 s");
    }
    // Between two that leave no room, near both: held in the middle of the
    // gap and turned directly away from the one it would pass through; 1.5 nm
    // from that one, taken past it - above them all or below, whichever asks
    // less - still turned away.
    {
        const auto l = glideslope::sim::separate({at(0, 3000, 100, true, 3000),
                                                  at(500, 3800, 100, true, 3800),
                                                  at(-1000, 3300, 100, true, 3300)});
        // At 3,300 ft, between one at 3,000 and one at 3,800: above them all
        // is 4,500 ft, 1,200 up; below them all 2,300, 1,000 down - so down,
        // past the first, 1,000 m south of it: turned south, held at 3,400.
        check(about(l[2].floor_ft, 3400) && about(l[2].ceiling_ft, 3400) &&
                  l[2].clear_of == std::size_t{0} && l[2].heading_deg &&
                  std::abs(std::remainder(*l[2].heading_deg - 180.0, 360.0)) < 1.0,
              "squeezed between two near both, it is held in the middle and turned away");
        const auto far = glideslope::sim::separate({at(0, 3000, 100, true, 3000),
                                                    at(-2500, 3800, 100, true, 3800),
                                                    at(-3000, 3300, 100, true, 3300)});
        check(about(far[2].ceiling_ft, 3000 - apart_ft) && !far[2].floor_ft &&
                  far[2].heading_deg &&
                  std::abs(std::remainder(*far[2].heading_deg - 180.0, 360.0)) < 1.0,
              "squeezed, but 1.5 nm from the one it passes, it is taken below them all, "
              "still turned away");
        check(!l[0].heading_deg && !l[1].heading_deg, "nothing is turned that is not squeezed");
    }
    // A later one keeps clear of the height an earlier one is held to, not
    // the one it would have flown to.
    {
        const auto l = glideslope::sim::separate({at(0, 4000, 100, false, std::nullopt),
                                                  at(-500, 3500, 100, true, 3500),
                                                  at(-1000, 2000, 100, true, 4000)});
        check(about(l[1].ceiling_ft, 4000 - apart_ft), "the second is held below the first");
        check(about(l[2].ceiling_ft, 4000 - apart_ft - apart_ft),
              "the third keeps clear of the height the second is held to");
    }
}

namespace {

// **Two Cessnas, flown in one sky**, the second built to lose separation
// from the first: what the monitor is for. Each is the AI's, holding a
// heading, a height and a speed; with `monitor` the limits it gives are put
// on their autopilots every step. Returns the least height between them
// while within the horizontal minimum, feet, over `seconds`.
struct Flight {
    double least_ft_within = 1e18;
    double lost_s = 0.0;
};

Flight fly_two(double second_north_m, double second_ft, double second_heading_deg,
               double second_to_ft, bool monitor, double seconds) {
    struct One {
        glideslope::sim::Aircraft aircraft;
        std::optional<glideslope::sim::Controller> controller;
        One() : aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p") {}
    };
    std::vector<std::unique_ptr<One>> two;
    const double north[2] = {0.0, second_north_m};
    const double ft[2] = {3000.0, second_ft};
    const double heading[2] = {0.0, second_heading_deg};
    const double to_ft[2] = {3000.0, second_to_ft};
    for (int i = 0; i < 2; ++i) {
        auto one = std::make_unique<One>();
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9 + north[i] / metres_per_degree;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = ft[i];
        ic.heading_deg = heading[i];
        ic.airspeed_kts = 100.0;
        ic.gear = 0.0;
        one->aircraft.initialize(ic);
        glideslope::sim::Controls controls;
        controls.throttle = 0.7;
        one->controller.emplace(one->aircraft, controls);
        one->controller->to_ai();
        glideslope::sim::AutopilotModes m = one->controller->autopilot()->modes();
        m.heading_deg = heading[i];
        m.altitude_ft = to_ft[i];
        m.airspeed_kts = 100.0;
        one->controller->autopilot()->set(m);
        two.push_back(std::move(one));
    }
    Flight out;
    const int steps = static_cast<int>(seconds) * steps_per_second;
    for (int step = 0; step < steps; ++step) {
        std::vector<Traffic> traffic;
        for (const auto& one : two) {
            const glideslope::sim::AircraftState s = one->aircraft.state();
            Traffic t;
            t.latitude_deg = s.latitude_deg;
            t.longitude_deg = s.longitude_deg;
            t.altitude_ft = s.altitude_ft;
            t.north_fps = one->aircraft.property("velocities/v-north-fps");
            t.east_fps = one->aircraft.property("velocities/v-east-fps");
            t.climb_fpm = s.climb_rate_fpm;
            t.gives_way = true;
            t.held_ft = one->controller->autopilot()->modes().altitude_ft;
            traffic.push_back(t);
        }
        if (monitor) {
            const std::vector<HeightLimit> limits = glideslope::sim::separate(traffic);
            for (std::size_t i = 0; i < two.size(); ++i) {
                two[i]->controller->limit_height(limits[i].floor_ft, limits[i].ceiling_ft);
            }
        }
        for (const auto& one : two) {
            one->aircraft.set_controls(one->controller->fly());
            one->aircraft.step();
        }
        if (glideslope::sim::horizontal_m(traffic[0], traffic[1]) < Separation::minimum_m) {
            const double h = std::abs(traffic[0].altitude_ft - traffic[1].altitude_ft);
            out.least_ft_within = std::min(out.least_ft_within, h);
            if (h < Separation::minimum_ft) {
                out.lost_s += 1.0 / steps_per_second;
            }
        }
    }
    return out;
}

} // namespace

// **Built to lose separation, and kept from it**: a Cessna climbing through
// the height of one 1 km ahead of it, and one meeting another head on at its
// height. Without the monitor each loses separation - the situation is
// built, and shown to be one - and with it neither comes within the
// minimum, flown in JSBSim, through the autopilot.
GLIDESLOPE_TEST(a_cessna_climbing_through_another_or_meeting_one_head_on_is_kept_apart_by_the_monitor_and_without_it_is_not) {
    struct Case {
        const char* name;
        double north_m, ft, heading_deg, to_ft;
    };
    const Case cases[] = {
        {"climbing through the height of one 1 km ahead", -1000.0, 2000.0, 0.0, 4000.0},
        {"meeting one head on at its height", 12000.0, 3000.0, 180.0, 3000.0},
    };
    int walked = 0;
    for (const Case& c : cases) {
        const Flight without = fly_two(c.north_m, c.ft, c.heading_deg, c.to_ft, false, 240.0);
        const Flight with = fly_two(c.north_m, c.ft, c.heading_deg, c.to_ft, true, 240.0);
        std::fprintf(stderr,
                     "%s: without the monitor at least %.0f ft apart within 1.5 nm, lost for "
                     "%.1f s; with it at least %.0f ft, lost for %.1f s\n",
                     c.name, without.least_ft_within, without.lost_s, with.least_ft_within,
                     with.lost_s);
        check(without.lost_s > 0.0, std::string(c.name) + ": without the monitor, separation is lost");
        check(with.least_ft_within < 1e17,
              std::string(c.name) + ": with it, they still come within 1.5 nm of each other");
        check(with.lost_s == 0.0 && with.least_ft_within >= Separation::minimum_ft,
              std::string(c.name) + ": with the monitor, never within the minimum");
        ++walked;
    }
    check(walked == static_cast<int>(std::size(cases)), "every case was flown");
}

namespace {

// **One handed to the AI between two layers**: three Cessnas in JSBSim, the
// AI's. The first two fly north at 100 kt on layers 1,000 ft apart, at 3,000
// and 4,000 ft, 300 m apart; the third, the one handed over, is 600 m behind
// the first at 3,400 ft - where its player's spiral left it - holding its
// course: north, 3,400 ft. No height is 700 ft from both layers, and it is
// 400 ft from the lower and 600 m behind it at the start. With `monitor` the
// limits are put on all three every step, in that order. Measured over
// `seconds`: for each pair with the third, the least height within 1.5 nm
// and the time under the minimum; and the most the third's aileron,
// elevator or rudder moved in a step.
struct Between {
    double least_ft_within[2] = {1e18, 1e18};
    double lost_s[2] = {0.0, 0.0};
    double last_lost_s = 0.0;
    double most_control_step = 0.0;
    double turned_s = 0.0;
    int steps = 0;
};

Between fly_between_layers(bool monitor, double seconds) {
    struct One {
        glideslope::sim::Aircraft aircraft;
        std::optional<glideslope::sim::Controller> controller;
        One() : aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p") {}
    };
    std::vector<std::unique_ptr<One>> three;
    const double north[3] = {0.0, 300.0, -600.0};
    const double ft[3] = {3000.0, 4000.0, 3400.0};
    for (int i = 0; i < 3; ++i) {
        auto one = std::make_unique<One>();
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9 + north[i] / metres_per_degree;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = ft[i];
        ic.heading_deg = 0.0;
        ic.airspeed_kts = 100.0;
        ic.gear = 0.0;
        one->aircraft.initialize(ic);
        glideslope::sim::Controls controls;
        controls.throttle = 0.7;
        one->controller.emplace(one->aircraft, controls);
        one->controller->to_ai();
        glideslope::sim::AutopilotModes m = one->controller->autopilot()->modes();
        m.heading_deg = 0.0;
        m.altitude_ft = ft[i];
        m.airspeed_kts = 100.0;
        one->controller->autopilot()->set(m);
        three.push_back(std::move(one));
    }
    Between out;
    std::optional<glideslope::sim::Controls> last;
    const int steps = static_cast<int>(seconds) * steps_per_second;
    for (int step = 0; step < steps; ++step) {
        std::vector<Traffic> traffic;
        for (const auto& one : three) {
            const glideslope::sim::AircraftState s = one->aircraft.state();
            Traffic t;
            t.latitude_deg = s.latitude_deg;
            t.longitude_deg = s.longitude_deg;
            t.altitude_ft = s.altitude_ft;
            t.north_fps = one->aircraft.property("velocities/v-north-fps");
            t.east_fps = one->aircraft.property("velocities/v-east-fps");
            t.climb_fpm = s.climb_rate_fpm;
            t.gives_way = true;
            t.held_ft = one->controller->autopilot()->modes().altitude_ft;
            traffic.push_back(t);
        }
        if (monitor) {
            const std::vector<HeightLimit> limits = glideslope::sim::separate(traffic);
            for (std::size_t i = 0; i < three.size(); ++i) {
                three[i]->controller->limit_height(limits[i].floor_ft, limits[i].ceiling_ft);
                three[i]->controller->turn_away(limits[i].heading_deg);
            }
            if (limits[2].heading_deg) {
                out.turned_s += 1.0 / steps_per_second;
            }
        }
        for (std::size_t i = 0; i < three.size(); ++i) {
            const glideslope::sim::Controls c = three[i]->controller->fly();
            if (i == 2) {
                if (last) {
                    out.most_control_step = std::max(
                        {out.most_control_step, std::abs(c.aileron - last->aileron),
                         std::abs(c.elevator - last->elevator), std::abs(c.rudder - last->rudder)});
                }
                last = c;
            }
            three[i]->aircraft.set_controls(c);
            three[i]->aircraft.step();
        }
        for (std::size_t k = 0; k < 2; ++k) {
            if (glideslope::sim::horizontal_m(traffic[k], traffic[2]) < Separation::minimum_m) {
                const double h = std::abs(traffic[k].altitude_ft - traffic[2].altitude_ft);
                out.least_ft_within[k] = std::min(out.least_ft_within[k], h);
                if (h < Separation::minimum_ft) {
                    out.lost_s[k] += 1.0 / steps_per_second;
                    out.last_lost_s = static_cast<double>(step) / steps_per_second;
                }
            }
        }
        ++out.steps;
    }
    return out;
}

} // namespace

// **Squeezed between two layers, turned away**: one handed to the AI 400 ft
// over a layer and 600 ft under the next, behind the lower - where no
// height is 700 ft from both - is held in the middle of the gap and turned
// away until 1.5 nm from the layer it must pass, then taken past it, and
// never loses separation again. **Not zero**: it arrives within the
// minimum of the lower, and no height in a 1,000 ft gap is more than 500 ft
// from both, so until it is 1.5 nm away it is at the minimum at best - the
// bound is the time to turn about at the autopilot's 25 degrees of bank and
// open 1.5 nm (38.9 s here; 50 s allowed), and nothing after the first
// minute. Without the monitor it
// loses separation from the lower for most of the run. Its controls move no
// faster than a hand, full travel in a second, through all of it.
GLIDESLOPE_TEST(an_aircraft_handed_to_the_ai_between_two_layers_is_turned_away_and_kept_apart_from_both_after_its_arrival) {
    const double seconds = 300.0;
    const Between without = fly_between_layers(false, seconds);
    const Between with = fly_between_layers(true, seconds);
    const char* layer[2] = {"the lower layer", "the higher layer"};
    for (std::size_t k = 0; k < 2; ++k) {
        std::fprintf(stderr,
                     "%s: without the monitor at least %.0f ft within 1.5 nm, lost %.1f s; "
                     "with it at least %.0f ft, lost %.1f s\n",
                     layer[k], without.least_ft_within[k], without.lost_s[k],
                     with.least_ft_within[k], with.lost_s[k]);
    }
    std::fprintf(stderr,
                 "with the monitor: turned away for %.1f s, last under the minimum at %.1f s, "
                 "the most a control moved in a step %.4f; %d of %d steps flown\n",
                 with.turned_s, with.last_lost_s, with.most_control_step, with.steps,
                 static_cast<int>(seconds) * steps_per_second);
    check(with.steps == static_cast<int>(seconds) * steps_per_second &&
              without.steps == with.steps,
          "every step of the run was flown and measured, both ways");
    check(without.lost_s[0] > 60.0,
          "without the monitor, separation from the lower is lost: the situation is built");
    check(with.turned_s > 0.0, "it was squeezed between the two, and turned away");
    check(with.lost_s[0] + with.lost_s[1] <= 50.0,
          "with it, separation is lost only while it turns away and opens 1.5 nm: 50 s at most");
    check(with.last_lost_s <= 60.0, "with it, never lost after the first minute");
    check(with.least_ft_within[1] >= Separation::minimum_ft,
          "with it, never within the minimum of the higher layer");
    check(with.most_control_step <= 1.0 / steps_per_second + 1e-9,
          "its controls move no faster than a hand, full travel in a second");
}

// **A limit holds a vertical speed too**: an autopilot told to climb at
// 700 ft a minute with no height to fly to stops at its ceiling, and one told
// to descend stops at its floor, each held there within 50 ft for the minute
// after; with no limit, each goes on past it.
GLIDESLOPE_TEST(an_autopilot_holding_a_climb_stops_at_its_ceiling_and_one_holding_a_descent_at_its_floor) {
    struct Case {
        const char* name;
        double vs_fpm;
        bool limited;
    };
    const Case cases[] = {{"climbing, a ceiling 500 ft up", 700.0, true},
                          {"descending, a floor 500 ft down", -700.0, true},
                          {"climbing, no ceiling", 700.0, false},
                          {"descending, no floor", -700.0, false}};
    int flown = 0;
    for (const Case& c : cases) {
        glideslope::sim::Aircraft aircraft(GLIDESLOPE_TEST_DATA_DIR, "c172p");
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        ic.altitude_ft = 3000.0;
        ic.airspeed_kts = 90.0;
        ic.gear = 0.0;
        aircraft.initialize(ic);
        glideslope::sim::Controls controls;
        controls.throttle = 0.7;
        glideslope::sim::Autopilot autopilot(aircraft, controls);
        glideslope::sim::AutopilotModes m = autopilot.modes();
        m.heading_deg = 0.0;
        m.altitude_ft.reset();
        m.vertical_speed_fpm = c.vs_fpm;
        m.airspeed_kts = 90.0;
        autopilot.set(m);
        const double limit_ft = c.vs_fpm > 0.0 ? 3500.0 : 2500.0;
        if (c.limited) {
            if (c.vs_fpm > 0.0) {
                autopilot.limit_height(std::nullopt, limit_ft);
            } else {
                autopilot.limit_height(limit_ft, std::nullopt);
            }
        }
        double lowest = 1e18;
        double highest = -1e18;
        for (int step = 0; step < 180 * steps_per_second; ++step) {
            aircraft.set_controls(autopilot.fly());
            aircraft.step();
            if (step >= 120 * steps_per_second) {
                const double h = aircraft.state().altitude_ft;
                lowest = std::min(lowest, h);
                highest = std::max(highest, h);
            }
        }
        std::fprintf(stderr, "%s: %.0f to %.0f ft in its last minute\n", c.name, lowest,
                     highest);
        if (c.limited) {
            check(lowest >= limit_ft - 50.0 && highest <= limit_ft + 50.0,
                  std::string(c.name) + ": held at its limit");
        } else {
            check(c.vs_fpm > 0.0 ? lowest > limit_ft + 200.0 : highest < limit_ft - 200.0,
                  std::string(c.name) + ": goes on past where the limit would be");
        }
        ++flown;
    }
    check(flown == static_cast<int>(std::size(cases)), "every case was flown");
}
