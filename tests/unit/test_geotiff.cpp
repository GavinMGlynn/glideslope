#include "harness.hpp"
#include "tiff_writer.hpp"

#include "world/geotiff.hpp"
#include "world/inflate.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::FileSource;
using glideslope::world::GeoTiff;
using glideslope::world::GeoTiffError;
using glideslope::world::MemorySource;
using glideslope::world::read_block;
using glideslope::world::read_geotiff;

namespace {

using Bytes = std::vector<std::uint8_t>;
using glideslope::test::tiff::mask_samples;
using glideslope::test::tiff::samples;
using glideslope::test::tiff::Spec;
using glideslope::test::tiff::Value;
using glideslope::test::tiff::write_tiff;

// Every sample of every image the reader finds, against what was written.
void check_reads_back(const Spec& spec, const std::string& what) {
    const bool mask = spec.bits == 8;
    const std::vector<float> full = mask ? mask_samples(spec.width, spec.height, 1)
                                         : samples(spec.width, spec.height, 1.0);
    const std::uint32_t ow = (spec.width + 1) / 2;
    const std::uint32_t oh = (spec.height + 1) / 2;
    const std::vector<float> overview =
        mask ? mask_samples(ow, oh, 2) : samples(ow, oh, -0.5);
    const MemorySource source(write_tiff(spec, full, overview));
    const GeoTiff tiff = read_geotiff(source);

    check(tiff.images.size() == 2, what + ": two images");
    check(std::abs(tiff.origin_longitude_deg - spec.origin_longitude) < 1e-12 &&
              std::abs(tiff.origin_latitude_deg - spec.origin_latitude) < 1e-12,
          what + ": the first sample is where it was put");
    check(tiff.longitude_step_deg == spec.longitude_step &&
              tiff.latitude_step_deg == spec.latitude_step,
          what + ": the steps are as written");
    check(tiff.nodata == -32767.0f, what + ": the no-data value is read");

    for (std::size_t i = 0; i < 2; ++i) {
        const auto& image = tiff.images[i];
        const auto& expected = i == 0 ? full : overview;
        check(image.width == (i == 0 ? spec.width : ow) &&
                  image.height == (i == 0 ? spec.height : oh),
              what + ": image " + std::to_string(i) + " is the size written");
        std::size_t compared = 0;
        for (std::uint32_t by = 0; by < image.blocks_down(); ++by) {
            for (std::uint32_t bx = 0; bx < image.blocks_across(); ++bx) {
                const std::vector<float> block = read_block(source, image, bx, by);
                const std::size_t rows = block.size() / image.block_width;
                for (std::size_t y = 0; y < rows; ++y) {
                    for (std::size_t x = 0; x < image.block_width; ++x) {
                        const std::size_t ix = bx * image.block_width + x;
                        const std::size_t iy = by * image.block_height + y;
                        if (ix >= image.width || iy >= image.height) {
                            continue;
                        }
                        if (block[y * image.block_width + x] !=
                            expected[iy * image.width + ix]) {
                            fail(what + ": image " + std::to_string(i) + " sample (" +
                                 std::to_string(ix) + ", " + std::to_string(iy) +
                                 ") differs");
                        }
                        ++compared;
                    }
                }
            }
        }
        check(compared == std::size_t{image.width} * image.height,
              what + ": every sample of image " + std::to_string(i) + " was compared");
    }
}

void refuses(const Spec& spec, const std::string& says,
             std::source_location where = std::source_location::current()) {
    const std::uint32_t ow = (spec.width + 1) / 2;
    const std::uint32_t oh = (spec.height + 1) / 2;
    const bool mask = spec.bits == 8;
    const std::vector<float> full = mask ? mask_samples(spec.width, spec.height, 1)
                                         : samples(spec.width, spec.height, 1.0);
    const std::vector<float> overview =
        mask ? mask_samples(ow, oh, 2) : samples(ow, oh, 1.0);
    const MemorySource source(write_tiff(spec, full, overview));
    try {
        const GeoTiff tiff = read_geotiff(source);
        for (const auto& image : tiff.images) {
            for (std::uint32_t by = 0; by < image.blocks_down(); ++by) {
                for (std::uint32_t bx = 0; bx < image.blocks_across(); ++bx) {
                    read_block(source, image, bx, by);
                }
            }
        }
    } catch (const GeoTiffError& e) {
        if (std::string(e.what()).find(says) == std::string::npos) {
            fail("refused with \"" + std::string(e.what()) + "\", not \"" + says + "\"",
                 where);
        }
        return;
    }
    fail("read a file it should refuse for \"" + says + "\"", where);
}

} // namespace

GLIDESLOPE_TEST(a_geotiff_in_every_layout_the_reader_takes_reads_back_every_sample) {
    int layouts = 0;
    for (const bool big : {false, true}) {
        for (const bool strips : {false, true}) {
            for (const bool deflate : {false, true}) {
                for (const bool predictor : {false, true}) {
                    for (const bool point : {false, true}) {
                        Spec spec;
                        spec.big_endian = big;
                        spec.strips = strips;
                        spec.deflate = deflate;
                        spec.float_predictor = predictor;
                        spec.pixel_is_point = point;
                        spec.block = strips ? 7 : 16;
                        check_reads_back(
                            spec, std::string(big ? "big-endian" : "little-endian") +
                                      (strips ? " strips" : " tiles") +
                                      (deflate ? " deflate" : " uncompressed") +
                                      (predictor ? " predictor 3" : " no predictor") +
                                      (point ? " point" : " area"));
                        ++layouts;
                    }
                }
            }
        }
    }
    // Masks: 8-bit unsigned samples, with or without horizontal differencing.
    for (const bool big : {false, true}) {
        for (const bool strips : {false, true}) {
            for (const bool deflate : {false, true}) {
                for (const bool differencing : {false, true}) {
                    Spec spec;
                    spec.bits = 8;
                    spec.big_endian = big;
                    spec.strips = strips;
                    spec.deflate = deflate;
                    spec.differencing = differencing;
                    spec.block = strips ? 7 : 16;
                    check_reads_back(
                        spec, std::string("8-bit ") +
                                  (big ? "big-endian" : "little-endian") +
                                  (strips ? " strips" : " tiles") +
                                  (deflate ? " deflate" : " uncompressed") +
                                  (differencing ? " predictor 2" : " no predictor"));
                    ++layouts;
                }
            }
        }
    }
    check(layouts == 48, "all 48 layouts were read");
}

GLIDESLOPE_TEST(a_geotiff_the_reader_does_not_take_is_refused_saying_why) {
    Spec s;
    s.magic = 43;
    refuses(s, "BigTIFF");
    s = {};
    s.magic = 41;
    refuses(s, "magic number 41");
    s = {};
    s.tags[258] = {3, {16}, {}};
    refuses(s, "BitsPerSample is 16");
    s = {};
    s.tags[339] = {3, {2}, {}};
    refuses(s, "SampleFormat is 2");
    s = {};
    s.tags[277] = {3, {3}, {}};
    refuses(s, "SamplesPerPixel is 3");
    s = {};
    s.tags[259] = {3, {5}, {}};
    refuses(s, "Compression is 5");
    s = {};
    s.tags[317] = {3, {2}, {}};
    refuses(s, "Predictor is 2");
    s = {};
    s.bits = 8;
    s.tags[339] = {3, {3}, {}};
    refuses(s, "SampleFormat is 3");
    s = {};
    s.bits = 8;
    s.tags[317] = {3, {3}, {}};
    refuses(s, "Predictor is 3");
    s = {};
    s.keys[1024] = 1;
    refuses(s, "not in geographic coordinates");
    s = {};
    s.keys[2048] = 4269;
    refuses(s, "not on WGS84");
    s = {};
    s.tags[34264] = {12, std::vector<double>(16, 0.0), {}};
    refuses(s, "ModelTransformationTag");
    s = {};
    s.omit = {34735};
    refuses(s, "not a GeoTIFF");
    s = {};
    s.tags[324] = {4, {1e8, 8, 8, 8, 8, 8}, {}};
    refuses(s, "past the end of the file");
    s = {};
    s.tags[324] = {4, {8}, {}};
    refuses(s, "6 blocks, but 1 offsets");
    s = {};
    s.deflate = true;
    s.tags[325] = {4, {20, 20, 20, 20, 20, 20}, {}};
    refuses(s, "block 0:");
}

GLIDESLOPE_TEST(
    the_copernicus_dem_tile_south_of_sydney_decodes_to_the_heights_an_independent_decoder_gives) {
    const std::filesystem::path path =
        std::filesystem::path(GLIDESLOPE_TEST_DOWNLOADS_DIR) /
        "Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif";
    if (!std::filesystem::exists(path)) {
        glideslope::test::skip(path.string() + " was not fetched");
    }
    const FileSource source(path);
    const GeoTiff tiff = read_geotiff(source);

    check(tiff.images.size() == 4, "the full image and three overviews");
    check(tiff.images[0].width == 3600 && tiff.images[0].height == 3600,
          "3600 by 3600 samples");
    check(tiff.images[1].width == 1800 && tiff.images[2].width == 900 &&
              tiff.images[3].width == 450,
          "overviews of 1800, 900 and 450");
    check(tiff.origin_longitude_deg == 151.0 && tiff.origin_latitude_deg == -33.0,
          "the first sample is exactly at 151 E, 33 S");
    check(tiff.longitude_step_deg == 1.0 / 3600.0 &&
              tiff.latitude_step_deg == 1.0 / 3600.0,
          "a sample every arc-second");

    // Decoded by a separate decoder - Python, its zlib, and the TIFF predictor
    // written again from Technical Note 3 - when this test was written.
    struct Sample {
        std::uint32_t column;
        std::uint32_t row;
        float height;
    };
    const std::vector<Sample> expected{{0, 0, 274.5896911621094f},
                                       {1023, 0, 199.3792266845703f},
                                       {1024, 0, 195.0670623779297f},
                                       {0, 3599, 83.65388488769531f},
                                       {720, 3132, 0.0f},
                                       {100, 1500, 1.8994510173797607f},
                                       {1500, 500, 27.805686950683594f},
                                       {900, 2400, 119.47073364257812f},
                                       {2000, 300, 0.0f},
                                       {2048, 1023, 31.21465301513672f},
                                       {500, 3000, 34.58142852783203f},
                                       {1023, 1023, 89.92703247070312f},
                                       {1024, 1024, 76.25465393066406f},
                                       {3599, 3599, 0.0f},
                                       {3100, 3100, 0.0f}};
    const auto& image = tiff.images[0];
    for (const Sample& s : expected) {
        const std::uint32_t bx = s.column / image.block_width;
        const std::uint32_t by = s.row / image.block_height;
        const std::vector<float> block = read_block(source, image, bx, by);
        const float got = block[(s.row % image.block_height) * image.block_width +
                                s.column % image.block_width];
        check(got == s.height,
              "sample (" + std::to_string(s.column) + ", " + std::to_string(s.row) +
                  ") is " + std::to_string(s.height) + ", not " + std::to_string(got));
    }
}

GLIDESLOPE_TEST(
    the_copernicus_water_body_mask_south_of_sydney_decodes_to_the_values_an_independent_decoder_gives) {
    const std::filesystem::path path =
        std::filesystem::path(GLIDESLOPE_TEST_DOWNLOADS_DIR) /
        "Copernicus_DSM_COG_10_S34_00_E151_00_WBM.tif";
    if (!std::filesystem::exists(path)) {
        glideslope::test::skip(path.string() + " was not fetched");
    }
    const FileSource source(path);
    const GeoTiff tiff = read_geotiff(source);

    check(tiff.images.size() == 4, "the full image and three overviews");
    check(tiff.images[0].width == 3600 && tiff.images[0].height == 3600 &&
              tiff.images[0].bits == 8 && tiff.images[0].predictor == 2,
          "3600 by 3600 bytes, horizontally differenced");
    check(tiff.origin_longitude_deg == 151.0 && tiff.origin_latitude_deg == -33.0,
          "the first sample is exactly at 151 E, 33 S, as the heights' is");
    check(tiff.longitude_step_deg == 1.0 / 3600.0 &&
              tiff.latitude_step_deg == 1.0 / 3600.0,
          "a sample every arc-second, as the heights have");
    check(!tiff.nodata, "no value stands for no data");

    // Decoded by a separate decoder - Python, its zlib, and the horizontal
    // differencing undone by hand - when this test was written: blocks'
    // corners, and the samples nearest the tests' reference places.
    struct Sample {
        std::uint32_t column;
        std::uint32_t row;
        float value;
    };
    const std::vector<Sample> expected{
        {0, 0, 0.0f},       {1023, 0, 0.0f},    {1024, 0, 0.0f},    {1023, 1023, 0.0f},
        {1024, 1024, 0.0f}, {0, 3599, 0.0f},    {3599, 3599, 1.0f}, {3599, 2000, 1.0f},
        {3071, 3072, 1.0f}, {1260, 3240, 1.0f}, {2160, 288, 2.0f},  {1764, 1152, 2.0f},
        {918, 3064, 3.0f},  {684, 3564, 3.0f},  {774, 1962, 3.0f},  {638, 3406, 0.0f},
        {760, 3143, 0.0f},  {11, 2934, 0.0f}};
    const auto& image = tiff.images[0];
    for (const Sample& s : expected) {
        const std::vector<float> block = read_block(
            source, image, s.column / image.block_width, s.row / image.block_height);
        const float got = block[(s.row % image.block_height) * image.block_width +
                                s.column % image.block_width];
        check(got == s.value,
              "sample (" + std::to_string(s.column) + ", " + std::to_string(s.row) +
                  ") is " + std::to_string(s.value) + ", not " + std::to_string(got));
    }
}
