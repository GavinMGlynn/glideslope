#include "harness.hpp"
#include "tiff_writer.hpp"

#include "world/byte_source.hpp"
#include "world/dem.hpp"
#include "world/digest.hpp"
#include "world/download.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <iterator>
#include <mutex>
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
