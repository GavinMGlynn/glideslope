#pragma once

// canvas.hpp - frames the HUD's mesh is painted into on the CPU, as the GPU
// draws it, for the tests that build frames on purpose:
// glideslope_hud_horizon_check and glideslope_checklist_horizon_check.

#include "gfx/renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace glideslope::test {

// What the frames are cleared to before the HUD: a sky, nothing like the
// HUD's colour.
inline constexpr std::array<std::uint8_t, 3> sky{102, 153, 230};

// **A frame the HUD is painted into, as the GPU draws it**: each triangle
// fills the pixels whose centres it covers, blended over what is there by its
// alpha. Only what the last mesh touched is put back to sky before the next,
// so a large frame costs no more than the HUD on it.
class Canvas {
public:
    Canvas(int width, int height) {
        frame_.width = width;
        frame_.height = height;
        frame_.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                           4);
        for (std::size_t i = 0; i < frame_.rgba.size(); i += 4) {
            clear(i);
        }
    }

    const gfx::Frame& paint(const gfx::Mesh& mesh) {
        for (const std::size_t i : touched_) {
            clear(i);
        }
        touched_.clear();
        const int width = frame_.width;
        const int height = frame_.height;
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            std::array<std::array<double, 2>, 3> p{};
            for (std::size_t k = 0; k < 3; ++k) {
                const auto& v = mesh.vertices[mesh.indices[t + k]].position;
                p[k] = {(static_cast<double>(v[0]) + 1.0) / 2.0 * width,
                        (1.0 - static_cast<double>(v[1])) / 2.0 * height};
            }
            const auto& c = mesh.vertices[mesh.indices[t]].colour;
            const auto edge = [](const std::array<double, 2>& a, const std::array<double, 2>& b,
                                 double x, double y) {
                return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
            };
            const double area = edge(p[0], p[1], p[2][0], p[2][1]);
            if (area == 0.0) {
                continue;
            }
            const int x_from = std::max(
                0, static_cast<int>(std::floor(std::min({p[0][0], p[1][0], p[2][0]}))));
            const int x_to = std::min(
                width - 1, static_cast<int>(std::ceil(std::max({p[0][0], p[1][0], p[2][0]}))));
            const int y_from = std::max(
                0, static_cast<int>(std::floor(std::min({p[0][1], p[1][1], p[2][1]}))));
            const int y_to = std::min(
                height - 1, static_cast<int>(std::ceil(std::max({p[0][1], p[1][1], p[2][1]}))));
            for (int y = y_from; y <= y_to; ++y) {
                for (int x = x_from; x <= x_to; ++x) {
                    const double cx = x + 0.5;
                    const double cy = y + 0.5;
                    if (edge(p[1], p[2], cx, cy) * area < 0.0 ||
                        edge(p[2], p[0], cx, cy) * area < 0.0 ||
                        edge(p[0], p[1], cx, cy) * area < 0.0) {
                        continue;
                    }
                    const std::size_t at =
                        (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(x)) *
                        4;
                    for (std::size_t i = 0; i < 3; ++i) {
                        const float under = static_cast<float>(frame_.rgba[at + i]) / 255.0f;
                        const float over = c[i] * c[3] + under * (1.0f - c[3]);
                        frame_.rgba[at + i] = static_cast<std::uint8_t>(
                            std::lround(std::clamp(over, 0.0f, 1.0f) * 255.0f));
                    }
                    touched_.push_back(at);
                }
            }
        }
        return frame_;
    }

private:
    void clear(std::size_t at) {
        frame_.rgba[at] = sky[0];
        frame_.rgba[at + 1] = sky[1];
        frame_.rgba[at + 2] = sky[2];
        frame_.rgba[at + 3] = 255;
    }

    gfx::Frame frame_;
    std::vector<std::size_t> touched_;
};

} // namespace glideslope::test
