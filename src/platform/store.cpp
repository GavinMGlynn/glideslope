#include "platform/store.hpp"

#include <sqlite3.h>

#include <utility>

namespace glideslope::platform {
namespace {

void must(sqlite3* db, int rc, const char* what) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        const char* said = db != nullptr ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
        throw StoreError(std::string(what) + ": " + (said != nullptr ? said : "?"));
    }
}

} // namespace

Store::Store(const std::filesystem::path& file) : file_(file) {
    const int rc = sqlite3_open(file.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        const std::string said = db_ != nullptr ? sqlite3_errmsg(db_) : "cannot open";
        sqlite3_close(db_);
        db_ = nullptr;
        throw StoreError("cannot open " + file.string() + ": " + said);
    }
    must(db_,
         sqlite3_exec(db_,
                      "CREATE TABLE IF NOT EXISTS kept ("
                      "  name TEXT PRIMARY KEY NOT NULL,"
                      "  value TEXT NOT NULL)",
                      nullptr, nullptr, nullptr),
         "cannot make the table");
}

Store::~Store() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

Store::Store(Store&& other) noexcept
    : file_(std::move(other.file_)), db_(std::exchange(other.db_, nullptr)) {}

Store& Store::operator=(Store&& other) noexcept {
    if (this != &other) {
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
        file_ = std::move(other.file_);
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

std::optional<std::string> Store::get(std::string_view name) const {
    sqlite3_stmt* statement = nullptr;
    must(db_,
         sqlite3_prepare_v2(db_, "SELECT value FROM kept WHERE name = ?", -1, &statement,
                            nullptr),
         "cannot ask the store");
    must(db_,
         // nullptr is SQLITE_STATIC written without the cast SQLITE_TRANSIENT
         // hides: the caller's bytes are alive across the step below, so
         // SQLite has no copy to make.
         sqlite3_bind_text(statement, 1, name.data(), static_cast<int>(name.size()),
                           nullptr),
         "cannot ask the store");
    std::optional<std::string> out;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const auto* text = sqlite3_column_text(statement, 0);
        if (text != nullptr) {
            out = std::string(reinterpret_cast<const char*>(text));
        }
    }
    sqlite3_finalize(statement);
    return out;
}

void Store::set(std::string_view name, std::string_view value) {
    sqlite3_stmt* statement = nullptr;
    must(db_,
         sqlite3_prepare_v2(db_,
                            "INSERT INTO kept (name, value) VALUES (?, ?) "
                            "ON CONFLICT(name) DO UPDATE SET value = excluded.value",
                            -1, &statement, nullptr),
         "cannot keep it");
    must(db_,
         sqlite3_bind_text(statement, 1, name.data(), static_cast<int>(name.size()),
                           nullptr),
         "cannot keep it");
    must(db_,
         sqlite3_bind_text(statement, 2, value.data(), static_cast<int>(value.size()),
                           nullptr),
         "cannot keep it");
    const int rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    must(db_, rc, "cannot keep it");
}

} // namespace glideslope::platform
