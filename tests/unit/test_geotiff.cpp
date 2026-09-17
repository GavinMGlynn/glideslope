#include "harness.hpp"

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
};

// The samples of an image: varied, negative and large, never NaN.
std::vector<float> samples(std::uint32_t width, std::uint32_t height, double scale) {
    std::vector<float> out;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const double v =
                std::sin(x * 0.37 + y * 0.11) * 4000.0 * scale + x * 0.25 - y;
            out.push_back(static_cast<float>(v));
        }
    }
    return out;
}

class Writer {
public:
    explicit Writer(bool big) : big_(big) {}

    void u8(std::uint8_t v) {
        bytes.push_back(v);
    }
    void u16(std::uint32_t v) {
        put(v, 2);
    }
    void u32(std::uint32_t v) {
        put(v, 4);
    }
    void u32_at(std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            bytes[at + static_cast<std::size_t>(i)] =
                static_cast<std::uint8_t>(v >> (big_ ? 24 - 8 * i : 8 * i));
        }
    }
    void value(std::uint16_t type, double n) {
        switch (type) {
        case 3: u16(static_cast<std::uint32_t>(n)); break;
        case 4: u32(static_cast<std::uint32_t>(n)); break;
        case 12: {
            const auto bits = std::bit_cast<std::uint64_t>(n);
            if (big_) {
                u32(static_cast<std::uint32_t>(bits >> 32));
                u32(static_cast<std::uint32_t>(bits));
            } else {
                u32(static_cast<std::uint32_t>(bits));
                u32(static_cast<std::uint32_t>(bits >> 32));
            }
            break;
        }
        default: fail("the test writer has no type " + std::to_string(type));
        }
    }
    std::uint32_t here() const {
        return static_cast<std::uint32_t>(bytes.size());
    }

    Bytes bytes;

private:
    void put(std::uint32_t v, int n) {
        for (int i = 0; i < n; ++i) {
            const int shift = big_ ? 8 * (n - 1 - i) : 8 * i;
            bytes.push_back(static_cast<std::uint8_t>(v >> shift));
        }
    }
    bool big_;
};

std::size_t type_size(std::uint16_t type) {
    return type == 2 ? 1 : type == 3 ? 2 : type == 4 ? 4 : 8;
}

// One block's bytes, predicted and compressed as the spec says.
Bytes encode_block(const Spec& spec, const std::vector<float>& values,
                   std::size_t width, std::size_t rows) {
    Bytes raw;
    if (spec.float_predictor) {
        for (std::size_t r = 0; r < rows; ++r) {
            Bytes row(width * 4);
            for (std::size_t x = 0; x < width; ++x) {
                const auto bits = std::bit_cast<std::uint32_t>(values[r * width + x]);
                for (std::size_t plane = 0; plane < 4; ++plane) {
                    row[plane * width + x] =
                        static_cast<std::uint8_t>(bits >> (24 - 8 * plane));
                }
            }
            for (std::size_t i = row.size() - 1; i > 0; --i) {
                row[i] = static_cast<std::uint8_t>(row[i] - row[i - 1]);
            }
            raw.insert(raw.end(), row.begin(), row.end());
        }
    } else {
        for (const float v : values) {
            const auto bits = std::bit_cast<std::uint32_t>(v);
            for (int i = 0; i < 4; ++i) {
                raw.push_back(static_cast<std::uint8_t>(
                    bits >> (spec.big_endian ? 24 - 8 * i : 8 * i)));
            }
        }
    }
    if (!spec.deflate) {
        return raw;
    }
    // A zlib stream of stored blocks: DEFLATE, without compressing.
    Bytes z{0x78, 0x01};
    std::size_t at = 0;
    do {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - at);
        z.push_back(at + n == raw.size() ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n));
        z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n));
        z.push_back(static_cast<std::uint8_t>(~n >> 8));
        z.insert(z.end(), raw.begin() + static_cast<long>(at),
                 raw.begin() + static_cast<long>(at + n));
        at += n;
    } while (at < raw.size());
    const std::uint32_t a = glideslope::world::adler32(raw);
    for (const int shift : {24, 16, 8, 0}) {
        z.push_back(static_cast<std::uint8_t>(a >> shift));
    }
    return z;
}

// Writes an image's blocks and returns its tags.
std::map<std::uint16_t, Value> write_image(Writer& w, const Spec& spec,
                                           std::uint32_t width, std::uint32_t height,
                                           const std::vector<float>& values) {
    const std::uint32_t bw = spec.strips ? width : spec.block;
    const std::uint32_t bh = spec.block;
    const std::uint32_t across = (width + bw - 1) / bw;
    const std::uint32_t down = (height + bh - 1) / bh;
    Value offsets{4, {}, {}};
    Value counts{4, {}, {}};
    for (std::uint32_t by = 0; by < down; ++by) {
        for (std::uint32_t bx = 0; bx < across; ++bx) {
            const std::uint32_t rows =
                spec.strips ? std::min(bh, height - by * bh) : bh;
            std::vector<float> block(std::size_t{bw} * rows, 0.0f);
            for (std::uint32_t y = 0; y < rows; ++y) {
                for (std::uint32_t x = 0; x < bw; ++x) {
                    const std::uint32_t ix = bx * bw + x;
                    const std::uint32_t iy = by * bh + y;
                    if (ix < width && iy < height) {
                        block[std::size_t{y} * bw + x] =
                            values[std::size_t{iy} * width + ix];
                    }
                }
            }
            const Bytes encoded = encode_block(spec, block, bw, rows);
            offsets.numbers.push_back(w.here());
            counts.numbers.push_back(static_cast<double>(encoded.size()));
            w.bytes.insert(w.bytes.end(), encoded.begin(), encoded.end());
        }
    }
    std::map<std::uint16_t, Value> tags;
    tags[256] = {4, {double(width)}, {}};
    tags[257] = {4, {double(height)}, {}};
    tags[258] = {3, {32}, {}};
    tags[259] = {3, {spec.deflate ? 8.0 : 1.0}, {}};
    tags[262] = {3, {1}, {}};
    tags[277] = {3, {1}, {}};
    tags[284] = {3, {1}, {}};
    tags[317] = {3, {spec.float_predictor ? 3.0 : 1.0}, {}};
    tags[339] = {3, {3}, {}};
    if (spec.strips) {
        tags[273] = offsets;
        tags[278] = {4, {double(bh)}, {}};
        tags[279] = counts;
    } else {
        tags[322] = {3, {double(bw)}, {}};
        tags[323] = {3, {double(bh)}, {}};
        tags[324] = offsets;
        tags[325] = counts;
    }
    return tags;
}

// Writes an IFD at the end of the file; returns where the next-IFD offset goes.
std::size_t write_ifd(Writer& w, const std::map<std::uint16_t, Value>& tags) {
    std::uint32_t start = w.here();
    if (start % 2 == 1) {
        w.u8(0);
        ++start;
    }
    w.u16(static_cast<std::uint32_t>(tags.size()));
    std::vector<std::pair<std::size_t, const Value*>> external;
    for (const auto& [tag, v] : tags) {
        const std::size_t n = v.type == 2 ? v.text.size() + 1 : v.numbers.size();
        w.u16(tag);
        w.u16(v.type);
        w.u32(static_cast<std::uint32_t>(n));
        if (n * type_size(v.type) <= 4) {
            const std::size_t before = w.bytes.size();
            if (v.type == 2) {
                for (const char c : v.text) {
                    w.u8(static_cast<std::uint8_t>(c));
                }
                w.u8(0);
            } else {
                for (const double x : v.numbers) {
                    w.value(v.type, x);
                }
            }
            while (w.bytes.size() < before + 4) {
                w.u8(0);
            }
        } else {
            external.emplace_back(w.bytes.size(), &v);
            w.u32(0);
        }
    }
    const std::size_t next = w.bytes.size();
    w.u32(0);
    for (const auto& [at, v] : external) {
        w.u32_at(at, w.here());
        if (v->type == 2) {
            for (const char c : v->text) {
                w.u8(static_cast<std::uint8_t>(c));
            }
            w.u8(0);
        } else {
            for (const double x : v->numbers) {
                w.value(v->type, x);
            }
        }
    }
    return next;
}

Bytes write_tiff(const Spec& spec, const std::vector<float>& full,
                 const std::vector<float>& overview) {
    Writer w(spec.big_endian);
    w.u8(spec.big_endian ? 'M' : 'I');
    w.u8(spec.big_endian ? 'M' : 'I');
    w.u16(spec.magic);
    w.u32(0); // the first IFD, patched below

    auto tags = write_image(w, spec, spec.width, spec.height, full);
    const std::uint32_t ow = (spec.width + 1) / 2;
    const std::uint32_t oh = (spec.height + 1) / 2;
    auto overview_tags = write_image(w, spec, ow, oh, overview);
    overview_tags[254] = {4, {1}, {}};

    const double half = spec.pixel_is_point ? 0.0 : 0.5;
    tags[33550] = {12, {spec.longitude_step, spec.latitude_step, 0.0}, {}};
    tags[33922] = {12,
                   {0.0, 0.0, 0.0, spec.origin_longitude - half * spec.longitude_step,
                    spec.origin_latitude + half * spec.latitude_step, 0.0},
                   {}};
    std::map<std::uint16_t, std::uint16_t> keys{
        {1024, 2}, {1025, spec.pixel_is_point ? 2 : 1}, {2048, 4326}};
    for (const auto& [k, v] : spec.keys) {
        keys[k] = v;
    }
    Value directory{3, {1, 1, 0, double(keys.size())}, {}};
    for (const auto& [k, v] : keys) {
        directory.numbers.insert(directory.numbers.end(), {double(k), 0, 1, double(v)});
    }
    tags[34735] = directory;
    tags[42113] = {2, {}, "-32767"};
    for (const auto& [tag, v] : spec.tags) {
        tags[tag] = v;
    }
    for (const std::uint16_t tag : spec.omit) {
        tags.erase(tag);
    }

    std::size_t patch = 4;
    for (const auto* image : {&tags, &overview_tags}) {
        const std::uint32_t at = w.here() + (w.here() % 2);
        w.u32_at(patch, at);
        patch = write_ifd(w, *image);
    }
    return w.bytes;
}

// Every sample of every image the reader finds, against what was written.
void check_reads_back(const Spec& spec, const std::string& what) {
    const std::vector<float> full = samples(spec.width, spec.height, 1.0);
    const std::uint32_t ow = (spec.width + 1) / 2;
    const std::uint32_t oh = (spec.height + 1) / 2;
    const std::vector<float> overview = samples(ow, oh, -0.5);
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
    const std::vector<float> full = samples(spec.width, spec.height, 1.0);
    const std::vector<float> overview =
        samples((spec.width + 1) / 2, (spec.height + 1) / 2, 1.0);
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
    check(layouts == 32, "all 32 layouts were read");
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
