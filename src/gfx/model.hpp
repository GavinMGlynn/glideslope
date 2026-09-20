#pragma once

// An aircraft's visual model, read from the form tools/make_models.py writes.
//
// The models come from FlightGear's aircraft; docs/ASSETS.md names each one's
// source, revision and licence. A model holds geometry and nothing else: no
// texture, no livery and no animation, so control surfaces, gear and
// propellers are welded where the model has them.
//
// **The frame is the aircraft's body frame**, JSBSim's: +x forward out of the
// nose, +y out of the starboard wing, +z down, in metres from the origin the
// FlightGear model was authored around. That origin is the FlightGear
// aircraft's own, which is not always its flight model's, so a model is drawn
// at the flight model's visual reference point moved by the alignment below.
//
// Positions are quantised to 16 bits across the model's own bounding box -
// under a millimetre on the largest aeroplane here - because these are the
// largest files the repository carries.

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace glideslope::gfx {

struct ModelVertex {
    std::array<float, 3> position{}; // metres, body frame
    std::array<float, 3> normal{};   // unit, body frame
    std::array<float, 4> colour{};   // linear RGBA, 0..1; alpha is always 1
};

struct Model {
    std::vector<ModelVertex> vertices;
    std::vector<std::uint32_t> indices; // triangles
    std::array<float, 3> low{};         // the bounding box, body frame
    std::array<float, 3> high{};

    std::array<float, 3> size() const {
        return {high[0] - low[0], high[1] - low[1], high[2] - low[2]};
    }
};

class ModelError : public std::runtime_error {
public:
    explicit ModelError(const std::string& what) : std::runtime_error(what) {}
};

// The model in `path`. Throws ModelError if the file is not one, is of a
// version this does not read, is cut short, or holds an index past its own
// vertices - which a renderer would read off the end of the buffer.
Model read_model(const std::filesystem::path& path);

// The same, from bytes already in hand. `name` appears in what it throws.
Model read_model(const std::vector<std::uint8_t>& bytes, const std::string& name);

// Where an aircraft's visual model sits on the aeroplane it draws.
//
// A model is drawn at its flight model's visual reference point - JSBSim's
// VRP, which is what a VRP is for - moved by `offset`, in metres in the body
// frame. tools/align_models.py measures it by putting the model's
// undercarriage on the flight model's, and writes assets/models/alignment.txt;
// its docstring says how, and a test fails if the committed file differs from
// what it makes.
//
// The two distances are what is left over, because a flight model and a
// visual model of the same aeroplane do not always agree and no placement can
// make them: `on_wheels_m` is the worst distance between a contact the
// aeroplane rests on and the model beneath it, and `at_shape_m` the worst at
// the `shape` contacts that describe it elsewhere - a wingtip, a tailcone, a
// radome. Each aircraft is held to its own two.
struct ModelAlignment {
    std::array<double, 3> offset{}; // metres, body frame
    int wheels = 0;                 // contacts the aeroplane rests on
    double on_wheels_m = 0.0;
    int shape = 0; // contacts that describe it elsewhere
    double at_shape_m = 0.0;
};

class AlignmentError : public std::runtime_error {
public:
    explicit AlignmentError(const std::string& what) : std::runtime_error(what) {}
};

// assets/models/alignment.txt, by aircraft id. Throws AlignmentError naming
// the line of anything it cannot read.
std::map<std::string, ModelAlignment> read_alignments(
    const std::filesystem::path& path);

} // namespace glideslope::gfx
