#include "harness.hpp"

#include "gfx/aircraft.hpp"
#include "gfx/model.hpp"
#include "gfx/parts.hpp"
#include "net/interpolation.hpp"
#include "net/state.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/learnt.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
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
        {"b2",
         {{"rudder", "it has no fin; it yaws on split drag rudders at its wingtips, "
                     "each two halves opening up and down at once about one hinge, "
                     "which one part turned one way cannot draw"}}},
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
    // Each exclusion names an aircraft in the catalogue, and what it names is
    // really not there.
    for (const auto& [id, out] : left_out()) {
        const bool listed = std::any_of(catalogue.begin(), catalogue.end(),
                                        [&](const CatalogueEntry& e) { return e.id == id; });
        if (!listed) {
            failed += "\n  " + id + " is left out of something, and is not in the catalogue";
            continue;
        }
        const std::vector<Surface> surfaces =
            glideslope::gfx::read_surfaces(models_dir() / (id + ".surfaces"));
        for (const auto& [name, why] : out) {
            for (const Surface& s : surfaces) {
                if (s.name.find(name) != std::string::npos) {
                    failed += "\n  " + id + "'s " + name + " is named left out, and its " +
                              s.name + " is there";
                }
            }
        }
    }
    std::printf("%zu of the catalogue's %zu aircraft covered\n", covered, catalogue.size());
    check(covered == catalogue.size() && catalogue.size() == 16,
          "the catalogue has " + std::to_string(catalogue.size()) + " aircraft, " +
              std::to_string(covered) + " covered");
    check(failed.empty(), "wrong:" + failed);
}

namespace {

// **How many fins each aircraft has**, counted off the aircraft and not off
// its .surfaces file: the twin-finned fighters two, the B-2 none, every
// other one. With two ailerons and two elevators each, this is how many
// surfaces the stick test must see, so a line taken out of a file fails it.
std::size_t fins_of(const std::string& id) {
    static const std::map<std::string, std::size_t> twins_and_none = {
        {"f15c", 2}, {"f22", 2}, {"f35b", 2}, {"b2", 0}};
    const auto found = twins_and_none.find(id);
    return found == twins_and_none.end() ? 1 : found->second;
}

// **The one surface held to less than a degree**, and why: the F-22's
// rudders, whose command passes through her rudder limiter
// (fcs/rudder-cmd-limiter) and her yaw law, swing 0.9 degrees in the quarter
// of a second the test holds the pedals. Held to half a degree instead.
bool under_a_degree_allowed(const std::string& id, const std::string& surface) {
    return id == "f22" && surface.find("rudder") != std::string::npos;
}

// A trimmed aircraft in the air, flown a quarter of a second with the stick
// and pedals at `way` (nothing: held where her trim had them), and what each
// surface's property reports then.
// `e` flown trimmed for a quarter of a second with its stick and pedals full
// `way` - or left where its trim had them, at nought - and handed to `then`.
void fly_over(const CatalogueEntry& e, double way,
              const std::function<void(const glideslope::sim::Aircraft&)>& then) {
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
    if (way != 0.0) {
        c.aileron = way;
        c.elevator = way;
        c.rudder = way;
    }
    for (int i = 0; i < 30; ++i) {
        aircraft.set_controls(c);
        aircraft.step();
    }
    then(aircraft);
}

std::vector<double> angles_after(const CatalogueEntry& e, const std::vector<Surface>& surfaces,
                                 double way) {
    std::vector<double> out;
    fly_over(e, way, [&](const glideslope::sim::Aircraft& aircraft) {
        for (const Surface& s : surfaces) {
            out.push_back(aircraft.property(s.property));
        }
    });
    return out;
}

} // namespace

// **The stick moves every aircraft's surfaces the way it should.** Each
// aircraft flies, trimmed, three times for a quarter of a second: its stick
// and pedals left where its trim had them, then over one way, then the
// other. Each surface's swing is what its property reports less what it
// reported with the controls left alone - a trimmed elevator already sits a
// degree or two over - and that swing turns it about its hinge: its trailing
// edge must go the way the control asks, by a degree at least, but for the
// one exception named above. Stick right, the left aileron's down and the
// right's up; stick back, the elevators' up; right pedal, the rudder's to the
// right. The movements are counted against the catalogue, not the files: two
// ailerons, two elevators and each fin's rudder, both ways, less what
// left_out() names.
GLIDESLOPE_TEST(the_stick_and_pedals_move_every_aircrafts_surfaces_the_way_they_should) {
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data_dir());
    std::size_t expected = 0;
    std::size_t surfaces_seen = 0;
    std::size_t surfaces_right = 0;
    std::string failed;
    for (const CatalogueEntry& e : catalogue) {
        const auto& out = left_out().count(e.id) ? left_out().at(e.id)
                                                 : std::map<std::string, std::string>{};
        expected += 2 * (2 + (out.count("elevator") ? 0 : 2) +
                         (out.count("rudder") ? 0 : fins_of(e.id)));
        const std::vector<Surface> surfaces =
            glideslope::gfx::read_surfaces(models_dir() / (e.id + ".surfaces"));
        const std::vector<double> neutral = angles_after(e, surfaces, 0.0);
        for (const double way : {1.0, -1.0}) {
            const std::vector<double> angles = angles_after(e, surfaces, way);
            for (std::size_t i = 0; i < surfaces.size(); ++i) {
                const Surface& s = surfaces[i];
                const Hinge hinge = glideslope::gfx::hinge_of(s);
                const double swing = angles[i] - neutral[i];
                const std::array<double, 3> edge = trailing_edge(s);
                const std::array<double, 3> moved =
                    sub(glideslope::gfx::turned(hinge, swing, edge), edge);
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
                const double least_deg = under_a_degree_allowed(e.id, s.name) ? 0.5 : 1.0;
                std::printf("  %-13s %-15s stick %+.0f: swung %+6.2f deg from %+6.2f, edge "
                            "%+.3f m the way asked\n",
                            e.id.c_str(), s.name.c_str(), way, swing * 57.29578,
                            neutral[i] * 57.29578, went);
                ++surfaces_seen;
                if (asked != 0.0 && went > reach * std::sin(least_deg / 57.29578)) {
                    ++surfaces_right;
                } else {
                    failed += "\n  " + e.id + "'s " + s.name + " with the controls " +
                              (way > 0 ? "right, back and right" : "left, forward and left") +
                              ": its trailing edge went " + std::to_string(went) +
                              " m the way asked, a swing of " + std::to_string(swing) + " rad";
                }
            }
        }
    }
    std::printf("%zu of %zu surface movements the way asked; %zu expected from the catalogue\n",
                surfaces_right, surfaces_seen, expected);
    check(failed.empty(), "wrong:" + failed);
    check(surfaces_seen == expected && surfaces_right == expected,
          std::to_string(surfaces_seen) + " surface movements seen, where the catalogue's " +
              "aircraft have " + std::to_string(expected));
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

// **Every aircraft's every surface reaches another client where its flight
// model has it.** Each aircraft flies, trimmed, with its stick and pedals
// full one way and then the other for a quarter of a second, as above; what
// the server writes of it (net::surfaces_on_the_wire) goes through a state
// update written and read, and into the interpolation a client draws
// another aircraft from, two updates apart; and each surface its file names
// is turned by the carried deflection its property is - every property a
// file names must be one a state update carries. Each must come out within
// a quarter of a degree of what its flight model flew, and over by more than
// that: a surface sent as nought is seen as not sent. Counted against the
// catalogue, not the files, as above - 164: every aircraft's two ailerons,
// two elevators and each fin's rudder, both ways, less the B-2's rudder,
// which is not drawn (left_out()).
GLIDESLOPE_TEST(every_aircrafts_every_surface_reaches_another_client_where_its_flight_model_has_it) {
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data_dir());
    constexpr double quarter_degree = 0.25 / 57.29577951308232;
    std::size_t expected = 0;
    std::size_t seen = 0;
    std::size_t right = 0;
    std::size_t aircraft = 0;
    std::string failed;
    for (const CatalogueEntry& e : catalogue) {
        const auto& out = left_out().count(e.id) ? left_out().at(e.id)
                                                 : std::map<std::string, std::string>{};
        expected += 2 * (2 + (out.count("elevator") ? 0 : 2) +
                         (out.count("rudder") ? 0 : fins_of(e.id)));
        ++aircraft;
        const std::vector<Surface> surfaces =
            glideslope::gfx::read_surfaces(models_dir() / (e.id + ".surfaces"));
        for (const double way : {1.0, -1.0}) {
            std::vector<double> flown;
            glideslope::net::AircraftState a;
            fly_over(e, way, [&](const glideslope::sim::Aircraft& flying) {
                for (const Surface& s : surfaces) {
                    flown.push_back(flying.property(s.property));
                }
                a.surfaces = glideslope::net::surfaces_on_the_wire(
                    [&](const std::string& property) -> std::optional<double> {
                        if (!flying.has_property(property)) {
                            return std::nullopt;
                        }
                        return flying.property(property);
                    });
            });
            glideslope::net::StatePacket packet;
            packet.aircraft.push_back(a);
            const auto bytes = glideslope::net::write_state(packet);
            check(bytes.has_value(), "the state update is written");
            const auto read = glideslope::net::read_state(
                std::span<const std::uint8_t>(bytes->data(), bytes->size()));
            check(read.has_value() && read->aircraft.size() == 1, "and read");
            glideslope::net::Interpolated shown;
            for (const double t : {0.0, 0.04}) {
                glideslope::net::RemoteState r;
                r.time_s = t;
                for (std::size_t i = 0; i < glideslope::net::carried_surfaces; ++i) {
                    r.surfaces_rad[i] =
                        glideslope::net::surface_off_the_wire(read->aircraft[0].surfaces[i]);
                }
                shown.received(r);
            }
            const glideslope::net::RemoteState drawn =
                shown.at(0.02 + glideslope::net::shown_behind_s);
            for (std::size_t i = 0; i < surfaces.size(); ++i) {
                const Surface& s = surfaces[i];
                ++seen;
                const auto slot = glideslope::net::carried_surface(s.property);
                if (!slot) {
                    failed += "\n  " + e.id + "'s " + s.name + " is turned by " + s.property +
                              ", which a state update does not carry";
                    continue;
                }
                const double got = drawn.surfaces_rad[*slot];
                std::printf("  %-13s %-15s stick %+.0f: flown %+6.2f deg, drawn %+6.2f\n",
                            e.id.c_str(), s.name.c_str(), way, flown[i] * 57.29578,
                            got * 57.29578);
                if (std::abs(got - flown[i]) <= quarter_degree + 1e-12 &&
                    std::abs(got) > quarter_degree) {
                    ++right;
                } else {
                    failed += "\n  " + e.id + "'s " + s.name + " with the controls " +
                              (way > 0 ? "right" : "left") + ": flown " +
                              std::to_string(flown[i]) + " rad, drawn " + std::to_string(got);
                }
            }
        }
    }
    std::printf("%zu aircraft; %zu of %zu surfaces drawn where flown; %zu expected from the "
                "catalogue\n",
                aircraft, right, seen, expected);
    check(aircraft == catalogue.size() && aircraft == 16,
          "all 16 aircraft were flown, not " + std::to_string(aircraft));
    check(failed.empty(), "wrong:" + failed);
    check(seen == expected && right == expected,
          std::to_string(right) + " of " + std::to_string(seen) +
              " surfaces drawn where flown, where the catalogue's aircraft have " +
              std::to_string(expected));
}

// **Every aircraft's drawn meshes carry a unit normal at every vertex, and
// colours with no light in them.** The light is the shader's
// (gfx/aircraft.hpp): it needs each vertex's normal, in the part's own frame,
// and a mesh whose colours were already lit would be lit twice. For every
// aircraft in the catalogue - counted against it - the body and every
// surface's part, as the client makes them: each vertex's normal within 0.02
// of unit length (a model's normals are stored to a 127th), and its colour and
// normal the model's own.
GLIDESLOPE_TEST(every_aircrafts_drawn_meshes_carry_a_unit_normal_at_every_vertex_and_unlit_colours) {
    const std::vector<CatalogueEntry> catalogue = glideslope::sim::read_catalogue(data_dir());
    std::size_t covered = 0;
    std::size_t meshes = 0;
    std::size_t vertices = 0;
    std::string failed;
    for (const CatalogueEntry& e : catalogue) {
        const std::filesystem::path base = models_dir() / e.id;
        const Model model = glideslope::gfx::read_model(base.string() + ".mesh");
        const ModelParts parts = glideslope::gfx::split_model(
            model, glideslope::gfx::read_livery(base.string() + ".livery"),
            glideslope::gfx::read_surfaces(base.string() + ".surfaces"));
        std::vector<const Model*> drawn{&parts.body};
        for (const Model& part : parts.surfaces) {
            drawn.push_back(&part);
        }
        double worst = 0.0;
        for (const Model* part : drawn) {
            const glideslope::gfx::Mesh mesh = glideslope::gfx::mesh_from_model(*part);
            if (mesh.vertices.size() != part->vertices.size() ||
                mesh.normals.size() != part->vertices.size() || mesh.vertices.empty()) {
                failed += "\n  " + e.id + ": a mesh of " + std::to_string(mesh.vertices.size()) +
                          " vertices and " + std::to_string(mesh.normals.size()) +
                          " normals from a part of " + std::to_string(part->vertices.size());
                continue;
            }
            for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
                const glideslope::gfx::Vertex& v = mesh.vertices[i];
                double l2 = 0.0;
                for (const float n : mesh.normals[i]) {
                    l2 += static_cast<double>(n) * static_cast<double>(n);
                }
                worst = std::max(worst, std::abs(std::sqrt(l2) - 1.0));
                if (v.colour[0] != part->vertices[i].colour[0] ||
                    v.colour[1] != part->vertices[i].colour[1] ||
                    v.colour[2] != part->vertices[i].colour[2] ||
                    mesh.normals[i] != part->vertices[i].normal) {
                    failed += "\n  " + e.id + ": vertex " + std::to_string(i) +
                              " is not the model's colour and normal";
                    break;
                }
            }
            ++meshes;
            vertices += mesh.vertices.size();
        }
        std::printf("  %-13s %zu meshes, normals off unit length by %.4f at worst\n",
                    e.id.c_str(), drawn.size(), worst);
        if (!(worst <= 0.02)) {
            failed += "\n  " + e.id + "'s normals are off unit length by " + std::to_string(worst);
        }
        ++covered;
    }
    std::printf("%zu of the catalogue's %zu aircraft covered: %zu meshes, %zu vertices\n",
                covered, catalogue.size(), meshes, vertices);
    check(covered == catalogue.size() && catalogue.size() == 16,
          "the catalogue has " + std::to_string(catalogue.size()) + " aircraft, " +
              std::to_string(covered) + " covered");
    check(failed.empty(), "wrong:" + failed);
}
