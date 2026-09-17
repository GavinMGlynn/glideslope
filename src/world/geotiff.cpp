#include "world/geotiff.hpp"

#include "world/inflate.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <map>
#include <string>

namespace glideslope::world {

namespace {

// TIFF tags read here.
enum Tag : std::uint16_t {
    new_subfile_type = 254,
    image_width = 256,
    image_length = 257,
    bits_per_sample = 258,
    compression_tag = 259,
    strip_offsets = 273,
    samples_per_pixel = 277,
    rows_per_strip = 278,
    strip_byte_counts = 279,
    planar_configuration = 284,
    predictor_tag = 317,
    tile_width = 322,
    tile_length = 323,
    tile_offsets = 324,
    tile_byte_counts = 325,
    sample_format = 339,
    model_pixel_scale = 33550,
    model_tiepoint = 33922,
    model_transformation = 34264,
    geo_key_directory = 34735,
    gdal_nodata = 42113,
};

// GeoTIFF keys read here, and the values accepted.
constexpr std::uint16_t key_model_type = 1024;
constexpr std::uint16_t key_raster_type = 1025;
constexpr std::uint16_t key_geographic_type = 2048;
constexpr std::uint16_t model_type_geographic = 2;
constexpr std::uint16_t raster_pixel_is_area = 1;
constexpr std::uint16_t raster_pixel_is_point = 2;
constexpr std::uint16_t epsg_wgs84 = 4326;

// A tag's values, whatever their type, as numbers or text.
struct Field {
    std::uint16_t type = 0;
    std::vector<double> numbers;
    std::string text;
};

class Reader {
public:
    explicit Reader(const ByteSource& source) : source_(source) {
        std::uint8_t header[8];
        if (source.size() < 8) {
            throw GeoTiffError("too short to be a TIFF");
        }
        source.read(0, header);
        if (header[0] == 'I' && header[1] == 'I') {
            big_ = false;
        } else if (header[0] == 'M' && header[1] == 'M') {
            big_ = true;
        } else {
            throw GeoTiffError("not a TIFF: no byte-order mark");
        }
        const std::uint16_t magic = u16(header + 2);
        if (magic == 43) {
            throw GeoTiffError("a BigTIFF, which is not read");
        }
        if (magic != 42) {
            throw GeoTiffError("not a TIFF: magic number " + std::to_string(magic));
        }
        first_ifd_ = u32(header + 4);
    }

    bool big_endian() const {
        return big_;
    }
    std::uint32_t first_ifd() const {
        return first_ifd_;
    }

    // The fields of the IFD at `offset`, and the offset of the next (0 at the
    // end).
    std::map<std::uint16_t, Field> ifd(std::uint64_t offset,
                                       std::uint64_t& next) const {
        std::uint8_t count_bytes[2];
        source_.read(offset, count_bytes);
        const std::uint16_t count = u16(count_bytes);
        std::vector<std::uint8_t> entries(std::size_t{count} * 12 + 4);
        source_.read(offset + 2, entries);
        std::map<std::uint16_t, Field> fields;
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint8_t* e = entries.data() + i * 12;
            const std::uint16_t tag = u16(e);
            Field field;
            field.type = u16(e + 2);
            const std::uint32_t n = u32(e + 4);
            const std::size_t width = type_size(field.type);
            if (width == 0) {
                continue; // a type this reader has no use for
            }
            const std::uint64_t bytes = std::uint64_t{n} * width;
            if (bytes > (std::uint64_t{64} << 20)) {
                throw GeoTiffError("tag " + std::to_string(tag) + " claims " +
                                   std::to_string(bytes) + " bytes");
            }
            std::vector<std::uint8_t> value(static_cast<std::size_t>(bytes));
            if (bytes <= 4) {
                std::memcpy(value.data(), e + 8, static_cast<std::size_t>(bytes));
            } else {
                source_.read(u32(e + 8), value);
            }
            if (field.type == 2) {
                field.text.assign(value.begin(), value.end());
                while (!field.text.empty() && field.text.back() == '\0') {
                    field.text.pop_back();
                }
            } else {
                field.numbers.reserve(n);
                for (std::size_t k = 0; k < n; ++k) {
                    field.numbers.push_back(
                        number(field.type, value.data() + k * width));
                }
            }
            fields[tag] = std::move(field);
        }
        next = u32(entries.data() + std::size_t{count} * 12);
        return fields;
    }

    std::uint16_t u16(const std::uint8_t* p) const {
        return big_ ? static_cast<std::uint16_t>((p[0] << 8) | p[1])
                    : static_cast<std::uint16_t>((p[1] << 8) | p[0]);
    }
    std::uint32_t u32(const std::uint8_t* p) const {
        const std::uint32_t a = p[0], b = p[1], c = p[2], d = p[3];
        return big_ ? (a << 24) | (b << 16) | (c << 8) | d
                    : (d << 24) | (c << 16) | (b << 8) | a;
    }
    std::uint64_t u64(const std::uint8_t* p) const {
        const std::uint64_t hi = u32(big_ ? p : p + 4);
        const std::uint64_t lo = u32(big_ ? p + 4 : p);
        return (hi << 32) | lo;
    }

private:
    static std::size_t type_size(std::uint16_t type) {
        switch (type) {
        case 1:
        case 2:
        case 6:
        case 7: return 1; // BYTE ASCII SBYTE UNDEFINED
        case 3:
        case 8: return 2; // SHORT SSHORT
        case 4:
        case 9:
        case 11: return 4; // LONG SLONG FLOAT
        case 5:
        case 10:
        case 12: return 8; // RATIONAL SRATIONAL DOUBLE
        default: return 0;
        }
    }

    double number(std::uint16_t type, const std::uint8_t* p) const {
        switch (type) {
        case 1:
        case 7: return p[0];
        case 6: return static_cast<std::int8_t>(p[0]);
        case 3: return u16(p);
        case 8: return static_cast<std::int16_t>(u16(p));
        case 4: return u32(p);
        case 9: return static_cast<std::int32_t>(u32(p));
        case 5: return static_cast<double>(u32(p)) / static_cast<double>(u32(p + 4));
        case 10:
            return static_cast<double>(static_cast<std::int32_t>(u32(p))) /
                   static_cast<double>(static_cast<std::int32_t>(u32(p + 4)));
        case 11: return static_cast<double>(std::bit_cast<float>(u32(p)));
        case 12: return std::bit_cast<double>(u64(p));
        default: return 0.0;
        }
    }

    const ByteSource& source_;
    bool big_ = false;
    std::uint32_t first_ifd_ = 0;
};

const Field* find(const std::map<std::uint16_t, Field>& fields, std::uint16_t tag) {
    const auto it = fields.find(tag);
    return it == fields.end() ? nullptr : &it->second;
}

double scalar(const std::map<std::uint16_t, Field>& fields, std::uint16_t tag,
              std::optional<double> fallback, const char* name) {
    const Field* f = find(fields, tag);
    if (f == nullptr || f->numbers.empty()) {
        if (fallback) {
            return *fallback;
        }
        throw GeoTiffError(std::string("no ") + name + " tag");
    }
    return f->numbers.front();
}

// Every value of a tag with one value per sample the same, as TIFF allows a
// file to write BitsPerSample and SampleFormat once per sample.
void require(const std::map<std::uint16_t, Field>& fields, std::uint16_t tag,
             double wanted, double fallback, const char* what) {
    const Field* f = find(fields, tag);
    const std::vector<double> values =
        f == nullptr ? std::vector<double>{fallback} : f->numbers;
    for (const double v : values) {
        if (v != wanted) {
            throw GeoTiffError(std::string(what) + " is " +
                               std::to_string(static_cast<long long>(v)) + "; only " +
                               std::to_string(static_cast<long long>(wanted)) +
                               " is read");
        }
    }
}

RasterImage image_from(const Reader& reader,
                       const std::map<std::uint16_t, Field>& fields,
                       std::uint64_t file_size) {
    RasterImage image;
    image.big_endian = reader.big_endian();
    image.width =
        static_cast<std::uint32_t>(scalar(fields, image_width, {}, "ImageWidth"));
    image.height =
        static_cast<std::uint32_t>(scalar(fields, image_length, {}, "ImageLength"));
    if (image.width == 0 || image.height == 0 || image.width > 1u << 20 ||
        image.height > 1u << 20) {
        throw GeoTiffError("an image of " + std::to_string(image.width) + " by " +
                           std::to_string(image.height) + " samples");
    }
    require(fields, samples_per_pixel, 1, 1, "SamplesPerPixel");
    require(fields, bits_per_sample, 32, 1, "BitsPerSample");
    require(fields, sample_format, 3, 1, "SampleFormat");
    require(fields, planar_configuration, 1, 1, "PlanarConfiguration");

    image.compression =
        static_cast<std::uint16_t>(scalar(fields, compression_tag, 1.0, ""));
    if (image.compression != 1 && image.compression != 8 &&
        image.compression != 32946) {
        throw GeoTiffError("Compression is " + std::to_string(image.compression) +
                           "; only none (1) and DEFLATE (8, 32946) are read");
    }
    image.predictor =
        static_cast<std::uint16_t>(scalar(fields, predictor_tag, 1.0, ""));
    if (image.predictor != 1 && image.predictor != 3) {
        throw GeoTiffError("Predictor is " + std::to_string(image.predictor) +
                           "; only none (1) and floating point (3) are read");
    }

    const Field* offsets = nullptr;
    const Field* counts = nullptr;
    if (find(fields, tile_offsets) != nullptr) {
        image.block_width =
            static_cast<std::uint32_t>(scalar(fields, tile_width, {}, "TileWidth"));
        image.block_height =
            static_cast<std::uint32_t>(scalar(fields, tile_length, {}, "TileLength"));
        offsets = find(fields, tile_offsets);
        counts = find(fields, tile_byte_counts);
    } else {
        image.strips = true;
        image.block_width = image.width;
        image.block_height = static_cast<std::uint32_t>(std::min<double>(
            scalar(fields, rows_per_strip, 4294967295.0, ""), image.height));
        offsets = find(fields, strip_offsets);
        counts = find(fields, strip_byte_counts);
    }
    if (image.block_width == 0 || image.block_height == 0 ||
        image.block_width > 1u << 16 || image.block_height > 1u << 20) {
        throw GeoTiffError("blocks of " + std::to_string(image.block_width) + " by " +
                           std::to_string(image.block_height) + " samples");
    }
    if (offsets == nullptr || counts == nullptr) {
        throw GeoTiffError("no tile or strip offsets and byte counts");
    }
    const std::size_t blocks =
        std::size_t{image.blocks_across()} * std::size_t{image.blocks_down()};
    if (offsets->numbers.size() != blocks || counts->numbers.size() != blocks) {
        throw GeoTiffError(std::to_string(blocks) + " blocks, but " +
                           std::to_string(offsets->numbers.size()) + " offsets and " +
                           std::to_string(counts->numbers.size()) + " byte counts");
    }
    for (std::size_t i = 0; i < blocks; ++i) {
        const auto offset = static_cast<std::uint64_t>(offsets->numbers[i]);
        const auto count = static_cast<std::uint64_t>(counts->numbers[i]);
        if (offset > file_size || count > file_size - offset) {
            throw GeoTiffError("block " + std::to_string(i) +
                               " lies past the end of the file");
        }
        image.offsets.push_back(offset);
        image.byte_counts.push_back(count);
    }
    return image;
}

void georeference(GeoTiff& tiff, const std::map<std::uint16_t, Field>& fields) {
    if (find(fields, model_transformation) != nullptr) {
        throw GeoTiffError("a ModelTransformationTag, which is not read");
    }
    const Field* keys = find(fields, geo_key_directory);
    const Field* scale = find(fields, model_pixel_scale);
    const Field* tie = find(fields, model_tiepoint);
    if (keys == nullptr || scale == nullptr || tie == nullptr) {
        throw GeoTiffError("not a GeoTIFF: no GeoKeyDirectory, ModelPixelScale or "
                           "ModelTiepoint");
    }
    const auto& k = keys->numbers;
    if (k.size() < 4 || k.size() < 4 + 4 * static_cast<std::size_t>(k[3])) {
        throw GeoTiffError("a GeoKeyDirectory shorter than it says");
    }
    std::map<std::uint16_t, double> geo_keys;
    for (std::size_t i = 0; i < static_cast<std::size_t>(k[3]); ++i) {
        // Only keys held inline in the directory (location 0) are needed.
        if (k[4 + 4 * i + 1] == 0) {
            geo_keys[static_cast<std::uint16_t>(k[4 + 4 * i])] = k[4 + 4 * i + 3];
        }
    }
    const auto key = [&](std::uint16_t id) -> std::optional<double> {
        const auto it = geo_keys.find(id);
        return it == geo_keys.end() ? std::nullopt : std::optional(it->second);
    };
    if (key(key_model_type) != model_type_geographic) {
        throw GeoTiffError(
            "not in geographic coordinates (GTModelTypeGeoKey is not 2)");
    }
    if (key(key_geographic_type) != epsg_wgs84) {
        throw GeoTiffError("not on WGS84 (GeographicTypeGeoKey is not 4326)");
    }
    const double raster_type = key(key_raster_type).value_or(raster_pixel_is_area);
    if (raster_type != raster_pixel_is_area && raster_type != raster_pixel_is_point) {
        throw GeoTiffError("an unknown GTRasterTypeGeoKey");
    }
    if (scale->numbers.size() < 2 || tie->numbers.size() < 6) {
        throw GeoTiffError("a ModelPixelScale or ModelTiepoint too short");
    }
    tiff.longitude_step_deg = scale->numbers[0];
    tiff.latitude_step_deg = scale->numbers[1];
    if (!(tiff.longitude_step_deg > 0.0) || !(tiff.latitude_step_deg > 0.0)) {
        throw GeoTiffError("a pixel scale that is not positive");
    }
    // Tiepoint raster (I, J) is at model (X, Y). For areas, raster coordinates
    // name corners, and a sample is at its area's centre, half a step in.
    const double half = raster_type == raster_pixel_is_area ? 0.5 : 0.0;
    tiff.origin_longitude_deg =
        tie->numbers[3] + (half - tie->numbers[0]) * tiff.longitude_step_deg;
    tiff.origin_latitude_deg =
        tie->numbers[4] - (half - tie->numbers[1]) * tiff.latitude_step_deg;

    if (const Field* nodata = find(fields, gdal_nodata); nodata != nullptr) {
        try {
            tiff.nodata = std::stof(nodata->text);
        } catch (const std::exception&) {
            throw GeoTiffError("a GDAL_NODATA value that is not a number: " +
                               nodata->text);
        }
    }
}

} // namespace

namespace {

GeoTiff read_geotiff_from(const ByteSource& source) {
    const Reader reader(source);
    GeoTiff tiff;
    std::uint64_t offset = reader.first_ifd();
    std::vector<std::uint64_t> seen;
    while (offset != 0) {
        if (std::find(seen.begin(), seen.end(), offset) != seen.end() ||
            seen.size() > 64) {
            throw GeoTiffError("its image directories loop");
        }
        seen.push_back(offset);
        std::uint64_t next = 0;
        const auto fields = reader.ifd(offset, next);
        // Masks and other subfiles that are not overviews are skipped.
        const auto subfile =
            static_cast<std::uint32_t>(scalar(fields, new_subfile_type, 0.0, ""));
        if (tiff.images.empty()) {
            if (subfile != 0) {
                throw GeoTiffError("the first image is not the full-resolution one");
            }
            tiff.images.push_back(image_from(reader, fields, source.size()));
            georeference(tiff, fields);
        } else if (subfile == 1) {
            tiff.images.push_back(image_from(reader, fields, source.size()));
        }
        offset = next;
    }
    if (tiff.images.empty()) {
        throw GeoTiffError("no images");
    }
    return tiff;
}

std::vector<float> read_block_from(const ByteSource& source, const RasterImage& image,
                                   std::uint32_t across, std::uint32_t down) {
    if (across >= image.blocks_across() || down >= image.blocks_down()) {
        throw GeoTiffError("no block (" + std::to_string(across) + ", " +
                           std::to_string(down) + ")");
    }
    const std::size_t index =
        std::size_t{down} * std::size_t{image.blocks_across()} + std::size_t{across};
    const std::size_t width = image.block_width;
    // A strip image's last strip holds only the rows left.
    std::size_t rows = image.block_height;
    if (image.strips && down + 1 == image.blocks_down()) {
        rows = image.height - std::size_t{down} * image.block_height;
    }
    const std::size_t samples = width * rows;
    const std::size_t bytes = samples * 4;

    std::vector<std::uint8_t> stored(
        static_cast<std::size_t>(image.byte_counts[index]));
    source.read(image.offsets[index], stored);
    std::vector<std::uint8_t> raw;
    if (image.compression == 1) {
        raw = std::move(stored);
    } else {
        try {
            raw = inflate_zlib(stored, bytes);
        } catch (const InflateError& e) {
            throw GeoTiffError("block " + std::to_string(index) + ": " + e.what());
        }
    }
    if (raw.size() != bytes) {
        throw GeoTiffError("block " + std::to_string(index) + " holds " +
                           std::to_string(raw.size()) + " bytes, not " +
                           std::to_string(bytes));
    }

    std::vector<float> out(samples);
    if (image.predictor == 3) {
        // TIFF Technical Note 3: each row's samples are split into byte
        // planes, most significant first, and every byte of the row is stored
        // as its difference from the byte before.
        const std::size_t row_bytes = width * 4;
        for (std::size_t r = 0; r < rows; ++r) {
            std::uint8_t* row = raw.data() + r * row_bytes;
            for (std::size_t i = 1; i < row_bytes; ++i) {
                row[i] = static_cast<std::uint8_t>(row[i] + row[i - 1]);
            }
            for (std::size_t x = 0; x < width; ++x) {
                const std::uint32_t bits = (std::uint32_t{row[x]} << 24) |
                                           (std::uint32_t{row[x + width]} << 16) |
                                           (std::uint32_t{row[x + 2 * width]} << 8) |
                                           std::uint32_t{row[x + 3 * width]};
                out[r * width + x] = std::bit_cast<float>(bits);
            }
        }
    } else {
        for (std::size_t i = 0; i < samples; ++i) {
            const std::uint8_t* p = raw.data() + i * 4;
            const std::uint32_t bits =
                image.big_endian
                    ? (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) |
                          (std::uint32_t{p[2]} << 8) | std::uint32_t{p[3]}
                    : (std::uint32_t{p[3]} << 24) | (std::uint32_t{p[2]} << 16) |
                          (std::uint32_t{p[1]} << 8) | std::uint32_t{p[0]};
            out[i] = std::bit_cast<float>(bits);
        }
    }
    return out;
}

} // namespace

// A file that ends early, or cannot be read, is a GeoTIFF that cannot be read.
GeoTiff read_geotiff(const ByteSource& source) {
    try {
        return read_geotiff_from(source);
    } catch (const ByteSourceError& e) {
        throw GeoTiffError(e.what());
    }
}

std::vector<float> read_block(const ByteSource& source, const RasterImage& image,
                              std::uint32_t across, std::uint32_t down) {
    try {
        return read_block_from(source, image, across, down);
    } catch (const ByteSourceError& e) {
        throw GeoTiffError(e.what());
    }
}

} // namespace glideslope::world
