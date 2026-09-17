#include "world/inflate.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace glideslope::world {

namespace {

constexpr int max_bits = 15;
constexpr int fast_bits = 10;

// Reads bits least significant first, as DEFLATE packs them.
class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> data) : data_(data) {}

    // At least `n` (up to 32) bits in the buffer. Past the end of the input the
    // buffer is filled with zeros, so a code near the end can be looked up with
    // bits to spare; finish() says whether any of them were used. More than four
    // bytes past the end - more than any lookahead - is truncation, and is
    // refused at once, because zeros can decode forever.
    void need(int n) {
        while (count_ < n) {
            std::uint64_t byte = 0;
            if (next_ < data_.size()) {
                byte = data_[next_];
            } else if (++overrun_ > 4) {
                throw InflateError("DEFLATE data ends early");
            }
            ++next_;
            buffer_ |= byte << count_;
            count_ += 8;
        }
    }

    std::uint32_t peek(int n) {
        need(n);
        return static_cast<std::uint32_t>(buffer_ & ((std::uint64_t{1} << n) - 1));
    }

    void consume(int n) {
        buffer_ >>= n;
        count_ -= n;
    }

    std::uint32_t bits(int n) {
        if (n == 0) {
            return 0;
        }
        const std::uint32_t value = peek(n);
        consume(n);
        return value;
    }

    // Drops the bits left in the current byte.
    void align() {
        consume(count_ % 8);
    }

    // Whole bytes, after align(). Fails rather than padding: a stored block's
    // length says exactly how much input there must be.
    std::span<const std::uint8_t> bytes(std::size_t n) {
        // Bytes already pulled into the buffer are returned to the input.
        const std::size_t buffered = static_cast<std::size_t>(count_ / 8);
        if (overrun_ > buffered) {
            throw InflateError("DEFLATE data ends early");
        }
        const std::size_t at = next_ - buffered;
        if (data_.size() - at < n) {
            throw InflateError("DEFLATE data ends inside a stored block");
        }
        next_ = at + n;
        buffer_ = 0;
        count_ = 0;
        return data_.subspan(at, n);
    }

    // Where the reader is, in whole bytes of input. Throws if it has read past
    // the end.
    std::size_t finish() {
        align();
        const std::size_t buffered = static_cast<std::size_t>(count_ / 8);
        if (overrun_ > buffered) {
            throw InflateError("DEFLATE data ends early");
        }
        return next_ - buffered;
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t next_ = 0;
    std::size_t overrun_ = 0;
    std::uint64_t buffer_ = 0;
    int count_ = 0;
};

// A canonical Huffman code: a table for codes of up to fast_bits bits, and
// the counts and symbols to decode longer ones a bit at a time.
class Huffman {
public:
    // Lengths of 0 mean the symbol is unused. A code must be complete, except
    // that - as zlib accepts - a literal/length or distance code may have one
    // symbol, or a distance code none, when `sparse` is set.
    void build(std::span<const std::uint8_t> lengths, bool sparse) {
        counts_.fill(0);
        fast_.fill(0);
        int used = 0;
        for (const std::uint8_t length : lengths) {
            if (length > max_bits) {
                throw InflateError("a Huffman code length is over 15");
            }
            ++counts_[length];
            used += length == 0 ? 0 : 1;
        }
        counts_[0] = 0;
        int left = 1;
        for (int len = 1; len <= max_bits; ++len) {
            left = (left << 1) - counts_[static_cast<std::size_t>(len)];
            if (left < 0) {
                throw InflateError("a Huffman code is over-subscribed");
            }
        }
        if (left > 0 && !(sparse && used <= 1)) {
            throw InflateError("a Huffman code is incomplete");
        }

        std::array<std::uint16_t, max_bits + 2> offsets{};
        for (int len = 1; len <= max_bits; ++len) {
            offsets[static_cast<std::size_t>(len + 1)] =
                static_cast<std::uint16_t>(offsets[static_cast<std::size_t>(len)] +
                                           counts_[static_cast<std::size_t>(len)]);
        }
        std::array<int, max_bits + 1> next_code{};
        int code = 0;
        for (int len = 1; len <= max_bits; ++len) {
            code = (code + counts_[static_cast<std::size_t>(len - 1)]) << 1;
            next_code[static_cast<std::size_t>(len)] = code;
        }
        for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
            const int len = lengths[symbol];
            if (len == 0) {
                continue;
            }
            symbols_[offsets[static_cast<std::size_t>(len)]++] =
                static_cast<std::uint16_t>(symbol);
            const int assigned = next_code[static_cast<std::size_t>(len)]++;
            if (len <= fast_bits) {
                // Codes are packed most significant bit first, and read least
                // significant first, so the table is indexed by the reversed
                // code, with every value of the bits above it.
                int reversed = 0;
                for (int i = 0; i < len; ++i) {
                    reversed |= ((assigned >> i) & 1) << (len - 1 - i);
                }
                for (int fill = reversed; fill < (1 << fast_bits); fill += 1 << len) {
                    fast_[static_cast<std::size_t>(fill)] = static_cast<std::uint16_t>(
                        (len << 12) | static_cast<int>(symbol));
                }
            }
        }
    }

    int decode(BitReader& in) const {
        const std::uint16_t entry = fast_[in.peek(fast_bits)];
        if (entry != 0) {
            in.consume(entry >> 12);
            return entry & 0x0fff;
        }
        // Longer than fast_bits: canonical decoding, a bit at a time.
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= max_bits; ++len) {
            code |= static_cast<int>(in.bits(1));
            const int count = counts_[static_cast<std::size_t>(len)];
            if (code - count < first) {
                return symbols_[static_cast<std::size_t>(index + (code - first))];
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        throw InflateError("DEFLATE data holds a code its Huffman table does not");
    }

private:
    std::array<std::uint16_t, max_bits + 1> counts_{};
    std::array<std::uint16_t, 288> symbols_{};
    std::array<std::uint16_t, 1 << fast_bits> fast_{};
};

constexpr std::array<std::uint16_t, 29> length_base{
    3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> length_extra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                                    1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                                    4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> distance_base{
    1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
    33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> distance_extra{
    0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

class Inflater {
public:
    Inflater(std::span<const std::uint8_t> data, std::size_t max_size)
        : in_(data), max_size_(max_size) {}

    std::size_t run(std::vector<std::uint8_t>& out) {
        out_ = &out;
        bool last = false;
        while (!last) {
            last = in_.bits(1) == 1;
            switch (in_.bits(2)) {
            case 0: stored(); break;
            case 1: fixed(); break;
            case 2: dynamic(); break;
            default: throw InflateError("DEFLATE block of the reserved type 3");
            }
        }
        return in_.finish();
    }

private:
    void grow(std::size_t n) {
        if (out_->size() + n > max_size_) {
            throw InflateError("DEFLATE data decompresses to more than " +
                               std::to_string(max_size_) + " bytes");
        }
    }

    void stored() {
        in_.align();
        const std::uint32_t len = in_.bits(16);
        const std::uint32_t nlen = in_.bits(16);
        if ((len ^ 0xffffu) != nlen) {
            throw InflateError("a stored DEFLATE block's length does not match its "
                               "complement");
        }
        const auto bytes = in_.bytes(len);
        grow(len);
        out_->insert(out_->end(), bytes.begin(), bytes.end());
    }

    void fixed() {
        if (!fixed_built_) {
            std::array<std::uint8_t, 288> lengths{};
            for (std::size_t i = 0; i < 288; ++i) {
                lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
            }
            fixed_lengths_.build(lengths, false);
            // All 32 five-bit codes, as RFC 1951 defines them; 30 and 31 are
            // refused when they appear.
            std::array<std::uint8_t, 32> distances{};
            distances.fill(5);
            fixed_distances_.build(distances, false);
            fixed_built_ = true;
        }
        codes(fixed_lengths_, fixed_distances_);
    }

    void dynamic() {
        const std::size_t nlen = in_.bits(5) + 257;
        const std::size_t ndist = in_.bits(5) + 1;
        const std::size_t ncode = in_.bits(4) + 4;
        if (nlen > 286 || ndist > 30) {
            throw InflateError("a dynamic DEFLATE block declares too many codes");
        }
        static constexpr std::array<std::uint8_t, 19> order{
            16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        std::array<std::uint8_t, 19> code_lengths{};
        for (std::size_t i = 0; i < ncode; ++i) {
            code_lengths[order[i]] = static_cast<std::uint8_t>(in_.bits(3));
        }
        Huffman lencode;
        lencode.build(code_lengths, false);

        std::array<std::uint8_t, 316> lengths{};
        std::size_t index = 0;
        while (index < nlen + ndist) {
            const int symbol = lencode.decode(in_);
            if (symbol < 16) {
                lengths[index++] = static_cast<std::uint8_t>(symbol);
                continue;
            }
            std::uint8_t value = 0;
            std::uint32_t repeat = 0;
            if (symbol == 16) {
                if (index == 0) {
                    throw InflateError("a DEFLATE code length repeats nothing");
                }
                value = lengths[index - 1];
                repeat = 3 + in_.bits(2);
            } else if (symbol == 17) {
                repeat = 3 + in_.bits(3);
            } else {
                repeat = 11 + in_.bits(7);
            }
            if (index + repeat > nlen + ndist) {
                throw InflateError("DEFLATE code lengths run past their count");
            }
            for (std::uint32_t i = 0; i < repeat; ++i) {
                lengths[index++] = value;
            }
        }
        if (lengths[256] == 0) {
            throw InflateError("a dynamic DEFLATE block has no end-of-block code");
        }
        Huffman literal;
        literal.build(std::span(lengths).first(nlen), true);
        Huffman distance;
        distance.build(std::span(lengths).subspan(nlen, ndist), true);
        codes(literal, distance);
    }

    void codes(const Huffman& literal, const Huffman& distance) {
        std::vector<std::uint8_t>& out = *out_;
        for (;;) {
            const int symbol = literal.decode(in_);
            if (symbol < 256) {
                grow(1);
                out.push_back(static_cast<std::uint8_t>(symbol));
                continue;
            }
            if (symbol == 256) {
                return;
            }
            const auto l = static_cast<std::size_t>(symbol - 257);
            if (l >= length_base.size()) {
                throw InflateError("DEFLATE length code out of range");
            }
            const std::size_t length = length_base[l] + in_.bits(length_extra[l]);
            const auto d = static_cast<std::size_t>(distance.decode(in_));
            if (d >= distance_base.size()) {
                throw InflateError("DEFLATE distance code out of range");
            }
            const std::size_t back = distance_base[d] + in_.bits(distance_extra[d]);
            if (back > out.size()) {
                throw InflateError("DEFLATE data refers back before its start");
            }
            grow(length);
            std::size_t from = out.size() - back;
            out.resize(out.size() + length);
            std::size_t to = out.size() - length;
            // Byte by byte: a match may overlap what it is copying.
            for (std::size_t i = 0; i < length; ++i) {
                out[to++] = out[from++];
            }
        }
    }

    BitReader in_;
    std::size_t max_size_;
    std::vector<std::uint8_t>* out_ = nullptr;
    Huffman fixed_lengths_;
    Huffman fixed_distances_;
    bool fixed_built_ = false;
};

} // namespace

std::uint32_t adler32(std::span<const std::uint8_t> data) {
    constexpr std::uint32_t mod = 65521;
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    // 5552 bytes is the most that can be summed before b can overflow 32 bits.
    for (std::size_t start = 0; start < data.size(); start += 5552) {
        const std::size_t end = std::min(data.size(), start + 5552);
        for (std::size_t i = start; i < end; ++i) {
            a += data[i];
            b += a;
        }
        a %= mod;
        b %= mod;
    }
    return (b << 16) | a;
}

std::vector<std::uint8_t> inflate_raw(std::span<const std::uint8_t> data,
                                      std::size_t max_size) {
    std::vector<std::uint8_t> out;
    Inflater(data, max_size).run(out);
    return out;
}

std::vector<std::uint8_t> inflate_zlib(std::span<const std::uint8_t> stream,
                                       std::size_t max_size) {
    if (stream.size() < 6) {
        throw InflateError("a zlib stream is at least six bytes");
    }
    const unsigned cmf = stream[0];
    const unsigned flg = stream[1];
    if ((cmf & 0x0f) != 8 || (cmf >> 4) > 7) {
        throw InflateError("not a zlib stream of DEFLATE data");
    }
    if ((cmf * 256 + flg) % 31 != 0) {
        throw InflateError("a zlib header fails its check bits");
    }
    if ((flg & 0x20) != 0) {
        throw InflateError("a zlib stream asks for a preset dictionary");
    }
    std::vector<std::uint8_t> out;
    const std::size_t used = Inflater(stream.subspan(2), max_size).run(out);
    const std::size_t at = 2 + used;
    if (stream.size() - at < 4) {
        throw InflateError("a zlib stream ends before its checksum");
    }
    const std::uint32_t expected =
        (std::uint32_t{stream[at]} << 24) | (std::uint32_t{stream[at + 1]} << 16) |
        (std::uint32_t{stream[at + 2]} << 8) | std::uint32_t{stream[at + 3]};
    if (adler32(out) != expected) {
        throw InflateError("a zlib stream fails its Adler-32 checksum");
    }
    return out;
}

} // namespace glideslope::world
