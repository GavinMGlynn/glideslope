#include "harness.hpp"
#include "tiff_writer.hpp"

#include "world/dem.hpp"
#include "world/digest.hpp"
#include "world/download.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <string>
#include <vector>

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
