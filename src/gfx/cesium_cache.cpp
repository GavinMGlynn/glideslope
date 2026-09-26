#include "gfx/cesium_cache.hpp"

#include <CesiumAsync/CacheItem.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/ICacheDatabase.h>
#include <CesiumAsync/SqliteCache.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <span>
#include <string>
#include <thread>

namespace glideslope::gfx {

namespace {

std::atomic<std::uint64_t> waits{0};

// When the wait for the lock now being waited for began. SQLite calls the
// busy handler on the thread running the statement, and counts its calls for
// one lock from zero, so the start is this thread's own.
thread_local std::chrono::steady_clock::time_point wait_began;

// SQLite's busy handler: called when the file is locked by another writer,
// `count` times before for this same lock. Nonzero means try again.
int wait_for_the_other_writer(void*, int count) {
    const auto now = std::chrono::steady_clock::now();
    if (count == 0) {
        wait_began = now;
        waits.fetch_add(1, std::memory_order_relaxed);
    }
    if (now - wait_began >= cesium_cache_wait) {
        return 0;
    }
    // A writer holds the file for a statement, so the first tries are soon.
    std::this_thread::sleep_for(std::chrono::milliseconds(count < 10 ? 1 : 5));
    return 1;
}

// **The connection Cesium Native opens is handed to this as it is opened**,
// by SQLite's automatic extension - the one way in to a connection opened
// inside code that is not ours. It acts only on a thread that is opening a
// cache here, or calling into one (which may open its file again: SqliteCache
// deletes and reopens a corrupt file), and leaves every other connection in
// the process alone.
thread_local bool ours = false;
thread_local sqlite3* opened = nullptr;

int on_open(sqlite3* db, char** /*error*/, const sqlite3_api_routines* /*api*/) {
    if (ours) {
        sqlite3_busy_handler(db, &wait_for_the_other_writer, nullptr);
        opened = db;
    }
    return SQLITE_OK;
}

void watch_opens() {
    static std::once_flag once;
    std::call_once(once, [] {
        // SQLite takes every entry point as a void function and calls it with
        // the arguments above: its documented form.
        const int rc = sqlite3_auto_extension(reinterpret_cast<void (*)()>(&on_open));
        if (rc != SQLITE_OK) {
            throw std::runtime_error(std::string("SQLite would not watch the Cesium "
                                                 "cache's connection open: ") +
                                     sqlite3_errstr(rc));
        }
    });
}

// Marks this thread as ours for as long as it lives, and says afterwards
// which connection, if any, was opened meanwhile.
class Ours {
public:
    Ours() {
        ours = true;
        opened = nullptr;
    }
    ~Ours() { ours = false; }
    Ours(const Ours&) = delete;
    Ours& operator=(const Ours&) = delete;
    sqlite3* opened_meanwhile() const { return opened; }
};

// SqliteCache, with nothing left holding a transaction between calls.
class SharedCache final : public CesiumAsync::ICacheDatabase {
public:
    SharedCache(const std::filesystem::path& file, std::chrono::milliseconds rest)
        : rest_(rest) {
        const Ours marked;
        cache_ = std::make_unique<CesiumAsync::SqliteCache>(spdlog::default_logger(),
                                                            file.string());
        db_ = marked.opened_meanwhile();
        // Without it the cache would open and quietly refuse a second writer
        // again - an SQLite built without automatic extensions, say.
        if (db_ == nullptr) {
            throw std::runtime_error("the Cesium cache " + file.string() +
                                     " was opened without being seen opening");
        }
    }

private:
    // **Each call is one write transaction, taken before the call and let go
    // after it.** Taking it first is what lets the call wait for another
    // program's write. SqliteCache's own statements cannot: `getEntry` steps
    // its lookup and then updates the entry's last-used time while the lookup
    // is still open, so the update needs a write lock from inside a read
    // transaction - and SQLite refuses that at once if another program holds
    // the lock or has written since the read began, without calling the busy
    // handler, because waiting there could deadlock. "database is locked" at
    // SqliteCache.cpp:422, and the hit is reported as a miss. `BEGIN
    // IMMEDIATE` asks for the write lock with no transaction open, which is
    // where SQLite does wait.
    //
    // And none of its statements is left part-way through: `getEntry` stops
    // its lookup at the row it wants and never finishes it, which would keep
    // a transaction open until the next lookup. Resetting ends it. What the
    // call returned has been copied out of the statement by then, and
    // SqliteCache resets each statement before it uses it again.
    //
    // **A program that holds the file and never lets go costs one wait, not
    // one per tile.** When the lock is not had within cesium_cache_wait, the
    // call is not made - a lookup is a miss, a store is not stored - and for
    // cesium_cache_rest after it every call is skipped the same way without
    // waiting at all; then the cache is tried again.
    template <typename Call, typename Skipped>
    auto call(Call&& what, Skipped skipped) const -> decltype(what()) {
        const std::lock_guard<std::mutex> one_at_a_time(mutex_);
        const auto now = std::chrono::steady_clock::now();
        if (now < resting_until_) {
            return skipped;
        }
        const Ours marked;
        const int began = sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
        if (began != SQLITE_OK) {
            if (began == SQLITE_BUSY) {
                resting_until_ = std::chrono::steady_clock::now() + rest_;
            }
            SPDLOG_WARN("the Cesium cache was not taken ({}); {}", sqlite3_errstr(began),
                        began == SQLITE_BUSY ? "left alone for a while" : "this call skipped");
            return skipped;
        }
        auto result = what();
        if (sqlite3* reopened = marked.opened_meanwhile(); reopened != nullptr) {
            db_ = reopened;
        }
        for (sqlite3_stmt* s = sqlite3_next_stmt(db_, nullptr); s != nullptr;
             s = sqlite3_next_stmt(db_, s)) {
            if (sqlite3_stmt_busy(s) != 0) {
                sqlite3_reset(s);
            }
        }
        // A file SqliteCache finds corrupt it deletes and opens again, inside
        // the call, and the transaction went with the old connection.
        if (sqlite3_get_autocommit(db_) == 0) {
            const int committed = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
            if (committed != SQLITE_OK) {
                SPDLOG_WARN("the Cesium cache's write was not committed ({}), and is undone",
                            sqlite3_errstr(committed));
                sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
                return skipped;
            }
        }
        return result;
    }

public:
    std::optional<CesiumAsync::CacheItem> getEntry(const std::string& key) const override {
        return call([&] { return cache_->getEntry(key); },
                    std::optional<CesiumAsync::CacheItem>());
    }

    bool storeEntry(const std::string& key, std::time_t expiryTime, const std::string& url,
                    const std::string& requestMethod,
                    const CesiumAsync::HttpHeaders& requestHeaders, uint16_t statusCode,
                    const CesiumAsync::HttpHeaders& responseHeaders,
                    const std::span<const std::byte>& responseData) override {
        return call([&] {
            return cache_->storeEntry(key, expiryTime, url, requestMethod, requestHeaders,
                                      statusCode, responseHeaders, responseData);
        }, false);
    }

    bool prune() override {
        return call([&] { return cache_->prune(); }, false);
    }

    bool clearAll() override {
        return call([&] { return cache_->clearAll(); }, false);
    }

private:
    std::unique_ptr<CesiumAsync::SqliteCache> cache_;
    mutable sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;
    // Until when every call is skipped, after one that could not get the lock.
    mutable std::chrono::steady_clock::time_point resting_until_{};
    std::chrono::milliseconds rest_;
};

} // namespace

std::shared_ptr<CesiumAsync::ICacheDatabase>
open_cesium_cache(const std::filesystem::path& file, std::chrono::milliseconds rest) {
    watch_opens();
    return std::make_shared<SharedCache>(file, rest);
}

std::uint64_t cesium_cache_waits() {
    return waits.load(std::memory_order_relaxed);
}

} // namespace glideslope::gfx
