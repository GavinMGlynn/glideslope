#include "harness.hpp"

#include "gfx/aircraft.hpp"
#include "gfx/model.hpp"
#include "gfx/terrain_colour.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::gfx::Model;
using glideslope::gfx::ModelAlignment;
using glideslope::gfx::ModelError;
using glideslope::gfx::read_alignments;
using glideslope::gfx::read_model;

namespace {

std::filesystem::path data_dir() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

std::filesystem::path models_dir() {
    return data_dir() / "models";
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// The models that ship, by aircraft id.
std::vector<std::string> shipped() {
    std::vector<std::string> out;
    for (const auto& entry : std::filesystem::directory_iterator(models_dir())) {
        if (entry.path().extension() == ".mesh") {
            out.push_back(entry.path().stem().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// docs/ASSETS.md records each visual model under a heading of its own, with
// the table every entry there uses:
//
//   ### Visual model: <id> - <what it is>
//
//   | | |
//   | --- | --- |
//   | Source | ... |
//   | Revision | ... |
//   | Licence | ... |
//
// and each aircraft that has none under "### No visual model: <id>", whose
// table has a "Why" row. These read those back.
struct Entry {
    std::string source;
    std::string revision;
    std::string licence;
};

std::string assets_md() {
    return read_text(std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "docs" /
                     "ASSETS.md");
}

std::map<std::string, Entry> recorded_models(const std::string& text) {
    std::map<std::string, Entry> out;
    std::istringstream lines(text);
    std::string line;
    std::string current;
    while (std::getline(lines, line)) {
        if (line.rfind("### Visual model: ", 0) == 0) {
            const std::string rest = line.substr(18);
            const std::size_t dash = rest.find(" - ");
            current = dash == std::string::npos ? rest : rest.substr(0, dash);
            out[current];
        } else if (line.rfind("### ", 0) == 0) {
            current.clear();
        } else if (!current.empty()) {
            const auto field = [&](const char* label, std::string& into) {
                const std::string head = std::string("| ") + label + " | ";
                if (line.rfind(head, 0) == 0) {
                    into = line.substr(head.size());
                }
            };
            field("Source", out[current].source);
            field("Revision", out[current].revision);
            field("Licence", out[current].licence);
        }
    }
    return out;
}

std::map<std::string, std::string> recorded_absences(const std::string& text) {
    std::map<std::string, std::string> out;
    std::istringstream lines(text);
    std::string line;
    std::string current;
    while (std::getline(lines, line)) {
        if (line.rfind("### No visual model: ", 0) == 0) {
            current = line.substr(21);
            out[current];
        } else if (line.rfind("### ", 0) == 0) {
            current.clear();
        } else if (!current.empty() && line.rfind("| Why | ", 0) == 0) {
            out[current] = line.substr(8);
        }
    }
    return out;
}

// Each aircraft's published length, span and height in metres, and where the
// figure comes from. The model is held to length and span; its height is not
// checked, because a propeller's disc and an extended undercarriage are in
// the model and not in the published height - the Cub's disc alone puts it
// 35% over.
struct Dimensions {
    double length_m;
    double span_m;
};

const std::map<std::string, Dimensions>& published() {
    static const std::map<std::string, Dimensions> figures = {
        // Type certificate data sheets and the manufacturers' airport
        // planning documents, as docs/ASSETS.md records for each aircraft.
        {"737-300", {33.40, 28.88}},
        {"747-400", {70.66, 64.44}},
        {"787-8", {56.72, 60.12}},
        {"a320", {37.57, 35.80}},
        {"a380", {72.72, 79.75}},
        {"c172p", {8.28, 11.00}},
        {"c182", {8.84, 11.00}},
        {"j3cub", {6.83, 10.74}},
        // The air forces' and the manufacturers' figures, in feet and
        // inches: the B-2A 69 ft by 172 ft, the F-15C 63 ft 9 in by
        // 42 ft 9 3/4 in, the F-22A 62 ft 1 in by 44 ft 6 in, the Mosquito
        // FB Mk VI 41 ft 2 in by 54 ft 2 in and the Short S.23 88 ft by
        // 114 ft. docs/ASSETS.md records each one's source.
        {"b2", {21.03, 52.43}},
        {"f15c", {19.43, 13.05}},
        {"f22", {18.92, 13.56}},
        {"mosquito-fb6", {12.55, 16.51}},
        {"short_s23", {26.82, 34.75}},
        // The model is FlightGear's PA-28-161 Warrior II, whose figures these
        // are; glideslope's flight model is the PA-28-180 Cherokee, a
        // different wing. docs/ASSETS.md says so.
        {"pa28", {7.25, 10.67}},
    };
    return figures;
}

} // namespace

GLIDESLOPE_TEST(
    every_visual_model_that_ships_is_named_in_assets_md_with_its_source_revision_and_licence) {
    const std::vector<std::string> models = shipped();
    check(!models.empty(), "at least one visual model ships");
    const std::map<std::string, Entry> recorded = recorded_models(assets_md());
    for (const std::string& id : models) {
        const auto found = recorded.find(id);
        check(found != recorded.end(),
              id + " has an entry in docs/ASSETS.md");
        if (found == recorded.end()) {
            continue;
        }
        check(!found->second.source.empty(), id + "'s entry names its source");
        check(!found->second.revision.empty(),
              id + "'s entry names the revision it came from");
        check(!found->second.licence.empty(), id + "'s entry names its licence");
    }
    // ...and the other way: an entry for a model that does not ship is as
    // wrong as a model with no entry.
    for (const auto& [id, entry] : recorded) {
        check(std::find(models.begin(), models.end(), id) != models.end(),
              "docs/ASSETS.md's visual model " + id + " is a file that ships");
    }
    check(models.size() == recorded.size(),
          "every model that ships is recorded, and nothing else: " +
              std::to_string(models.size()) + " files, " +
              std::to_string(recorded.size()) + " entries");
}

GLIDESLOPE_TEST(every_aircraft_the_data_holds_has_a_visual_model_or_a_named_reason) {
    const std::vector<glideslope::sim::CatalogueEntry> roster =
        glideslope::sim::read_catalogue(data_dir());
    const std::vector<std::string> models = shipped();
    const std::string text = assets_md();
    const std::map<std::string, Entry> recorded = recorded_models(text);
    const std::map<std::string, std::string> absent = recorded_absences(text);

    std::size_t with = 0;
    std::size_t without = 0;
    for (const auto& aircraft : roster) {
        const bool has = std::find(models.begin(), models.end(), aircraft.id) !=
                         models.end();
        if (has) {
            ++with;
            check(recorded.count(aircraft.id) == 1,
                  aircraft.id + " ships a model and docs/ASSETS.md records it");
            continue;
        }
        ++without;
        const auto why = absent.find(aircraft.id);
        check(why != absent.end(),
              aircraft.id + " has no visual model, and docs/ASSETS.md says why");
        if (why != absent.end()) {
            check(why->second.size() > 20,
                  aircraft.id + "'s reason for having no model is a reason, "
                                "not a word: \"" + why->second + "\"");
        }
    }
    check(with + without == roster.size(),
          "every aircraft was looked at: " + std::to_string(roster.size()));
    // The roster is sixteen aircraft; fourteen have a FlightGear model. The
    // two that do not are the Learjet 35A and the F-35A, for which FGAddon
    // has nothing. If either number moves, this says so.
    check(roster.size() == 16,
          "the roster is sixteen aircraft, not " + std::to_string(roster.size()));
    check(with == 14, "fourteen of them ship a visual model, not " +
                          std::to_string(with));
    check(absent.size() == without,
          "docs/ASSETS.md names exactly the " + std::to_string(without) +
              " aircraft with no model, not " + std::to_string(absent.size()));
}

GLIDESLOPE_TEST(each_visual_model_is_its_aircrafts_size_and_faces_the_way_it_flies) {
    const std::vector<std::string> models = shipped();
    std::size_t checked = 0;
    std::size_t wider_aft = 0;
    std::size_t higher_aft = 0;
    std::size_t stands_on_its_gear = 0;
    for (const std::string& id : models) {
        const Model model = read_model(models_dir() / (id + ".mesh"));
        check(!model.vertices.empty(), id + " holds geometry");
        check(model.indices.size() % 3 == 0, id + " holds whole triangles");

        const auto found = published().find(id);
        check(found != published().end(),
              id + " has published dimensions to be held to");
        if (found == published().end()) {
            continue;
        }
        ++checked;
        const double length = static_cast<double>(model.size()[0]);
        const double span = static_cast<double>(model.size()[1]);
        // Five per cent: the models carry aerials, wingtip lights and static
        // wicks that the published figures do not, and the published figures
        // are rounded.
        check(std::abs(length / found->second.length_m - 1.0) < 0.05,
              id + " is " + std::to_string(length) + " m long, against a "
                   "published " + std::to_string(found->second.length_m));
        check(std::abs(span / found->second.span_m - 1.0) < 0.05,
              id + " spans " + std::to_string(span) + " m, against a "
                   "published " + std::to_string(found->second.span_m));

        // The way a model faces is held by two facts, and every model is
        // held to both but one, which is named here with the fact it
        // breaks, why it breaks it, and the fact held in its place.
        //
        //   wider aft   the aft seventh is at least 1.3 times as wide as
        //               the forward seventh - the tailplane against the
        //               nose. Mirrored nose to tail this reads the other
        //               way round. Every model but the Mosquito, whose two
        //               propellers stand at its nose and are 3.8 m across,
        //               wider than its tailplane: it measures 0.77.
        //
        //   higher aft  the aft seventh reaches higher than the forward
        //               seventh by at least a fiftieth of the model's
        //               height - the fin. Mirrored nose to tail, or turned
        //               upside down, it reads the other way round. Every
        //               model but the B-2, a flying wing with no fin,
        //               whose highest point is its cockpit: it measures
        //               -21.2%. The narrowest that holds is the F-22's
        //               5.4%, whose fins are canted and low and whose
        //               canopy is high; the widest is the A380's 58.9%.
        //
        // The B-2 is held instead to standing on its undercarriage: the
        // lowest tenth of it is spread along 55% of its length, at the
        // three legs, against 39% for the highest tenth, its cockpit and
        // engine humps. Upside down those swap. No other model is held to
        // it: drawn level, a taildragger's tailwheel is well above its
        // main wheels, so the Cub's lowest tenth is its two main wheels
        // alone and spreads over 7% of it.
        const double low_x = static_cast<double>(model.low[0]);
        const double high_x = static_cast<double>(model.high[0]);
        const double height = static_cast<double>(model.size()[2]);
        const double nose_cut = high_x - length / 7.0;
        const double tail_cut = low_x + length / 7.0;
        double at_nose = 0.0;
        double at_tail = 0.0;
        // Seeded from the box the model fills, so that a band's own
        // extreme is always what comes out of the walk.
        double top_nose = static_cast<double>(model.high[2]);
        double top_tail = top_nose;
        double low_first = high_x;
        double low_last = low_x;
        double high_first = high_x;
        double high_last = low_x;
        for (const auto& v : model.vertices) {
            const double x = static_cast<double>(v.position[0]);
            const double y = static_cast<double>(v.position[1]);
            const double z = static_cast<double>(v.position[2]);
            if (x >= nose_cut) {
                at_nose = std::max(at_nose, std::abs(y));
                top_nose = std::min(top_nose, z);
            }
            if (x <= tail_cut) {
                at_tail = std::max(at_tail, std::abs(y));
                top_tail = std::min(top_tail, z);
            }
            // +z is down, so the lowest tenth is the last tenth of z.
            if (z >= static_cast<double>(model.high[2]) - 0.10 * height) {
                low_first = std::min(low_first, x);
                low_last = std::max(low_last, x);
            }
            if (z <= static_cast<double>(model.low[2]) + 0.10 * height) {
                high_first = std::min(high_first, x);
                high_last = std::max(high_last, x);
            }
        }

        if (id != "mosquito-fb6") {
            ++wider_aft;
            check(at_tail > at_nose * 1.3,
                  id + " is wider at the tail (" + std::to_string(at_tail) +
                      " m) than at the nose (" + std::to_string(at_nose) +
                      " m)");
        }
        if (id != "b2") {
            ++higher_aft;
            check(top_nose - top_tail > 0.02 * height,
                  id + "'s fin makes its tail reach " +
                      std::to_string((top_nose - top_tail) / height * 100.0) +
                      "% of its height higher than its nose");
        } else {
            ++stands_on_its_gear;
            check(low_last - low_first > (high_last - high_first) * 1.2,
                  id + " stands on its undercarriage: its lowest tenth "
                       "spreads over " +
                      std::to_string((low_last - low_first) / length * 100.0) +
                      "% of it, its highest over " +
                      std::to_string((high_last - high_first) / length * 100.0) +
                      "%");
        }
    }
    // Every model was measured, and the two named exceptions are the only
    // ones: thirteen held to each of the two facts, and the B-2 to the one
    // of its own.
    check(wider_aft == models.size() - 1,
          "every model but the Mosquito was held to being wider aft: " +
              std::to_string(wider_aft));
    check(higher_aft == models.size() - 1,
          "every model but the B-2 was held to reaching higher aft: " +
              std::to_string(higher_aft));
    check(stands_on_its_gear == 1,
          "the B-2 alone was held to standing on its undercarriage: " +
              std::to_string(stands_on_its_gear));
    check(checked == models.size(),
          "every model that ships was measured: " + std::to_string(checked) +
              " of " + std::to_string(models.size()));
}

GLIDESLOPE_TEST(a_model_file_that_is_damaged_or_of_another_version_is_refused) {
    const std::vector<std::string> models = shipped();
    check(!models.empty(), "there is a model to damage");
    std::ifstream in(models_dir() / (models.front() + ".mesh"), std::ios::binary);
    const std::vector<std::uint8_t> whole((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    check(!whole.empty(), "the model was read");

    const auto refused = [](const std::vector<std::uint8_t>& bytes,
                            const std::string& what) {
        try {
            read_model(bytes, "damaged");
        } catch (const ModelError&) {
            return;
        }
        check(false, what);
    };

    refused({}, "an empty file is refused");
    refused({'n', 'o', 't', 'i', 't', 0, 0, 1}, "a file that is not a model is refused");

    std::vector<std::uint8_t> wrong_version = whole;
    wrong_version[7] = 2;
    refused(wrong_version, "a model of a version this does not read is refused");

    std::vector<std::uint8_t> cut = whole;
    cut.resize(whole.size() - 4);
    refused(cut, "a model cut short is refused");

    // An index past the vertices would be read off the end of the buffer.
    std::vector<std::uint8_t> bad_index = whole;
    for (std::size_t i = 0; i < 4; ++i) {
        bad_index[bad_index.size() - 4 + i] = 0xFF;
    }
    refused(bad_index, "a model holding an index past its vertices is refused");

    // ...and the whole file is still read, so the four above failed for the
    // damage and not because nothing loads.
    const Model model = read_model(whole, "whole");
    check(!model.vertices.empty(), "the undamaged model still loads");
}

// --- where a model sits on the aeroplane it draws ---------------------------
//
// A model is drawn at its flight model's visual reference point - JSBSim's
// VRP - moved by the offset in assets/models/alignment.txt, which
// tools/align_models.py measures from the committed meshes and the flight
// models. These read the same things out of a *running* JSBSim, so the
// script's reading of the XML and JSBSim's own are cross-checked rather than
// one trusting the other.

namespace {

constexpr double metres_per_inch = 0.0254;
constexpr double metres_per_foot = 0.3048;

std::filesystem::path alignment_file() {
    return models_dir() / "alignment.txt";
}

// JSBSim's structural frame is +x aft, +y starboard, +z up, in inches; the
// body frame is +x forward, +y starboard, +z down, in metres.
std::array<double, 3> body_from_structural(const std::array<double, 3>& point,
                                           const std::array<double, 3>& origin) {
    return {-(point[0] - origin[0]) * metres_per_inch,
            (point[1] - origin[1]) * metres_per_inch,
            -(point[2] - origin[2]) * metres_per_inch};
}

std::array<double, 3> visual_reference_point(const glideslope::sim::Aircraft& a) {
    return {a.property("metrics/visualrefpoint-x-in"),
            a.property("metrics/visualrefpoint-y-in"),
            a.property("metrics/visualrefpoint-z-in")};
}

std::array<double, 3> centre_of_gravity(const glideslope::sim::Aircraft& a) {
    return {a.property("inertia/cg-x-in"), a.property("inertia/cg-y-in"),
            a.property("inertia/cg-z-in")};
}

// Every contact the flight model has, in the body frame about the VRP.
// JSBSim names a wheel gear/unit[i] and a piece of structure contact/unit[i],
// so both are tried; the count is gear/num-units, which covers the two.
std::vector<std::array<double, 3>> contacts(const glideslope::sim::Aircraft& a) {
    const std::array<double, 3> vrp = visual_reference_point(a);
    const int units = static_cast<int>(a.property("gear/num-units"));
    std::vector<std::array<double, 3>> out;
    for (int i = 0; i < units; ++i) {
        const std::string index = "[" + std::to_string(i) + "]";
        for (const char* base : {"gear/unit", "contact/unit"}) {
            try {
                const std::string at = std::string(base) + index + "/";
                out.push_back(body_from_structural(
                    {a.property(at + "x-position"), a.property(at + "y-position"),
                     a.property(at + "z-position")},
                    vrp));
                break;
            } catch (const std::out_of_range&) {
                // The other of the two names it.
            }
        }
    }
    return out;
}

// The nearest the model comes to `point`, both in the body frame about the
// VRP, with the alignment already applied to the model.
double nearest(const Model& model, const std::array<double, 3>& offset,
               const std::array<double, 3>& point) {
    double best = std::numeric_limits<double>::infinity();
    for (const auto& v : model.vertices) {
        const double dx = static_cast<double>(v.position[0]) + offset[0] - point[0];
        const double dy = static_cast<double>(v.position[1]) + offset[1] - point[1];
        const double dz = static_cast<double>(v.position[2]) + offset[2] - point[2];
        best = std::min(best, dx * dx + dy * dy + dz * dz);
    }
    return std::sqrt(best);
}

} // namespace

GLIDESLOPE_TEST(every_visual_model_is_aligned_to_the_aeroplane_it_draws) {
    const std::vector<std::string> models = shipped();
    const std::map<std::string, ModelAlignment> aligned =
        read_alignments(alignment_file());
    check(aligned.size() == models.size(),
          "every model that ships is aligned, and nothing else: " +
              std::to_string(models.size()) + " models, " +
              std::to_string(aligned.size()) + " alignments");
    for (const std::string& id : models) {
        check(aligned.count(id) == 1, id + " has an alignment");
    }

    // The script read the flight models' XML; JSBSim reads them itself. If
    // the two disagree about how many contacts an aeroplane has, one of them
    // is reading the wrong file.
    std::size_t checked = 0;
    for (const auto& [id, a] : aligned) {
        glideslope::sim::Aircraft aircraft(std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR),
                                           id);
        const int units = static_cast<int>(aircraft.property("gear/num-units"));
        check(a.wheels + a.shape == units,
              id + " has " + std::to_string(units) +
                  " contacts in JSBSim, against the " +
                  std::to_string(a.wheels) + " it rests on and " +
                  std::to_string(a.shape) + " describing it that the "
                  "alignment names");
        check(static_cast<int>(contacts(aircraft).size()) == units,
              id + "'s contacts can all be read back out of JSBSim");
        ++checked;
    }
    check(checked == models.size(),
          "every model was looked at: " + std::to_string(checked));
}

GLIDESLOPE_TEST(each_visual_models_wheels_sit_on_the_ground_the_aeroplane_stands_on) {
    const std::map<std::string, ModelAlignment> aligned =
        read_alignments(alignment_file());
    const std::vector<glideslope::sim::CatalogueEntry> roster =
        glideslope::sim::read_catalogue(data_dir());
    std::size_t stood = 0;
    std::size_t afloat = 0;
    std::size_t without = 0;
    for (const auto& e : roster) {
        const auto it = aligned.find(e.id);
        if (it == aligned.end()) {
            ++without; // no visual model; named by the tests above
            continue;
        }
        if (e.seaplane) {
            // A flying boat does not stand: it floats, and its hull sits
            // below the surface by its draught - the Short S.23's is 3.7 ft -
            // so "the wheels on the ground" is not a fact about it. Its keels
            // are held to the flight model's by the geometry test below,
            // which is where its hull is pinned.
            ++afloat;
            continue;
        }
        glideslope::sim::Aircraft aircraft(data_dir() / "jsbsim", e.model);
        glideslope::sim::InitialConditions ic;
        ic.latitude_deg = -33.9;
        ic.longitude_deg = 151.2;
        // At the surface itself: initialize() raises the aircraft until its
        // lowest wheel touches.
        ic.altitude_ft = 0.0;
        ic.terrain_elevation_ft = 0.0;
        ic.airspeed_kts = 0.0;
        ic.engine_running = false;
        ic.gear = 1.0;
        aircraft.initialize(ic);
        glideslope::sim::Controls held;
        held.left_brake = 1.0;
        held.right_brake = 1.0;
        aircraft.set_controls(held);
        for (int i = 0; i < 20 * 120; ++i) {
            aircraft.step();
        }

        const glideslope::sim::AircraftState state = aircraft.state();
        const Model model = read_model(models_dir() / (e.id + ".mesh"));

        // The model is drawn about the visual reference point; the height
        // JSBSim reports is its centre of gravity's, so the two are put in
        // the same place first.
        const std::array<double, 3> vrp = visual_reference_point(aircraft);
        const std::array<double, 3> to_vrp =
            body_from_structural(vrp, centre_of_gravity(aircraft));
        // Which way is down, in the body frame, at the attitude it settled
        // at: an aeroplane standing on its wheels is pitched, and a
        // taildragger a long way.
        const double pitch = state.pitch_deg * 3.14159265358979323846 / 180.0;
        const double roll = state.roll_deg * 3.14159265358979323846 / 180.0;
        const std::array<double, 3> down{-std::sin(pitch),
                                         std::sin(roll) * std::cos(pitch),
                                         std::cos(roll) * std::cos(pitch)};
        const double cg_above_ground_m =
            state.height_above_ground_ft * metres_per_foot;
        const auto above_ground = [&](const std::array<double, 3>& about_cg) {
            return cg_above_ground_m - (about_cg[0] * down[0] +
                                        about_cg[1] * down[1] +
                                        about_cg[2] * down[2]);
        };

        // Which contacts are taking the weight is JSBSim's own answer, and
        // holding the model to those alone is the point: the lowest thing
        // on an aeroplane is not always a wheel - the B-2's bomb bay doors
        // hang 1.4 m below its undercarriage - and the ones that are not
        // wheels are not on the ground.
        //
        // How many there are is what tools/align_models.py worked out from
        // the shape of the contact set, without running anything. If the two
        // disagree, its reasoning about which contacts an aeroplane stands
        // on is wrong.
        const int units = static_cast<int>(aircraft.property("gear/num-units"));
        std::vector<std::array<double, 3>> resting;
        double squashed = 0.0;
        for (int i = 0; i < units; ++i) {
            const std::string index = "[" + std::to_string(i) + "]";
            for (const char* base : {"gear/unit", "contact/unit"}) {
                const std::string at = std::string(base) + index + "/";
                try {
                    const double weight = aircraft.property(at + "WOW");
                    if (weight != 0.0) {
                        resting.push_back(body_from_structural(
                            {aircraft.property(at + "x-position"),
                             aircraft.property(at + "y-position"),
                             aircraft.property(at + "z-position")},
                            vrp));
                        squashed = std::max(squashed,
                                            aircraft.property(at + "compression-ft"));
                    }
                    break;
                } catch (const std::out_of_range&) {
                    // The other of the two names it.
                }
            }
        }
        check(static_cast<int>(resting.size()) == it->second.wheels,
              e.id + " stands on the " + std::to_string(it->second.wheels) +
                  " contacts its alignment was fitted to, and JSBSim puts "
                  "weight on " + std::to_string(resting.size()));

        // Standing, the gear is compressed: the aeroplane rests lower than
        // the contact points the model was fitted to, which are where the
        // wheels are with the legs at full extension. A rigid model
        // therefore sinks by the compression, which is not a fault in the
        // alignment.
        const double sunk = squashed * metres_per_foot;
        // Below the ground it may go by what the fit left over plus the
        // compression; above it only by what the fit left over, because a
        // wheel drawn above the ground is an aeroplane floating.
        const double into = it->second.on_wheels_m + sunk + 0.02;
        const double over = it->second.on_wheels_m + 0.02;
        // The same radius tools/align_models.py fitted with: the model's own
        // ground under a wheel, and not the wing above it.
        const double radius =
            std::max(0.03 * static_cast<double>(model.size()[0]), 0.30);
        double worst_wheel = 0.0;
        for (std::size_t w = 0; w < resting.size(); ++w) {
            double lowest = std::numeric_limits<double>::infinity();
            for (const auto& v : model.vertices) {
                const std::array<double, 3> p{
                    static_cast<double>(v.position[0]) + it->second.offset[0],
                    static_cast<double>(v.position[1]) + it->second.offset[1],
                    static_cast<double>(v.position[2]) + it->second.offset[2]};
                if (std::abs(p[0] - resting[w][0]) >= radius ||
                    std::abs(p[1] - resting[w][1]) >= radius) {
                    continue;
                }
                lowest = std::min(lowest,
                                  above_ground({p[0] + to_vrp[0], p[1] + to_vrp[1],
                                                p[2] + to_vrp[2]}));
            }
            check(std::isfinite(lowest),
                  e.id + " has model under the wheel it rests on");
            check(lowest <= over && lowest >= -into,
                  e.id + "'s wheel " + std::to_string(w) +
                      " sits on the ground: the model under it is " +
                      std::to_string(lowest) + " m above it, outside the " +
                      std::to_string(over) + " m over and " +
                      std::to_string(into) + " m into it that its left-over (" +
                      std::to_string(it->second.on_wheels_m) +
                      " m) and its gear's compression (" + std::to_string(sunk) +
                      " m) allow");
            worst_wheel = std::max(worst_wheel, std::abs(lowest));
        }
        std::printf("%-14s stands on %zu wheels, %.3f m of compression; its "
                    "model under them is at most %.3f m off the ground, "
                    "against %.3f over and %.3f into it\n",
                    e.id.c_str(), resting.size(), sunk, worst_wheel, over, into);
        ++stood;
    }
    std::printf("stood %zu, afloat %zu, without a model %zu\n", stood, afloat,
                without);
}

GLIDESLOPE_TEST(each_visual_model_is_where_its_flight_model_says_the_aeroplane_is) {
    const std::map<std::string, ModelAlignment> aligned =
        read_alignments(alignment_file());
    std::size_t spanned = 0;
    std::size_t contacts_checked = 0;
    for (const auto& [id, a] : aligned) {
        glideslope::sim::Aircraft aircraft(data_dir() / "jsbsim", id);
        const Model model = read_model(models_dir() / (id + ".mesh"));

        // The span is the one figure every flight model states about its
        // shape. Six per cent covers them all: the model carries wingtip
        // lights and static wicks the flight model's figure does not, and
        // the A320 is the tightest at 5.5% - its model has sharklets and its
        // flight model's span is the fence-tipped wing's.
        const double stated = aircraft.property("metrics/bw-ft") * metres_per_foot;
        const double drawn = static_cast<double>(model.size()[1]);
        if (id == "pa28") {
            // FlightGear has no PA-28-180 Cherokee: the model is the
            // PA-28-161 Warrior II, whose wing is tapered and 35 ft 0 in
            // across where the Cherokee's is constant-chord and 30 ft.
            // docs/ASSETS.md says so. It is held to the wing it actually
            // draws instead.
            check(std::abs(drawn / 10.67 - 1.0) < 0.01,
                  id + " draws the Warrior II's 10.67 m wing: " +
                      std::to_string(drawn) + " m");
        } else {
            check(std::abs(drawn / stated - 1.0) < 0.06,
                  id + " spans " + std::to_string(drawn) +
                      " m, against the " + std::to_string(stated) +
                      " m its flight model states");
        }
        ++spanned;

        // Every contact the flight model has is a point on the aeroplane's
        // surface - a wheel on the ground, a wingtip, a tailcone, a radome -
        // so with the model put where the alignment says, the model should
        // be there too. How far it is at worst is measured per aircraft by
        // tools/align_models.py and recorded beside the offset; this holds
        // each one to its own figures and so fails if a model moves.
        const double allowed = std::max(a.on_wheels_m, a.at_shape_m) + 0.02;
        const std::vector<std::array<double, 3>> points = contacts(aircraft);
        check(static_cast<int>(points.size()) == a.wheels + a.shape,
              id + " has the contacts its alignment counts");
        for (std::size_t i = 0; i < points.size(); ++i) {
            const double d = nearest(model, a.offset, points[i]);
            check(d <= allowed,
                  id + "'s contact " + std::to_string(i) + " is " +
                      std::to_string(d) + " m from its model, against the " +
                      std::to_string(allowed) + " m it is held to");
            ++contacts_checked;
        }
    }
    check(spanned == aligned.size(),
          "every aligned model's span was held to its flight model's: " +
              std::to_string(spanned));

    // **Every flight model now describes the aeroplane beyond its
    // undercarriage.** Four once stated nothing but their wheels - the
    // 737-300, the 747-400, the B-2 and the F-22 - so their span was the
    // only shape they could be held to. Each has since gained the airframe
    // contacts a wheels-up landing comes down on, measured from its own
    // visual mesh by tools/ground.py. This set is empty so that an aeroplane
    // losing its shape again cannot pass unnoticed.
    const std::set<std::string> wheels_only{};
    std::set<std::string> found_wheels_only;
    for (const auto& [id, a] : aligned) {
        if (a.shape == 0) {
            found_wheels_only.insert(id);
        }
    }
    check(found_wheels_only == wheels_only,
          "no flight model describes nothing but its undercarriage, but these "
          "do: " +
              [&] {
                  std::string names;
                  for (const std::string& id : found_wheels_only) {
                      names += (names.empty() ? "" : ", ") + id;
                  }
                  return names.empty() ? "none" : names;
              }());
    std::printf("held %zu contacts across %zu aircraft to their models\n",
                contacts_checked, spanned);
}

// --- the views of an aeroplane ---------------------------------------------

namespace {

using glideslope::gfx::View;

// Where an aeroplane standing still at Sydney is, and which way its body
// points: level, heading north. The views are geometry, so any one attitude
// pins them; the flight screen's own axes are held by the frame tests.
glideslope::gfx::Placement level_at(double latitude_deg, double longitude_deg,
                                    double height_m) {
    const glideslope::world::Ecef up =
        glideslope::gfx::up_at(latitude_deg, longitude_deg);
    const double lat = latitude_deg * 3.14159265358979323846 / 180.0;
    const double lon = longitude_deg * 3.14159265358979323846 / 180.0;
    const glideslope::world::Ecef north{-std::sin(lat) * std::cos(lon),
                                        -std::sin(lat) * std::sin(lon),
                                        std::cos(lat)};
    const glideslope::world::Ecef east{-std::sin(lon), std::cos(lon), 0.0};
    glideslope::gfx::Placement p;
    p.origin = glideslope::world::to_ecef({latitude_deg, longitude_deg, height_m});
    // Forward north, starboard east, down the other way from up.
    p.world_from_local =
        glideslope::gfx::Mat3::columns(north, east, {-up.x, -up.y, -up.z});
    return p;
}

double apart(const glideslope::world::Ecef& a, const glideslope::world::Ecef& b) {
    return std::hypot(std::hypot(a.x - b.x, a.y - b.y), a.z - b.z);
}

} // namespace

GLIDESLOPE_TEST(the_cockpit_view_puts_the_eye_where_the_flight_model_says_the_pilots_is) {
    const std::map<std::string, ModelAlignment> aligned =
        read_alignments(alignment_file());
    std::size_t checked = 0;
    for (const auto& [id, a] : aligned) {
        glideslope::sim::Aircraft aircraft(data_dir() / "jsbsim", id);
        const auto inches = [&](const char* what, const char* axis) {
            return aircraft.property(std::string("metrics/") + what + axis);
        };
        // The eye, in the body frame, from the model's origin - the visual
        // reference point moved by the alignment - which is what the view
        // is given.
        const std::array<double, 3> eye{
            -(inches("eyepoint", "-x-in") - inches("visualrefpoint", "-x-in")) *
                    metres_per_inch -
                a.offset[0],
            (inches("eyepoint", "-y-in") - inches("visualrefpoint", "-y-in")) *
                    metres_per_inch -
                a.offset[1],
            -(inches("eyepoint", "-z-in") - inches("visualrefpoint", "-z-in")) *
                    metres_per_inch -
                a.offset[2]};

        const glideslope::gfx::Placement p = level_at(-33.9, 151.2, 1000.0);
        const glideslope::gfx::Camera camera =
            glideslope::gfx::camera_for(View::cockpit, p, 10.0, eye, 0.0);

        // The eye is where the flight model puts the pilot's, to the
        // millimetre: the camera's position less the model's origin, taken
        // back into the body frame, is the eye it was given.
        const glideslope::world::Ecef from{camera.position.x - p.origin.x,
                                           camera.position.y - p.origin.y,
                                           camera.position.z - p.origin.z};
        const auto& m = p.world_from_local.m;
        const std::array<double, 3> in_body{
            m[0] * from.x + m[1] * from.y + m[2] * from.z,
            m[3] * from.x + m[4] * from.y + m[5] * from.z,
            m[6] * from.x + m[7] * from.y + m[8] * from.z};
        for (std::size_t i = 0; i < 3; ++i) {
            check(std::abs(in_body[i] - eye[i]) < 0.001,
                  id + "'s cockpit eye is the pilot's along axis " +
                      std::to_string(i) + ": " + std::to_string(in_body[i]) +
                      " m against " + std::to_string(eye[i]));
        }
        // ...and it looks out along the nose, not along anything else.
        const glideslope::world::Ecef nose =
            p.world_from_local * glideslope::world::Ecef{1.0, 0.0, 0.0};
        const auto& c = camera.world_from_camera.m;
        check(std::abs(-c[6] - nose.x) < 1e-9 && std::abs(-c[7] - nose.y) < 1e-9 &&
                  std::abs(-c[8] - nose.z) < 1e-9,
              id + "'s cockpit looks out along its nose");
        ++checked;
    }
    check(checked == aligned.size(),
          "every aircraft with a model was looked at: " + std::to_string(checked));
}

GLIDESLOPE_TEST(every_view_stands_where_its_name_says_and_looks_at_the_aeroplane) {
    const glideslope::gfx::Placement p = level_at(-33.9, 151.2, 1000.0);
    const double radius = 5.0;
    const std::array<double, 3> eye{0.5, 0.0, -0.5};

    // The names round trip, and there are seven of them: the cockpit and the
    // six the plan asks for - ahead, behind, left, right, above and an orbit.
    const std::vector<View>& all = glideslope::gfx::every_view();
    check(all.size() == 7, "there are seven views, not " +
                               std::to_string(all.size()));
    for (const View v : all) {
        check(glideslope::gfx::view_named(glideslope::gfx::name_of(v)) == v,
              std::string(glideslope::gfx::name_of(v)) + " is named by its name");
    }
    check(!glideslope::gfx::view_named("nonesuch").has_value(),
          "a view there is none of is refused");

    // Each outside view stands where its name says, in the aeroplane's own
    // frame: ahead of it, behind it, off each wing, above it. The body is
    // +x forward, +y starboard, +z down.
    struct Expected {
        View view;
        int axis;  // 0 forward, 1 starboard, 2 down
        double way; // +1 or -1 along it
    };
    const Expected expected[] = {{View::ahead, 0, 1.0},  {View::behind, 0, -1.0},
                                 {View::left, 1, -1.0},  {View::right, 1, 1.0},
                                 {View::above, 2, -1.0}};
    std::size_t placed = 0;
    for (const Expected& e : expected) {
        const glideslope::gfx::Camera camera =
            glideslope::gfx::camera_for(e.view, p, radius, eye, 0.0);
        const glideslope::world::Ecef from{camera.position.x - p.origin.x,
                                           camera.position.y - p.origin.y,
                                           camera.position.z - p.origin.z};
        const auto& m = p.world_from_local.m;
        const std::array<double, 3> in_body{
            m[0] * from.x + m[1] * from.y + m[2] * from.z,
            m[3] * from.x + m[4] * from.y + m[5] * from.z,
            m[6] * from.x + m[7] * from.y + m[8] * from.z};
        const std::string what(glideslope::gfx::name_of(e.view));
        check(in_body[static_cast<std::size_t>(e.axis)] * e.way > radius,
              what + " stands that way from the aeroplane: " +
                  std::to_string(in_body[static_cast<std::size_t>(e.axis)]) +
                  " m along axis " + std::to_string(e.axis));
        // ...and looks back at it: the way it looks is the negative of its
        // third column, and the aeroplane is that way.
        const auto& c = camera.world_from_camera.m;
        const double towards =
            -c[6] * (-from.x) - c[7] * (-from.y) - c[8] * (-from.z);
        check(towards > 0.0, what + " looks at the aeroplane");
        ++placed;
    }
    check(placed == 5, "five outside views stand where their names say");

    // The orbit goes round: half a turn from where it started is the other
    // side of the aeroplane, and a whole turn is back where it began.
    const glideslope::gfx::Camera start =
        glideslope::gfx::camera_for(View::orbit, p, radius, eye, 0.0);
    const glideslope::gfx::Camera half = glideslope::gfx::camera_for(
        View::orbit, p, radius, eye, 3.14159265358979323846);
    const glideslope::gfx::Camera round = glideslope::gfx::camera_for(
        View::orbit, p, radius, eye, 2.0 * 3.14159265358979323846);
    check(apart(start.position, half.position) > radius * 4.0,
          "half an orbit is the other side of the aeroplane: " +
              std::to_string(apart(start.position, half.position)) + " m");
    check(apart(start.position, round.position) < 0.001,
          "a whole orbit is back where it began: " +
              std::to_string(apart(start.position, round.position)) + " m");
}
