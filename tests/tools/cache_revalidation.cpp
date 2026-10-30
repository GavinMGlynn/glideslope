// glideslope_cache_revalidation - a cached tile gone stale, asked for again
// when the network gives no answer, is answered as a failure and does not
// end the program.
//
//   glideslope_cache_revalidation DIR
//   glideslope_cache_revalidation stale CACHEFILE URL...
//
// **The crash, built on purpose.** The client with the window segfaulted as
// it closed: its imagery, cached and gone stale, was being asked for again
// when its transfers were given up, and Cesium Native's cache read the
// status of an answer that had no response (gfx/fetches.hpp). That needed
// a stale entry and a transfer given up at the right moment, so it was seen
// once in many runs. Here both are made: each entry is stored already
// expired, and each fetch fails every time.
//
// **Every case, counted.** The chain is the client's own -
// gfx::cached_fetches over gfx::platform_fetches, the cache opened by
// gfx::open_cesium_cache - and it is asked once for each of
//
//   - how the network fails: **refused** (a loopback port nothing listens
//     on) and **given up** (the terrain's closing flag already raised, as at
//     the crash);
//   - what the cache holds: **nothing** (a miss, which never crashed and is
//     here so that it stays a failure), a stale entry with an **ETag** (asked
//     again with If-None-Match), one with **Last-Modified**
//     (If-Modified-Since) and one with **neither** (asked again with no
//     condition at all, down the same path).
//
// For each: the network alone must answer nothing - so that the case tests
// what it says - and the cached fetch must then come back with a response,
// gfx::unanswered_fetch_status, and leave the stale entry as it was (nothing
// stored for a miss). The count of cases run must be the count there are.
// Exits 0 only if every case held; says which did not. Before the fix the
// first stale case ended the program with a segfault.
//
// **stale** stores each URL in CACHEFILE, opened as the client opens it, as
// a tile cached an hour ago with an ETag and gone stale since - for a test
// of the client itself, whose next run asks for each again
// (tests/cmake/imagery_stale_refused.cmake). Exits 0 if it stored them all.
//
// Exits 2 on bad arguments.

#include "gfx/cesium_cache.hpp"
#include "gfx/fetches.hpp"
#include "gfx/terrain_tiles.hpp"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/ICacheDatabase.h>
#include <CesiumAsync/ITaskProcessor.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Each task on a thread of its own, every one joined before the program ends.
class Threads final : public CesiumAsync::ITaskProcessor {
public:
    // Joined outside the lock, so that a task still starting another can.
    ~Threads() override {
        for (;;) {
            std::vector<std::thread> started;
            {
                const std::lock_guard lock(mutex_);
                started.swap(threads_);
            }
            if (started.empty()) {
                return;
            }
            for (std::thread& t : started) {
                t.join();
            }
        }
    }
    void startTask(std::function<void()> task) override {
        const std::lock_guard lock(mutex_);
        threads_.emplace_back(std::move(task));
    }

private:
    std::mutex mutex_;
    std::vector<std::thread> threads_;
};

// Nothing listens on port 1 of the loopback; the network alone is asked
// first, to show that nothing answered.
const std::string url = "http://127.0.0.1:1/wmts/7/88/234.jpg";
const std::string stale_body = "the tile as it was cached";

struct Failure {
    const char* name;
    bool closing;
};
struct Entry {
    const char* name;
    bool stored;
    const char* header; // the validator it carries, or none
    const char* value;
};

const Failure failures[] = {{"refused", false}, {"given up", true}};
const Entry entries[] = {
    {"nothing cached", false, nullptr, nullptr},
    {"stale, with an ETag", true, "ETag", "\"seven-88-234\""},
    {"stale, with Last-Modified", true, "Last-Modified", "Tue, 01 Aug 2017 14:47:42 GMT"},
    {"stale, with neither", true, nullptr, nullptr},
};

std::shared_ptr<CesiumAsync::IAssetRequest>
fetch(CesiumAsync::IAssetAccessor& accessor, const CesiumAsync::AsyncSystem& async) {
    return accessor.get(async, url, {}).wait();
}

// One case; true if it held.
bool run(const fs::path& dir, const Failure& failure, const Entry& entry,
         const CesiumAsync::AsyncSystem& async) {
    const std::string name = std::string(failure.name) + ", " + entry.name;
    const auto closing = std::make_shared<std::atomic<bool>>(failure.closing);
    const auto network = glideslope::gfx::platform_fetches(closing);
    if (fetch(*network, async)->response() != nullptr) {
        std::printf("%s: the network answered, so nothing was tested\n", name.c_str());
        return false;
    }

    const fs::path file = dir / ("revalidation-" + std::to_string(std::hash<std::string>{}(name)) +
                                 ".sqlite");
    for (const char* tail : {"", "-wal", "-shm"}) {
        fs::remove(fs::path(file.string() + tail));
    }
    const auto cache = glideslope::gfx::open_cesium_cache(file);
    const auto bytes = std::as_bytes(std::span(stale_body.data(), stale_body.size()));
    if (entry.stored) {
        CesiumAsync::HttpHeaders headers{{"Content-Type", "image/jpeg"},
                                         {"Cache-Control", "max-age=604800"}};
        if (entry.header != nullptr) {
            headers[entry.header] = entry.value;
        }
        // Expired an hour ago, so the cache asks for it again.
        if (!cache->storeEntry(url, std::time(nullptr) - 3600, url, "GET", {}, 200,
                               headers, bytes)) {
            std::printf("%s: the stale entry was not stored\n", name.c_str());
            return false;
        }
    }

    const auto fetches = glideslope::gfx::cached_fetches(network, cache);
    const auto answer = fetch(*fetches, async);
    bool held = true;
    if (answer == nullptr || answer->response() == nullptr) {
        std::printf("%s: came back with no response\n", name.c_str());
        held = false;
    } else if (answer->response()->statusCode() != glideslope::gfx::unanswered_fetch_status) {
        std::printf("%s: came back %d, not %d\n", name.c_str(),
                    answer->response()->statusCode(), glideslope::gfx::unanswered_fetch_status);
        held = false;
    }
    const auto kept = cache->getEntry(url);
    if (!entry.stored && kept.has_value()) {
        std::printf("%s: a failure was stored\n", name.c_str());
        held = false;
    }
    if (entry.stored) {
        const bool same = kept.has_value() && kept->cacheResponse.statusCode == 200 &&
                          kept->cacheResponse.data.size() == stale_body.size() &&
                          std::equal(kept->cacheResponse.data.begin(),
                                     kept->cacheResponse.data.end(), bytes.begin());
        if (!same) {
            std::printf("%s: the stale entry was not left as it was\n", name.c_str());
            held = false;
        }
    }
    std::printf("%s: %s\n", name.c_str(), held ? "answered as a failure" : "FAILED");
    return held;
}

} // namespace

// Stores each of `urls` in `file` gone stale; the count it stored.
int store_stale(const fs::path& file, const std::vector<std::string>& urls) {
    const auto cache = glideslope::gfx::open_cesium_cache(file);
    const auto bytes = std::as_bytes(std::span(stale_body.data(), stale_body.size()));
    const CesiumAsync::HttpHeaders headers{{"Content-Type", "image/jpeg"},
                                           {"Cache-Control", "max-age=604800"},
                                           {"ETag", "\"stale\""}};
    int stored = 0;
    for (const std::string& u : urls) {
        stored += cache->storeEntry(u, std::time(nullptr) - 3600, u, "GET", {}, 200,
                                    headers, bytes)
                      ? 1
                      : 0;
    }
    return stored;
}

int main(int argc, char** argv) {
    if (argc >= 4 && std::string(argv[1]) == "stale") {
        const std::vector<std::string> urls(argv + 3, argv + argc);
        const int stored = store_stale(argv[2], urls);
        std::printf("stored %d of %zu tiles, stale\n", stored, urls.size());
        return stored == static_cast<int>(urls.size()) ? 0 : 1;
    }
    if (argc != 2) {
        std::fprintf(stderr, "usage: glideslope_cache_revalidation DIR | "
                             "stale CACHEFILE URL...\n");
        return 2;
    }
    glideslope::gfx::log_to_standard_error();
    const fs::path dir = argv[1];
    fs::create_directories(dir);
    int held = 0;
    int ran = 0;
    {
        const CesiumAsync::AsyncSystem async(std::make_shared<Threads>());
        for (const Failure& failure : failures) {
            for (const Entry& entry : entries) {
                ++ran;
                held += run(dir, failure, entry, async) ? 1 : 0;
            }
        }
    }
    constexpr int cases = static_cast<int>(std::size(failures) * std::size(entries));
    std::printf("%d of %d cases run, %d held\n", ran, cases, held);
    return ran == cases && held == cases ? 0 : 1;
}
