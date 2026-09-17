#pragma once

// GeoTIFF, as the Copernicus DEM publishes it: single-channel 32-bit float
// rasters on a latitude/longitude grid, in tiles or strips, uncompressed or
// DEFLATE-compressed, with or without the floating-point predictor, and with
// their reduced-resolution overviews.
//
// Only what DEMs of that kind use is read; anything else - BigTIFF, integer
// samples, projected coordinates, a transformation matrix - is refused by
// name rather than misread.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace glideslope::world {

struct GeoTiffError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Where a GeoTIFF's bytes come from: a file, memory, or later a download.
class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual std::uint64_t size() const = 0;
    // Fills `out` from `offset`. Throws GeoTiffError if that runs past the end.
    virtual void read(std::uint64_t offset, std::span<std::uint8_t> out) const = 0;
};

class FileSource : public ByteSource {
public:
    explicit FileSource(const std::filesystem::path& path);
    std::uint64_t size() const override;
    void read(std::uint64_t offset, std::span<std::uint8_t> out) const override;

private:
    std::filesystem::path path_;
    mutable std::ifstream file_;
    mutable std::mutex mutex_;
    std::uint64_t size_ = 0;
};

class MemorySource : public ByteSource {
public:
    explicit MemorySource(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    std::uint64_t size() const override {
        return bytes_.size();
    }
    void read(std::uint64_t offset, std::span<std::uint8_t> out) const override;

private:
    std::vector<std::uint8_t> bytes_;
};

// One raster in the file: the full-resolution image or one of its overviews.
// It is stored in blocks - tiles, or strips as wide as the image - numbered
// across then down.
struct RasterImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t block_width = 0;
    std::uint32_t block_height = 0;
    std::uint16_t compression = 1; // 1 none, 8 or 32946 DEFLATE
    std::uint16_t predictor = 1;   // 1 none, 3 floating point
    bool big_endian = false;
    bool strips = false; // strips, whose last may be short, rather than tiles
    std::vector<std::uint64_t> offsets;
    std::vector<std::uint64_t> byte_counts;

    std::uint32_t blocks_across() const {
        return (width + block_width - 1) / block_width;
    }
    std::uint32_t blocks_down() const {
        return (height + block_height - 1) / block_height;
    }
};

struct GeoTiff {
    // The full-resolution image first, then its overviews, largest first.
    std::vector<RasterImage> images;

    // Where the full-resolution image's samples are, on WGS84: sample (column,
    // row) is at longitude origin_longitude_deg + column * longitude_step_deg
    // and latitude origin_latitude_deg - row * latitude_step_deg. For a raster
    // whose values are areas rather than points, that is each area's centre.
    double origin_longitude_deg = 0.0;
    double origin_latitude_deg = 0.0;
    double longitude_step_deg = 0.0;
    double latitude_step_deg = 0.0;

    // The value that means "no data", if the file names one.
    std::optional<float> nodata;
};

// Reads the header and every image's layout; no samples.
GeoTiff read_geotiff(const ByteSource& source);

// One block of an image, decompressed and decoded: block_width * block_height
// samples (fewer rows for a short last strip), row by row. Blocks at the right
// and bottom edges of a tiled image hold samples past the image's edge, which
// mean nothing.
std::vector<float> read_block(const ByteSource& source, const RasterImage& image,
                              std::uint32_t across, std::uint32_t down);

} // namespace glideslope::world
