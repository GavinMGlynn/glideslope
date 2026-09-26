#include "world/download.hpp"

#include "platform/paths.hpp"
#include "world/digest.hpp"

#include <atomic>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <iterator>
#include <random>
#include <span>
#include <stdexcept>
#include <system_error>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <unistd.h>
#endif

namespace glideslope::world {

namespace {

// Moves `from` to `to` unless something is at `to` already, in one step that
// cannot replace it: false, with `from` left, if something is.
bool move_unless_there(const std::filesystem::path& from, const std::filesystem::path& to) {
#if defined(_WIN32)
    // No MOVEFILE_REPLACE_EXISTING: a name free is taken, one in use
    // refuses. A name delete-pending refuses too, until it is free: asked
    // again, as file_is_there asks.
    for (int attempt = 1;; ++attempt) {
        if (MoveFileExW(from.c_str(), to.c_str(), 0)) {
            return true;
        }
        const DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) {
            return false;
        }
        const bool passes = error == ERROR_ACCESS_DENIED ||
                            error == ERROR_SHARING_VIOLATION ||
                            error == ERROR_DELETE_PENDING;
        if (!passes || attempt >= transient_refusal_tries) {
            throw DemError("cannot move " + from.string() +
                           " into place: Windows error " + std::to_string(error) +
                           " after " + std::to_string(attempt) + " tries");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#else
    // rename() replaces; a second name made with link() cannot.
    if (::link(from.c_str(), to.c_str()) != 0) {
        const int error = errno;
        if (error == EEXIST) {
            return false;
        }
        throw DemError("cannot move " + from.string() + " into place: " +
                       std::strerror(error));
    }
    std::error_code ignored;
    std::filesystem::remove(from, ignored);
    return true;
#endif
}

bool there(const std::filesystem::path& path) {
    try {
        return file_is_there(path);
    } catch (const ByteSourceError& e) {
        throw DemError(e.what());
    }
}

platform::HttpResponse get(const Fetch& fetch, const std::string& url) {
    platform::HttpResponse r;
    try {
        r = fetch_with_retries(fetch, url);
    } catch (const platform::HttpError& e) {
        throw DemError(std::string("could not download: ") + e.what());
    }
    if (r.status != 200) {
        throw DemError("could not download " + url + ": status " +
                       std::to_string(r.status));
    }
    return r;
}

} // namespace

bool put_in_place(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        throw DemError("cannot create " + path.parent_path().string() + ": " +
                       error.message());
    }
    static std::atomic<unsigned long long> writes{0};
    std::filesystem::path part = path;
    part += ".part." + std::to_string(std::random_device{}()) + "-" +
            std::to_string(writes++);
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            std::filesystem::remove(part, error);
            throw DemError("cannot write " + part.string());
        }
    }
    bool put = false;
    try {
        put = move_unless_there(part, path);
    } catch (...) {
        std::filesystem::remove(part, error);
        throw;
    }
    if (!put) {
        std::filesystem::remove(part, error);
    }
    return put;
}

platform::HttpResponse fetch_with_retries(const Fetch& fetch, const std::string& url,
                                          int attempts,
                                          std::chrono::milliseconds wait) {
    for (int attempt = 1;; ++attempt) {
        try {
            platform::HttpResponse r = fetch(url);
            const bool failed = r.status >= 500 || (r.status == 200 && r.body.empty());
            if (!failed || attempt >= attempts) {
                return r;
            }
        } catch (const platform::HttpError&) {
            if (attempt >= attempts) {
                throw;
            }
        }
        std::this_thread::sleep_for(wait);
        wait *= 2;
    }
}

bool weather_service_allowed(const std::string& service) {
    const auto rest_is = [&](std::size_t from, bool any_host) {
        std::size_t at = from;
        if (any_host) {
            while (at < service.size() &&
                   (std::isalnum(static_cast<unsigned char>(service[at])) != 0 ||
                    service[at] == '.' || service[at] == '-')) {
                ++at;
            }
            if (at == from) {
                return false;
            }
        }
        if (at == service.size()) {
            return true;
        }
        if (service[at] != ':' || at + 1 == service.size() || service.size() - at > 6) {
            return false;
        }
        for (std::size_t i = at + 1; i < service.size(); ++i) {
            if (std::isdigit(static_cast<unsigned char>(service[i])) == 0) {
                return false;
            }
        }
        return true;
    };
    for (const std::string loopback : {"http://127.0.0.1", "http://localhost"}) {
        if (service.rfind(loopback, 0) == 0) {
            return rest_is(loopback.size(), false);
        }
    }
    const std::string https = "https://";
    return service.rfind(https, 0) == 0 && rest_is(https.size(), true);
}

std::string weather_host(const std::string& own) {
    std::string instead = platform::weather_service();
    if (instead.empty()) {
        return own;
    }
    if (!weather_service_allowed(instead)) {
        throw std::runtime_error("GLIDESLOPE_WEATHER_SERVICE is \"" + instead +
                                 "\": it must be an https:// host, or http:// to "
                                 "127.0.0.1 or localhost");
    }
    return instead;
}

Fetch http_fetch() {
    return [](const std::string& url) {
        platform::HttpRequest request;
        request.url = url;
        request.user_agent = "glideslope (+https://github.com/GavinMGlynn/glideslope)";
        request.max_body = std::uint64_t{256} << 20;
        return platform::http_get(request);
    };
}

std::filesystem::path fetch_pinned(const std::filesystem::path& cache,
                                   const std::string& name, const std::string& url,
                                   const std::string& sha256, const Fetch& fetch) {
    const std::filesystem::path path = cache / name;
    if (there(path)) {
        return path;
    }
    const platform::HttpResponse r = get(fetch, url);
    const std::string got = sha256_hex(r.body);
    if (got != sha256) {
        throw DemError(url + " arrived with SHA-256 " + got + ", not the pinned " +
                       sha256);
    }
    put_in_place(path, r.body);
    return path;
}

DownloadedTiles::DownloadedTiles(std::filesystem::path cache, Fetch fetch)
    : cache_(std::move(cache)), fetch_(std::move(fetch)) {}

std::shared_ptr<const ByteSource> DownloadedTiles::open(DemDataset dataset,
                                                        DemCell cell) {
    return fetched(dataset, dem_tile_name(dataset, cell), dem_tile_url(dataset, cell));
}

std::shared_ptr<const ByteSource> DownloadedTiles::open_water_mask(DemDataset dataset,
                                                                   DemCell cell) {
    return fetched(dataset, dem_water_mask_name(dataset, cell),
                   dem_water_mask_url(dataset, cell));
}

std::shared_ptr<const ByteSource> DownloadedTiles::fetched(DemDataset dataset,
                                                           const std::string& name,
                                                           const std::string& url) {
    const std::filesystem::path path =
        cache_ /
        (dataset == DemDataset::glo30 ? "copernicus-dem-30m" : "copernicus-dem-90m") /
        (name + ".tif");
    if (!there(path)) {
        const platform::HttpResponse r = get(fetch_, url);
        // S3 gives a file uploaded whole its MD5 as its ETag. One uploaded in
        // parts has an ETag with a dash, which is not a digest of the file, and
        // then TLS is all there is to trust.
        const auto etag = r.headers.find("etag");
        if (etag == r.headers.end()) {
            throw DemError(url + " arrived with no ETag to check it against");
        }
        std::string tag = etag->second;
        if (tag.size() >= 2 && tag.front() == '"' && tag.back() == '"') {
            tag = tag.substr(1, tag.size() - 2);
        }
        for (char& ch : tag) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (tag.find('-') == std::string::npos) {
            const std::string md5 = md5_hex(r.body);
            if (md5 != tag) {
                throw DemError(url + " arrived damaged: its MD5 is " + md5 +
                               " and its ETag says " + tag);
            }
        }
        put_in_place(path, r.body);
        ++downloads_;
    }
    try {
        return std::make_shared<FileSource>(path);
    } catch (const ByteSourceError& e) {
        throw DemError(e.what());
    }
}

std::vector<RunwayEnd> world_runways(const std::filesystem::path& cache, const Fetch& fetch) {
    const std::filesystem::path path = fetch_pinned(
        cache, "ourairports-runways.csv",
        "https://raw.githubusercontent.com/davidmegginson/ourairports-data/"
        "a46b8eb13173dc6351a7b6abaf34bd0ec9db48d0/runways.csv",
        "ae9a7661f230731cb4fef3a291991cd440f8a68593f41d773092798fc6ec9a8c", fetch);
    // Read as a DEM tile is (world/byte_source.hpp): on Windows a file
    // fetched into place by another process's rename is still readable while
    // the rename holds it, which a plain stream is not.
    std::string text;
    try {
        const FileSource file(path);
        text.resize(static_cast<std::size_t>(file.size()));
        file.read(0, std::span<std::uint8_t>(reinterpret_cast<std::uint8_t*>(text.data()),
                                             text.size()));
    } catch (const ByteSourceError& e) {
        throw RunwayError(std::string("cannot read the runways: ") + e.what());
    }
    return read_runways(text);
}

Geoid egm2008_geoid(const std::filesystem::path& cache, const Fetch& fetch) {
    const std::filesystem::path path = fetch_pinned(
        cache, "egm2008-5.zip",
        "https://sourceforge.net/projects/geographiclib/files/geoids-distrib/"
        "egm2008-5.zip/download",
        "408f05e0c04a9f2e17b9ea2d27123f936e9dea60128bb3411a272f8ddbe318dd", fetch);
    try {
        return load_geoid_zip(FileSource(path));
    } catch (const std::exception& e) {
        throw DemError(path.string() + ": " + e.what());
    }
}

} // namespace glideslope::world
