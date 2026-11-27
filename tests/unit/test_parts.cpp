#include "harness.hpp"

#include "gfx/model.hpp"
#include "gfx/parts.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/learnt.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

using glideslope::gfx::Hinge;
using glideslope::gfx::Livery;
using glideslope::gfx::Model;
using glideslope::gfx::ModelParts;
using glideslope::gfx::PartsError;
using glideslope::gfx::Surface;
using glideslope::sim::CatalogueEntry;
using glideslope::test::check;

namespace {

std::filesystem::path data_dir() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

std::filesystem::path models_dir() {
    return data_dir() / "models";
}

// **What each aircraft is not drawn moving, and why.** Every aircraft has
// both ailerons; the elevators ("elevator", for a left- and a right-) and the
// rudder ("rudder", for one or a left- and a right-) are each left out only
// here, with the reason.
const std::map<std::string, std::map<std::string, std::string>>& left_out() {
    static const std::map<std::string, std::map<std::string, std::string>> out = {
    };
    return out;
}

bool has(const std::vector<Surface>& surfaces, const std::string& name) {
    return std::any_of(surfaces.begin(), surfaces.end(),
                       [&](const Surface& s) { return s.name == name; });
}

std::array<double, 3> sub(const std::array<double, 3>& a, const std::array<double, 3>& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

// A point on a surface's trailing edge, near the middle of its span.
std::array<double, 3> trailing_edge(const Surface& s) {
    const std::array<double, 3> ab = sub(s.b, s.a);
    const double span = std::sqrt(ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2]);
    const std::array<double, 3> along{ab[0] / span, ab[1] / span, ab[2] / span};
    // Aft of the hinge, square to it: -x with the hinge's share taken off.
    std::array<double, 3> aft{-1.0 + along[0] * along[0], along[1] * along[0],
                              along[2] * along[0]};
    const double l = std::sqrt(aft[0] * aft[0] + aft[1] * aft[1] + aft[2] * aft[2]);
    std::array<double, 3> out{};
    for (std::size_t i = 0; i < 3; ++i) {
        out[i] = 0.5 * (s.a[i] + s.b[i]) + aft[i] / l * s.chord_m * 0.8;
    }
    return out;
}

} // namespace

// **Every aircraft wears a livery and has its surfaces cut from its model.**
// Each aircraft in the catalogue - all of them, counted against the
// catalogue - has a .livery and a .surfaces file beside its mesh; every coat
// paints some of the model, and every surface cuts a part out of it that is
// some of the wing or tail and not most of the aeroplane. The cut loses
// nothing: the body and its parts together cover what the model did.
GLIDESLOPE_TEST(every_aircraft_wears_a_livery_and_has_its_ailerons_elevators_and_rudder_hinged) {
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data_dir());
    std::size_t covered = 0;
    std::string failed;
    for (const CatalogueEntry& e : catalogue) {
        const std::filesystem::path base = models_dir() / e.id;
        const Model model = glideslope::gfx::read_model(base.string() + ".mesh");
        const Livery livery = glideslope::gfx::read_livery(base.string() + ".livery");
        const std::vector<Surface> surfaces =
            glideslope::gfx::read_surfaces(base.string() + ".surfaces");
        const double whole = glideslope::gfx::area_of(model);

        // Every coat paints something: each alone, so that two coats of one
        // colour cannot hide that one of them misses the aeroplane.
        for (const glideslope::gfx::Coat& coat : livery.coats) {
            Livery alone{livery.name, {coat}};
            const ModelParts painted = glideslope::gfx::split_model(model, alone, {});
            const double area = glideslope::gfx::area_coloured(painted.body, coat.colour);
            std::printf("  %-13s coat %-14s %8.3f m2 of %.1f\n", e.id.c_str(),
                        coat.name.c_str(), area, whole);
            if (!(area > 0.0005 * whole)) {
                failed += "\n  " + e.id + "'s coat " + coat.name + " paints almost nothing";
            }
        }

        std::vector<std::string> wanted{"left-aileron", "right-aileron"};
        const auto& out = left_out().count(e.id) ? left_out().at(e.id)
                                                 : std::map<std::string, std::string>{};
        if (!out.count("elevator")) {
            wanted.push_back("left-elevator");
            wanted.push_back("right-elevator");
        }
        if (!out.count("rudder") && !(has(surfaces, "left-rudder") && has(surfaces, "right-rudder"))) {
            wanted.push_back("rudder");
        }
        for (const std::string& name : wanted) {
            if (!has(surfaces, name)) {
                failed += "\n  " + e.id + " has no " + name;
            }
        }
        for (const auto& [name, why] : out) {
            std::printf("  %-13s %s left out: %s\n", e.id.c_str(), name.c_str(), why.c_str());
        }

        const ModelParts parts = glideslope::gfx::split_model(model, livery, surfaces);
        double sum = glideslope::gfx::area_of(parts.body);
        for (std::size_t i = 0; i < surfaces.size(); ++i) {
            const double area = glideslope::gfx::area_of(parts.surfaces[i]);
            sum += area;
            std::printf("  %-13s %-15s %8.3f m2\n", e.id.c_str(), surfaces[i].name.c_str(), area);
            if (!(area > 0.0005 * whole) || !(area < 0.08 * whole)) {
                failed += "\n  " + e.id + "'s " + surfaces[i].name + " cuts " +
                          std::to_string(area) + " m2 of " + std::to_string(whole);
            }
        }
        if (std::abs(sum - whole) > 1e-4 * whole) {
            failed += "\n  " + e.id + "'s parts cover " + std::to_string(sum) +
                      " m2, where the model covers " + std::to_string(whole);
        }
        ++covered;
    }
    std::printf("%zu of the catalogue's %zu aircraft covered\n", covered, catalogue.size());
    check(covered == catalogue.size() && catalogue.size() == 16,
          "the catalogue has " + std::to_string(catalogue.size()) + " aircraft, " +
              std::to_string(covered) + " covered");
    check(failed.empty(), "wrong:" + failed);
}

// **The stick moves every aircraft's surfaces the way it should.** Each
// aircraft flies, trimmed, and its stick and pedals are put over one way and
// then the other, a quarter of a second each; the deflection each surface's
// property then reports turns the surface about its hinge, and its trailing
// edge must go the way the control asks: stick right, the left aileron's
// down and the right's up; stick back, the elevators' up; right pedal, the
// rudder's to the right. Every surface of every aircraft, counted.
GLIDESLOPE_TEST(the_stick_and_pedals_move_every_aircrafts_surfaces_the_way_they_should) {
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data_dir());
    std::size_t surfaces_seen = 0;
    std::size_t surfaces_right = 0;
    std::string failed;
    for (const CatalogueEntry& e : catalogue) {
        const std::vector<Surface> surfaces =
            glideslope::gfx::read_surfaces(models_dir() / (e.id + ".surfaces"));
        for (const double way : {1.0, -1.0}) {
            glideslope::sim::Aircraft aircraft(data_dir() / "jsbsim", e.model);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = -33.9;
            ic.longitude_deg = 151.2;
            ic.altitude_ft = 3000.0;
            ic.airspeed_kts = e.start_airspeed_kts;
            ic.engine_running = true;
            ic.gear = 0.0;
            ic.trim = true;
            aircraft.initialize(ic);
            glideslope::sim::Controls c = glideslope::sim::held_until_moved(aircraft);
            c.aileron = way;
            c.elevator = way;
            c.rudder = way;
            for (int i = 0; i < 30; ++i) {
                aircraft.set_controls(c);
                aircraft.step();
            }
            for (const Surface& s : surfaces) {
                const Hinge hinge = glideslope::gfx::hinge_of(s);
                const double angle = aircraft.property(s.property);
                const std::array<double, 3> edge = trailing_edge(s);
                const std::array<double, 3> moved =
                    sub(glideslope::gfx::turned(hinge, angle, edge), edge);
                // Where the control asks the trailing edge to go, per unit of
                // `way`: +z down, +y right.
                double asked = 0.0;
                std::size_t axis = 2;
                if (s.name.find("left-aileron") == 0) {
                    asked = 1.0;
                } else if (s.name.find("right-aileron") == 0) {
                    asked = -1.0;
                } else if (s.name.find("elevator") != std::string::npos) {
                    asked = -1.0;
                } else if (s.name.find("rudder") != std::string::npos) {
                    asked = 1.0;
                    axis = 1;
                }
                const double went = moved[axis] * way * asked;
                const double reach = s.chord_m * 0.8;
                std::printf("  %-13s %-15s stick %+.0f: %+6.2f deg, edge %+.3f m the way asked\n",
                            e.id.c_str(), s.name.c_str(), way, angle * 57.29578, went);
                ++surfaces_seen;
                // At least a degree's worth of the way asked.
                if (asked != 0.0 && went > reach * 0.01745) {
                    ++surfaces_right;
                } else {
                    failed += "\n  " + e.id + "'s " + s.name + " with the controls " +
                              (way > 0 ? "right, back and right" : "left, forward and left") +
                              ": its trailing edge went " + std::to_string(went) +
                              " m the way asked, " + std::to_string(angle) + " rad";
                }
            }
        }
    }
    std::printf("%zu of %zu surface movements the way asked\n", surfaces_right, surfaces_seen);
    check(failed.empty(), "wrong:" + failed);
    check(surfaces_seen > 0 && surfaces_right == surfaces_seen, "not every surface was seen");
}

// **A part turns about its hinge.** At no deflection it is where the
// aeroplane is; the hinge's own points never move; and a positive deflection
// takes the trailing edge the way the surface's line says - for each of the
// four ways.
GLIDESLOPE_TEST(a_surface_turns_about_its_hinge_its_trailing_edge_the_way_its_line_says) {
    const char* lines[] = {
        "surface a p down  0 -1 0  0 -3 0  0 0.5 0.1\n",
        "surface a p up    0 -1 0  0 -3 0  0 0.5 0.1\n",
        "surface a p down  0  1 0  0  3 0  0 0.5 0.1\n",
        "surface a p up    0  3 0  0  1 0  0 0.5 0.1\n",
        "surface a p left  0 0 -1  0 0 -2  0 0.5 0.1\n",
        "surface a p right 0 0 -2  0 0 -1  0 0.5 0.1\n",
    };
    for (const char* line : lines) {
        const Surface s = glideslope::gfx::parse_surfaces(line, "test").at(0);
        const Hinge h = glideslope::gfx::hinge_of(s);
        const std::array<double, 3> edge = trailing_edge(s);
        const auto same = [](const std::array<double, 3>& p, const std::array<double, 3>& q) {
            return std::abs(p[0] - q[0]) + std::abs(p[1] - q[1]) + std::abs(p[2] - q[2]) < 1e-9;
        };
        check(same(glideslope::gfx::turned(h, 0.0, edge), edge), "moved at no deflection");
        check(same(glideslope::gfx::turned(h, 0.3, s.a), s.a), "the hinge's end moved");
        check(same(glideslope::gfx::turned(h, 0.3, s.b), s.b), "the hinge's other end moved");
        const std::array<double, 3> moved = sub(glideslope::gfx::turned(h, 0.1, edge), edge);
        const double toward = s.toward == glideslope::gfx::Toward::down    ? moved[2]
                              : s.toward == glideslope::gfx::Toward::up    ? -moved[2]
                              : s.toward == glideslope::gfx::Toward::right ? moved[1]
                                                                           : -moved[1];
        check(toward > 0.03, std::string("not moved the way said by ") + line);

        // The placement does what `turned` does, about a moved aeroplane.
        glideslope::gfx::Placement plane;
        plane.origin = {1000.0, 2000.0, 3000.0};
        const glideslope::gfx::Placement p = glideslope::gfx::hinged(plane, h, 0.1);
        const glideslope::world::Ecef at =
            p.world_from_local * glideslope::world::Ecef{edge[0], edge[1], edge[2]};
        const std::array<double, 3> want = glideslope::gfx::turned(h, 0.1, edge);
        check(std::abs(p.origin.x + at.x - 1000.0 - want[0]) < 1e-9 &&
                  std::abs(p.origin.y + at.y - 2000.0 - want[1]) < 1e-9 &&
                  std::abs(p.origin.z + at.z - 3000.0 - want[2]) < 1e-9,
              "the placement does not turn the part as turned() does");
    }
}

// **A coat cuts what it crosses.** One triangle half inside a coat's box
// comes out as two pieces, the painted one exactly the half inside, and the
// two of them the triangle's area; a surface's box takes its share of the
// triangle out of the body the same way.
GLIDESLOPE_TEST(a_coat_or_a_surface_cuts_the_triangles_it_crosses_and_loses_nothing) {
    Model m;
    const auto vertex = [](float x, float y) {
        glideslope::gfx::ModelVertex v;
        v.position = {x, y, 0.0f};
        v.normal = {0.0f, 0.0f, -1.0f};
        v.colour = {1.0f, 1.0f, 1.0f, 1.0f};
        return v;
    };
    m.vertices = {vertex(0, 0), vertex(2, 0), vertex(0, 2)};
    m.indices = {0, 1, 2};
    const Livery red = glideslope::gfx::parse_livery(
        "name test\ncoat half 255 0 0  -1 1  -1 3  -1 1\n", "test");
    const ModelParts painted = glideslope::gfx::split_model(m, red, {});
    check(std::abs(glideslope::gfx::area_of(painted.body) - 2.0) < 1e-6, "area lost");
    // The part of the triangle with x < 1: 2 less the corner beyond, 0.5.
    check(std::abs(glideslope::gfx::area_coloured(painted.body, {1.0f, 0.0f, 0.0f}) - 1.5) < 1e-6,
          "the painted piece is not the half inside the box");
    const std::vector<Surface> flap = glideslope::gfx::parse_surfaces(
        "surface flap p down  1 0 0  1 2 0  0 1 0.5\n", "test");
    const ModelParts cut = glideslope::gfx::split_model(m, red, flap);
    check(std::abs(glideslope::gfx::area_of(cut.surfaces.at(0)) - 1.5) < 1e-6,
          "the surface did not take the triangle aft of its hinge");
    check(std::abs(glideslope::gfx::area_of(cut.body) - 0.5) < 1e-6,
          "the body did not keep what is forward of the hinge");
}

// **A livery or surfaces file that is wrong says where.**
GLIDESLOPE_TEST(a_livery_or_surfaces_line_that_cannot_be_read_is_refused_with_its_line) {
    const auto refused = [](auto parse, const char* text, const char* says) {
        try {
            parse(text);
        } catch (const PartsError& e) {
            check(std::string(e.what()).find(says) != std::string::npos,
                  std::string("refused, but saying: ") + e.what());
            return;
        }
        glideslope::test::fail(std::string("not refused: ") + text);
    };
    const auto livery = [](const char* t) { (void)glideslope::gfx::parse_livery(t, "L"); };
    const auto surfaces = [](const char* t) { (void)glideslope::gfx::parse_surfaces(t, "S"); };
    refused(livery, "name x\ncoat a 1 2 3 0 1 0 1 0\n", "L:2:");
    refused(livery, "name x\ncoat a 256 2 3 0 1 0 1 0 1\n", "past 0 to 255");
    refused(livery, "name x\ncoat a 1 2 3 1 0 0 1 0 1\n", "no extent");
    refused(livery, "name x\ncoat a 1 2 3 0 1 0 1 0 1\ncoat a 1 2 3 0 1 0 1 0 1\n", "twice");
    refused(livery, "coat a 1 2 3 0 1 0 1 0 1\n", "no name");
    refused(livery, "name x\n", "no coat");
    refused(surfaces, "surface a p sideways 0 0 0 0 1 0 0 1 1\n", "not down, up");
    refused(surfaces, "surface a p down 0 0 0 0 1 0 0 1\n", "S:1:");
    refused(surfaces, "surface a p down 0 0 0 0 0 0 0 1 1\n", "no extent");
    refused(surfaces, "surface a p left 0 0 0 0 1 0 0 1 1\n", "cannot move");
    refused(surfaces, "# nothing\n", "names no surface");
}
