#include "harness.hpp"

#include "world/byte_source.hpp"
#include "world/geoid.hpp"
#include "world/inflate.hpp"
#include "world/zip.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::crc32;
using glideslope::world::FileSource;
using glideslope::world::Geoid;
using glideslope::world::GeoidError;
using glideslope::world::MemorySource;
using glideslope::world::zip_entries;
using glideslope::world::zip_read;
using glideslope::world::ZipError;

namespace {

using Bytes = std::vector<std::uint8_t>;

void le16(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}

void le32(Bytes& b, std::uint32_t v) {
    le16(b, v & 0xffff);
    le16(b, v >> 16);
}

struct Member {
    std::string name;
    Bytes contents;
    std::uint16_t method = 0; // 0 stored; 8 as DEFLATE stored blocks
    std::uint16_t flags = 0;
    std::uint32_t crc_change = 0; // xored into the recorded CRC-32
};

// A zip archive, written the way the reader expects one.
Bytes make_zip(const std::vector<Member>& members, std::uint16_t count_override = 0) {
    Bytes zip;
    Bytes directory;
    for (const Member& m : members) {
        Bytes data = m.contents;
        if (m.method == 8) {
            data.clear();
            data.push_back(1); // one final stored block
            le16(data, static_cast<std::uint32_t>(m.contents.size()));
            le16(data, ~static_cast<std::uint32_t>(m.contents.size()) & 0xffff);
            data.insert(data.end(), m.contents.begin(), m.contents.end());
        }
        const std::uint32_t crc = crc32(m.contents) ^ m.crc_change;
        const auto local = static_cast<std::uint32_t>(zip.size());
        le32(zip, 0x04034b50);
        le16(zip, 20);
        le16(zip, m.flags);
        le16(zip, m.method);
        le32(zip, 0);
        le32(zip, crc);
        le32(zip, static_cast<std::uint32_t>(data.size()));
        le32(zip, static_cast<std::uint32_t>(m.contents.size()));
        le16(zip, static_cast<std::uint32_t>(m.name.size()));
        le16(zip, 0);
        zip.insert(zip.end(), m.name.begin(), m.name.end());
        zip.insert(zip.end(), data.begin(), data.end());

        le32(directory, 0x02014b50);
        le16(directory, 20);
        le16(directory, 20);
        le16(directory, m.flags);
        le16(directory, m.method);
        le32(directory, 0);
        le32(directory, crc);
        le32(directory, static_cast<std::uint32_t>(data.size()));
        le32(directory, static_cast<std::uint32_t>(m.contents.size()));
        le16(directory, static_cast<std::uint32_t>(m.name.size()));
        le16(directory, 0);
        le16(directory, 0);
        le16(directory, 0);
        le16(directory, 0);
        le32(directory, 0);
        le32(directory, local);
        directory.insert(directory.end(), m.name.begin(), m.name.end());
    }
    const auto directory_offset = static_cast<std::uint32_t>(zip.size());
    zip.insert(zip.end(), directory.begin(), directory.end());
    const std::uint32_t count = count_override != 0
                                    ? count_override
                                    : static_cast<std::uint32_t>(members.size());
    le32(zip, 0x06054b50);
    le16(zip, 0);
    le16(zip, 0);
    le16(zip, count);
    le16(zip, count);
    le32(zip, static_cast<std::uint32_t>(directory.size()));
    le32(zip, directory_offset);
    le16(zip, 0);
    return zip;
}

Bytes text(const std::string& s) {
    return Bytes(s.begin(), s.end());
}

void zip_refuses(const Bytes& zip, const std::string& says,
                 std::uint64_t max_size = 1 << 20,
                 std::source_location where = std::source_location::current()) {
    const MemorySource source(zip);
    try {
        for (const auto& entry : zip_entries(source)) {
            zip_read(source, entry, max_size);
        }
    } catch (const ZipError& e) {
        if (std::string(e.what()).find(says) == std::string::npos) {
            fail("refused with \"" + std::string(e.what()) + "\", not \"" + says + "\"",
                 where);
        }
        return;
    }
    fail("read an archive it should refuse for \"" + says + "\"", where);
}

// A PGM geoid grid of `width` by width/2 + 1 samples, 360/width degrees apart,
// whose raw sample at (column, row) is column * 10 + row * 100.
Bytes make_pgm(std::size_t width, const std::string& origin = "90N 0E",
               const std::string& maxval = "65535", std::size_t height = 0,
               std::size_t drop = 0) {
    if (height == 0) {
        height = width / 2 + 1;
    }
    const std::string header =
        "P5\n# Description a test grid\n# Offset -100\n# Scale 0.5\n"
        "# Origin " +
        origin + "\n" + std::to_string(width) + " " + std::to_string(height) + "\n" +
        maxval + "\n";
    Bytes pgm(header.begin(), header.end());
    for (std::size_t r = 0; r < height; ++r) {
        for (std::size_t c = 0; c < width; ++c) {
            const std::size_t v = c * 10 + r * 100;
            pgm.push_back(static_cast<std::uint8_t>(v >> 8));
            pgm.push_back(static_cast<std::uint8_t>(v));
        }
    }
    pgm.resize(pgm.size() - drop);
    return pgm;
}

// What make_pgm's grid holds at (column, row), in metres.
double metres(double column, double row) {
    return -100.0 + 0.5 * (column * 10.0 + row * 100.0);
}

void geoid_refuses(const Bytes& pgm, const std::string& says,
                   std::source_location where = std::source_location::current()) {
    try {
        Geoid g(pgm);
    } catch (const GeoidError& e) {
        if (std::string(e.what()).find(says) == std::string::npos) {
            fail("refused with \"" + std::string(e.what()) + "\", not \"" + says + "\"",
                 where);
        }
        return;
    }
    fail("read a grid it should refuse for \"" + says + "\"", where);
}

} // namespace

GLIDESLOPE_TEST(crc32_gives_the_published_check_value) {
    check(crc32(text("123456789")) == 0xcbf43926u,
          "CRC-32 of \"123456789\" is 0xCBF43926");
    check(crc32({}) == 0u, "CRC-32 of nothing is 0");
}

GLIDESLOPE_TEST(
    a_zip_archive_reads_back_its_stored_and_deflated_entries_and_refuses_bad_ones) {
    const Bytes big(70000, 0x5a);
    const Bytes zip = make_zip({{"readme.txt", text("a geoid"), 0},
                                {"geoids/test.pgm", text("P5 and so on"), 8},
                                {"empty", {}, 0}});
    const MemorySource source(zip);
    const auto entries = zip_entries(source);
    check(entries.size() == 3, "three entries");
    check(entries[0].name == "readme.txt" && entries[1].name == "geoids/test.pgm" &&
              entries[2].name == "empty",
          "named in order");
    check(zip_read(source, entries[0], 100) == text("a geoid"),
          "the stored entry reads back");
    check(zip_read(source, entries[1], 100) == text("P5 and so on"),
          "the deflated entry reads back");
    check(zip_read(source, entries[2], 100).empty(),
          "the empty entry reads back empty");

    zip_refuses(make_zip({{"x", text("contents"), 0, 0, 1}}), "fails its CRC-32");
    zip_refuses(make_zip({{"x", text("contents"), 12}}), "method 12");
    zip_refuses(make_zip({{"x", text("contents"), 0, 1}}), "encrypted");
    zip_refuses(make_zip({{"x", text("contents"), 0}}, 0xffff), "Zip64");
    zip_refuses(make_zip({{"x", big, 0}}), "70000 bytes, over the limit of 69999",
                69999);
    zip_refuses(text("not an archive at all, but long enough to look"),
                "no end of central directory");
    Bytes cut = make_zip({{"x", text("contents"), 0}});
    cut.erase(cut.begin() + 10, cut.begin() + 30);
    zip_refuses(cut, "");
}

GLIDESLOPE_TEST(
    a_geoid_grid_interpolates_bilinearly_across_the_date_line_and_to_both_poles) {
    // Eight columns 45 degrees apart; five rows, 90 N to 90 S.
    const Geoid g(make_pgm(8));
    check(g.description() == "a test grid", "the description is read");
    const auto near = [](double a, double b) { return std::abs(a - b) < 1e-9; };

    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 8; ++c) {
            check(near(g.undulation(90.0 - 45.0 * r, 45.0 * c), metres(c, r)),
                  "each sample is its own value");
        }
    }
    check(near(g.undulation(0.0, 22.5), (metres(0, 2) + metres(1, 2)) / 2),
          "halfway between columns");
    check(near(g.undulation(22.5, 90.0), (metres(2, 1) + metres(2, 2)) / 2),
          "halfway between rows");
    check(near(g.undulation(22.5, 22.5),
               (metres(0, 1) + metres(1, 1) + metres(0, 2) + metres(1, 2)) / 4),
          "in the middle of a cell");
    check(near(g.undulation(0.0, 337.5), (metres(7, 2) + metres(0, 2)) / 2),
          "across 360 degrees, between the last column and the first");
    check(near(g.undulation(0.0, -22.5), g.undulation(0.0, 337.5)),
          "a negative longitude is the same place");
    check(near(g.undulation(0.0, 360.0), metres(0, 2)) &&
              near(g.undulation(0.0, -360.0), metres(0, 2)),
          "360 degrees round is where it started");
    check(near(g.undulation(10.0, 180.0), g.undulation(10.0, -180.0)),
          "180 E and 180 W are the same meridian");
    check(near(g.undulation(-90.0, 45.0), metres(1, 4)), "the South Pole's row");
    check(near(g.undulation(-100.0, 45.0), metres(1, 4)),
          "past the South Pole is clamped");
    check(near(g.undulation(95.0, 45.0), metres(1, 0)),
          "past the North Pole is clamped");

    geoid_refuses(text("P2\n8 5\n65535\n"), "not a binary PGM");
    geoid_refuses(make_pgm(8, "90N 0E", "255"), "not a 16-bit PGM");
    geoid_refuses(make_pgm(8, "0N 180W"), "origin is not 90N 0E");
    geoid_refuses(make_pgm(8, "90N 0E", "65535", 4), "does not span the globe");
    geoid_refuses(make_pgm(8, "90N 0E", "65535", 0, 3), "bytes of samples, not 80");
}

GLIDESLOPE_TEST(
    the_egm2008_grid_gives_geographiclibs_undulations_within_its_stated_error) {
    const std::filesystem::path path =
        std::filesystem::path(GLIDESLOPE_TEST_DOWNLOADS_DIR) / "egm2008-5.zip";
    if (!std::filesystem::exists(path)) {
        glideslope::test::skip(path.string() + " was not fetched");
    }
    const Geoid g = glideslope::world::load_geoid_zip(FileSource(path));
    check(g.description() == "WGS84 EGM2008, 5-minute grid",
          "it is the 5-minute EGM2008 grid");

    // From GeographicLib's own GeoidEval service
    // (https://geographiclib.sourceforge.io/cgi-bin/GeoidEval), 2026-09-18:
    // the sea floor's low south of India, New Guinea's high, both poles, the
    // date line from both sides, Greenwich, summits and airfields.
    struct Point {
        double latitude;
        double longitude;
        double undulation;
        const char* where;
    };
    const std::vector<Point> points{
        {-33.9461, 151.1772, 22.0914, "Sydney airport"},
        {27.9881, 86.9250, -28.4939, "Everest"},
        {39.8617, -104.6731, -18.1943, "Denver airport"},
        {4.7, 78.8, -106.8991, "the Indian Ocean low"},
        {-5.0, 145.0, 70.2391, "New Guinea"},
        {90.0, 0.0, 14.8980, "the North Pole"},
        {-90.0, 0.0, -30.1500, "the South Pole"},
        {0.0, 180.0, 21.2813, "the date line from the east"},
        {0.0, -180.0, 21.2813, "the date line from the west"},
        {51.4779, 0.0, 45.8962, "Greenwich"},
        {-35.5, 138.5, -1.7632, "Kangaroo Island"},
        {46.8743190, 102.4487290, -44.4138, "Mongolia"},
        {-14.6212170, 305.0211140, -3.2064, "Brazil, east of 180 as 305"},
        {38.6281550, 269.7791550, -31.7032, "Illinois, as 269.8 E"}};
    double worst = 0.0;
    for (const Point& p : points) {
        const double error =
            std::abs(g.undulation(p.latitude, p.longitude) - p.undulation);
        worst = std::max(worst, error);
        // GeographicLib states the 5-minute grid's bilinear error at 0.478 m.
        check(error <= 0.478,
              std::string(p.where) + ": " +
                  std::to_string(g.undulation(p.latitude, p.longitude)) +
                  " m, GeographicLib " + std::to_string(p.undulation) + " m");
    }
    std::printf("worst of %zu points: %.3f m\n", points.size(), worst);
}
