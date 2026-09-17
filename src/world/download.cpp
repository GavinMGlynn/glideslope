#include "world/download.hpp"

#include "world/digest.hpp"

#include <cctype>
#include <fstream>
#include <system_error>

namespace glideslope::world {

namespace {

// Written beside its final name and renamed into place, so a download cut
// short never leaves a file that looks whole.
void write_whole(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        throw DemError("cannot create " + path.parent_path().string() + ": " +
                       error.message());
    }
    std::filesystem::path part = path;
    part += ".part";
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            std::filesystem::remove(part, error);
            throw DemError("cannot write " + part.string());
        }
    }
    std::filesystem::rename(part, path, error);
    if (error) {
        std::filesystem::remove(part, error);
        throw DemError("cannot move " + part.string() + " into place");
    }
}

platform::HttpResponse get(const Fetch& fetch, const std::string& url) {
    platform::HttpResponse r;
    try {
        r = fetch(url);
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
    if (std::filesystem::exists(path)) {
        return path;
    }
    const platform::HttpResponse r = get(fetch, url);
    const std::string got = sha256_hex(r.body);
    if (got != sha256) {
        throw DemError(url + " arrived with SHA-256 " + got + ", not the pinned " +
                       sha256);
    }
    write_whole(path, r.body);
    return path;
}

DownloadedTiles::DownloadedTiles(std::filesystem::path cache, Fetch fetch)
    : cache_(std::move(cache)), fetch_(std::move(fetch)) {}

std::shared_ptr<const ByteSource> DownloadedTiles::open(DemDataset dataset,
                                                        DemCell cell) {
    const std::string name = dem_tile_name(dataset, cell);
    const std::filesystem::path path =
        cache_ /
        (dataset == DemDataset::glo30 ? "copernicus-dem-30m" : "copernicus-dem-90m") /
        (name + ".tif");
    if (!std::filesystem::exists(path)) {
        const std::string url = dem_tile_url(dataset, cell);
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
        write_whole(path, r.body);
        ++downloads_;
    }
    try {
        return std::make_shared<FileSource>(path);
    } catch (const ByteSourceError& e) {
        throw DemError(e.what());
    }
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
