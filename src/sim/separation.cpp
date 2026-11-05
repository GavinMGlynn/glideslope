#include "sim/separation.hpp"

#include "sim/plan.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace glideslope::sim {

namespace {

constexpr double metres_per_foot = 0.3048;
constexpr double radians = std::numbers::pi / 180.0;
// The rate the autopilot climbs and descends at unless asked otherwise
// (AutopilotModes::vertical_speed_fpm): the least an aircraft's heights over
// the lookahead are taken to move by.
constexpr double least_rate_fpm = 700.0;

// **The heights an aircraft may be at over the lookahead**: from where it is
// to the height it is flying to, but no further than it would climb or
// descend in that time; or, with no height to fly to, along its climb.
struct Band {
    double low_ft;
    double high_ft;
};

Band band_of(const Traffic& t, std::optional<double> held_ft) {
    const double reach_ft =
        std::max(std::abs(t.climb_fpm), least_rate_fpm) * Separation::lookahead_s / 60.0;
    const double end = held_ft ? std::clamp(*held_ft, t.altitude_ft - reach_ft,
                                            t.altitude_ft + reach_ft)
                               : t.altitude_ft + t.climb_fpm * Separation::lookahead_s / 60.0;
    return {std::min(t.altitude_ft, end), std::max(t.altitude_ft, end)};
}

std::optional<double> limited(std::optional<double> held_ft, const HeightLimit& limit) {
    if (!held_ft) {
        return held_ft;
    }
    double h = *held_ft;
    if (limit.floor_ft) {
        h = std::max(h, *limit.floor_ft);
    }
    if (limit.ceiling_ft) {
        h = std::min(h, *limit.ceiling_ft);
    }
    return h;
}

} // namespace

double horizontal_m(const Traffic& a, const Traffic& b) {
    return distance_m(a.latitude_deg, a.longitude_deg, b.latitude_deg, b.longitude_deg);
}

bool within_guard(const Traffic& a, const Traffic& b) {
    const double d = horizontal_m(a, b);
    if (d < Separation::guard_m) {
        return true;
    }
    // Where b is from a, north and east, and how it moves from a.
    const double towards = bearing_deg(a.latitude_deg, a.longitude_deg, b.latitude_deg,
                                       b.longitude_deg) *
                           radians;
    const double rn = d * std::cos(towards);
    const double re = d * std::sin(towards);
    const double vn = (b.north_fps - a.north_fps) * metres_per_foot;
    const double ve = (b.east_fps - a.east_fps) * metres_per_foot;
    const double v2 = vn * vn + ve * ve;
    if (v2 <= 0.0) {
        return false;
    }
    const double t = std::clamp(-(rn * vn + re * ve) / v2, 0.0, Separation::lookahead_s);
    return std::hypot(rn + vn * t, re + ve * t) < Separation::guard_m;
}

namespace {

// **Room asked of a layer** for one squeezed between it and another: a
// ceiling for one below, a floor for one above, and which aircraft it is
// made for.
struct Room {
    std::optional<double> floor_ft;
    std::optional<double> ceiling_ft;
    std::size_t for_whom = 0;
};

std::vector<HeightLimit> separate_with(const std::vector<Traffic>& traffic,
                                       const std::vector<std::optional<Room>>& room) {
    const std::size_t n = traffic.size();
    std::vector<HeightLimit> limits(n);
    // The height each is flying to, as limited so far.
    std::vector<std::optional<double>> held(n);
    for (std::size_t i = 0; i < n; ++i) {
        held[i] = traffic[i].held_ft;
    }
    const double apart_ft = Separation::minimum_ft + Separation::margin_ft;
    // One it gives way to and is near: which, the heights it may be at, and
    // whether the one giving way keeps above it.
    struct Near {
        std::size_t j;
        Band band;
        bool above;
    };
    for (std::size_t i = 0; i < n; ++i) {
        const Traffic& g = traffic[i];
        if (!g.gives_way) {
            continue;
        }
        HeightLimit& limit = limits[i];
        std::optional<std::size_t> floor_for;
        std::optional<std::size_t> ceiling_for;
        std::vector<Near> near;
        for (std::size_t j = 0; j < n; ++j) {
            const Traffic& p = traffic[j];
            // Only to those it gives way to: every one that does not, and
            // every one before it that does.
            if (j == i || (p.gives_way && j > i) || !within_guard(g, p)) {
                continue;
            }
            const Band band = band_of(p, held[j]);
            // Which side it keeps to: the side it is on - or, within 50 ft
            // of level with the other, the side of it the height it is
            // flying to is on, and below if that is level too. Not which
            // way it is going: held off, it goes one way and then the
            // other, and the side with it.
            const double dh = g.altitude_ft - p.altitude_ft;
            bool above = dh > 0.0;
            if (std::abs(dh) < 50.0) {
                above = g.held_ft && *g.held_ft > p.altitude_ft;
            }
            const double ceiling = band.low_ft - apart_ft;
            if (!above && ceiling < g.ground_ft + Separation::least_above_ground_ft) {
                above = true; // not pushed into the ground: over it instead
            }
            near.push_back({j, band, above});
            if (above) {
                const double floor = band.high_ft + apart_ft;
                if (!limit.floor_ft || floor > *limit.floor_ft) {
                    limit.floor_ft = floor;
                    floor_for = j;
                }
            } else if (!limit.ceiling_ft || ceiling < *limit.ceiling_ft) {
                limit.ceiling_ft = ceiling;
                ceiling_for = j;
            }
        }
        // **Squeezed between two that leave no room** - an aircraft come
        // down between two layers 1,000 ft apart, where no height is 700 ft
        // from both: no height alone keeps it apart, so it is turned away
        // and taken to a height clear of all of them, as a controller's
        // safety alert does ("turn left heading ..., climb ... immediately",
        // FAA JO 7110.65 2-1-6). Above every one of them or below, the side
        // that asks less of it, and not into the ground. **It goes there
        // only once 1.5 nm from every one it would pass through**: until
        // then it is held in the middle of the gap, as far from both as the
        // gap allows, and turned directly away from the nearest of those.
        // **Latched while it is not yet clear**: once the layers have made
        // room (below), the gap is wide enough and the floor no longer
        // crosses the ceiling - but let go then, the room would be let go
        // too, the layers would close again, and it would be squeezed again.
        // **Only while there is still one on each side**: a gap opened
        // because a layer has gone - landed, left, or out of the guard - is
        // no squeeze, and it flies its ordinary limits again.
        const bool squeezed_now =
            limit.floor_ft && limit.ceiling_ft && *limit.floor_ft > *limit.ceiling_ft;
        const bool one_below = std::any_of(near.begin(), near.end(),
                                           [](const Near& k) { return k.above; });
        const bool one_above = std::any_of(near.begin(), near.end(),
                                           [](const Near& k) { return !k.above; });
        const bool still_held =
            g.squeezed && !g.squeezed->clear && one_below && one_above;
        if (squeezed_now || still_held) {
            double top = -1e18;
            double bottom = 1e18;
            for (const Near& k : near) {
                top = std::max(top, k.band.high_ft + apart_ft);
                bottom = std::min(bottom, k.band.low_ft - apart_ft);
            }
            const bool can_descend = bottom >= g.ground_ft + Separation::least_above_ground_ft;
            // The side, latched once chosen (`Squeeze`) - never into the ground.
            const Squeeze* was = g.squeezed ? &*g.squeezed : nullptr;
            const bool up = !can_descend ||
                            (was ? was->up : top - g.altitude_ft <= g.altitude_ft - bottom);
            // Those it would pass through: the side it goes to.
            std::optional<std::size_t> nearest;
            double nearest_m = 1e18;
            for (const Near& k : near) {
                if (k.above == up) {
                    continue;
                }
                const double d = horizontal_m(g, traffic[k.j]);
                if (d < nearest_m) {
                    nearest_m = d;
                    nearest = k.j;
                }
            }
            // Clear at 1.5 nm, and clear until under 1.3 nm.
            const bool clear =
                nearest_m >= (was && was->clear ? Separation::clear_again_m : Separation::minimum_m);
            // The heading, latched once chosen: away from the nearest then.
            std::optional<double> away;
            if (was) {
                away = was->away_deg;
            } else if (nearest) {
                const Traffic& p = traffic[*nearest];
                away = std::fmod(
                    bearing_deg(p.latitude_deg, p.longitude_deg, g.latitude_deg, g.longitude_deg) +
                        360.0,
                    360.0);
            }
            // The middle of the gap, latched once chosen: the layers make
            // room about it, and as they move their bands would move it.
            double middle = was && was->middle_ft
                                ? *was->middle_ft
                                : 0.5 * (limit.floor_ft.value_or(g.altitude_ft) +
                                         limit.ceiling_ft.value_or(g.altitude_ft));
            // **The middle follows those that cannot make room**: a person's
            // aircraft, one taking off, or a layer that would be taken
            // within 500 ft of its ground. The latch keeps the middle still
            // while the layers that can move do; one that cannot may climb
            // or descend, and the middle is kept the minimum and margin from
            // it. Between two that cannot, the middle of what they leave.
            double lowest = -1e18;
            double highest = 1e18;
            for (const Near& k : near) {
                const Traffic& p = traffic[k.j];
                const bool below = k.above;
                const bool movable =
                    p.gives_way &&
                    (!below ||
                     middle - apart_ft >= p.ground_ft + Separation::least_above_ground_ft);
                if (movable) {
                    continue;
                }
                if (below) {
                    lowest = std::max(lowest, k.band.high_ft + apart_ft);
                } else {
                    highest = std::min(highest, k.band.low_ft - apart_ft);
                }
            }
            middle = lowest <= highest ? std::clamp(middle, lowest, highest)
                                       : 0.5 * (lowest + highest);
            limit.heading_deg = away;
            if (away) {
                limit.squeezed = Squeeze{up, *away, clear, middle};
            }
            if (clear) {
                if (up) {
                    limit.floor_ft = top;
                    limit.ceiling_ft.reset();
                } else {
                    limit.ceiling_ft = bottom;
                    limit.floor_ft.reset();
                }
            } else {
                limit.floor_ft = middle;
                limit.ceiling_ft = middle;
            }
            limit.clear_of = nearest ? nearest : (up ? ceiling_for : floor_for);
        } else {
            limit.clear_of = ceiling_for ? ceiling_for : floor_for;
        }
        // **Room made for one squeezed** (`separate`): a ceiling under it
        // for a layer below, a floor over it for one above - but only where
        // that leaves the layer a height to fly, and none of its own limits
        // crossed.
        if (room[i] && !limit.squeezed) {
            HeightLimit with = limit;
            if (room[i]->ceiling_ft) {
                with.ceiling_ft = std::min(with.ceiling_ft.value_or(1e18), *room[i]->ceiling_ft);
            }
            if (room[i]->floor_ft) {
                with.floor_ft = std::max(with.floor_ft.value_or(-1e18), *room[i]->floor_ft);
            }
            const bool crossed =
                with.floor_ft && with.ceiling_ft && *with.floor_ft > *with.ceiling_ft;
            const bool grounded =
                with.ceiling_ft &&
                *with.ceiling_ft < g.ground_ft + Separation::least_above_ground_ft;
            if (!crossed && !grounded) {
                with.clear_of = room[i]->for_whom;
                limit = with;
            }
        }
        held[i] = limited(held[i], limit);
    }
    return limits;
}

} // namespace

// **Squeezed between two, the two make room** - after a controller's safety
// alert, which may move either aircraft (FAA JO 7110.65 2-1-6), and
// TCAS-like, one climbing and the other descending as TCAS II's
// complementary advisories have them: an aircraft that is squeezed and not yet clear is held in
// the middle of the gap, and every AI aircraft it gives way to there that
// gives way itself - an autopilot's, which can be moved - is moved away
// from it: one below to `minimum_ft` and `margin_ft` under the middle, one
// above to as much over it. Worked out twice: the first time finds who is
// squeezed, the second puts the room on the layers before those after them
// are worked out, so those keep clear of where the layers are going.
std::vector<HeightLimit> separate(const std::vector<Traffic>& traffic) {
    const std::size_t n = traffic.size();
    const std::vector<std::optional<Room>> none(n);
    const std::vector<HeightLimit> first = separate_with(traffic, none);
    const double apart_ft = Separation::minimum_ft + Separation::margin_ft;
    std::vector<std::optional<Room>> room(n);
    bool any = false;
    for (std::size_t i = 0; i < n; ++i) {
        const std::optional<Squeeze>& sq = first[i].squeezed;
        if (!sq || sq->clear || !sq->middle_ft) {
            continue;
        }
        const Traffic& g = traffic[i];
        for (std::size_t j = 0; j < n; ++j) {
            const Traffic& p = traffic[j];
            if (j == i || !p.gives_way || j > i || !within_guard(g, p)) {
                continue;
            }
            // Which side of it the layer is: of the squeezed aircraft, not
            // of the middle, which may have followed one that cannot move
            // past a layer that can. A layer level with it is left where
            // it is.
            Room r;
            r.for_whom = i;
            if (p.altitude_ft < g.altitude_ft - 50.0) {
                r.ceiling_ft = *sq->middle_ft - apart_ft;
            } else if (p.altitude_ft > g.altitude_ft + 50.0) {
                r.floor_ft = *sq->middle_ft + apart_ft;
            } else {
                continue;
            }
            // Two asking room of one layer: the most asked.
            if (room[j]) {
                if (r.ceiling_ft) {
                    room[j]->ceiling_ft =
                        std::min(room[j]->ceiling_ft.value_or(1e18), *r.ceiling_ft);
                }
                if (r.floor_ft) {
                    room[j]->floor_ft = std::max(room[j]->floor_ft.value_or(-1e18), *r.floor_ft);
                }
            } else {
                room[j] = r;
            }
            any = true;
        }
    }
    if (!any) {
        return first;
    }
    // Worked out again with the squeezes the first time latched, so the
    // second keeps the side, heading and middle the first chose.
    std::vector<Traffic> again = traffic;
    for (std::size_t i = 0; i < n; ++i) {
        if (first[i].squeezed) {
            again[i].squeezed = first[i].squeezed;
        }
    }
    return separate_with(again, room);
}

} // namespace glideslope::sim
