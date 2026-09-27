#pragma once

// Data fetched once and kept: DEM tiles and the geoid grid, in the cache
// directory, each checked as it arrives and written into place only whole.

#include "platform/http.hpp"
#include "world/dem.hpp"
#include "world/runways.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace glideslope::world {

using Fetch = std::function<platform::HttpResponse(const std::string& url)>;

// **A weather service that did not answer**: nothing answered at all, or
// the service answered a server error (5xx), or turned the request away for
// now (429), to every one of its retries, or
// a 200 plainly not JSON to every one of its tries (answered_json). The
// weather is live and somebody else's, so this is the one failure a flight's
// caller may take for "there is no weather to be had". An answer that refuses
// the request (any other 4xx), or JSON that is not the answer, is not this: it is a
// fault of the request or of the reading, and is thrown as DemError or the
// parse's error.
struct ServiceUnavailable : DemError {
    using DemError::DemError;
};

// **A weather service's scheme and host**: `own`, such as
// "https://aviationweather.gov", unless platform::weather_service() names
// another for a test. Throws std::runtime_error if it names one that
// weather_service_allowed refuses.
std::string weather_host(const std::string& own);

// **Where the weather may be asked instead**: an `https://` host, with a port
// or without, or `http://127.0.0.1` or `http://localhost`, with a port or
// without - the loopback, for a stub a test runs. Nothing else: weather asked
// over plain HTTP of another machine could be read and changed on the way.
bool weather_service_allowed(const std::string& service);

// A GET through the platform's HTTP client, with a body limit to suit a DEM
// tile, given up as a FetchesGivenUp on its thread says.
Fetch http_fetch();

// How a wait between tries is waited: wait_before_trying_again (which a
// FetchesGivenUp on this thread ends at once) unless a test counts the waits
// instead.
using Sleep = std::function<void(std::chrono::milliseconds)>;

// **The longest a Retry-After is waited**, however long it asks for.
inline constexpr std::chrono::milliseconds retry_after_limit{10000};

// **Fetches on this thread given up once `flag` rises**, for as long as this
// lives: every request http_fetch makes carries the flag as its
// HttpRequest::abandon, and every wait between tries (wait_before_trying_again)
// ends at once, throwing FetchGivenUp. A fetch nothing will wait for must not
// hold up the end of what asked for it - a program quit while the weather
// service answered 503 sat through every retry, about 80 s, before it could
// end. One at a time on a thread; the flag must outlive it.
class FetchesGivenUp {
public:
    explicit FetchesGivenUp(const std::atomic<bool>& flag);
    ~FetchesGivenUp();
    FetchesGivenUp(const FetchesGivenUp&) = delete;
    FetchesGivenUp& operator=(const FetchesGivenUp&) = delete;
};

// What a fetch given up throws. Not an HttpError, so no retry takes it for a
// transfer that failed and tries again.
struct FetchGivenUp : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Waits `wait`, or throws FetchGivenUp as soon as a FetchesGivenUp on this
// thread says to - before waiting, or in the middle of it.
void wait_before_trying_again(std::chrono::milliseconds wait);

// `fetch(url)`, tried again when the server fails - a 5xx status, or a 200
// with nothing in it, which nothing fetched here ever is - or turns the
// request away for now (429, Too Many Requests), or nothing answers,
// `attempts` times in all, waiting `wait` before the second try and twice as
// long before each after. Services have bad minutes: aviationweather.gov
// answers 504 now and then, and one of the weather services once answered an
// empty 200 in CI. Networks have bad seconds too: a macOS runner could not
// resolve api.open-meteo.com for longer than the 6 s three tries spanned, so
// five tries span 30 s. What comes back last is returned, or what it threw
// thrown; any other status is returned at once.
//
// **A 429 is waited out as it asks**: Open-Meteo turned CI's parallel weather
// tests away with it on 2026-09-27. Its Retry-After, in seconds, is the wait
// before the next try; one without a Retry-After in seconds (none, a date, or
// not a number) waits the backoff a server error does. Either way a wait after
// a 429 is never more than retry_after_limit, so five tries all turned away
// wait 40 s at most. (A wait after a server error or no answer is the backoff
// alone, uncapped, as before.) The doubling goes on either way. **A real
// Retry-After overrides a shortened `wait`**: a test that passes 0 still waits
// what a service's 429 asks, up to the limit.
platform::HttpResponse
fetch_with_retries(const Fetch& fetch, const std::string& url, int attempts = 5,
                   std::chrono::milliseconds wait = std::chrono::milliseconds(2000),
                   const Sleep& sleep = {});

// **How many times an answer that does not parse is fetched again** - each
// through fetch_with_retries - before it is taken for a download that failed.
inline constexpr int parse_attempts = 3;
// And how long before the first retry, twice that before the next.
inline constexpr std::chrono::milliseconds parse_wait{1000};

// **Whether a weather service's 200 is JSON**, for the `attempt`-th fetch
// of `url`: true if it parses; false, after waiting parse_wait times the
// attempt, if it does not and another fetch is due. On the last of
// parse_attempts: ServiceUnavailable if the body is plainly not JSON - empty,
// or beginning with anything but "{" or "[", as a page of HTML does - and
// DemError, without "could not download", if it began as JSON and did not
// parse. aviationweather.gov and Open-Meteo serve nothing but JSON, and each
// has, now and then, answered a 200 whose body was not on every try: in CI on
// 2026-09-26, Open-Meteo, empty. That is the service not answering, live and
// somebody else's. What begins as JSON and does not parse could be our
// parser's fault, and a parser that broke must fail the weather tests, not
// skip them for good. JSON that is not the answer asked for is ours too, and
// is the caller's to throw.
bool answered_json(std::string_view text, const std::string& url, int attempt);

// **`bytes` written to `path` whole, unless a file is there already**: true if
// they were put there, false if one was there and is left as it is. Written
// beside its final name and moved into place, so a download cut short never
// leaves a file that looks whole.
//
// **Two at once fetch the same file.** Tests run in parallel, and a flight
// and the terrain fetch the same tiles, so the name written to first is this
// writer's alone - no two share a half-written file - and the move into place
// does not replace (move_into_place_unless_there): what is there is what was
// asked for, pinned by its hash or checked against its ETag. And on Windows a
// replaced file that anybody still has open is delete-pending, and its name
// refuses every reader until the last handle closes: CI saw `exists: Access
// is denied` of a DEM tile. Only a POSIX filesystem with neither hard links
// nor an exclusive rename is replaced on, where replacing is harmless.
// Throws DemError if the file cannot be written or moved, or this writer's
// own temporary file cannot be removed.
bool put_in_place(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes);

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
