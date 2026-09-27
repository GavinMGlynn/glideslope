#include "world/download.hpp"

#include "platform/paths.hpp"
#include "world/digest.hpp"
#include "world/json.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace glideslope::world {

namespace {

// The flag of the FetchesGivenUp living on this thread, if one is.
thread_local const std::atomic<bool>* given_up_when = nullptr;

bool given_up() {
    return given_up_when != nullptr && given_up_when->load();
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

// A response's Retry-After, if it is in seconds, as the wait it asks for -
// never more than retry_after_limit. Nothing if there is none, or it is a
// date or not a number.
std::optional<std::chrono::milliseconds> retry_after_seconds(const platform::HttpResponse& r) {
    const auto header = r.headers.find("retry-after");
    if (header == r.headers.end()) {
        return std::nullopt;
    }
    const std::string& value = header->second;
    const std::size_t first = value.find_first_not_of(" \t");
    const std::size_t last = value.find_last_not_of(" \t");
    if (first == std::string::npos) {
        return std::nullopt;
    }
    const long long limit_s =
        std::chrono::duration_cast<std::chrono::seconds>(retry_after_limit).count();
    long long seconds = 0;
    for (std::size_t i = first; i <= last; ++i) {
        const char c = value[i];
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        // Past the limit is the limit, however many digits follow.
        seconds = std::min(seconds * 10 + (c - '0'), limit_s);
    }
    return std::chrono::milliseconds(seconds * 1000);
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
        put = move_into_place_unless_there(part, path);
    } catch (const ByteSourceError& e) {
        // Taken away if it is still there; the move's failure is what is said.
        std::filesystem::remove(part, error);
        throw DemError(e.what());
    }
    if (!put) {
        std::filesystem::remove(part, error);
        if (error) {
            throw DemError(path.string() + " was there already, but this writer's " +
                           part.string() + " cannot be removed: " + error.message());
        }
    }
    return put;
}

FetchesGivenUp::FetchesGivenUp(const std::atomic<bool>& flag) {
    given_up_when = &flag;
}

FetchesGivenUp::~FetchesGivenUp() {
    given_up_when = nullptr;
}

void wait_before_trying_again(std::chrono::milliseconds wait) {
    // In slices, each short enough that a flag raised is seen at once.
    const auto until = std::chrono::steady_clock::now() + wait;
    for (;;) {
        if (given_up()) {
            throw FetchGivenUp("the fetch was given up");
        }
        const auto now = std::chrono::steady_clock::now();
        if (now >= until) {
            return;
        }
        std::this_thread::sleep_for(
            std::min<std::chrono::steady_clock::duration>(until - now,
                                                          std::chrono::milliseconds(10)));
    }
}

platform::HttpResponse fetch_with_retries(const Fetch& fetch, const std::string& url,
                                          int attempts,
                                          std::chrono::milliseconds wait,
                                          const Sleep& sleep) {
    for (int attempt = 1;; ++attempt) {
        std::chrono::milliseconds this_wait = wait;
        try {
            platform::HttpResponse r = fetch(url);
            const bool turned_away = r.status == 429;
            const bool failed =
                r.status >= 500 || turned_away || (r.status == 200 && r.body.empty());
            if (!failed || attempt >= attempts) {
                return r;
            }
            // A 429 waits what it asks, or the backoff, and never more
            // than retry_after_limit either way.
            if (turned_away) {
                if (const auto asked = retry_after_seconds(r)) {
                    this_wait = *asked;
                }
                this_wait = std::min(this_wait, retry_after_limit);
            }
        } catch (const platform::HttpError&) {
            if (attempt >= attempts) {
                throw;
            }
        }
        if (sleep) {
            sleep(this_wait);
        } else {
            wait_before_trying_again(this_wait);
        }
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

bool answered_json(std::string_view text, const std::string& url, int attempt) {
    // Plainly not JSON: nothing, or what JSON never begins with - "<" for a
    // page of HTML. Only that is taken for the service not answering; what
    // begins as JSON and does not parse may be our parser's fault, and is
    // never let become a skip.
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    const bool plainly_not = first == std::string_view::npos ||
                             (text[first] != '{' && text[first] != '[');
    std::string why;
    if (plainly_not) {
        why = first == std::string_view::npos
                  ? "it was empty"
                  : "it began with byte " +
                        std::to_string(static_cast<unsigned char>(text[first])) +
                        " at byte " + std::to_string(first);
    } else {
        try {
            (void)parse_json(text);
            return true;
        } catch (const JsonError& e) {
            why = e.what();
        }
    }
    if (attempt >= parse_attempts) {
        if (plainly_not) {
            throw ServiceUnavailable("could not download " + url +
                                     ": its answer was not JSON (" + why + ")");
        }
        throw DemError(url + ": its answer began as JSON and did not parse (" + why + ")");
    }
    wait_before_trying_again(parse_wait * attempt);
    return false;
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
        request.abandon = given_up_when;
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
