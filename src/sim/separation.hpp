#pragma once

// **AI aircraft kept apart along the whole of their routes** (REQUIREMENTS.md
// section 6.5, decided 2026-10-02): what the server enforces between every two
// aircraft it flies, and what it measures.
//
// **The minimum is 500 ft vertically or 1.5 nm (2,778 m) horizontally**: two
// aircraft closer than both at once have lost separation. It is the minimum a
// US Class B controller keeps between VFR aircraft and others (FAA JO
// 7110.65, 7-9-4) - the nearest real rule for light aeroplanes worked close
// together round one city - and it lets two orbits of one place be flown one
// above the other, which no horizontal minimum the size of an orbit would.
//
// **Two things keep it.**
//   - **Layers**: AI aircraft flying one route are stacked `layer_ft` apart,
//     twice the minimum, so that each holding its height to within tens of
//     feet is nowhere near it; and planned aircraft that take off one after
//     another are stacked downwards - the first away flies highest - so that
//     none climbs through another's height on the way to its own - on
//     their first departures, in turn; one that flies again after a wreck
//     does, and the monitor keeps it apart.
//   - **A monitor** (`separate`), every step: an aircraft that gives way, and
//     is near another it must give way to - within `guard_m` horizontally now,
//     or by the straight lines both are flying within `lookahead_s` - may not
//     come within `minimum_ft` and `margin_ft` of the other's height, nor of
//     the height it is flying to: held below it if it is below, above if
//     above. Like a resolution advisory, it acts on heights only, and only
//     through the autopilot - the limit is a ceiling or a floor on the height
//     the autopilot flies to, never a control moved. Nothing turns - but
//     one squeezed between two, below.
//   - **Squeezed between two**, which no height keeps it apart from - one
//     handed to the AI after its player had come down between two layers
//     1,000 ft apart, where no height is 700 ft from both: it is turned
//     away, a heading the autopilot turns to at its own rates, and held
//     in the middle of the gap until 1.5 nm from every one it would pass
//     through, then taken above them all or below, as a controller's
//     safety alert turns and climbs an aircraft (FAA JO 7110.65 2-1-6).
//     TCAS's resolution advisories are vertical only, and the vertical
//     alone cannot be had here.
//
// **Who gives way**: an AI aircraft whose autopilot is flying (not one taking
// off, landing or gliding with its engine stopped, nor a person's), to every aircraft that does not give way -
// a person's, one taking off, one the server holds on a course its operator
// set - and to every AI aircraft before it in the
// server's order, which is the order they were made or took off in. So the
// first never moves for the second, and nothing is asked of a person.
//
// **What it does not do**: it cannot keep apart two aircraft neither of
// which gives way - two people, or a person flying into an AI aircraft faster
// than it can climb or descend out of the way; and a limit that would hold an
// aircraft within `least_above_ground_ft` of the ground under it is put on
// the other side instead.

#include <cstddef>
#include <optional>
#include <vector>

namespace glideslope::sim {

struct Separation {
    // The minimum: closer than both at once is separation lost.
    static constexpr double minimum_ft = 500.0;
    static constexpr double minimum_m = 2778.0; // 1.5 nm
    // How far apart AI aircraft on one route are stacked.
    static constexpr double layer_ft = 1000.0;
    // What the monitor keeps in hand over the minimum: an aircraft held
    // off another's height is held this much beyond the minimum.
    static constexpr double margin_ft = 200.0;
    // Near: within this horizontally now, or by the lines both are flying
    // within `lookahead_s`. 90 s at the autopilot's 700 ft a minute is
    // 1,050 ft, more than the minimum and margin from level.
    static constexpr double guard_m = minimum_m + 1000.0;
    static constexpr double lookahead_s = 90.0;
    // Never held this close to the ground under it.
    static constexpr double least_above_ground_ft = 500.0;
    // Squeezed and clear, it stays clear until this close to one it passes
    // through (1.3 nm): 370 m of hysteresis, so a distance that dips back
    // under 1.5 nm mid-climb does not send it back to the middle.
    static constexpr double clear_again_m = 2408.0;
};

// **Squeezed between two**: which way it is taken - above them all, or
// below - the heading it is turned to, and whether it is clear (1.5 nm from
// every one it would pass through) and so taken past them. Latched: the
// caller hands back what `separate` gave it last step (`Traffic::squeezed`),
// so the side and the heading are chosen once, not again every step - two
// layers equally far could otherwise flip it - and clear stays clear until
// it is under `Separation::clear_again_m`.
struct Squeeze {
    bool up = false;
    double away_deg = 0.0;
    bool clear = false;
};

// One aircraft, as the monitor sees it. Heights are in one frame - the
// server's, above the ellipsoid - whatever it is.
struct Traffic {
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double altitude_ft = 0.0;
    double north_fps = 0.0;
    double east_fps = 0.0;
    double climb_fpm = 0.0;
    // The height its autopilot is flying to, with any limit on it, if it
    // has one.
    std::optional<double> held_ft;
    // Whether it gives way: an AI aircraft whose autopilot is flying it.
    bool gives_way = false;
    double ground_ft = 0.0;
    // What `separate` gave it last step, if it was squeezed (`Squeeze`).
    std::optional<Squeeze> squeezed;
};

// A limit on the height an aircraft's autopilot flies to, and which aircraft
// it keeps it clear of (its index), or nothing; and, squeezed between two, a
// heading to turn to, away from them - true, degrees.
struct HeightLimit {
    std::optional<double> floor_ft;
    std::optional<double> ceiling_ft;
    std::optional<double> heading_deg;
    std::optional<std::size_t> clear_of;
    // Squeezed between two: what is latched for next step.
    std::optional<Squeeze> squeezed;
};

// **The limits, one for each aircraft**, in order. One that does not give way
// is given none. Each aircraft's limit is worked out from the limits of those
// before it, so the heights they are held to are what it keeps clear of.
std::vector<HeightLimit> separate(const std::vector<Traffic>& traffic);

// Whether two aircraft are near enough for the one giving way to keep clear
// of the other's height (`Separation`'s guard and lookahead). Not called
// `near`, which Windows' headers define as nothing.
bool within_guard(const Traffic& a, const Traffic& b);

// How far apart two aircraft are over the ground, metres.
double horizontal_m(const Traffic& a, const Traffic& b);

} // namespace glideslope::sim
