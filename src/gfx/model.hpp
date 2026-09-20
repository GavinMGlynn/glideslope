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
// aircraft's own, which is not always its flight model's: putting a model on
// the aircraft glideslope flies needs an alignment this does not yet carry.
//
// Positions are quantised to 16 bits across the model's own bounding box -
// under a millimetre on the largest aeroplane here - because these are the
// largest files the repository carries.

#include <array>
#include <cstdint>
#include <filesystem>
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

} // namespace glideslope::gfx
