#pragma once

// **What Cesium Native fetches: through the platform's HTTPS, and kept in
// its cache.**
//
// Two layers of the terrain's chain of asset accessors, here rather than in
// terrain_tiles.cpp so that a test can build the one situation that crashed
// the client with the window (tests/tools/cache_revalidation.cpp).
//
// **A fetch that gets no answer must not reach the cache's revalidation as
// one.** Cesium Native's CachingAssetAccessor, finding an entry gone stale,
// asks for it again (with If-None-Match or If-Modified-Since when it can),
// and reads the answer's status with
//
//     pCompletedRequest->response()->statusCode()
//
// having checked only that the request is there, not its response
// (CachingAssetAccessor.cpp, at Cesium Native v0.64.0). A request with no
// response is how a fetch that failed says so - refused, timed out, or given
// up because the terrain is closing - and is the convention everywhere else
// in Cesium Native. Through that one path it is a null pointer called
// through: the client segfaulted as it closed, its stale imagery being
// revalidated when its transfers were given up. So every GET that gets no
// answer is answered, below the cache, with a 504 and nothing in it - the
// status for a gateway that heard nothing back - which the cache does not
// store, and which every reader above it takes for a failed fetch, as it took
// the missing response.

#include <atomic>
#include <memory>

namespace CesiumAsync {
class IAssetAccessor;
class ICacheDatabase;
} // namespace CesiumAsync

namespace glideslope::gfx {

// The status a fetch that got no answer is given (see above).
inline constexpr int unanswered_fetch_status = 504;

// Fetches through platform::http_get - GETs only; anything else is refused -
// each given up at once when `closing` is raised. A fetch that fails has no
// response, and says why on standard error.
std::shared_ptr<CesiumAsync::IAssetAccessor>
platform_fetches(std::shared_ptr<const std::atomic<bool>> closing);

// `network`'s fetches, kept in `cache` for as long as their caching headers
// allow, and a GET that gets no answer answered with unanswered_fetch_status.
std::shared_ptr<CesiumAsync::IAssetAccessor>
cached_fetches(std::shared_ptr<CesiumAsync::IAssetAccessor> network,
               std::shared_ptr<CesiumAsync::ICacheDatabase> cache);

} // namespace glideslope::gfx
