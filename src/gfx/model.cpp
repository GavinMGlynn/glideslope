#include "gfx/model.hpp"

#include <cstring>
#include <fstream>
#include <iterator>

namespace glideslope::gfx {

namespace {

constexpr char magic[] = "GSMESH";
constexpr std::size_t magic_size = 7; // six letters and the nul after them
constexpr std::uint8_t version = 1;
constexpr std::size_t header_size = magic_size + 1 + 4 + 4 + 6 * 4;
constexpr std::size_t vertex_size = 3 * 2 + 3 + 3;

std::uint32_t read_u32(const std::uint8_t* at) {
    return static_cast<std::uint32_t>(at[0]) |
           static_cast<std::uint32_t>(at[1]) << 8 |
           static_cast<std::uint32_t>(at[2]) << 16 |
           static_cast<std::uint32_t>(at[3]) << 24;
}

std::uint16_t read_u16(const std::uint8_t* at) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(at[0]) |
                                      static_cast<std::uint16_t>(at[1]) << 8);
}

float read_f32(const std::uint8_t* at) {
    const std::uint32_t bits = read_u32(at);
    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof out);
    return out;
}

} // namespace

Model read_model(const std::vector<std::uint8_t>& bytes, const std::string& name) {
    if (bytes.size() < header_size ||
        std::memcmp(bytes.data(), magic, magic_size) != 0) {
        throw ModelError(name + " is not a glideslope model");
    }
    if (bytes[magic_size] != version) {
        throw ModelError(name + " is a model of version " +
                         std::to_string(bytes[magic_size]) + ", not " +
                         std::to_string(version));
    }
    Model model;
    const std::uint32_t vertices = read_u32(&bytes[magic_size + 1]);
    const std::uint32_t indices = read_u32(&bytes[magic_size + 5]);
    for (std::size_t i = 0; i < 3; ++i) {
        model.low[i] = read_f32(&bytes[magic_size + 9 + i * 4]);
        model.high[i] = read_f32(&bytes[magic_size + 21 + i * 4]);
    }
    if (indices % 3 != 0) {
        throw ModelError(name + " holds " + std::to_string(indices) +
                         " indices, which is not whole triangles");
    }
    const std::size_t want =
        header_size + static_cast<std::size_t>(vertices) * vertex_size +
        static_cast<std::size_t>(indices) * 4;
    if (bytes.size() != want) {
        throw ModelError(name + " is " + std::to_string(bytes.size()) +
                         " bytes, where its counts need " + std::to_string(want));
    }

    std::array<float, 3> span{};
    for (std::size_t i = 0; i < 3; ++i) {
        span[i] = model.high[i] - model.low[i];
    }
    model.vertices.reserve(vertices);
    for (std::uint32_t v = 0; v < vertices; ++v) {
        const std::uint8_t* at = &bytes[header_size + v * vertex_size];
        ModelVertex vertex;
        for (std::size_t i = 0; i < 3; ++i) {
            const float t = static_cast<float>(read_u16(at + i * 2)) / 65535.0f;
            vertex.position[i] = model.low[i] + t * span[i];
        }
        for (std::size_t i = 0; i < 3; ++i) {
            const auto q = static_cast<std::int8_t>(at[6 + i]);
            vertex.normal[i] = static_cast<float>(q) / 127.0f;
        }
        for (std::size_t i = 0; i < 3; ++i) {
            vertex.colour[i] = static_cast<float>(at[9 + i]) / 255.0f;
        }
        vertex.colour[3] = 1.0f;
        model.vertices.push_back(vertex);
    }

    model.indices.reserve(indices);
    const std::size_t first = header_size +
                              static_cast<std::size_t>(vertices) * vertex_size;
    for (std::uint32_t i = 0; i < indices; ++i) {
        const std::uint32_t index = read_u32(&bytes[first + i * 4]);
        if (index >= vertices) {
            throw ModelError(name + " holds the index " +
                             std::to_string(index) + ", past its " +
                             std::to_string(vertices) + " vertices");
        }
        model.indices.push_back(index);
    }
    return model;
}

Model read_model(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw ModelError("cannot open " + path.string());
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    return read_model(bytes, path.string());
}

} // namespace glideslope::gfx
