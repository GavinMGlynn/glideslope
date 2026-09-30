#include "harness.hpp"
#include "tiff_writer.hpp"

#include "world/byte_source.hpp"
#include "world/dem.hpp"
#include "world/digest.hpp"
#include "world/download.hpp"
#include "world/geotiff.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <iterator>
#include <mutex>
#include <optional>
#include <thread>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using glideslope::platform::HttpError;
using glideslope::platform::HttpResponse;
using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::DemCell;
using glideslope::world::DemDataset;
using glideslope::world::DemError;
using glideslope::world::DownloadedTiles;

namespace {

// A fresh directory under the build tree for one test.
std::filesystem::path scratch(const std::string& name) {
    const std::filesystem::path dir =
        std::filesystem::path(GLIDESLOPE_TEST_DOWNLOADS_DIR) / "test-scratch" / name;
    std::filesystem::remove_all(dir);
    return dir;
}

std::vector<std::uint8_t> small_tile() {
    glideslope::test::tiff::Spec spec;
    spec.width = 4;
    spec.height = 4;
    spec.block = 4;
    spec.overview = false;
    spec.latitude_step = 0.25;
    spec.longitude_step = 0.25;
    spec.origin_latitude = -33.0;
    spec.origin_longitude = 151.0;
    return glideslope::test::tiff::write_tiff(spec, std::vector<float>(16, 42.0f), {});
}

// What the fake bucket answers, and what it was asked.
struct FakeBucket {
    int status = 200;
    std::vector<std::uint8_t> body = small_tile();
    std::string etag = "\"" + glideslope::world::md5_hex(small_tile()) + "\"";
    bool unreachable = false;
    std::vector<std::string> asked;

    glideslope::world::Fetch fetch() {
        return [this](const std::string& url) {
            asked.push_back(url);
            if (unreachable) {
                throw HttpError(url + ": no route to host");
            }
            HttpResponse r;
            r.status = status;
            r.body = body;
            if (!etag.empty()) {
                r.headers["etag"] = etag;
            }
            return r;
        };
    }
};

std::string refusal(DownloadedTiles& tiles) {
    try {
        tiles.open(DemDataset::glo30, {-34, 151});
    } catch (const DemError& e) {
        return e.what();
    }
    return {};
}

} // namespace

GLIDESLOPE_TEST(
    a_dem_tile_is_fetched_once_checked_against_its_etag_and_read_from_the_cache_after) {
    const auto cache = scratch("fetched-once");
    FakeBucket bucket;
    {
        DownloadedTiles tiles(cache, bucket.fetch());
        const auto source = tiles.open(DemDataset::glo30, {-34, 151});
        check(source->size() == bucket.body.size(), "the tile is what the bucket sent");
        check(tiles.downloads() == 1, "one download");
        check(bucket.asked.size() == 1 &&
                  bucket.asked[0] ==
                      glideslope::world::dem_tile_url(DemDataset::glo30, {-34, 151}),
              "fetched from the tile's URL");
    }
    check(std::filesystem::exists(cache / "copernicus-dem-30m" /
                                  "Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif"),
          "kept in the cache under the dataset's name");
    DownloadedTiles again(cache, bucket.fetch());
    again.open(DemDataset::glo30, {-34, 151});
    check(again.downloads() == 0 && bucket.asked.size() == 1,
          "a second run reads the cache and asks nothing");
}

// Tests, flights and the terrain fetch the same files at the same time; every
// one of them must end with the file, and none of them with a half-written
// one or another's temporary name.
GLIDESLOPE_TEST(several_fetches_of_one_file_at_once_all_end_with_it) {
    const auto cache = scratch("at-once");
    const std::vector<std::uint8_t> body = small_tile();
    const std::string sha = glideslope::world::sha256_hex(body);
    // Its own answer, holding nothing the threads share.
    const glideslope::world::Fetch fetch = [&body](const std::string&) {
        HttpResponse r;
        r.status = 200;
        r.body = body;
        return r;
    };
    std::vector<std::thread> fetchers;
    std::atomic<int> got{0};
    for (int i = 0; i < 4; ++i) {
        fetchers.emplace_back([&] {
            const std::filesystem::path path = glideslope::world::fetch_pinned(
                cache, "tile.tif", "https://example.invalid/tile.tif", sha, fetch);
            if (std::filesystem::exists(path)) {
                ++got;
            }
        });
    }
    for (std::thread& t : fetchers) {
        t.join();
    }
    check(got == 4, "all four fetches ended with the file");
    std::ifstream in(cache / "tile.tif", std::ios::binary);
    const std::vector<std::uint8_t> kept((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
    check(kept == body, "the file kept is what the bucket sent, whole");
    int leftovers = 0;
    for (const auto& entry : std::filesystem::directory_iterator(cache)) {
        leftovers += entry.path().filename().string().find(".part") != std::string::npos;
    }
    check(leftovers == 0, "no half-written file is left behind");
}

GLIDESLOPE_TEST(
    a_dem_tile_that_arrives_damaged_missing_or_not_at_all_is_refused_and_not_kept) {
    const auto cache = scratch("refused");
    const auto kept = [&] {
        return std::filesystem::exists(cache / "copernicus-dem-30m") &&
               !std::filesystem::is_empty(cache / "copernicus-dem-30m");
    };

    FakeBucket damaged;
    damaged.body[100] ^= 0xff;
    DownloadedTiles a(cache, damaged.fetch());
    check(refusal(a).find("arrived damaged") != std::string::npos,
          "a damaged tile is refused");
    check(!kept(), "and nothing is kept");

    FakeBucket missing;
    missing.status = 404;
    DownloadedTiles b(cache, missing.fetch());
    check(refusal(b).find("status 404") != std::string::npos,
          "a missing tile is refused");

    FakeBucket unreachable;
    unreachable.unreachable = true;
    DownloadedTiles c(cache, unreachable.fetch());
    check(refusal(c).find("no route to host") != std::string::npos,
          "an unreachable bucket is refused with the reason");

    FakeBucket untagged;
    untagged.etag.clear();
    DownloadedTiles d(cache, untagged.fetch());
    check(refusal(d).find("no ETag") != std::string::npos,
          "a tile with nothing to check it against is refused");
    check(!kept(), "none of them was kept");

    // An ETag of a multipart upload is not a digest of the file; the tile is
    // kept on TLS's word.
    FakeBucket multipart;
    multipart.etag = "\"0123456789abcdef0123456789abcdef-3\"";
    DownloadedTiles e(cache, multipart.fetch());
    check(refusal(e).empty() && kept(), "a multipart upload's tile is kept");
}

GLIDESLOPE_TEST(a_pinned_file_is_kept_only_if_it_arrives_with_its_pinned_hash) {
    const auto cache = scratch("pinned");
    FakeBucket bucket;
    bucket.body = {'g', 'e', 'o', 'i', 'd'};
    const std::string right = glideslope::world::sha256_hex(bucket.body);
    try {
        glideslope::world::fetch_pinned(cache, "grid.zip",
                                        "https://example.invalid/grid.zip",
                                        std::string(64, '0'), bucket.fetch());
        fail("a file with the wrong hash was accepted");
    } catch (const DemError& e) {
        check(std::string(e.what()).find("not the pinned") != std::string::npos,
              std::string("refused for its hash, not: ") + e.what());
    }
    check(!std::filesystem::exists(cache / "grid.zip"), "the wrong file is not kept");
    const auto path = glideslope::world::fetch_pinned(
        cache, "grid.zip", "https://example.invalid/grid.zip", right, bucket.fetch());
    check(std::filesystem::exists(path) && std::filesystem::file_size(path) == 5,
          "the right file is kept");
    glideslope::world::fetch_pinned(
        cache, "grid.zip", "https://example.invalid/grid.zip", right, bucket.fetch());
    check(bucket.asked.size() == 2, "and not fetched again");
}

namespace {

// What a cached file can have become that makes it unreadable: empty or cut
// short, as a power cut between the rename and the data reaching the disk
// leaves it - empty the commonest, the name on the disk and none of the data
// - or damaged where its samples are, as a failing disk leaves it.
enum class Fault { empty, cut_short, damaged };
constexpr Fault every_fault[] = {Fault::empty, Fault::cut_short, Fault::damaged};

const char* fault_name(Fault fault) {
    switch (fault) {
    case Fault::empty:
        return "empty";
    case Fault::cut_short:
        return "cut short";
    case Fault::damaged:
        return "damaged";
    }
    return "?";
}

std::string fault_dir(Fault fault) {
    std::string name = fault_name(fault);
    std::replace(name.begin(), name.end(), ' ', '-');
    return name;
}

// A 6-by-6 tile, or 8-bit water mask, of `cell`, compressed in four blocks
// as the bucket's are, every sample `value`.
std::vector<std::uint8_t> whole_file(DemCell cell, bool mask, float value) {
    glideslope::test::tiff::Spec spec;
    spec.bits = mask ? 8 : 32;
    spec.width = 6;
    spec.height = 6;
    spec.block = 3;
    spec.deflate = true;
    spec.float_predictor = !mask;
    spec.differencing = mask;
    spec.overview = false;
    spec.latitude_step = 1.0 / 6.0;
    spec.longitude_step = 1.0 / 6.0;
    spec.origin_latitude = cell.latitude + 1;
    spec.origin_longitude = cell.longitude;
    return glideslope::test::tiff::write_tiff(spec, std::vector<float>(36, value), {});
}

// `whole` with `fault`, built so that it fails where it is meant to: a file
// empty or cut short fails as it is opened, its header or its blocks lying
// past its end; a damaged one opens, and its first block fails its checksum
// as it is read.
std::vector<std::uint8_t> with_fault(const std::vector<std::uint8_t>& whole, Fault fault) {
    using glideslope::world::MemorySource;
    const auto tiff = glideslope::world::read_geotiff(MemorySource(whole));
    const auto& image = tiff.images[0];
    std::vector<std::uint8_t> bytes = whole;
    if (fault == Fault::empty || fault == Fault::cut_short) {
        bytes.resize(fault == Fault::empty ? 0
                                           : static_cast<std::size_t>(image.offsets.back() + 1));
        bool refused = false;
        try {
            (void)glideslope::world::read_geotiff(MemorySource(bytes));
        } catch (const glideslope::world::GeoTiffError&) {
            refused = true;
        }
        check(refused, std::string("the file ") + fault_name(fault) +
                           " is refused as it is opened");
    } else {
        bytes[static_cast<std::size_t>(image.offsets[0] + image.byte_counts[0] / 2)] ^= 0x5a;
        const MemorySource damaged(bytes);
        const auto layout = glideslope::world::read_geotiff(damaged);
        bool refused = false;
        try {
            (void)glideslope::world::read_block(damaged, layout.images[0], 0, 0);
        } catch (const glideslope::world::GeoTiffError&) {
            refused = true;
        }
        check(refused, "the damaged file opens, and its first block is refused");
    }
    return bytes;
}

void leave_in_cache(const std::filesystem::path& path,
                    const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    check(static_cast<bool>(out), "the faulty file is left in the cache");
}

std::vector<std::uint8_t> in_cache(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Names in `directory` a file was set aside under, or written to first.
int leftovers(const std::filesystem::path& directory) {
    int found = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        const std::string name = entry.path().filename().string();
        found += name.find(".taken-away") != std::string::npos ||
                 name.find(".part") != std::string::npos;
    }
    return found;
}

// A bucket answering the one file asked of it, with its ETag, or `status`.
glideslope::world::Fetch serving(const std::vector<std::uint8_t>& body,
                                 const std::string& etag, std::vector<std::string>& asked,
                                 int status = 200) {
    return [&body, etag, &asked, status](const std::string& url) {
        asked.push_back(url);
        HttpResponse r;
        r.status = status;
        r.body = body;
        r.headers["etag"] = etag;
        return r;
    };
}

// An ETag of a multipart upload: nothing checks what arrives with it.
const std::string unchecked_etag = "\"0123456789abcdef0123456789abcdef-3\"";

glideslope::world::DemCoverage real_coverage() {
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR) /
                     "../assets/dem/coverage.txt");
    const std::string text{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()};
    glideslope::world::DemCoverage coverage(text);
    check(coverage.at({-34, 151}) == DemDataset::glo30, "Sydney's cell has a 30 m tile");
    check(coverage.at({39, 45}) == DemDataset::glo90, "Armenia's has only a 90 m one");
    return coverage;
}

// The dataset's directory in the cache, as DownloadedTiles names it.
std::string dataset_dir(DemDataset dataset) {
    return dataset == DemDataset::glo30 ? "copernicus-dem-30m" : "copernicus-dem-90m";
}

// Asks `dem` for the layer at a place in `cell`'s first block - the damaged
// one - and says what the query threw, or nothing.
std::string ask(glideslope::world::Dem& dem, DemCell cell, bool mask, float* value) {
    const double latitude = cell.latitude + 1 - 0.2;
    const double longitude = cell.longitude + 0.2;
    try {
        *value = mask ? static_cast<float>(dem.water(latitude, longitude))
                      : static_cast<float>(dem.height_above_geoid(latitude, longitude));
    } catch (const DemError& e) {
        return e.what();
    }
    return {};
}

} // namespace

// A tile left in the cache empty, cut short, or damaged must not fail every
// query of its cell for good: the Dem takes it away and it is fetched again,
// once, and read whole. Both datasets (glo30 at Sydney, glo90 in Armenia:
// each has its own directory), both layers (heights and the water mask), and
// every fault.
GLIDESLOPE_TEST(a_cached_tile_cut_short_or_damaged_is_fetched_again_and_read_whole) {
    const auto coverage = real_coverage();
    const std::pair<DemDataset, DemCell> cells[] = {{DemDataset::glo30, {-34, 151}},
                                                    {DemDataset::glo90, {39, 45}}};
    int covered = 0;
    for (const auto& [dataset, cell] : cells) {
        for (const bool mask : {false, true}) {
            for (const Fault fault : every_fault) {
                const std::string what =
                    std::string(dataset == DemDataset::glo30 ? "glo30 " : "glo90 ") +
                    (mask ? "water mask " : "height tile ") + fault_name(fault);
                const auto cache = scratch("faulty-" + dataset_dir(dataset) +
                                           (mask ? "-mask-" : "-tile-") + fault_dir(fault));
                const float value = mask ? 2.0f : 42.0f;
                const std::vector<std::uint8_t> whole = whole_file(cell, mask, value);
                const std::string name =
                    mask ? glideslope::world::dem_water_mask_name(dataset, cell)
                         : glideslope::world::dem_tile_name(dataset, cell);
                const std::string url = mask
                                            ? glideslope::world::dem_water_mask_url(dataset, cell)
                                            : glideslope::world::dem_tile_url(dataset, cell);
                const auto path = cache / dataset_dir(dataset) / (name + ".tif");
                leave_in_cache(path, with_fault(whole, fault));

                std::vector<std::string> asked;
                DownloadedTiles tiles(
                    cache,
                    serving(whole, "\"" + glideslope::world::md5_hex(whole) + "\"", asked));
                glideslope::world::Dem dem(coverage, tiles, nullptr);
                float got = -1.0f;
                const std::string refused = ask(dem, cell, mask, &got);
                check(refused.empty(), what + " was not fetched again: " + refused);
                check(got == value, what + " is read whole once fetched again: " +
                                        std::to_string(got));
                check(tiles.downloads() == 1 && asked.size() == 1 && asked[0] == url,
                      what + " is fetched again, once, from its own URL");
                check(in_cache(path) == whole, what + " is whole in the cache after");
                check(leftovers(path.parent_path()) == 0,
                      what + " leaves no file set aside or half-written");
                ++covered;
            }
        }
    }
    check(covered == 12, "2 datasets, 2 layers and 3 faults: 12 cases, " +
                             std::to_string(covered) + " covered");
}

// The retry is once per tile for as long as the Dem lives: a tile the bucket
// itself has bad - a multipart upload's, which no ETag checks - fails every
// query of it rather than being fetched again for each. Asked twice, each
// fault in the cache with each at the bucket, the damaged-then-cut-short
// cases among them, where one query finds a block bad and then the fresh
// copy's layout. Left out: a bucket answering an empty 200, which
// fetch_with_retries takes for a server that failed and asks again - its
// own retries, not a tile fetched again.
GLIDESLOPE_TEST(a_tile_bad_where_it_is_fetched_from_is_fetched_again_once_however_often_it_is_asked_for) {
    const DemCell sydney{-34, 151};
    const auto coverage = real_coverage();
    const std::vector<std::uint8_t> whole = whole_file(sydney, false, 42.0f);
    int covered = 0;
    for (const Fault cached : every_fault) {
        for (const Fault at_source : {Fault::cut_short, Fault::damaged}) {
            const std::string what = std::string("a tile ") + fault_name(cached) +
                                     " in the cache and " + fault_name(at_source) +
                                     " at the bucket";
            const auto cache =
                scratch("bad-at-source-" + fault_dir(cached) + "-" + fault_dir(at_source));
            const auto path =
                cache / "copernicus-dem-30m" /
                (glideslope::world::dem_tile_name(DemDataset::glo30, sydney) + ".tif");
            leave_in_cache(path, with_fault(whole, cached));
            const std::vector<std::uint8_t> bad = with_fault(whole, at_source);
            std::vector<std::string> asked;
            DownloadedTiles tiles(cache, serving(bad, unchecked_etag, asked));
            glideslope::world::Dem dem(coverage, tiles, nullptr);
            for (int query = 1; query <= 2; ++query) {
                float got = -1.0f;
                const std::string refused = ask(dem, sydney, false, &got);
                check(refused.find("had anew as it was") != std::string::npos,
                      what + ": query " + std::to_string(query) +
                          " fails, saying the tile was had anew as it was, not: " + refused);
            }
            check(asked.size() == 1, what + ": fetched again once over two queries, not " +
                                         std::to_string(asked.size()) + " times");
            ++covered;
        }
    }
    check(covered == 6, "3 faults in the cache by 2 at the bucket: 6 cases, " +
                            std::to_string(covered) + " covered");
}

// A tile that cannot be fetched at all was never read, so nothing is taken
// away and it is not fetched a second time within the query.
GLIDESLOPE_TEST(a_tile_that_cannot_be_fetched_is_not_taken_away_or_fetched_twice) {
    const DemCell sydney{-34, 151};
    const auto coverage = real_coverage();
    const auto cache = scratch("cannot-be-fetched");
    const std::vector<std::uint8_t> nothing;
    std::vector<std::string> asked;
    DownloadedTiles tiles(cache, serving(nothing, unchecked_etag, asked, 404));
    glideslope::world::Dem dem(coverage, tiles, nullptr);
    float got = -1.0f;
    const std::string refused = ask(dem, sydney, false, &got);
    check(refused.find("status 404") != std::string::npos,
          "the query fails with the fetch's reason, not: " + refused);
    check(asked.size() == 1, "asked once, not " + std::to_string(asked.size()) + " times");
}

// Tiles in a directory have nowhere to be had anew from: one that cannot be
// read fails its query with the reason, and is left where it is.
GLIDESLOPE_TEST(a_bad_tile_in_a_directory_fails_its_query_and_is_left_as_it_is) {
    const DemCell sydney{-34, 151};
    const auto coverage = real_coverage();
    int covered = 0;
    for (const Fault fault : every_fault) {
        const auto directory = scratch("directory-" + fault_dir(fault));
        const auto path =
            directory / (glideslope::world::dem_tile_name(DemDataset::glo30, sydney) + ".tif");
        const std::vector<std::uint8_t> bad =
            with_fault(whole_file(sydney, false, 42.0f), fault);
        leave_in_cache(path, bad);
        glideslope::world::DirectoryTiles tiles(directory);
        glideslope::world::Dem dem(coverage, tiles, nullptr);
        float got = -1.0f;
        const std::string refused = ask(dem, sydney, false, &got);
        check(!refused.empty() && refused.find("had anew") == std::string::npos,
              std::string("a tile ") + fault_name(fault) +
                  " in a directory fails its query with its reason, not: " + refused);
        check(in_cache(path) == bad, std::string("a tile ") + fault_name(fault) +
                                         " in a directory is left as it is");
        ++covered;
    }
    check(covered == 3, "3 faults: 3 cases, " + std::to_string(covered) + " covered");
}

// A pinned file - the geoid, the runways - left in the cache empty, cut
// short or damaged is checked against its pin, taken away and fetched again
// whole.
GLIDESLOPE_TEST(a_cached_pinned_file_cut_short_or_damaged_is_fetched_again_and_read_whole) {
    const std::vector<std::uint8_t> whole = whole_file({-34, 151}, false, 7.0f);
    const std::string sha = glideslope::world::sha256_hex(whole);
    int covered = 0;
    for (const Fault fault : every_fault) {
        const std::string what = std::string("a pinned file ") + fault_name(fault);
        const auto cache = scratch("pinned-" + fault_dir(fault));
        leave_in_cache(cache / "grid.tif", with_fault(whole, fault));
        std::vector<std::string> asked;
        const auto path = glideslope::world::fetch_pinned(
            cache, "grid.tif", "https://example.invalid/grid.tif", sha,
            serving(whole, "\"unchecked\"", asked));
        check(asked.size() == 1, what + " is fetched again, once");
        check(in_cache(path) == whole, what + " is whole in the cache after");
        check(leftovers(cache) == 0, what + " leaves no file set aside or half-written");
        glideslope::world::fetch_pinned(cache, "grid.tif", "https://example.invalid/grid.tif",
                                        sha, serving(whole, "\"unchecked\"", asked));
        check(asked.size() == 1, what + ": once whole, it is not fetched again");
        ++covered;
    }
    check(covered == 3, "3 faults: 3 cases, " + std::to_string(covered) + " covered");
}

// A file found wanting is taken away only if it is still the file that was
// read: one another process has put in its place since - having found the
// same file wanting and fetched it first - is left there.
namespace {
// GLIDESLOPE_RUNWAYS_SOURCE set to `value` for as long as this lives, and
// taken away after.
struct RunwaysSource {
    explicit RunwaysSource(const std::string& value) {
#if defined(_WIN32)
        _putenv_s("GLIDESLOPE_RUNWAYS_SOURCE", value.c_str());
#else
        setenv("GLIDESLOPE_RUNWAYS_SOURCE", value.c_str(), 1);
#endif
    }
    ~RunwaysSource() {
#if defined(_WIN32)
        _putenv_s("GLIDESLOPE_RUNWAYS_SOURCE", "");
#else
        unsetenv("GLIDESLOPE_RUNWAYS_SOURCE");
#endif
    }
    RunwaysSource(const RunwaysSource&) = delete;
    RunwaysSource& operator=(const RunwaysSource&) = delete;
};
} // namespace

// A test may send the runways' fetch elsewhere - to build "OurAirports
// unreachable" - but only over https or to the loopback, and what arrives
// from there is still held to the pinned SHA-256.
GLIDESLOPE_TEST(the_runways_may_be_fetched_elsewhere_for_a_test_and_are_still_held_to_their_pin) {
    const auto cache = scratch("runways-elsewhere");
    check(glideslope::world::runways_host() == "https://raw.githubusercontent.com",
          "unset, the runways come from GitHub");
    {
        const RunwaysSource source("http://127.0.0.1:1");
        const std::vector<std::uint8_t> not_them = {'a', ',', 'b', '\n'};
        std::vector<std::string> asked;
        bool refused = false;
        try {
            (void)glideslope::world::world_runways(cache,
                                                   serving(not_them, "\"x\"", asked));
        } catch (const DemError& e) {
            refused = std::string(e.what()).find("not the pinned") != std::string::npos;
        }
        check(asked.size() == 1 &&
                  asked[0].rfind("http://127.0.0.1:1/davidmegginson/ourairports-data/", 0) == 0,
              "the fetch goes where the test says: " + (asked.empty() ? "" : asked[0]));
        check(refused, "a file that is not the pinned one is refused, wherever it came from");
        check(!std::filesystem::exists(cache / "ourairports-runways.csv"),
              "and is not kept");
    }
    {
        const RunwaysSource source("http://example.org");
        bool refused = false;
        try {
            (void)glideslope::world::runways_host();
        } catch (const glideslope::world::RunwayError& e) {
            refused = std::string(e.what()).find("GLIDESLOPE_RUNWAYS_SOURCE") !=
                      std::string::npos;
        }
        check(refused, "plain http to another machine is refused, by name");
    }
    check(glideslope::world::runways_host() == "https://raw.githubusercontent.com",
          "unset again, GitHub again");
}

GLIDESLOPE_TEST(a_file_put_in_place_by_another_since_it_was_read_is_not_taken_away) {
    const auto directory = scratch("taken-away-by-identity");
    const auto path = directory / "tile.tif";
    leave_in_cache(path, {1, 2, 3});
    std::optional<glideslope::world::FileIdentity> read;
    {
        const glideslope::world::FileSource wanting(path);
        read = wanting.identity();
    }
    check(read.has_value(), "a file read says which file it is");
    // Another's whole copy, moved into place over the name.
    const std::vector<std::uint8_t> theirs = {4, 5, 6, 7};
    leave_in_cache(directory / "theirs.part", theirs);
    std::filesystem::remove(path);
    check(glideslope::world::move_into_place_unless_there(directory / "theirs.part", path),
          "the other's copy is moved into place");
    check(glideslope::world::identity_of(path) != read, "the name holds another file now");
    check(!glideslope::world::take_away(path, read), "the other's copy is not taken away");
    check(in_cache(path) == theirs, "and is there, whole");
    check(glideslope::world::take_away(path, glideslope::world::identity_of(path)),
          "the file that was read is taken away");
    check(!std::filesystem::exists(path) && leftovers(directory) == 0,
          "its name is free, and nothing is left set aside");
}

GLIDESLOPE_TEST(
    a_fetch_is_tried_again_after_a_server_error_or_no_answer_and_not_after_a_refusal) {
    using glideslope::world::fetch_with_retries;
    constexpr std::chrono::milliseconds no_wait{0};
    // Answers with each status in turn, a 0 being no answer at all and `empty`
    // a 200 with nothing in it; every other answer has something in it.
    constexpr int empty = -200;
    const auto answering = [](std::vector<int> statuses, int& calls) {
        return [statuses, &calls](const std::string&) {
            const int status = statuses.at(static_cast<std::size_t>(calls++));
            if (status == 0) {
                throw HttpError("no answer");
            }
            HttpResponse r;
            r.status = status == empty ? 200 : status;
            if (status != empty) {
                r.body = {std::uint8_t{'x'}};
            }
            return r;
        };
    };
    int calls = 0;
    check(
        fetch_with_retries(answering({504, 503, 200}, calls), "u", 3, no_wait).status ==
                200 &&
            calls == 3,
        "two server errors, then the answer");
    calls = 0;
    check(fetch_with_retries(answering({0, 0, 200}, calls), "u", 3, no_wait).status ==
                  200 &&
              calls == 3,
          "no answer twice, then the answer");
    calls = 0;
    const auto after_empty = fetch_with_retries(answering({empty, 200}, calls), "u", 3, no_wait);
    check(after_empty.status == 200 && !after_empty.body.empty() && calls == 2,
          "a 200 with nothing in it, then the answer");
    calls = 0;
    check(
        fetch_with_retries(answering({504, 504, 502}, calls), "u", 3, no_wait).status ==
                502 &&
            calls == 3,
        "three server errors: the last is returned");
    calls = 0;
    try {
        fetch_with_retries(answering({0, 0, 0}, calls), "u", 3, no_wait);
        fail("no answer three times was not thrown");
    } catch (const HttpError&) {
        check(calls == 3, "no answer three times is thrown after the third");
    }
    for (const int refusal : {404, 204, 403, 301}) {
        calls = 0;
        check(fetch_with_retries(answering({refusal, 200}, calls), "u", 3, no_wait)
                          .status == refusal &&
                  calls == 1,
              "status " + std::to_string(refusal) + " is not tried again");
    }
}

GLIDESLOPE_TEST(a_wait_to_try_again_given_up_while_it_waits_ends_within_a_slice) {
    // **The flag is heeded mid-wait**, not only before it: raised by another
    // thread once a minute's wait has begun, the wait ends within a slice
    // (given_up_slice, 10 ms). The bound allows the slice and whatever a
    // loaded runner takes to run the thread again; a wait that looked only
    // before sleeping would sleep out its minute.
    using glideslope::world::FetchesGivenUp;
    using glideslope::world::FetchGivenUp;
    using glideslope::world::given_up_slice;
    using glideslope::world::wait_before_trying_again;
    using Clock = std::chrono::steady_clock;
    const auto bound = given_up_slice + std::chrono::seconds(1);
    std::atomic<bool> give_up{false};
    std::promise<void> began;
    auto waited = std::async(std::launch::async, [&] {
        const FetchesGivenUp given_up(give_up);
        try {
            wait_before_trying_again(std::chrono::minutes(1), [&] { began.set_value(); });
        } catch (const FetchGivenUp&) {
            return true;
        }
        return false;
    });
    began.get_future().wait();
    const Clock::time_point raised = Clock::now();
    give_up = true;
    const bool threw = waited.get();
    const auto took = Clock::now() - raised;
    check(threw, "a wait given up throws FetchGivenUp");
    check(took < bound, "given up mid-wait, it took " +
                            std::to_string(std::chrono::duration<double>(took).count()) +
                            " s to end");

    // And a FetchesGivenUp let go hands the thread back to the one it
    // replaced: the outer flag, raised, still ends a wait.
    std::atomic<bool> outer_flag{false};
    std::atomic<bool> inner_flag{false};
    const FetchesGivenUp outer(outer_flag);
    { const FetchesGivenUp inner(inner_flag); }
    outer_flag = true;
    bool outer_heeded = false;
    try {
        wait_before_trying_again(std::chrono::minutes(1));
    } catch (const FetchGivenUp&) {
        outer_heeded = true;
    }
    check(outer_heeded, "the FetchesGivenUp before an inner one is heeded again once it goes");
}

GLIDESLOPE_TEST(a_429_is_waited_out_as_its_retry_after_asks_within_a_limit_then_returned) {
    // **Too Many Requests**, as Open-Meteo answered CI's parallel weather
    // tests on 2026-09-27: tried again, waiting what Retry-After asks, never
    // more than retry_after_limit, and as a server error waits without one.
    // The waits are counted, not waited.
    using glideslope::world::fetch_with_retries;
    using glideslope::world::retry_after_limit;
    using std::chrono::milliseconds;
    constexpr milliseconds backoff{2000};
    struct Answer {
        int status;
        const char* retry_after; // nullptr: no Retry-After
    };
    struct Case {
        const char* what;
        std::vector<Answer> answers;
        int attempts;
        int status;                 // what is returned
        std::vector<milliseconds> waits; // what was waited, in turn
    };
    const std::vector<Case> cases{
        {"a 429 asking 3 s, then the answer", {{429, "3"}, {200, nullptr}}, 5, 200,
         {milliseconds(3000)}},
        {"a 429 with no Retry-After, then the answer", {{429, nullptr}, {200, nullptr}}, 5,
         200, {backoff}},
        {"a 429 asking an hour waits the limit", {{429, "3600"}, {200, nullptr}}, 5, 200,
         {retry_after_limit}},
        {"a 429 asking a number too long to hold waits the limit",
         {{429, "99999999999999999999999"}, {200, nullptr}}, 5, 200, {retry_after_limit}},
        {"a 429 asking 0 s is tried again at once", {{429, "0"}, {200, nullptr}}, 5, 200,
         {milliseconds(0)}},
        {"a 429 with a date for its Retry-After waits as a server error does",
         {{429, "Wed, 21 Oct 2026 07:28:00 GMT"}, {200, nullptr}}, 5, 200, {backoff}},
        {"a 429 with a Retry-After that is not a number waits as a server error does",
         {{429, "soon"}, {200, nullptr}}, 5, 200, {backoff}},
        {"the doubling goes on under a 429's Retry-After",
         {{429, "1"}, {503, nullptr}, {429, nullptr}, {200, nullptr}}, 5, 200,
         {milliseconds(1000), 2 * backoff, 4 * backoff}},
        {"a 429 with no Retry-After to every try: the backoff, capped at the limit",
         {{429, nullptr}, {429, nullptr}, {429, nullptr}, {429, nullptr}, {429, nullptr}}, 5,
         429, {backoff, 2 * backoff, 4 * backoff, retry_after_limit}},
        {"429s asking a minute, then one with none: every wait capped",
         {{429, "60"}, {429, "60"}, {429, "60"}, {429, nullptr}, {429, nullptr}}, 5, 429,
         {retry_after_limit, retry_after_limit, retry_after_limit, retry_after_limit}},
        {"429s with a date for their Retry-After to every try: capped too",
         {{429, "Wed, 21 Oct 2026 07:28:00 GMT"},
          {429, "Wed, 21 Oct 2026 07:28:00 GMT"},
          {429, "Wed, 21 Oct 2026 07:28:00 GMT"},
          {429, "Wed, 21 Oct 2026 07:28:00 GMT"},
          {429, "Wed, 21 Oct 2026 07:28:00 GMT"}},
         5, 429, {backoff, 2 * backoff, 4 * backoff, retry_after_limit}},
        {"a 429 to every try is returned after the last, the waits bounded",
         {{429, "60"}, {429, "60"}, {429, "60"}, {429, "60"}, {429, "60"}}, 5, 429,
         {retry_after_limit, retry_after_limit, retry_after_limit, retry_after_limit}},
    };
    std::size_t walked = 0;
    for (const Case& c : cases) {
        std::size_t calls = 0;
        std::vector<milliseconds> waited;
        const auto fetch = [&](const std::string&) {
            const Answer& a = c.answers.at(calls++);
            HttpResponse r;
            r.status = a.status;
            r.body = {std::uint8_t{'x'}};
            if (a.retry_after != nullptr) {
                r.headers["retry-after"] = a.retry_after;
            }
            return r;
        };
        const HttpResponse r = fetch_with_retries(
            fetch, "u", c.attempts, backoff, [&](milliseconds w) { waited.push_back(w); });
        check(r.status == c.status && calls == c.answers.size() && waited == c.waits,
              std::string(c.what) + ": status " + std::to_string(r.status) + " after " +
                  std::to_string(calls) + " fetches and " + std::to_string(waited.size()) +
                  " waits");
        std::chrono::milliseconds total{0};
        for (const milliseconds w : waited) {
            check(w <= retry_after_limit,
                  std::string(c.what) + ": waited " + std::to_string(w.count()) + " ms");
            total += w;
        }
        check(total <= 4 * retry_after_limit,
              std::string(c.what) + ": waited " + std::to_string(total.count()) +
                  " ms in all, over the 40 s five tries may wait");
        ++walked;
    }
    check(walked == cases.size() && walked == 12,
          std::to_string(walked) + " cases walked, not every one");
    // **Any other 4xx is not tried again**, with a Retry-After or without.
    for (const int refusal : {400, 401, 403, 404, 413, 418, 428, 431}) {
        std::size_t calls = 0;
        int waits = 0;
        const auto fetch = [&](const std::string&) {
            ++calls;
            HttpResponse r;
            r.status = refusal;
            r.headers["retry-after"] = "1";
            return r;
        };
        check(fetch_with_retries(fetch, "u", 5, backoff, [&](milliseconds) { ++waits; })
                          .status == refusal &&
                  calls == 1 && waits == 0,
              "status " + std::to_string(refusal) + " is not tried again");
    }
}

// **A file held open for deletion, as a rename holds it, still opens.** This
// is the moment CI's `cannot open ...DEM.tif` came from, built rather than
// waited for: while a rename moves a fetched file into place it has the file
// open with DELETE access, sharing everything, and a reader that does not
// share deletion is refused. The handle here asks exactly that.
GLIDESLOPE_TEST(a_tile_held_open_for_deletion_as_a_rename_holds_it_is_still_read_whole) {
#if defined(_WIN32)
    const auto dir = scratch("held-for-deletion");
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / "tile.tif";
    const std::vector<std::uint8_t> body = small_tile();
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(body.data()),
                  static_cast<std::streamsize>(body.size()));
    }
    const HANDLE renaming =
        CreateFileW(path.c_str(), DELETE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(renaming != INVALID_HANDLE_VALUE, "the file is held open for deletion");
    std::string refused;
    bool read_whole = false;
    try {
        const glideslope::world::FileSource source(path);
        std::vector<std::uint8_t> got(static_cast<std::size_t>(source.size()));
        source.read(0, got);
        read_whole = got == body;
    } catch (const std::exception& e) {
        refused = e.what();
    }
    CloseHandle(renaming);
    check(refused.empty(), "while it is held for deletion, it opens; not: " + refused);
    check(read_whole, "and it reads whole");
#else
    glideslope::test::skip("POSIX has no sharing modes: a file held open by one "
                           "process cannot keep another from opening it");
#endif
}

// **A file put in place never replaces one already there**, on every
// platform: the second of two fetches of one file leaves the first's as it
// is, and its own bytes go nowhere - no temporary name is left beside it. A
// replaced file is what makes a name delete-pending on Windows (below).
GLIDESLOPE_TEST(a_file_put_in_place_never_replaces_one_already_there) {
    const auto dir = scratch("put-in-place");
    const std::filesystem::path path = dir / "tile.tif";
    const std::vector<std::uint8_t> first(1000, std::uint8_t{0x11});
    const std::vector<std::uint8_t> second(2000, std::uint8_t{0x22});
    const auto read = [&path] {
        const glideslope::world::FileSource source(path);
        std::vector<std::uint8_t> got(static_cast<std::size_t>(source.size()));
        source.read(0, got);
        return got;
    };
    check(glideslope::world::put_in_place(path, first), "the first is put in place");
    check(read() == first, "and is there whole");
    check(!glideslope::world::put_in_place(path, second),
          "the second finds it there and says so");
    check(read() == first, "and the first is left as it is");
    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        (void)entry;
        ++files;
    }
    check(files == 1, std::to_string(files) + " files beside it, not 1: a temporary "
                                              "name was left behind");
}

// **A cached file whose name is delete-pending is waited for, then fetched
// and read whole**, by every fetch of a cached download: a DEM tile, its
// water mask, and a file pinned by its hash. On Windows a file deleted, or
// replaced by a rename, while a handle to it is still open is delete-pending,
// and until that handle closes its name answers every look and every open
// with ERROR_ACCESS_DENIED - which std::filesystem::exists throws as `exists:
// Access is denied`, as CI saw of a DEM tile under 3200 threads at once.
//
// Built rather than waited for: the file is marked for deletion through a
// handle this test holds, the old way (FileDispositionInformation, not
// POSIX), so its name is certainly delete-pending; the fetch is started; the
// handle is closed only once the fetch has been refused and is waiting it
// out - transient_refusals_waited says so - and then the name is free, and
// the fetch must fetch the file and read it whole.
GLIDESLOPE_TEST(a_cached_file_whose_name_is_delete_pending_is_waited_for_then_fetched_and_read_whole) {
#if defined(_WIN32)
    constexpr DemCell cell{-34, 151};
    const std::vector<std::uint8_t> body = small_tile();
    const std::string md5 = glideslope::world::md5_hex(body);
    const std::string sha = glideslope::world::sha256_hex(body);
    struct Fetcher {
        std::string what;
        std::filesystem::path path;
        std::function<std::shared_ptr<const glideslope::world::ByteSource>(
            const glideslope::world::Fetch&)>
            fetch;
    };
    const auto cache = scratch("delete-pending");
    const std::filesystem::path tiles = cache / "copernicus-dem-30m";
    const std::string pinned = "egm2008-5.zip";
    const std::vector<Fetcher> fetchers = {
        {"a dem tile",
         tiles / (glideslope::world::dem_tile_name(DemDataset::glo30, cell) + ".tif"),
         [&](const glideslope::world::Fetch& fetch) {
             DownloadedTiles t(cache, fetch);
             return t.open(DemDataset::glo30, cell);
         }},
        {"a water mask",
         tiles / (glideslope::world::dem_water_mask_name(DemDataset::glo30, cell) + ".tif"),
         [&](const glideslope::world::Fetch& fetch) {
             DownloadedTiles t(cache, fetch);
             return t.open_water_mask(DemDataset::glo30, cell);
         }},
        {"a pinned file", cache / pinned,
         [&](const glideslope::world::Fetch& fetch)
             -> std::shared_ptr<const glideslope::world::ByteSource> {
             return std::make_shared<glideslope::world::FileSource>(
                 glideslope::world::fetch_pinned(cache, pinned,
                                                 "https://example.invalid/pinned", sha,
                                                 fetch));
         }},
    };
    int covered = 0;
    for (const Fetcher& f : fetchers) {
        std::filesystem::create_directories(f.path.parent_path());
        {
            std::ofstream out(f.path, std::ios::binary);
            out << "the old file, being deleted";
        }
        const HANDLE deleting =
            CreateFileW(f.path.c_str(), DELETE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(deleting != INVALID_HANDLE_VALUE, f.what + ": the old file is held");
        FILE_DISPOSITION_INFO disposition{};
        disposition.DeleteFile = TRUE;
        check(SetFileInformationByHandle(deleting, FileDispositionInfo, &disposition,
                                         sizeof disposition) != 0,
              f.what + ": the old file is marked for deletion");
        check(GetFileAttributesW(f.path.c_str()) == INVALID_FILE_ATTRIBUTES &&
                  GetLastError() == ERROR_ACCESS_DENIED,
              f.what + ": its name is delete-pending, refusing a look with "
                       "ERROR_ACCESS_DENIED");

        std::atomic<int> asked{0};
        const glideslope::world::Fetch fetch = [&](const std::string&) {
            ++asked;
            HttpResponse r;
            r.status = 200;
            r.body = body;
            r.headers["etag"] = "\"" + md5 + "\"";
            return r;
        };
        const std::uint64_t waited_before = glideslope::world::transient_refusals_waited();
        std::atomic<bool> finished{false};
        std::string refused;
        bool read_whole = false;
        std::thread fetching([&] {
            try {
                const auto source = f.fetch(fetch);
                std::vector<std::uint8_t> got(static_cast<std::size_t>(source->size()));
                source->read(0, got);
                read_whole = got == body;
            } catch (const std::exception& e) {
                refused = e.what();
            }
            finished = true;
        });
        // Until the fetch is waiting the refusal out, or has given up.
        while (glideslope::world::transient_refusals_waited() == waited_before &&
               !finished.load()) {
            std::this_thread::yield();
        }
        const bool was_waiting = !finished.load();
        CloseHandle(deleting);
        fetching.join();
        check(refused.empty(), f.what + ": fetched while its name was delete-pending; "
                                        "not: " + refused);
        check(was_waiting, f.what + ": the fetch waited while the name was refused");
        check(read_whole, f.what + ": and it reads whole");
        check(asked == 1, f.what + ": fetched once, when the name was free, not " +
                              std::to_string(asked.load()) + " times");
        ++covered;
    }
    check(covered == 3 && fetchers.size() == 3,
          std::to_string(covered) + " of the 3 fetches of a cached download covered");
#else
    glideslope::test::skip("POSIX has no delete-pending state: a deleted file's name "
                           "is free at once, whoever has the file open");
#endif
}

namespace {

std::vector<std::uint8_t> file_bytes(const std::filesystem::path& path) {
    const glideslope::world::FileSource source(path);
    std::vector<std::uint8_t> got(static_cast<std::size_t>(source.size()));
    source.read(0, got);
    return got;
}

void write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

#if defined(_WIN32)
// A file at `path`, marked for deletion the old way (FileDispositionInfo, not
// POSIX) through the handle returned: until that closes, the name is
// delete-pending, and the test checks it is.
HANDLE hold_delete_pending(const std::filesystem::path& path) {
    write_file(path, {'o', 'l', 'd'});
    const HANDLE h = CreateFileW(path.c_str(), DELETE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(h != INVALID_HANDLE_VALUE, "the old file is held");
    FILE_DISPOSITION_INFO disposition{};
    disposition.DeleteFile = TRUE;
    check(SetFileInformationByHandle(h, FileDispositionInfo, &disposition,
                                     sizeof disposition) != 0,
          "the old file is marked for deletion");
    check(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES &&
              GetLastError() == ERROR_ACCESS_DENIED,
          "its name is delete-pending, refusing a look with ERROR_ACCESS_DENIED");
    return h;
}

// Runs `call` on a thread of its own; once it is waiting out a refusal -
// transient_refusals_waited says so - or has finished, runs `let_go`. Says
// whether it was waiting when let go, and what it threw, if anything.
struct WaitedFor {
    bool was_waiting = false;
    std::string refused;
};
WaitedFor let_go_while_waiting(const std::function<void()>& call,
                               const std::function<void()>& let_go) {
    const std::uint64_t before = glideslope::world::transient_refusals_waited();
    std::atomic<bool> finished{false};
    WaitedFor result;
    std::thread calling([&] {
        try {
            call();
        } catch (const std::exception& e) {
            result.refused = e.what();
        }
        finished = true;
    });
    while (glideslope::world::transient_refusals_waited() == before && !finished.load()) {
        std::this_thread::yield();
    }
    result.was_waiting = !finished.load();
    let_go();
    calling.join();
    return result;
}
#else
// Refusals a test makes in place of the C library's calls.
int link_refusal = 0;
int exclusive_refusal = 0;
int refuse_link(const char*, const char*) {
    errno = link_refusal;
    return -1;
}
int refuse_exclusive(const char*, const char*) {
    errno = exclusive_refusal;
    return -1;
}
#endif

} // namespace

// **Where the filesystem has no hard links, a file is still moved into place,
// and not over one already there if the filesystem can refuse to.** link()
// is refused as vfat, exFAT, SMB and FUSE filesystems refuse it, with each
// of EPERM, EOPNOTSUPP, ENOTSUP and ENOSYS: the exclusive rename then moves
// the file, and refuses to move it over one already there. With the
// exclusive rename refused too - EINVAL, EOPNOTSUPP, ENOTSUP, ENOSYS, EPERM -
// plain rename() moves it, and replaces, which is harmless on POSIX. Any
// other refusal of link() is a failure, said.
GLIDESLOPE_TEST(where_the_filesystem_has_no_hard_links_a_file_is_still_moved_into_place) {
#if defined(_WIN32)
    glideslope::test::skip("Windows moves with MoveFileExW, which needs no hard links; "
                           "the move there is tested by the delete-pending tests");
#else
    using glideslope::world::move_into_place_unless_there;
    using glideslope::world::PosixMoves;
    const auto dir = scratch("no-hard-links");
    const std::filesystem::path path = dir / "tile.tif";
    const std::filesystem::path part = dir / "tile.tif.part";
    const std::vector<std::uint8_t> first(100, std::uint8_t{1});
    const std::vector<std::uint8_t> second(200, std::uint8_t{2});
    const std::vector<int> link_refusals = {EPERM, EOPNOTSUPP, ENOTSUP, ENOSYS};
    const std::vector<int> exclusive_refusals = {EINVAL, EPERM, EOPNOTSUPP, ENOTSUP,
                                                 ENOSYS};
    const PosixMoves& real = glideslope::world::posix_moves();
    int covered = 0;
    for (const int refused : link_refusals) {
        link_refusal = refused;
        const std::string why = std::string("link refused with ") + std::strerror(refused);
        const PosixMoves exclusive{refuse_link, real.rename_exclusive, real.rename};
        std::filesystem::remove_all(dir);
        write_file(part, first);
        check(move_into_place_unless_there(part, path, exclusive),
              why + ": the exclusive rename moves it");
        check(file_bytes(path) == first && !std::filesystem::exists(part),
              why + ": and it is there, whole, under its name alone");
        write_file(part, second);
        check(!move_into_place_unless_there(part, path, exclusive),
              why + ": a second is not moved over it");
        check(file_bytes(path) == first && std::filesystem::exists(part),
              why + ": the first is left, and the second where it was");
        ++covered;
        for (const int also : exclusive_refusals) {
            exclusive_refusal = also;
            const std::string both =
                why + ", the exclusive rename with " + std::strerror(also);
            const PosixMoves plain{refuse_link, refuse_exclusive, real.rename};
            std::filesystem::remove_all(dir);
            write_file(part, first);
            check(move_into_place_unless_there(part, path, plain),
                  both + ": rename moves it");
            check(file_bytes(path) == first && !std::filesystem::exists(part),
                  both + ": and it is there, whole");
            ++covered;
        }
    }
    link_refusal = EACCES;
    std::filesystem::remove_all(dir);
    write_file(part, first);
    std::string said;
    try {
        move_into_place_unless_there(part, path, {refuse_link, real.rename_exclusive,
                                                  real.rename});
    } catch (const glideslope::world::ByteSourceError& e) {
        said = e.what();
    }
    check(said.find("link") != std::string::npos && !std::filesystem::exists(path),
          "link refused with EACCES is a failure, said; not: \"" + said + "\"");
    ++covered;
    const std::size_t space = link_refusals.size() * (1 + exclusive_refusals.size()) + 1;
    check(static_cast<std::size_t>(covered) == space,
          std::to_string(covered) + " of " + std::to_string(space) + " refusals covered");
#endif
}

// **A file another program holds open without sharing is opened once it
// lets go.** On Windows an open that asks for what the holder does not share
// is refused with ERROR_SHARING_VIOLATION until the holder closes it: a
// scanner or an indexer can hold a tile so. Built: the file is held with no
// sharing at all, FileSource is started, and the file is let go only once
// FileSource is waiting the refusal out; it must then read the file whole.
GLIDESLOPE_TEST(a_file_another_holds_open_without_sharing_is_opened_once_it_lets_go) {
#if defined(_WIN32)
    const auto dir = scratch("held-unshared");
    const std::filesystem::path path = dir / "tile.tif";
    const std::vector<std::uint8_t> body = small_tile();
    write_file(path, body);
    const HANDLE holding = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(holding != INVALID_HANDLE_VALUE, "the file is held, sharing nothing");
    check(CreateFileW(path.c_str(), GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                      nullptr) == INVALID_HANDLE_VALUE &&
              GetLastError() == ERROR_SHARING_VIOLATION,
          "while it is held, an open is refused with ERROR_SHARING_VIOLATION");
    std::vector<std::uint8_t> got;
    const WaitedFor w =
        let_go_while_waiting([&] { got = file_bytes(path); }, [&] { CloseHandle(holding); });
    check(w.refused.empty(), "opened once let go; not: " + w.refused);
    check(w.was_waiting, "the open waited while the file was held");
    check(got == body, "and it reads whole");
#else
    glideslope::test::skip("POSIX has no sharing modes: a file held open by one "
                           "process cannot keep another from opening it");
#endif
}

// **A file put where a name is delete-pending waits for the name, then takes
// it.** On Windows a move onto a name whose file is delete-pending is
// refused until the last handle to that file closes. Built: the name is held
// delete-pending, put_in_place is started, and the name is let go only once
// the move is waiting; the new file must then be in place, whole.
GLIDESLOPE_TEST(a_file_put_where_a_name_is_delete_pending_waits_for_the_name_then_takes_it) {
#if defined(_WIN32)
    const auto dir = scratch("put-on-delete-pending");
    const std::filesystem::path path = dir / "tile.tif";
    const std::vector<std::uint8_t> body = small_tile();
    const HANDLE deleting = hold_delete_pending(path);
    bool put = false;
    const WaitedFor w =
        let_go_while_waiting([&] { put = glideslope::world::put_in_place(path, body); },
                             [&] { CloseHandle(deleting); });
    check(w.refused.empty(), "put once the name was free; not: " + w.refused);
    check(w.was_waiting, "the move waited while the name was delete-pending");
    check(put, "and says it put the file there");
    check(file_bytes(path) == body, "which is whole");
    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        (void)entry;
        ++files;
    }
    check(files == 1, std::to_string(files) + " files, not 1: a temporary name was left");
#else
    glideslope::test::skip("POSIX has no delete-pending state: a deleted file's name "
                           "is free at once, whoever has the file open");
#endif
}

// **A file held without sharing deletion while it is moved into place is
// moved once it is let go.** The move itself refuses too: a delete-pending
// name answers it that the name is taken, which the test above builds, but a
// file to be moved that another holds without sharing deletion - a scanner
// reading a fresh download - is refused with ERROR_SHARING_VIOLATION. Built:
// the file is held so, the move is started, and the file is let go only once
// the move is waiting; it must then be in place, whole.
GLIDESLOPE_TEST(a_file_held_unshared_while_it_is_moved_into_place_is_moved_once_let_go) {
#if defined(_WIN32)
    const auto dir = scratch("moved-while-held");
    const std::filesystem::path path = dir / "tile.tif";
    const std::filesystem::path part = dir / "tile.tif.part";
    const std::vector<std::uint8_t> body = small_tile();
    write_file(part, body);
    const HANDLE holding = CreateFileW(part.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(holding != INVALID_HANDLE_VALUE, "the file is held, not sharing deletion");
    check(!MoveFileExW(part.c_str(), path.c_str(), 0) &&
              GetLastError() == ERROR_SHARING_VIOLATION,
          "while it is held, a move is refused with ERROR_SHARING_VIOLATION");
    bool moved = false;
    const WaitedFor w = let_go_while_waiting(
        [&] { moved = glideslope::world::move_into_place_unless_there(part, path); },
        [&] { CloseHandle(holding); });
    check(w.refused.empty(), "moved once let go; not: " + w.refused);
    check(w.was_waiting, "the move waited while the file was held");
    check(moved && !std::filesystem::exists(part), "and says it moved it, which it did");
    check(file_bytes(path) == body, "and it is in place, whole");
#else
    glideslope::test::skip("POSIX has no sharing modes: a file held open by one "
                           "process cannot keep another from moving it");
#endif
}

// **A refusal that stays is given up on after transient_refusal_wait**, by
// each of the look, the open and the move, and said with the tries and the
// time it took: ERROR_ACCESS_DENIED is also what a file its ACL denies
// answers, and no wait changes that. Built with a name held delete-pending
// throughout. Each must take at least the stated wait, and the time each took
// is printed.
GLIDESLOPE_TEST(a_refusal_that_stays_is_given_up_on_after_its_stated_wait) {
#if defined(_WIN32)
    const auto dir = scratch("refused-throughout");
    const std::filesystem::path path = dir / "tile.tif";
    const HANDLE deleting = hold_delete_pending(path);
    const std::filesystem::path part = dir / "new.part";
    write_file(part, small_tile());
    const std::vector<std::pair<std::string, std::function<void()>>> calls = {
        {"the look", [&] { (void)glideslope::world::file_is_there(path); }},
        {"the open", [&] { (void)glideslope::world::FileSource(path); }},
        {"the move", [&] { (void)glideslope::world::move_into_place_unless_there(part, path); }},
    };
    int covered = 0;
    for (const auto& [what, call] : calls) {
        const auto start = std::chrono::steady_clock::now();
        std::string said;
        try {
            call();
        } catch (const glideslope::world::ByteSourceError& e) {
            said = e.what();
        }
        const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        std::printf("%s gave up after %lld ms: %s\n", what.c_str(),
                    static_cast<long long>(took.count()), said.c_str());
        check(!said.empty(), what + " gives up");
        check(took >= glideslope::world::transient_refusal_wait,
              what + " gave up after " + std::to_string(took.count()) +
                  " ms, before the stated wait");
        check(said.find(" ms") != std::string::npos && said.find("tries") != std::string::npos,
              what + " says how long and how often it asked: " + said);
        ++covered;
    }
    CloseHandle(deleting);
    check(covered == 3, std::to_string(covered) + " of the 3 calls that wait covered");
#else
    glideslope::test::skip("POSIX has no refusal that passes, so nothing waits");
#endif
}

// **Many at once, fetching and reading one tile.** Tests run in parallel and
// a flight and its terrain fetch the same tiles, so one process renames a
// fresh copy into place while another opens it. On Windows a file being
// renamed is held open for deletion, and a reader that does not share
// deletion could not open it then: CI saw `cannot open ...DEM.tif`.
//
// Each round starts with no file. Tile fetchers (DownloadedTiles, as the
// terrain fetches) and pinned fetchers (fetch_pinned, as the geoid is
// fetched, to the same path) all start at once on a barrier with readers
// that wait for the file to appear and open it that instant - the moment a
// rename has just put it there. Every one of them reads the whole file.
GLIDESLOPE_TEST(many_fetches_and_reads_of_one_tile_at_once_all_read_it_whole) {
    using glideslope::world::ByteSource;
    using glideslope::world::FileSource;
    constexpr int rounds = 200;
    constexpr int tile_fetchers = 4;
    constexpr int pinned_fetchers = 4;
    constexpr int readers = 8;
    constexpr int threads = tile_fetchers + pinned_fetchers + readers;
    constexpr int fetchers = tile_fetchers + pinned_fetchers;
    constexpr DemCell cell{-34, 151};

    // Big enough that writing it takes a while; a tile's shape at the front.
    std::vector<std::uint8_t> body = small_tile();
    body.resize(std::size_t{1} << 18, std::uint8_t{0x5a});
    const std::string md5 = glideslope::world::md5_hex(body);
    const std::string sha = glideslope::world::sha256_hex(body);
    const std::string name = glideslope::world::dem_tile_name(DemDataset::glo30, cell);
    const std::string url = glideslope::world::dem_tile_url(DemDataset::glo30, cell);
    // The network, standing in: its own answer each call, nothing shared.
    const glideslope::world::Fetch fetch = [&body, &md5](const std::string&) {
        HttpResponse r;
        r.status = 200;
        r.body = body;
        r.headers["etag"] = "\"" + md5 + "\"";
        return r;
    };

    const auto whole = [&body](const ByteSource& source) {
        if (source.size() != body.size()) {
            return false;
        }
        std::vector<std::uint8_t> got(body.size());
        source.read(0, got);
        return got == body;
    };

    std::atomic<int> tile_reads{0};
    std::atomic<int> pinned_reads{0};
    std::atomic<int> plain_reads{0};
    std::atomic<int> failures{0};
    std::string first_failure;
    std::mutex failure_mutex;
    const auto failed = [&](const std::string& why) {
        const std::lock_guard lock(failure_mutex);
        if (failures++ == 0) {
            first_failure = why;
        }
    };

    for (int round = 0; round < rounds; ++round) {
        const auto cache = scratch("many-at-once");
        const std::filesystem::path dir = cache / "copernicus-dem-30m";
        const std::filesystem::path path = dir / (name + ".tif");
        std::atomic<int> waiting{threads};
        std::atomic<int> fetched{0};
        const auto start_together = [&waiting] {
            --waiting;
            while (waiting.load() > 0) {
                std::this_thread::yield();
            }
        };
        std::vector<std::thread> running;
        for (int i = 0; i < threads; ++i) {
            running.emplace_back([&, i] {
                start_together();
                try {
                    if (i < tile_fetchers) {
                        DownloadedTiles tiles(cache, fetch);
                        const auto source = tiles.open(DemDataset::glo30, cell);
                        whole(*source) ? void(++tile_reads)
                                       : failed("a fetched tile was not whole");
                    } else if (i < fetchers) {
                        const auto kept = glideslope::world::fetch_pinned(
                            dir, name + ".tif", url, sha, fetch);
                        whole(FileSource(kept)) ? void(++pinned_reads)
                                                : failed("a pinned file was not whole");
                    } else {
                        // Opened the moment it appears. Once every fetcher
                        // has finished it is there, or something failed:
                        // the count is read before the look, so a fetcher
                        // that finishes between the two is not missed.
                        for (;;) {
                            const bool all_finished = fetched.load() == fetchers;
                            if (glideslope::world::file_is_there(path)) {
                                break;
                            }
                            if (all_finished) {
                                failed("no fetcher left the file in place");
                                return;
                            }
                            std::this_thread::yield();
                        }
                        whole(FileSource(path)) ? void(++plain_reads)
                                                : failed("a file read was not whole");
                    }
                } catch (const std::exception& e) {
                    failed(e.what());
                }
                if (i < fetchers) {
                    ++fetched;
                }
            });
        }
        for (std::thread& t : running) {
            t.join();
        }
    }
    check(failures == 0, std::to_string(failures.load()) + " of " +
                             std::to_string(rounds * threads) +
                             " threads failed to fetch or read the file whole; the "
                             "first: " +
                             first_failure);
    check(tile_reads == rounds * tile_fetchers,
          std::to_string(tile_reads.load()) + " tile fetches read the tile whole, of " +
              std::to_string(rounds * tile_fetchers));
    check(pinned_reads == rounds * pinned_fetchers,
          std::to_string(pinned_reads.load()) + " pinned fetches read it whole, of " +
              std::to_string(rounds * pinned_fetchers));
    check(plain_reads == rounds * readers,
          std::to_string(plain_reads.load()) + " readers read it whole, of " +
              std::to_string(rounds * readers));
}
