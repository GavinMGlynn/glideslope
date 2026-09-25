#pragma once

// Data fetched once and kept: DEM tiles and the geoid grid, in the cache
// directory, each checked as it arrives and written into place only whole.

#include "platform/http.hpp"
#include "world/dem.hpp"
#include "world/runways.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace glideslope::world {

using Fetch = std::function<platform::HttpResponse(const std::string& url)>;

// **A weather service's scheme and host**: `own`, such as
// "https://aviationweather.gov", unless platform::weather_service() names
// another for a test.
std::string weather_host(const std::string& own);

// A GET through the platform's HTTP client, with a body limit to suit a DEM tile.
Fetch http_fetch();

// `fetch(url)`, tried again when the server fails - a 5xx status, or a 200
// with nothing in it, which nothing fetched here ever is - or nothing answers,
// `attempts` times in all, waiting `wait` before the second try and twice as
// long before each after. Services have bad minutes: aviationweather.gov
// answers 504 now and then, and one of the weather services once answered an
// empty 200 in CI. Networks have bad seconds too: a macOS runner could not
// resolve api.open-meteo.com for longer than the 6 s three tries spanned, so
// five tries span 30 s. What comes back last is returned, or what it threw
// thrown; any other status is returned at once.
platform::HttpResponse
fetch_with_retries(const Fetch& fetch, const std::string& url, int attempts = 5,
                   std::chrono::milliseconds wait = std::chrono::milliseconds(2000));

// **How many times an answer that does not parse is fetched again** - each
// through fetch_with_retries - before it is taken for a download that failed.
inline constexpr int parse_attempts = 3;
// And how long before the first retry, twice that before the next.
inline constexpr std::chrono::milliseconds parse_wait{1000};

// A file pinned by SHA-256, from the cache or else fetched into it. Throws
// DemError if it cannot be had, or arrives as anything but what was pinned.
std::filesystem::path fetch_pinned(const std::filesystem::path& cache,
                                   const std::string& name, const std::string& url,
                                   const std::string& sha256, const Fetch& fetch);

// DEM tiles and their water body masks from the cache, fetched from the
// public buckets into it when they are not there yet. A file is checked
// against the MD5 the bucket gives as its ETag before it is kept; one that
// arrives different, or not at all, is not kept, and the query that wanted it
// fails with the reason.
class DownloadedTiles : public DemTiles {
public:
    DownloadedTiles(std::filesystem::path cache, Fetch fetch);

    std::shared_ptr<const ByteSource> open(DemDataset dataset, DemCell cell) override;
    std::shared_ptr<const ByteSource> open_water_mask(DemDataset dataset,
                                                      DemCell cell) override;

    // Files fetched, rather than found in the cache, since construction.
    int downloads() const {
        return downloads_;
    }

private:
    std::shared_ptr<const ByteSource> fetched(DemDataset dataset, const std::string& name,
                                              const std::string& url);

    std::filesystem::path cache_;
    Fetch fetch_;
    int downloads_ = 0;
};

// Every runway in the world: OurAirports' `runways.csv` at its pinned commit,
// from the cache or fetched into it, read (world/runways.hpp). See
// docs/ASSETS.md.
std::vector<RunwayEnd> world_runways(const std::filesystem::path& cache, const Fetch& fetch);

// The EGM2008 5-minute geoid, from the cache or fetched into it. See
// docs/ASSETS.md.
Geoid egm2008_geoid(const std::filesystem::path& cache, const Fetch& fetch);

} // namespace glideslope::world
