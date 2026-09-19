#include "tiff_writer.hpp"

#include "harness.hpp"
#include "world/inflate.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace glideslope::test::tiff {

namespace {

using Bytes = std::vector<std::uint8_t>;

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
        default:
            glideslope::test::fail("the test writer has no type " +
                                   std::to_string(type));
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
    if (spec.bits == 8) {
        for (std::size_t r = 0; r < rows; ++r) {
            Bytes row(width);
            for (std::size_t x = 0; x < width; ++x) {
                row[x] = static_cast<std::uint8_t>(values[r * width + x]);
            }
            if (spec.differencing) {
                for (std::size_t i = row.size() - 1; i > 0; --i) {
                    row[i] = static_cast<std::uint8_t>(row[i] - row[i - 1]);
                }
            }
            raw.insert(raw.end(), row.begin(), row.end());
        }
    } else if (spec.float_predictor) {
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
    tags[258] = {3, {double(spec.bits)}, {}};
    tags[259] = {3, {spec.deflate ? 8.0 : 1.0}, {}};
    tags[262] = {3, {1}, {}};
    tags[277] = {3, {1}, {}};
    tags[284] = {3, {1}, {}};
    tags[317] = {3, {spec.differencing ? 2.0 : spec.float_predictor ? 3.0 : 1.0}, {}};
    tags[339] = {3, {spec.bits == 8 ? 1.0 : 3.0}, {}};
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

} // namespace

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

std::vector<float> mask_samples(std::uint32_t width, std::uint32_t height,
                                std::uint32_t seed) {
    std::vector<float> out;
    std::uint32_t state = seed * 2654435761u + 1;
    for (std::uint32_t i = 0; i < width * height; ++i) {
        state = state * 1664525u + 1013904223u;
        out.push_back(static_cast<float>(state >> 24));
    }
    return out;
}

std::vector<std::uint8_t> write_tiff(const Spec& spec, const std::vector<float>& full,
                                     const std::vector<float>& overview) {
    Writer w(spec.big_endian);
    w.u8(spec.big_endian ? 'M' : 'I');
    w.u8(spec.big_endian ? 'M' : 'I');
    w.u16(spec.magic);
    w.u32(0); // the first IFD, patched below

    auto tags = write_image(w, spec, spec.width, spec.height, full);
    const std::uint32_t ow = (spec.width + 1) / 2;
    const std::uint32_t oh = (spec.height + 1) / 2;
    std::map<std::uint16_t, Value> overview_tags;
    if (spec.overview) {
        overview_tags = write_image(w, spec, ow, oh, overview);
        overview_tags[254] = {4, {1}, {}};
    }

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
        if (image->empty()) {
            continue;
        }
        const std::uint32_t at = w.here() + (w.here() % 2);
        w.u32_at(patch, at);
        patch = write_ifd(w, *image);
    }
    return w.bytes;
}

} // namespace glideslope::test::tiff
