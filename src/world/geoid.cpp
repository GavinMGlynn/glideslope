#include "world/geoid.hpp"

#include "world/zip.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace glideslope::world {

namespace {

// Reads PGM header tokens, keeping GeographicLib's "# Key value" comments.
class Header {
public:
    explicit Header(std::span<const std::uint8_t> pgm) : pgm_(pgm) {}

    std::string token() {
        for (;;) {
            while (at_ < pgm_.size() && std::isspace(pgm_[at_]) != 0) {
                ++at_;
            }
            if (at_ < pgm_.size() && pgm_[at_] == '#') {
                std::string line;
                while (at_ < pgm_.size() && pgm_[at_] != '\n') {
                    line.push_back(static_cast<char>(pgm_[at_++]));
                }
                comments_.push_back(line);
                continue;
            }
            break;
        }
        std::string t;
        while (at_ < pgm_.size() && std::isspace(pgm_[at_]) == 0) {
            t.push_back(static_cast<char>(pgm_[at_++]));
        }
        if (t.empty()) {
            throw GeoidError("the PGM header ends early");
        }
        return t;
    }

    // The value of a "# Key value" comment, or empty.
    std::string comment(const std::string& key) const {
        const std::string prefix = "# " + key + " ";
        for (const std::string& c : comments_) {
            if (c.rfind(prefix, 0) == 0) {
                return c.substr(prefix.size());
            }
        }
        return {};
    }

    // Past the single whitespace byte that ends a PGM header.
    std::size_t data_start() const {
        return at_ + 1;
    }

private:
    std::span<const std::uint8_t> pgm_;
    std::size_t at_ = 0;
    std::vector<std::string> comments_;
};

std::size_t positive(const std::string& token, const char* what) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(token.c_str(), &end, 10);
    if (*end != '\0' || value == 0 || value > 100000) {
        throw GeoidError(std::string("a PGM ") + what + " of " + token);
    }
    return value;
}

double number(const std::string& text, const char* what) {
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (text.empty() || *end != '\0') {
        throw GeoidError(std::string("no ") + what + " in the geoid grid's header");
    }
    return value;
}

} // namespace

Geoid::Geoid(std::span<const std::uint8_t> pgm) {
    Header header(pgm);
    if (header.token() != "P5") {
        throw GeoidError("not a binary PGM");
    }
    width_ = positive(header.token(), "width");
    height_ = positive(header.token(), "height");
    if (header.token() != "65535") {
        throw GeoidError("not a 16-bit PGM");
    }
    offset_ = number(header.comment("Offset"), "Offset");
    scale_ = number(header.comment("Scale"), "Scale");
    description_ = header.comment("Description");
    // GeographicLib's grids all start at the North Pole and the prime meridian,
    // run east and south, and include both poles.
    if (header.comment("Origin") != "90N 0E") {
        throw GeoidError("a grid whose origin is not 90N 0E");
    }
    if (width_ % 2 != 0 || height_ != width_ / 2 + 1) {
        throw GeoidError("a grid of " + std::to_string(width_) + " by " +
                         std::to_string(height_) +
                         " samples, which does not span the globe pole to pole");
    }
    const std::size_t start = header.data_start();
    if (pgm.size() < start || pgm.size() - start != width_ * height_ * 2) {
        throw GeoidError("the grid holds " +
                         std::to_string(pgm.size() - std::min(start, pgm.size())) +
                         " bytes of samples, not " +
                         std::to_string(width_ * height_ * 2));
    }
    grid_.resize(width_ * height_);
    for (std::size_t i = 0; i < grid_.size(); ++i) {
        // PGM's 16-bit samples are big-endian.
        grid_[i] = static_cast<std::uint16_t>((pgm[start + 2 * i] << 8) |
                                              pgm[start + 2 * i + 1]);
    }
}

double Geoid::sample(std::size_t column, std::size_t row) const {
    return offset_ + scale_ * grid_[row * width_ + column % width_];
}

double Geoid::undulation(double latitude_deg, double longitude_deg) const {
    const double step = 360.0 / static_cast<double>(width_);
    double longitude = std::fmod(longitude_deg, 360.0);
    if (longitude < 0.0) {
        longitude += 360.0;
    }
    const double latitude = std::clamp(latitude_deg, -90.0, 90.0);

    const double fx = longitude / step;
    const double fy = (90.0 - latitude) / step;
    // At the South Pole the last row is the lower of the pair.
    const double column = std::floor(fx);
    const double row = std::min(std::floor(fy), static_cast<double>(height_ - 2));
    const double dx = fx - column;
    const double dy = fy - row;
    const auto c = static_cast<std::size_t>(column);
    const auto r = static_cast<std::size_t>(row);
    return (1.0 - dy) * ((1.0 - dx) * sample(c, r) + dx * sample(c + 1, r)) +
           dy * ((1.0 - dx) * sample(c, r + 1) + dx * sample(c + 1, r + 1));
}

Geoid load_geoid_zip(const ByteSource& archive) {
    for (const ZipEntry& entry : zip_entries(archive)) {
        if (entry.name.rfind("geoids/", 0) == 0 && entry.name.size() > 4 &&
            entry.name.compare(entry.name.size() - 4, 4, ".pgm") == 0) {
            // The 1-minute grid, the largest GeographicLib makes, is 450 MB.
            return Geoid(zip_read(archive, entry, std::uint64_t{512} << 20));
        }
    }
    throw GeoidError("no geoids/*.pgm in the archive");
}

} // namespace glideslope::world
