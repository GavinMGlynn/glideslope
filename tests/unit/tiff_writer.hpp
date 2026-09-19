#pragma once

// Writes small GeoTIFFs for tests, in every layout the reader claims to read.

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace glideslope::test::tiff {

// A tag's value as the writer writes it: a TIFF type and its numbers, or text.
struct Value {
    std::uint16_t type = 3;
    std::vector<double> numbers;
    std::string text;
};

// A small GeoTIFF, built the way the reader claims to read.
struct Spec {
    bool big_endian = false;
    bool strips = false;
    bool deflate = false;
    bool float_predictor = false;
    std::uint16_t bits = 32;   // 32, float samples; 8, unsigned, as masks are
    bool differencing = false; // the horizontal differencing predictor, for 8 bits
    bool pixel_is_point = true;
    std::uint32_t width = 37;
    std::uint32_t height = 23;
    std::uint32_t block = 16;        // tile size, or rows per strip
    double origin_longitude = 151.0; // of sample (0, 0)
    double origin_latitude = -33.0;
    double longitude_step = 1.5 / 3600.0;
    double latitude_step = 1.0 / 3600.0;
    std::uint16_t magic = 42;
    std::map<std::uint16_t, Value> tags;         // replace or add, in the first image
    std::set<std::uint16_t> omit;                // drop, from the first image
    std::map<std::uint16_t, std::uint16_t> keys; // replace or add GeoKeys
    bool overview = true;                        // a half-size overview after it
};

// The samples of an image: varied, negative and large, never NaN.
std::vector<float> samples(std::uint32_t width, std::uint32_t height, double scale);

// The samples of an 8-bit mask: every value 0 to 255, in no order.
std::vector<float> mask_samples(std::uint32_t width, std::uint32_t height,
                                std::uint32_t seed);

// The file: `full` is width * height samples, row by row; `overview` is the
// half-size overview's, if the spec has one.
std::vector<std::uint8_t> write_tiff(const Spec& spec, const std::vector<float>& full,
                                     const std::vector<float>& overview);

} // namespace glideslope::test::tiff
