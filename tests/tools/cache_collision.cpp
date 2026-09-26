// glideslope_cache_collision - programs writing one Cesium cache at once, made
// to collide, and a count of what they stored.
//
//   glideslope_cache_collision hold CACHEFILE DIR NAME...
//   glideslope_cache_collision write CACHEFILE DIR NAME COUNT
//   glideslope_cache_collision count CACHEFILE NAME COUNT
//   glideslope_cache_collision pause CACHEFILE REST_MS
//
// Every cache is opened as the client opens it, by gfx::open_cesium_cache.
// The programs signal each other with empty files in DIR, and nothing waits
// for a time: each step waits for the files that say the step before it has
// happened.
//
// **Nothing waits on a program that has failed.** hold and write, failing,
// say NAME.failed (hold.failed for the holder) on the way out, and every
// wait gives up the moment any .failed file is in DIR - so one program's
// failure ends all three at once, rather than leaving the others waiting on
// a signal that will never come.
//
// **write** opens the cache, stores NAME-seed and reads it back - a cache hit,
// which is what left SqliteCache holding a stale read transaction - and says
// NAME.ready. When DIR/go appears it first checks, with a connection of its
// own that does not wait, that the file is held by another writer; then it
// stores NAME-0 to NAME-(COUNT-1), reading the one before back between each
// store. The first store meets the held file, and waits for it: a thread
// watching the cache's count of waits says NAME.waiting the moment it does.
// Says how many it stored and how many times it waited; exits 0 only if it
// stored and read back all of them and its first store waited, which is to
// say it collided.
//
// **hold** makes the cache, then waits for every NAME's .ready. It then takes
// the file's write lock with a connection of its own, writes a row of its own
// - so every read made before now is stale - and says go. It lets go only
// when every NAME is waiting for it, or is past its first store without
// having waited - so a cache that refuses rather than waits is not waited for
// - and exits 0.
//
// **pause** shows a program that holds the file and never lets go costs one
// wait, not one a call. It opens the cache, left alone for REST_MS after a
// call that could not get the file, and holds the file with a connection of
// its own. Then, each call timed on the steady clock and the cache's count
// of waits read around it:
//   1. a store waits, once, for no more than cesium_cache_wait and a half,
//      and is not stored;
//   2. three more stores are skipped without waiting - no wait counted, each
//      well under the wait;
//   3. the file is let go, and a store made at once, still in the pause, is
//      skipped too - so it is the pause that skipped them, not the lock;
//   4. once REST_MS has passed since the first store gave up, a store is
//      stored, and read back.
// Exits 0 only if every step held; says which did not.
//
// **count** reads NAME-seed and NAME-0 to NAME-(COUNT-1) back and says how
// many of the COUNT + 1 it found; exits 0 only if it found them all.
//
// Exits 2 on bad arguments.

#include "gfx/cesium_cache.hpp"
#include "gfx/terrain_tiles.hpp"

#include <CesiumAsync/CacheItem.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/ICacheDatabase.h>
#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;

int refuse(const char* why) {
    std::fprintf(stderr, "glideslope_cache_collision: %s\n", why);
    return 2;
}

void say(const fs::path& dir, const std::string& what) {
    std::ofstream(dir / what).close();
}

// Throws if another program has said it failed.
void unless_failed(const fs::path& dir) {
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".failed") {
            throw std::runtime_error("gave up waiting: " + entry.path().filename().string());
        }
    }
}

void wait_for(const fs::path& dir, const std::string& what) {
    while (!fs::exists(dir / what)) {
        unless_failed(dir);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool store(CesiumAsync::ICacheDatabase& cache, const std::string& key) {
    const std::string body = "the response for " + key;
    const auto bytes = std::as_bytes(std::span(body.data(), body.size()));
    return cache.storeEntry(key, std::time(nullptr) + 86400, "https://example.invalid/" + key,
                            "GET", CesiumAsync::HttpHeaders{}, 200,
                            CesiumAsync::HttpHeaders{{"Content-Type", "text/plain"}}, bytes);
}

bool holds(CesiumAsync::ICacheDatabase& cache, const std::string& key) {
    const std::optional<CesiumAsync::CacheItem> item = cache.getEntry(key);
    if (!item) {
        return false;
    }
    const std::string body = "the response for " + key;
    const auto& data = item->cacheResponse.data;
    return std::string(reinterpret_cast<const char*>(data.data()), data.size()) == body;
}

// A connection of the test's own, which never waits.
sqlite3* open_raw(const fs::path& file) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(file.string().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) !=
        SQLITE_OK) {
        std::fprintf(stderr, "glideslope_cache_collision: cannot open %s\n",
                     file.string().c_str());
        sqlite3_close(db);
        return nullptr;
    }
    sqlite3_busy_timeout(db, 0);
    return db;
}

int hold(const fs::path& file, const fs::path& dir, const std::vector<std::string>& names) {
    {
        // The file, its table and its WAL, made before anyone collides on it.
        const auto made = glideslope::gfx::open_cesium_cache(file);
    }
    say(dir, "made");
    for (const std::string& name : names) {
        wait_for(dir, name + ".ready");
    }
    sqlite3* db = open_raw(file);
    if (db == nullptr) {
        return 1;
    }
    char* error = nullptr;
    if (sqlite3_exec(db,
                     "BEGIN IMMEDIATE; CREATE TABLE IF NOT EXISTS held(at INTEGER); "
                     "INSERT INTO held VALUES (1);",
                     nullptr, nullptr, &error) != SQLITE_OK) {
        std::fprintf(stderr, "glideslope_cache_collision: could not hold %s: %s\n",
                     file.string().c_str(), error != nullptr ? error : "?");
        sqlite3_free(error);
        sqlite3_close(db);
        return 1;
    }
    say(dir, "go");
    for (const std::string& name : names) {
        while (!fs::exists(dir / (name + ".waiting")) &&
               !fs::exists(dir / (name + ".first-done"))) {
            try {
                unless_failed(dir);
            } catch (const std::exception&) {
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    const int rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    std::fprintf(stderr, "glideslope_cache_collision: held %s until %zu writers met it\n",
                 file.string().c_str(), names.size());
    return rc == SQLITE_OK ? 0 : 1;
}

int write(const fs::path& file, const fs::path& dir, const std::string& name, int count) {
    wait_for(dir, "made");
    const auto cache = glideslope::gfx::open_cesium_cache(file);
    if (!store(*cache, name + "-seed") || !holds(*cache, name + "-seed")) {
        std::fprintf(stderr, "glideslope_cache_collision: %s could not store its seed\n",
                     name.c_str());
        return 1;
    }
    say(dir, name + ".ready");
    wait_for(dir, "go");

    // The file is held: a write that does not wait is refused.
    sqlite3* raw = open_raw(file);
    if (raw == nullptr) {
        return 1;
    }
    const int tried = sqlite3_exec(raw, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
    sqlite3_exec(raw, "ROLLBACK;", nullptr, nullptr, nullptr);
    sqlite3_close(raw);
    if (tried != SQLITE_BUSY) {
        std::fprintf(stderr,
                     "glideslope_cache_collision: %s found the file not held (%d), so "
                     "nothing would collide\n",
                     name.c_str(), tried);
        say(dir, name + ".first-done");
        return 1;
    }

    // Says NAME.waiting the moment the cache waits.
    // Only a wait from here on counts: the seed may have waited for the
    // other writer's.
    const std::uint64_t before = glideslope::gfx::cesium_cache_waits();
    std::uint64_t first_waits = 0;
    std::atomic<bool> first_done{false};
    std::thread watcher([&] {
        while (!first_done.load()) {
            if (glideslope::gfx::cesium_cache_waits() > before) {
                say(dir, name + ".waiting");
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    int stored = 0;
    int found = 0;
    for (int i = 0; i < count; ++i) {
        const std::string key = name + "-" + std::to_string(i);
        if (store(*cache, key)) {
            ++stored;
        }
        if (i == 0) {
            first_done = true;
            watcher.join();
            first_waits = glideslope::gfx::cesium_cache_waits() - before;
            say(dir, name + ".first-done"); // waited or not, the holder may let go
        }
        if (holds(*cache, key)) {
            ++found;
        }
    }
    const std::uint64_t waits = glideslope::gfx::cesium_cache_waits() - before;
    std::fprintf(stderr,
                 "glideslope_cache_collision: %s stored %d of %d, read back %d, and waited "
                 "for another writer %llu times, the first store %llu\n",
                 name.c_str(), stored, count, found, static_cast<unsigned long long>(waits),
                 static_cast<unsigned long long>(first_waits));
    return stored == count && found == count && first_waits > 0 ? 0 : 1;
}

int tally(const fs::path& file, const std::string& name, int count) {
    const auto cache = glideslope::gfx::open_cesium_cache(file);
    int found = holds(*cache, name + "-seed") ? 1 : 0;
    for (int i = 0; i < count; ++i) {
        if (holds(*cache, name + "-" + std::to_string(i))) {
            ++found;
        }
    }
    std::fprintf(stderr, "glideslope_cache_collision: %s has %d of its %d entries\n",
                 name.c_str(), found, count + 1);
    return found == count + 1 ? 0 : 1;
}

int pause(const fs::path& file, std::chrono::milliseconds rest) {
    using clock = std::chrono::steady_clock;
    // A fresh file, so nothing found was stored by an earlier run.
    for (const char* end : {"", "-wal", "-shm"}) {
        fs::remove(file.string() + end);
    }
    const auto cache = glideslope::gfx::open_cesium_cache(file, rest);
    sqlite3* holder = open_raw(file);
    if (holder == nullptr) {
        return 1;
    }
    if (sqlite3_exec(holder, "BEGIN IMMEDIATE; CREATE TABLE IF NOT EXISTS held(at INTEGER); "
                             "INSERT INTO held VALUES (1);",
                     nullptr, nullptr, nullptr) != SQLITE_OK) {
        sqlite3_close(holder);
        std::fprintf(stderr, "glideslope_cache_collision: could not hold %s\n",
                     file.string().c_str());
        return 1;
    }
    int wrong = 0;
    const auto check = [&wrong](bool ok, const char* what) {
        std::fprintf(stderr, "glideslope_cache_collision: %s %s\n", ok ? "yes:" : "NO: ", what);
        if (!ok) {
            ++wrong;
        }
    };
    const auto ms = [](clock::duration d) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    };
    const auto wait = glideslope::gfx::cesium_cache_wait;
    const auto well_under = wait / 4;

    // 1. One wait, bounded, and nothing stored.
    std::uint64_t waits = glideslope::gfx::cesium_cache_waits();
    auto t0 = clock::now();
    bool stored = store(*cache, "held-0");
    const auto gave_up = clock::now();
    std::fprintf(stderr, "glideslope_cache_collision: the first store took %lld ms\n",
                 static_cast<long long>(ms(gave_up - t0)));
    check(!stored, "the first store, meeting the held file, was not stored");
    check(glideslope::gfx::cesium_cache_waits() == waits + 1, "it waited, once");
    check(gave_up - t0 >= wait && gave_up - t0 <= wait + wait / 2,
          "for the wait and no more than half as long again");

    // 2. Within the pause: skipped, and no waiting.
    for (int i = 1; i <= 3; ++i) {
        waits = glideslope::gfx::cesium_cache_waits();
        t0 = clock::now();
        stored = store(*cache, "held-" + std::to_string(i));
        const auto took = clock::now() - t0;
        std::fprintf(stderr, "glideslope_cache_collision: store %d in the pause took %lld ms\n",
                     i, static_cast<long long>(ms(took)));
        check(!stored && glideslope::gfx::cesium_cache_waits() == waits && took < well_under,
              "a store in the pause was skipped without waiting");
    }

    // 3. Let go: still the pause, so still skipped.
    const int released = sqlite3_exec(holder, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(holder);
    check(released == SQLITE_OK, "the holder let go");
    waits = glideslope::gfx::cesium_cache_waits();
    stored = store(*cache, "free-in-pause");
    check(clock::now() - gave_up < rest, "(this store was made inside the pause)");
    check(!stored && glideslope::gfx::cesium_cache_waits() == waits,
          "a store in the pause, the file free, was skipped too");

    // 4. After the pause: stored again.
    std::this_thread::sleep_until(gave_up + rest);
    stored = store(*cache, "after-pause");
    check(stored && holds(*cache, "after-pause"), "after the pause a store is stored again");
    check(!holds(*cache, "free-in-pause"), "and what was skipped is not there");
    return wrong == 0 ? 0 : 1;
}

int count_of(const char* text) {
    try {
        const int n = std::stoi(text);
        return n > 0 ? n : -1;
    } catch (const std::exception&) {
        return -1;
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    // Cesium Native's log - a refused write among it - where the test reads.
    glideslope::gfx::log_to_standard_error();
    // Says who failed, for the others to stop waiting on it.
    const auto failing = [](const fs::path& dir, const std::string& who, int rc) {
        if (rc != 0) {
            say(dir, who + ".failed");
        }
        return rc;
    };
    try {
        if (args.size() >= 4 && args[0] == "hold") {
            try {
                return failing(args[2], "hold",
                               hold(args[1], args[2],
                                    std::vector<std::string>(args.begin() + 3, args.end())));
            } catch (const std::exception&) {
                failing(args[2], "hold", 1);
                throw;
            }
        }
        if (args.size() == 5 && args[0] == "write") {
            const int n = count_of(args[4].c_str());
            if (n < 0) {
                return failing(args[2], args[3], refuse("COUNT must be a positive number"));
            }
            try {
                return failing(args[2], args[3], write(args[1], args[2], args[3], n));
            } catch (const std::exception&) {
                failing(args[2], args[3], 1);
                throw;
            }
        }
        if (args.size() == 3 && args[0] == "pause") {
            const int n = count_of(args[2].c_str());
            return n < 0 ? refuse("REST_MS must be a positive number")
                         : pause(args[1], std::chrono::milliseconds(n));
        }
        if (args.size() == 4 && args[0] == "count") {
            const int n = count_of(args[3].c_str());
            return n < 0 ? refuse("COUNT must be a positive number") : tally(args[1], args[2], n);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "glideslope_cache_collision: %s\n", e.what());
        return 1;
    }
    return refuse("usage: glideslope_cache_collision hold CACHEFILE DIR NAME... | "
                  "write CACHEFILE DIR NAME COUNT | count CACHEFILE NAME COUNT | "
                  "pause CACHEFILE REST_MS");
}
