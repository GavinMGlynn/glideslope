#include "harness.hpp"

#include "gfx/model.hpp"
#include "sim/catalogue.hpp"

#include <algorithm>
#include <cmath>
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
using glideslope::gfx::ModelError;
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
