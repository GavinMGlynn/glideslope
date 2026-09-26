#include "gfx/terrain_tiles.hpp"

#include "platform/http.hpp"
#include "world/json.hpp"

#include "gfx/terrain_colour.hpp"
#include "platform/paths.hpp"
#include "platform/stop.hpp"

#include <Cesium3DTilesContent/registerAllTileContentTypes.h>
#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/TileLoadResult.h>
#include <Cesium3DTilesSelection/TileRefine.h>
#include <Cesium3DTilesSelection/SampleHeightResult.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetContentLoader.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/CachingAssetAccessor.h>
#include <CesiumAsync/SqliteCache.h>
#include <CesiumRasterOverlays/BingMapsRasterOverlay.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumGeometry/Axis.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GeographicProjection.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGltf/Accessor.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/Buffer.h>
#include <CesiumGltf/BufferView.h>
#include <CesiumGltf/MeshPrimitive.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumImage/ImageAsset.h>
#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>
#include <CesiumRasterOverlays/WebMapTileServiceRasterOverlay.h>
#include <CesiumUtility/CreditSystem.h>
#include <CesiumUtility/IntrusivePointer.h>
#include <CesiumUtility/JsonValue.h>

#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace glideslope::gfx {

namespace {

// **Nothing reads a tile's content until its readers are registered.**
// Cesium Native keeps a table of converters - glTF, B3DM, PNTS, composite -
// looked up by the first bytes of what arrives, and
// registerAllTileContentTypes() is what fills it. Without it a tileset whose
// tiles are glTF loads every one of them and draws none: not knowing what a
// body is, it falls back to reading it as an external tileset, and 441
// perfectly good glTF binaries came back as "Error when parsing JSON
// content, error code Invalid value. at byte offset 0".
//
// The open provider never needed it - it builds its glTF here - and neither
// does Cesium ion's quantized mesh, which has a reader of its own. Google's
// Photorealistic 3D Tiles are the first thing here that arrives as glTF.
void register_tile_readers() {
    static const bool done = [] {
        Cesium3DTilesContent::registerAllTileContentTypes();
        return true;
    }();
    (void)done;
}

} // namespace

void log_to_standard_error() {
    // Once: spdlog refuses a logger of a name it already holds.
    static const bool done = [] {
        spdlog::set_default_logger(spdlog::stderr_logger_mt("glideslope"));
        // Warnings and worse only. Cesium Native says at info level which
        // URLs it fetched, and a Cesium ion URL carries the user's own token
        // in it - which would then be in whatever the output was kept in,
        // including a CI log. What goes wrong is still said.
        spdlog::set_level(spdlog::level::warn);
        return true;
    }();
    (void)done;
}

namespace {

using Cesium3DTilesSelection::Tile;
using Cesium3DTilesSelection::TileChildrenResult;
using Cesium3DTilesSelection::TileLoadInput;
using Cesium3DTilesSelection::TileLoadResult;
using Cesium3DTilesSelection::TileLoadResultState;
using CesiumGeometry::QuadtreeTileID;
using CesiumGeospatial::BoundingRegion;
using CesiumGeospatial::Ellipsoid;
using CesiumGeospatial::GlobeRectangle;

constexpr double metres_per_degree_of_latitude = 111320.0;
constexpr int deepest_possible_level = 24;

// Cesium Native's tasks, on a fixed set of threads.
class WorkerPool final : public CesiumAsync::ITaskProcessor {
public:
    explicit WorkerPool(int threads) {
        for (int i = 0; i < std::max(1, threads); ++i) {
            threads_.emplace_back([this] { run(); });
        }
    }

    ~WorkerPool() override {
        stop();
    }

    // Finishes the tasks queued and running, and joins the threads. Cesium
    // Native's pending work holds the pool as long as it lives, so without this
    // its threads could outlive the terrain - and, still downloading, run into
    // the process's exit as it tears down the libraries under them.
    void stop() {
        {
            const std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_all();
        for (std::thread& t : threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    // Whether nothing is queued or running.
    bool idle() {
        const std::lock_guard lock(mutex_);
        return tasks_.empty() && running_ == 0;
    }

    void startTask(std::function<void()> task) override {
        {
            const std::lock_guard lock(mutex_);
            tasks_.push_back(std::move(task));
        }
        wake_.notify_one();
    }

private:
    // Runs tasks until told to stop, finishing any still queued first.
    void run() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
                ++running_;
            }
            task();
            // Whatever the task queued for the main thread is queued by now.
            const std::lock_guard lock(mutex_);
            --running_;
        }
    }

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> tasks_;
    std::vector<std::thread> threads_;
    int running_ = 0;
    bool stopping_ = false;
};

// Another accessor's requests, counted from their asking to their answer -
// the cache's own thread included, which the worker pool does not see - so the
// terrain can wait out what is in flight before it stops its workers.
class CountingAccessor final : public CesiumAsync::IAssetAccessor {
public:
    CountingAccessor(std::shared_ptr<CesiumAsync::IAssetAccessor> inner,
                     std::shared_ptr<std::atomic<int>> in_flight)
        : inner_(std::move(inner)), in_flight_(std::move(in_flight)) {}

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    get(const CesiumAsync::AsyncSystem& async, const std::string& url,
        const std::vector<THeader>& headers) override {
        return counted(inner_->get(async, url, headers));
    }

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    request(const CesiumAsync::AsyncSystem& async, const std::string& verb,
            const std::string& url, const std::vector<THeader>& headers,
            const std::span<const std::byte>& body) override {
        return counted(inner_->request(async, verb, url, headers, body));
    }

    void tick() noexcept override {
        inner_->tick();
    }

private:
    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    counted(CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>&& answer) {
        ++*in_flight_;
        // Counted down on a worker, which the pool counts as running until what
        // follows the answer is queued: the terrain never sees neither.
        return std::move(answer)
            .thenInWorkerThread(
                [n = in_flight_](
                    std::shared_ptr<CesiumAsync::IAssetRequest>&& request) {
                    --*n;
                    return std::move(request);
                })
            .catchImmediately([n = in_flight_](std::exception&& e)
                                  -> std::shared_ptr<CesiumAsync::IAssetRequest> {
                --*n;
                throw std::runtime_error(e.what());
            });
    }

    std::shared_ptr<CesiumAsync::IAssetAccessor> inner_;
    std::shared_ptr<std::atomic<int>> in_flight_;
};

// Cesium Native's requests, through the platform's HTTPS. Plain GETs only:
// nothing drawn yet needs a request header or a body, and a request that asks
// for either fails rather than going out without them.
// Puts a bearer token on every request to one place, and touches no other.
// A token is a secret: it goes only to the host that issued it, so that a
// tileset naming a URL elsewhere cannot make it leak.
class AuthorisingAccessor final : public CesiumAsync::IAssetAccessor {
public:
    AuthorisingAccessor(std::shared_ptr<CesiumAsync::IAssetAccessor> next,
                        std::string prefix, std::string token)
        : next_(std::move(next)), prefix_(std::move(prefix)),
          token_(std::move(token)) {}

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    get(const CesiumAsync::AsyncSystem& async, const std::string& url,
        const std::vector<THeader>& headers) override {
        return next_->get(async, url, with_token(url, headers));
    }

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    request(const CesiumAsync::AsyncSystem& async, const std::string& verb,
            const std::string& url, const std::vector<THeader>& headers,
            const std::span<const std::byte>& body) override {
        return next_->request(async, verb, url, with_token(url, headers), body);
    }

    void tick() noexcept override {
        next_->tick();
    }

private:
    std::vector<THeader> with_token(const std::string& url,
                                    const std::vector<THeader>& headers) const {
        std::vector<THeader> out = headers;
        if (url.rfind(prefix_, 0) != 0) {
            return out; // somewhere else: it gets nothing of ours
        }
        for (const auto& [name, value] : out) {
            if (name == "Authorization") {
                return out; // already carried
            }
        }
        out.emplace_back("Authorization", "Bearer " + token_);
        return out;
    }

    std::shared_ptr<CesiumAsync::IAssetAccessor> next_;
    std::string prefix_;
    std::string token_;
};

// Puts a query onto every request to one place, and touches no other.
//
// Google's Photorealistic 3D Tiles are reached by a URL carrying a session
// and a key, and the child tiles it names carry neither: they are paths
// alone. Every request for one has to carry them again, and Cesium Native
// v0.64.0 has no loader that does it - so this does, for that host and no
// other, and only where the request does not already say them.
class QueryAccessor final : public CesiumAsync::IAssetAccessor {
public:
    QueryAccessor(std::shared_ptr<CesiumAsync::IAssetAccessor> next,
                  std::string prefix, std::string query)
        : next_(std::move(next)), prefix_(std::move(prefix)),
          query_(std::move(query)) {}

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    get(const CesiumAsync::AsyncSystem& async, const std::string& url,
        const std::vector<THeader>& headers) override {
        return next_->get(async, with_query(url), headers);
    }

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    request(const CesiumAsync::AsyncSystem& async, const std::string& verb,
            const std::string& url, const std::vector<THeader>& headers,
            const std::span<const std::byte>& body) override {
        return next_->request(async, verb, with_query(url), headers, body);
    }

    void tick() noexcept override {
        next_->tick();
    }

private:
    std::string with_query(const std::string& url) const {
        if (query_.empty() || url.rfind(prefix_, 0) != 0) {
            return url; // somewhere else: it is left alone
        }
        // **Each parameter on its own.** Google's child tiles carry a session
        // of their own and no key, and its root carries a key and no session,
        // so neither "it already has a query" nor "it already has a session"
        // says whether the key is there. Taking either for an answer is a
        // 403: what is missing is added, what is there stands.
        std::string out = url;
        std::size_t at = 0;
        while (at < query_.size()) {
            const std::size_t amp = query_.find('&', at);
            const std::string one =
                query_.substr(at, amp == std::string::npos ? amp : amp - at);
            at = amp == std::string::npos ? query_.size() : amp + 1;
            const std::size_t equals = one.find('=');
            if (equals == std::string::npos || one.empty()) {
                continue;
            }
            const std::string name = one.substr(0, equals + 1); // with its '='
            const std::size_t question = out.find('?');
            bool already = false;
            if (question != std::string::npos) {
                // As the whole query, the first parameter, or a later one.
                already = out.compare(question + 1, name.size(), name) == 0 ||
                          out.find("&" + name, question) != std::string::npos;
            }
            if (already) {
                continue;
            }
            out += (question == std::string::npos ? "?" : "&") + one;
        }
        return out;
    }

    std::shared_ptr<CesiumAsync::IAssetAccessor> next_;
    std::string prefix_;
    std::string query_;
};

class PlatformAccessor final : public CesiumAsync::IAssetAccessor {
public:
    // Every transfer is given up once `closing` is raised: see ~TerrainTiles.
    explicit PlatformAccessor(std::shared_ptr<const std::atomic<bool>> closing)
        : closing_(std::move(closing)) {}

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    get(const CesiumAsync::AsyncSystem& async, const std::string& url,
        const std::vector<THeader>& headers) override {
        return request(async, "GET", url, headers, {});
    }

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    request(const CesiumAsync::AsyncSystem& async, const std::string& verb,
            const std::string& url, const std::vector<THeader>& headers,
            const std::span<const std::byte>& body) override {
        // Only GETs are made. A terrain provider asks for nothing else -
        // Cesium ion and Google both serve tiles by GET - and a body going
        // out is not something this needs to be able to do.
        //
        // The headers a provider asks for are sent, less any whose name or
        // value holds a control character: a newline in either would end the
        // header and begin whatever followed it, so those are dropped rather
        // than passed to the operating system.
        const bool only_get = verb == "GET" && body.empty();
        std::vector<std::pair<std::string, std::string>> sent;
        for (const auto& [name, value] : headers) {
            const auto printable = [](const std::string& s) {
                return std::none_of(s.begin(), s.end(), [](unsigned char c) {
                    return c < 0x20 || c == 0x7F;
                });
            };
            // How a body is compressed on the way is the HTTP layer's own
            // business: each of the three undoes whatever it asked for. A
            // provider's Accept-Encoding asking for something the layer did
            // not negotiate hands back a body nothing can read - Cesium ion's
            // layer.json arrives gzipped and unreadable - so it is not passed
            // on.
            std::string lower;
            for (const char c : name) {
                lower += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }
            if (lower == "accept-encoding") {
                continue;
            }
            if (printable(name) && printable(value)) {
                sent.emplace_back(name, value);
            }
        }
        return async.runInWorkerThread(
            [verb, url, only_get, sent,
             closing = closing_]() -> std::shared_ptr<CesiumAsync::IAssetRequest> {
                auto request = std::make_shared<Request>(verb, url);
                if (!only_get) {
                    std::fprintf(stderr,
                                 "glideslope: %s %s refused: only GETs are made\n",
                                 verb.c_str(), url.c_str());
                    return request;
                }
                try {
                    platform::HttpRequest q;
                    q.url = url;
                    q.user_agent =
                        "glideslope (+https://github.com/GavinMGlynn/glideslope)";
                    q.headers = sent;
                    q.abandon = closing.get();
                    request->set(platform::http_get(q));
                } catch (const platform::HttpError& e) {
                    std::fprintf(stderr, "glideslope: %s: %s\n", url.c_str(), e.what());
                }
                return request;
            });
    }

    void tick() noexcept override {}

private:
    std::shared_ptr<const std::atomic<bool>> closing_;

    class Response final : public CesiumAsync::IAssetResponse {
    public:
        explicit Response(platform::HttpResponse r) : response_(std::move(r)) {
            for (const auto& [name, value] : response_.headers) {
                headers_[name] = value;
            }
        }
        uint16_t statusCode() const override {
            return static_cast<uint16_t>(response_.status);
        }
        std::string contentType() const override {
            const auto it = headers_.find("content-type");
            return it == headers_.end() ? std::string() : it->second;
        }
        const CesiumAsync::HttpHeaders& headers() const override {
            return headers_;
        }
        std::span<const std::byte> data() const override {
            return {reinterpret_cast<const std::byte*>(response_.body.data()),
                    response_.body.size()};
        }

    private:
        platform::HttpResponse response_;
        CesiumAsync::HttpHeaders headers_;
    };

    class Request final : public CesiumAsync::IAssetRequest {
    public:
        Request(std::string method, std::string url)
            : method_(std::move(method)), url_(std::move(url)) {}
        void set(platform::HttpResponse r) {
            response_ = std::make_unique<Response>(std::move(r));
        }
        const std::string& method() const override {
            return method_;
        }
        const std::string& url() const override {
            return url_;
        }
        const CesiumAsync::HttpHeaders& headers() const override {
            return headers_;
        }
        // Null when there was no response at all.
        const CesiumAsync::IAssetResponse* response() const override {
            return response_.get();
        }

    private:
        std::string method_;
        std::string url_;
        CesiumAsync::HttpHeaders headers_;
        std::unique_ptr<Response> response_;
    };
};

// Appends `values` to the model's one buffer as a new buffer view and accessor,
// and returns the accessor's index.
template <typename T>
int32_t add_accessor(CesiumGltf::Model& model, const std::vector<T>& values,
                     int32_t component_type, const std::string& type, int64_t count,
                     int32_t target) {
    std::vector<std::byte>& data = model.buffers[0].cesium.data;
    // glTF wants every view aligned to four bytes.
    while (data.size() % 4 != 0) {
        data.push_back(std::byte{0});
    }
    const std::size_t offset = data.size();
    const std::size_t bytes = values.size() * sizeof(T);
    data.resize(offset + bytes);
    std::memcpy(data.data() + offset, values.data(), bytes);
    model.buffers[0].byteLength = static_cast<int64_t>(data.size());

    CesiumGltf::BufferView& view = model.bufferViews.emplace_back();
    view.buffer = 0;
    view.byteOffset = static_cast<int64_t>(offset);
    view.byteLength = static_cast<int64_t>(bytes);
    view.target = target;

    CesiumGltf::Accessor& accessor = model.accessors.emplace_back();
    accessor.bufferView = static_cast<int32_t>(model.bufferViews.size() - 1);
    accessor.componentType = component_type;
    accessor.type = type;
    accessor.count = count;
    return static_cast<int32_t>(model.accessors.size() - 1);
}

// A terrain mesh as a glTF model, its positions relative to the mesh's origin
// - the tile's transform - and each vertex coloured.
CesiumGltf::Model terrain_model(const world::TerrainMesh& mesh, bool imagery) {
    CesiumGltf::Model model;
    model.asset.version = "2.0";
    model.extras["gltfUpAxis"] =
        CesiumUtility::JsonValue(static_cast<int64_t>(CesiumGeometry::Axis::Z));
    model.buffers.emplace_back();
    model.materials.emplace_back();

    std::vector<glm::vec4> colours;
    colours.reserve(mesh.positions.size());
    for (std::size_t v = 0; v < mesh.positions.size(); ++v) {
        const auto& p = mesh.positions[v];
        const world::Geodetic g =
            world::to_geodetic({mesh.origin.x + static_cast<double>(p[0]),
                                mesh.origin.y + static_cast<double>(p[1]),
                                mesh.origin.z + static_cast<double>(p[2])});
        const auto& n = mesh.normals[v];
        const world::Ecef normal{static_cast<double>(n[0]), static_cast<double>(n[1]),
                                 static_cast<double>(n[2])};
        const world::Ecef up = up_at(g.latitude_deg, g.longitude_deg);
        if (imagery) {
            const auto light = static_cast<float>(terrain_light(normal, up));
            colours.emplace_back(light, light, light, 1.0f);
        } else {
            const auto c = terrain_colour(
                static_cast<double>(mesh.heights_above_sea_level[v]), normal, up);
            colours.emplace_back(c[0], c[1], c[2], c[3]);
        }
    }

    const auto vertices = static_cast<int64_t>(mesh.positions.size());
    CesiumGltf::MeshPrimitive& primitive =
        model.meshes.emplace_back().primitives.emplace_back();
    primitive.mode = CesiumGltf::MeshPrimitive::Mode::TRIANGLES;
    primitive.material = 0;
    const int32_t positions =
        add_accessor(model, mesh.positions, CesiumGltf::Accessor::ComponentType::FLOAT,
                     CesiumGltf::Accessor::Type::VEC3, vertices,
                     CesiumGltf::BufferView::Target::ARRAY_BUFFER);
    // glTF requires a position accessor's bounds.
    std::array<double, 3> low{1e300, 1e300, 1e300};
    std::array<double, 3> high{-1e300, -1e300, -1e300};
    for (const auto& p : mesh.positions) {
        for (std::size_t k = 0; k < 3; ++k) {
            low[k] = std::min(low[k], static_cast<double>(p[k]));
            high[k] = std::max(high[k], static_cast<double>(p[k]));
        }
    }
    model.accessors[static_cast<std::size_t>(positions)].min = {low[0], low[1], low[2]};
    model.accessors[static_cast<std::size_t>(positions)].max = {high[0], high[1],
                                                                high[2]};
    primitive.attributes["POSITION"] = positions;
    primitive.attributes["COLOR_0"] =
        add_accessor(model, colours, CesiumGltf::Accessor::ComponentType::FLOAT,
                     CesiumGltf::Accessor::Type::VEC4, vertices,
                     CesiumGltf::BufferView::Target::ARRAY_BUFFER);
    primitive.indices = add_accessor(
        model, mesh.indices, CesiumGltf::Accessor::ComponentType::UNSIGNED_INT,
        CesiumGltf::Accessor::Type::SCALAR, static_cast<int64_t>(mesh.indices.size()),
        CesiumGltf::BufferView::Target::ELEMENT_ARRAY_BUFFER);

    model.nodes.emplace_back().mesh = 0;
    model.scenes.emplace_back().nodes.push_back(0);
    model.scene = 0;
    return model;
}

// The open-data terrain: a quadtree over the region, each tile built from the
// DEM.
class DemLoader final : public Cesium3DTilesSelection::TilesetContentLoader {
public:
    DemLoader(const world::GeoRectangle& region, world::HeightSource heights,
              bool imagery, std::atomic<std::size_t>& failures)
        : region_(region), heights_(std::move(heights)), imagery_(imagery),
          failures_(failures) {
        const double span = region.north_deg - region.south_deg;
        while (leaf_level_ < deepest_possible_level &&
               span / std::ldexp(1.0, static_cast<int>(leaf_level_)) /
                       terrain_tile_cells >
                   terrain_finest_spacing_deg * 1.0000001) {
            ++leaf_level_;
        }
    }

    // A tile, with its bounds as loose as the Earth's until it is loaded.
    Tile make_tile(const QuadtreeTileID& id, double lowest_m, double highest_m) {
        const world::GeoRectangle r = rectangle(id);
        Tile tile(this);
        tile.setTileID(id);
        tile.setRefine(Cesium3DTilesSelection::TileRefine::Replace);
        const world::Ecef origin = world::to_ecef(
            {(r.south_deg + r.north_deg) / 2, (r.west_deg + r.east_deg) / 2, 0.0});
        tile.setTransform(
            glm::translate(glm::dmat4(1.0), glm::dvec3(origin.x, origin.y, origin.z)));
        tile.setBoundingVolume(
            BoundingRegion(globe(r), lowest_m, highest_m, Ellipsoid::WGS84));
        tile.setGeometricError(id.level >= leaf_level_
                                   ? 0.0
                                   : 0.5 * (r.north_deg - r.south_deg) /
                                         terrain_tile_cells *
                                         metres_per_degree_of_latitude);
        return tile;
    }

    CesiumAsync::Future<TileLoadResult>
    loadTileContent(const TileLoadInput& input) override {
        const auto* id = std::get_if<QuadtreeTileID>(&input.tile.getTileID());
        if (id == nullptr) {
            return input.asyncSystem.createResolvedFuture(
                TileLoadResult::createFailedResult(input.pAssetAccessor, nullptr));
        }
        const world::GeoRectangle r = rectangle(*id);
        const double cell_m = (r.north_deg - r.south_deg) / terrain_tile_cells *
                              metres_per_degree_of_latitude;
        // Deep enough to cover what a tile a level coarser can differ by.
        const double skirt_m = std::max(20.0, 2.0 * cell_m);
        return input.asyncSystem.runInWorkerThread([this, r, skirt_m,
                                                    accessor = input.pAssetAccessor]() {
            try {
                world::TerrainMesh mesh;
                {
                    const std::lock_guard lock(mutex_);
                    mesh = world::make_terrain_mesh(r, terrain_tile_cells, skirt_m,
                                                    heights_);
                }
                TileLoadResult result{terrain_model(mesh, imagery_),
                                      CesiumGeometry::Axis::Z,
                                      BoundingRegion(globe(r), mesh.minimum_height_m,
                                                     mesh.maximum_height_m,
                                                     Ellipsoid::WGS84),
                                      std::nullopt,
                                      std::nullopt,
                                      accessor,
                                      nullptr,
                                      {},
                                      TileLoadResultState::Success,
                                      Ellipsoid::WGS84};
                return result;
            } catch (const std::exception& e) {
                ++failures_;
                std::fprintf(stderr, "glideslope: a terrain tile failed: %s\n",
                             e.what());
                return TileLoadResult::createFailedResult(accessor, nullptr);
            }
        });
    }

    TileChildrenResult createTileChildren(const Tile& tile,
                                          const Ellipsoid& /*ellipsoid*/) override {
        const auto* id = std::get_if<QuadtreeTileID>(&tile.getTileID());
        if (id == nullptr) {
            return {{}, TileLoadResultState::Failed};
        }
        if (id->level >= leaf_level_) {
            return {{}, TileLoadResultState::Success};
        }
        // Children start inside the parent's heights, with room for what its
        // coarser grid missed; each is tightened when it loads.
        double lowest = -1000.0;
        double highest = 9000.0;
        if (const auto* region =
                std::get_if<BoundingRegion>(&tile.getBoundingVolume())) {
            lowest = region->getMinimumHeight() - 250.0;
            highest = region->getMaximumHeight() + 250.0;
        }
        std::vector<Tile> children;
        children.reserve(4);
        for (uint32_t dy = 0; dy < 2; ++dy) {
            for (uint32_t dx = 0; dx < 2; ++dx) {
                children.push_back(make_tile(
                    QuadtreeTileID(id->level + 1, id->x * 2 + dx, id->y * 2 + dy),
                    lowest, highest));
            }
        }
        return {std::move(children), TileLoadResultState::Success};
    }

private:
    // A tile's rectangle: the region halved `level` times, `x` from the west
    // and `y` from the south, landing on the region's edges exactly.
    world::GeoRectangle rectangle(const QuadtreeTileID& id) const {
        const double n = std::ldexp(1.0, static_cast<int>(id.level));
        const auto split = [n](double from, double to, uint32_t i) {
            return i == 0 ? from
                          : (static_cast<double>(i) == n
                                 ? to
                                 : from + (to - from) * static_cast<double>(i) / n);
        };
        return {split(region_.south_deg, region_.north_deg, id.y),
                split(region_.west_deg, region_.east_deg, id.x),
                split(region_.south_deg, region_.north_deg, id.y + 1),
                split(region_.west_deg, region_.east_deg, id.x + 1)};
    }

    static GlobeRectangle globe(const world::GeoRectangle& r) {
        return GlobeRectangle::fromDegrees(r.west_deg, r.south_deg, r.east_deg,
                                           r.north_deg);
    }

    world::GeoRectangle region_;
    world::HeightSource heights_;
    bool imagery_ = false;
    std::atomic<std::size_t>& failures_;
    std::mutex mutex_;
    uint32_t leaf_level_ = 0;
};

// What a tile's glTF becomes: the renderer's meshes, placed.
// A picture ready for the GPU: an imagery tile's, or one out of a glTF.
struct RasterPixels {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct LoadedTile {
    std::vector<std::pair<Mesh, Placement>> meshes;
    // **A tile's own textures, where it has them.** The open provider's
    // tiles are coloured per vertex and Cesium ion's are draped with imagery,
    // so neither needs these; Google's Photorealistic 3D Tiles carry their
    // pictures inside their glTF, and without them they draw white. One
    // entry per mesh above: which of `images` it uses, or -1 for none.
    std::vector<int> mesh_image;
    std::vector<RasterPixels> images;
};

struct DrawnTile {
    std::vector<Draw> draws;
    std::vector<TextureId> textures; // the tile's own, to free with it
    bool imagery = false;            // an imagery tile is attached
};

struct RasterTexture {
    TextureId texture = no_texture;
};

// A decoded picture to RGBA, whatever it arrived as. Nothing but eight bits
// a channel with three or four of them is taken: anything else is a picture
// this does not know how to put on the GPU, and is drawn untextured rather
// than wrongly.
bool rgba_from_image(const CesiumImage::ImageAsset& image, RasterPixels& out) {
    if (image.bytesPerChannel != 1 ||
        (image.channels != 3 && image.channels != 4) || image.width <= 0 ||
        image.height <= 0) {
        return false;
    }
    out.width = image.width;
    out.height = image.height;
    const auto count = static_cast<std::size_t>(image.width) *
                       static_cast<std::size_t>(image.height);
    const auto channels = static_cast<std::size_t>(image.channels);
    if (image.pixelData.size() < count * channels) {
        return false;
    }
    out.rgba.resize(count * 4);
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            out.rgba[i * 4 + c] =
                static_cast<std::uint8_t>(image.pixelData[i * channels + c]);
        }
        out.rgba[i * 4 + 3] =
            channels == 4
                ? static_cast<std::uint8_t>(image.pixelData[i * channels + 3])
                : 255;
    }
    return true;
}

// Which of a glTF's images a primitive's base colour comes from, or -1.
int base_colour_image(const CesiumGltf::Model& gltf,
                      const CesiumGltf::MeshPrimitive& primitive) {
    if (primitive.material < 0 ||
        static_cast<std::size_t>(primitive.material) >= gltf.materials.size()) {
        return -1;
    }
    const auto& pbr =
        gltf.materials[static_cast<std::size_t>(primitive.material)]
            .pbrMetallicRoughness;
    if (!pbr || !pbr->baseColorTexture) {
        return -1;
    }
    const int32_t which = pbr->baseColorTexture->index;
    if (which < 0 || static_cast<std::size_t>(which) >= gltf.textures.size()) {
        return -1;
    }
    const int32_t source = gltf.textures[static_cast<std::size_t>(which)].source;
    if (source < 0 || static_cast<std::size_t>(source) >= gltf.images.size()) {
        return -1;
    }
    return source;
}

// Reads a vertex colour accessor of any type glTF allows into linear RGBA.
bool read_colours(const CesiumGltf::Model& model, int32_t accessor,
                  std::vector<Vertex>& vertices) {
    using CesiumGltf::AccessorView;
    using CesiumGltf::AccessorViewStatus;
    const auto fill = [&](const auto& view, float scale, bool alpha) {
        if (view.status() != AccessorViewStatus::Valid ||
            static_cast<std::size_t>(view.size()) != vertices.size()) {
            return false;
        }
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto& c = view[static_cast<int64_t>(i)];
            vertices[i].colour = {static_cast<float>(c[0]) * scale,
                                  static_cast<float>(c[1]) * scale,
                                  static_cast<float>(c[2]) * scale,
                                  alpha ? static_cast<float>(c[3]) * scale : 1.0f};
        }
        return true;
    };
    const CesiumGltf::Accessor& a =
        model.accessors.at(static_cast<std::size_t>(accessor));
    const bool vec4 = a.type == CesiumGltf::Accessor::Type::VEC4;
    switch (a.componentType) {
    case CesiumGltf::Accessor::ComponentType::FLOAT:
        return vec4 ? fill(AccessorView<glm::vec4>(model, accessor), 1.0f, true)
                    : fill(AccessorView<glm::vec3>(model, accessor), 1.0f, false);
    case CesiumGltf::Accessor::ComponentType::UNSIGNED_BYTE:
        return vec4 ? fill(AccessorView<glm::u8vec4>(model, accessor), 1.0f / 255, true)
                    : fill(AccessorView<glm::u8vec3>(model, accessor), 1.0f / 255,
                           false);
    case CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT:
        return vec4 ? fill(AccessorView<glm::u16vec4>(model, accessor), 1.0f / 65535,
                           true)
                    : fill(AccessorView<glm::u16vec3>(model, accessor), 1.0f / 65535,
                           false);
    default: return false;
    }
}

bool read_indices(const CesiumGltf::Model& model, int32_t accessor,
                  std::vector<std::uint32_t>& indices) {
    using CesiumGltf::AccessorView;
    using CesiumGltf::AccessorViewStatus;
    const auto fill = [&](const auto& view) {
        if (view.status() != AccessorViewStatus::Valid) {
            return false;
        }
        indices.resize(static_cast<std::size_t>(view.size()));
        for (int64_t i = 0; i < view.size(); ++i) {
            indices[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(view[i]);
        }
        return true;
    };
    switch (model.accessors.at(static_cast<std::size_t>(accessor)).componentType) {
    case CesiumGltf::Accessor::ComponentType::UNSIGNED_BYTE:
        return fill(AccessorView<std::uint8_t>(model, accessor));
    case CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT:
        return fill(AccessorView<std::uint16_t>(model, accessor));
    case CesiumGltf::Accessor::ComponentType::UNSIGNED_INT:
        return fill(AccessorView<std::uint32_t>(model, accessor));
    default: return false;
    }
}

// Every triangle primitive of a glTF model as a mesh placed on the Earth.
// Primitives this cannot read - not triangles, positions not floats - are
// skipped and counted.
void meshes_from_model(CesiumGltf::Model& model, const glm::dmat4& tile_transform,
                       LoadedTile& out, std::size_t& skipped) {
    glm::dmat4 root =
        CesiumGltfContent::GltfUtilities::applyRtcCenter(model, tile_transform);
    root = CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(model, root);
    model.forEachPrimitiveInScene(-1, [&](CesiumGltf::Model& gltf, CesiumGltf::Node&,
                                          CesiumGltf::Mesh&,
                                          CesiumGltf::MeshPrimitive& primitive,
                                          const glm::dmat4& node) {
        const auto position = primitive.attributes.find("POSITION");
        if (primitive.mode != CesiumGltf::MeshPrimitive::Mode::TRIANGLES ||
            position == primitive.attributes.end()) {
            ++skipped;
            return;
        }
        const CesiumGltf::AccessorView<glm::vec3> positions(gltf, position->second);
        if (positions.status() != CesiumGltf::AccessorViewStatus::Valid) {
            ++skipped;
            return;
        }
        Mesh mesh;
        mesh.vertices.resize(static_cast<std::size_t>(positions.size()));
        for (int64_t i = 0; i < positions.size(); ++i) {
            const glm::vec3 p = positions[i];
            mesh.vertices[static_cast<std::size_t>(i)] = {{p.x, p.y, p.z},
                                                          {0.8f, 0.8f, 0.8f, 1.0f}};
        }
        // Imagery's texture coordinates, which Cesium Native adds to a tile's
        // glTF for each raster overlay; the terrain has one.
        const auto overlay = primitive.attributes.find("_CESIUMOVERLAY_0");
        if (overlay != primitive.attributes.end()) {
            const CesiumGltf::AccessorView<glm::vec2> uvs(gltf, overlay->second);
            if (uvs.status() == CesiumGltf::AccessorViewStatus::Valid &&
                uvs.size() == positions.size()) {
                for (int64_t i = 0; i < uvs.size(); ++i) {
                    const glm::vec2 uv = uvs[i];
                    mesh.vertices[static_cast<std::size_t>(i)].uv = {uv.x, uv.y};
                }
            }
        }
        // A tile's own picture, where it has one and no imagery is draped
        // over it. Its coordinates are TEXCOORD_0; imagery brings its own
        // above, and a mesh has room for one set.
        int uses_image = -1;
        if (overlay == primitive.attributes.end()) {
            const int source = base_colour_image(gltf, primitive);
            const auto texcoord = primitive.attributes.find("TEXCOORD_0");
            if (source >= 0 && texcoord != primitive.attributes.end()) {
                const CesiumGltf::AccessorView<glm::vec2> uvs(gltf, texcoord->second);
                if (uvs.status() == CesiumGltf::AccessorViewStatus::Valid &&
                    uvs.size() == positions.size()) {
                    for (int64_t i = 0; i < uvs.size(); ++i) {
                        const glm::vec2 uv = uvs[i];
                        mesh.vertices[static_cast<std::size_t>(i)].uv = {uv.x, uv.y};
                    }
                    uses_image = source;
                }
            }
        }

        const auto colour = primitive.attributes.find("COLOR_0");
        if (colour == primitive.attributes.end() ||
            !read_colours(gltf, colour->second, mesh.vertices)) {
            // The shader draws the vertex colour times the texture, so a
            // textured surface starts white and the picture is what is seen.
            std::array<float, 4> base{0.8f, 0.8f, 0.8f, 1.0f};
            if (uses_image >= 0) {
                base = {1.0f, 1.0f, 1.0f, 1.0f};
            }
            if (primitive.material >= 0 &&
                static_cast<std::size_t>(primitive.material) < gltf.materials.size()) {
                const auto& pbr =
                    gltf.materials[static_cast<std::size_t>(primitive.material)]
                        .pbrMetallicRoughness;
                if (pbr) {
                    for (std::size_t k = 0; k < 4; ++k) {
                        base[k] = static_cast<float>(pbr->baseColorFactor[k]);
                    }
                }
            }
            for (Vertex& v : mesh.vertices) {
                v.colour = base;
            }
        }
        if (primitive.indices >= 0) {
            if (!read_indices(gltf, primitive.indices, mesh.indices)) {
                ++skipped;
                return;
            }
        } else {
            mesh.indices.resize(mesh.vertices.size());
            for (std::size_t i = 0; i < mesh.indices.size(); ++i) {
                mesh.indices[i] = static_cast<std::uint32_t>(i);
            }
        }
        const glm::dmat4 m = root * node;
        Placement placement;
        placement.origin = {m[3][0], m[3][1], m[3][2]};
        placement.world_from_local =
            Mat3::columns({m[0][0], m[0][1], m[0][2]}, {m[1][0], m[1][1], m[1][2]},
                          {m[2][0], m[2][1], m[2][2]});
        out.meshes.emplace_back(std::move(mesh), placement);
        out.mesh_image.push_back(uses_image);
    });

    // Each picture the meshes above use, decoded once. The indices recorded
    // are into the glTF's images; they become indices into `out.images`.
    std::vector<int> where(model.images.size(), -1);
    for (int& which : out.mesh_image) {
        if (which < 0) {
            continue;
        }
        const auto at = static_cast<std::size_t>(which);
        if (where[at] < 0) {
            RasterPixels pixels;
            const auto& image = model.images[at];
            if (!image.pAsset || !rgba_from_image(*image.pAsset, pixels)) {
                which = -1;
                continue;
            }
            where[at] = static_cast<int>(out.images.size());
            out.images.push_back(std::move(pixels));
        }
        which = where[at];
    }
}

class RendererResources final
    : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
    RendererResources(Renderer& renderer, std::atomic<std::size_t>& skipped)
        : renderer_(renderer), skipped_(skipped) {}

    CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
    prepareInLoadThread(const CesiumAsync::AsyncSystem& async, TileLoadResult&& result,
                        const glm::dmat4& transform,
                        const std::any& /*rendererOptions*/) override {
        auto loaded = std::make_unique<LoadedTile>();
        if (auto* model = std::get_if<CesiumGltf::Model>(&result.contentKind)) {
            std::size_t skipped = 0;
            meshes_from_model(*model, transform, *loaded, skipped);
            skipped_ += skipped;
        }
        return async.createResolvedFuture(
            Cesium3DTilesSelection::TileLoadResultAndRenderResources{std::move(result),
                                                                     loaded.release()});
    }

    void* prepareInMainThread(Tile& /*tile*/, void* load_result) override {
        const std::unique_ptr<LoadedTile> loaded(static_cast<LoadedTile*>(load_result));
        if (!loaded) {
            return nullptr;
        }
        auto drawn = std::make_unique<DrawnTile>();
        for (const RasterPixels& pixels : loaded->images) {
            drawn->textures.push_back(renderer_.add_texture(
                pixels.width, pixels.height, pixels.rgba.data()));
        }
        for (std::size_t i = 0; i < loaded->meshes.size(); ++i) {
            const auto& [mesh, placement] = loaded->meshes[i];
            if (mesh.indices.empty()) {
                continue;
            }
            Draw draw{renderer_.add_mesh(mesh), placement};
            const int which =
                i < loaded->mesh_image.size() ? loaded->mesh_image[i] : -1;
            if (which >= 0 && static_cast<std::size_t>(which) < drawn->textures.size()) {
                draw.texture = drawn->textures[static_cast<std::size_t>(which)];
            }
            drawn->draws.push_back(draw);
        }
        return drawn.release();
    }

    void free(Tile& /*tile*/, void* load_result, void* main_result) noexcept override {
        delete static_cast<LoadedTile*>(load_result);
        const std::unique_ptr<DrawnTile> drawn(static_cast<DrawnTile*>(main_result));
        if (drawn) {
            for (const Draw& d : drawn->draws) {
                renderer_.remove_mesh(d.mesh);
            }
            for (const TextureId id : drawn->textures) {
                renderer_.remove_texture(id);
            }
        }
    }

    // Imagery: each tile decoded to RGBA on a worker thread, uploaded as a
    // texture on the main thread, and put on the terrain tiles it covers, with
    // the scale and offset that take a tile's coordinates into its part of it.
    void* prepareRasterInLoadThread(CesiumImage::ImageAsset& image,
                                    const std::any& /*rendererOptions*/) override {
        auto pixels = std::make_unique<RasterPixels>();
        if (!rgba_from_image(image, *pixels)) {
            ++skipped_;
            return nullptr;
        }
        return pixels.release();
    }
    void* prepareRasterInMainThread(CesiumRasterOverlays::RasterOverlayTile& /*tile*/,
                                    void* load_result) override {
        const std::unique_ptr<RasterPixels> pixels(
            static_cast<RasterPixels*>(load_result));
        if (!pixels) {
            return nullptr;
        }
        auto texture = std::make_unique<RasterTexture>();
        texture->texture =
            renderer_.add_texture(pixels->width, pixels->height, pixels->rgba.data());
        return texture.release();
    }
    void freeRaster(const CesiumRasterOverlays::RasterOverlayTile& /*tile*/,
                    void* load_result, void* main_result) noexcept override {
        delete static_cast<RasterPixels*>(load_result);
        const std::unique_ptr<RasterTexture> texture(
            static_cast<RasterTexture*>(main_result));
        if (texture && texture->texture != no_texture) {
            renderer_.remove_texture(texture->texture);
        }
    }
    // Cesium Native hands over the imagery tile's own resources - what
    // prepareRasterInMainThread made - and the terrain tile, whose are the
    // draws.
    static DrawnTile* drawn_of(const Tile& tile) {
        const auto* content = tile.getContent().getRenderContent();
        return content == nullptr
                   ? nullptr
                   : static_cast<DrawnTile*>(content->getRenderResources());
    }
    void
    attachRasterInMainThread(const Tile& tile, int32_t coordinates,
                             const CesiumRasterOverlays::RasterOverlayTile& /*raster*/,
                             void* raster_result, const glm::dvec2& translation,
                             const glm::dvec2& scale) override {
        DrawnTile* drawn = drawn_of(tile);
        const auto* texture = static_cast<const RasterTexture*>(raster_result);
        if (drawn == nullptr || texture == nullptr || coordinates != 0) {
            return;
        }
        // The overlay's coordinates run north from the tile's south edge; the
        // image's rows run south from its top.
        for (Draw& d : drawn->draws) {
            d.texture = texture->texture;
            d.uv_transform = {static_cast<float>(scale.x), static_cast<float>(-scale.y),
                              static_cast<float>(translation.x),
                              static_cast<float>(1.0 - translation.y)};
        }
        drawn->imagery = true;
    }
    void
    detachRasterInMainThread(const Tile& tile, int32_t coordinates,
                             const CesiumRasterOverlays::RasterOverlayTile& /*raster*/,
                             void* /*raster_result*/) noexcept override {
        DrawnTile* drawn = drawn_of(tile);
        if (drawn == nullptr || coordinates != 0) {
            return;
        }
        for (Draw& d : drawn->draws) {
            d.texture = no_texture;
            d.uv_transform = {1.0f, 1.0f, 0.0f, 0.0f};
        }
        drawn->imagery = false;
    }

private:
    Renderer& renderer_;
    std::atomic<std::size_t>& skipped_;
};

} // namespace

// Cesium ion's own asset numbers, which are the same for every account.
constexpr std::int64_t ion_world_terrain_asset = 1;      // Cesium World Terrain
constexpr std::int64_t ion_aerial_imagery_asset = 2;     // Bing Maps Aerial
constexpr std::int64_t google_photorealistic_asset = 2275207; // through ion

struct TerrainTiles::Impl {
    std::shared_ptr<std::atomic<int>> requests = std::make_shared<std::atomic<int>>(0);
    // Raised as the terrain closes: every transfer still going is given up.
    std::shared_ptr<std::atomic<bool>> closing = std::make_shared<std::atomic<bool>>(false);
    std::atomic<std::size_t> failures{0};
    std::atomic<std::size_t> skipped{0};
    bool imagery = false;
    Provider provider = Provider::open;
    std::shared_ptr<CesiumUtility::CreditSystem> credit_system;
    // What the provider itself said must be shown, before a tile has loaded.
    std::vector<std::string> from_provider;
    std::shared_ptr<WorkerPool> workers;
    CesiumAsync::AsyncSystem async;
    std::unique_ptr<Cesium3DTilesSelection::Tileset> tileset;
    Counts counts;

    explicit Impl(int threads)
        : workers(std::make_shared<WorkerPool>(threads)), async(workers) {}
};

namespace {

const std::vector<Provider>& providers() {
    static const std::vector<Provider> all{Provider::open, Provider::ion,
                                           Provider::google};
    return all;
}

} // namespace

const std::vector<Provider>& every_provider() {
    return providers();
}

std::string_view name_of(Provider provider) {
    switch (provider) {
    case Provider::open:
        return "open";
    case Provider::ion:
        return "ion";
    case Provider::google:
        return "google";
    }
    return "open";
}

std::optional<Provider> provider_named(std::string_view name) {
    for (const Provider provider : providers()) {
        if (name_of(provider) == name) {
            return provider;
        }
    }
    return std::nullopt;
}

std::string provider_names() {
    std::string out;
    for (const Provider provider : providers()) {
        out += (out.empty() ? "" : ", ") + std::string(name_of(provider));
    }
    return out;
}

std::string why_not(Provider provider, const std::string& ion_token,
                    const std::string& google_key) {
    switch (provider) {
    case Provider::open:
        return {}; // the open data needs nothing of the user
    case Provider::ion:
        if (ion_token.empty()) {
            return "Cesium ion needs your own token: put it in "
                   "cesium-ion-token in glideslope's config directory, or set "
                   "GLIDESLOPE_CESIUM_ION_TOKEN. One is free from "
                   "https://cesium.com/ion/";
        }
        return {};
    case Provider::google:
        if (google_key.empty() && ion_token.empty()) {
            return "Google's Photorealistic 3D Tiles need either your own "
                   "Google Maps Platform key - google-maps-key in glideslope's "
                   "config directory, or GLIDESLOPE_GOOGLE_MAPS_KEY - or your "
                   "own Cesium ion token, which serves them too";
        }
        return {};
    }
    return {};
}

namespace {

// What Cesium ion says about an asset: where its tiles are, and the token
// that authorises them.
// Everything up to the end of a URL's host: the place a token may go to.
std::string host_of(const std::string& url) {
    const std::size_t scheme = url.find("://");
    if (scheme == std::string::npos) {
        return url;
    }
    const std::size_t slash = url.find('/', scheme + 3);
    return slash == std::string::npos ? url : url.substr(0, slash);
}

struct IonEndpoint {
    std::string type;          // "TERRAIN", "IMAGERY" or "3DTILES"
    std::string external_type; // "BING" for imagery ion does not serve itself
    std::string url;
    std::string access_token; // what ion serves itself is authorised by this
    std::string key;          // what it does not is authorised by its own
    std::string style;
    std::vector<std::string> attributions; // HTML, as ion gives it
};

// Asks Cesium ion where an asset is.
//
// **This is done here rather than by Cesium Native's own ion loader**, which
// cannot be used: in v0.64.0 `TileLoadInput::pAssetAccessor` is a reference
// member, `CesiumIonTilesetLoader` holds a `shared_ptr` to a *derived*
// accessor, and passing it makes a temporary `shared_ptr<IAssetAccessor>`
// that is bound to that reference and destroyed at the end of the statement.
// Every tile load then reads a dangling reference, which the sanitized build
// catches as a stack-use-after-scope. The open provider never meets it,
// because the types match there and no temporary is made. Asking ion for the
// endpoint is one plain GET, and the tiles are then an ordinary tileset with
// an Authorization header, which goes nowhere near that code.
IonEndpoint ion_endpoint(std::int64_t asset, const std::string& token) {
    platform::HttpRequest q;
    q.url = platform::cesium_ion_api() + "/v1/assets/" + std::to_string(asset) +
            "/endpoint?access_token=" + token;
    q.user_agent = "glideslope (+https://github.com/GavinMGlynn/glideslope)";
    // Asked on the main thread, before there is a terrain to close: a
    // program told to stop gives it up.
    q.abandon = &platform::stop_flag();
    const platform::HttpResponse response = platform::http_get(q);
    if (response.status != 200) {
        throw std::runtime_error(
            "Cesium ion would not say where asset " + std::to_string(asset) +
            " is: it answered " + std::to_string(response.status) +
            (response.status == 401 || response.status == 403
                 ? ", which means the token is not one it accepts"
                 : ""));
    }
    const world::Json said = world::parse_json(std::string_view(
        reinterpret_cast<const char*>(response.body.data()), response.body.size()));
    const world::Json* url = said.find("url");
    const world::Json* access = said.find("accessToken");
    const world::Json* type = said.find("type");
    IonEndpoint endpoint;
    if (type != nullptr && type->kind() == world::Json::Kind::string) {
        endpoint.type = type->string();
    }
    if (const world::Json* external = said.find("externalType");
        external != nullptr && external->kind() == world::Json::Kind::string) {
        endpoint.external_type = external->string();
    }
    // What ion serves itself carries a url and a token; what it does not -
    // Bing's imagery, say - carries the other service's own url and key
    // under "options" instead.
    if (url != nullptr && access != nullptr) {
        endpoint.url = url->string();
        endpoint.access_token = access->string();
    } else if (const world::Json* o = said.find("options");
               o != nullptr && o->kind() == world::Json::Kind::object) {
        if (const world::Json* u = o->find("url"); u != nullptr) {
            endpoint.url = u->string();
        }
        if (const world::Json* k = o->find("key"); k != nullptr) {
            endpoint.key = k->string();
        }
        if (const world::Json* s = o->find("mapStyle"); s != nullptr) {
            endpoint.style = s->string();
        }
    }
    if (endpoint.url.empty()) {
        throw std::runtime_error("Cesium ion's answer for asset " +
                                 std::to_string(asset) + " says nowhere to "
                                 "fetch it from");
    }
    // Quantized-mesh terrain is served from a directory whose layer.json
    // describes it; 3D Tiles are the URL itself.
    if (endpoint.type == "TERRAIN") {
        if (endpoint.url.back() != '/') {
            endpoint.url += '/';
        }
        endpoint.url += "layer.json";
    }
    // What ion says must be shown wherever the asset is drawn.
    if (const world::Json* credits = said.find("attributions");
        credits != nullptr && credits->kind() == world::Json::Kind::array) {
        for (const world::Json& credit : credits->array()) {
            if (credit.kind() != world::Json::Kind::object) {
                continue;
            }
            if (const world::Json* html = credit.find("html");
                html != nullptr && html->kind() == world::Json::Kind::string) {
                endpoint.attributions.push_back(html->string());
            }
        }
    }
    return endpoint;
}

} // namespace

TerrainTiles::TerrainTiles(Renderer& renderer, const TerrainOptions& options,
                           world::HeightSource heights)
    : impl_(std::make_unique<Impl>(options.worker_threads)) {
    register_tile_readers();
    // What is fetched, through the platform's HTTPS, kept in Cesium Native's
    // SQLite cache for as long as its caching headers allow.
    std::shared_ptr<CesiumAsync::IAssetAccessor> accessor =
        std::make_shared<PlatformAccessor>(impl_->closing);
    if (!options.cache_file.empty()) {
        std::error_code error;
        std::filesystem::create_directories(options.cache_file.parent_path(), error);
        accessor = std::make_shared<CesiumAsync::CachingAssetAccessor>(
            spdlog::default_logger(), accessor,
            std::make_shared<CesiumAsync::SqliteCache>(spdlog::default_logger(),
                                                       options.cache_file.string()));
    }
    accessor = std::make_shared<CountingAccessor>(accessor, impl_->requests);
    // Cesium ion authorises every tile, not just the tileset that names them,
    // so the token goes on each request to where that asset is served from.
    // Cesium Native does this with an accessor of its own, which cannot be
    // used here - see ion_endpoint below - so this is one of ours.
    // The query a URL carries, for its children to carry too.
    const auto query_of = [](const std::string& url) {
        const std::size_t question = url.find('?');
        return question == std::string::npos ? std::string()
                                             : url.substr(question + 1);
    };
    const auto carrying = [&accessor](const std::string& prefix,
                                      const std::string& query) {
        if (query.empty()) {
            return;
        }
        accessor = std::make_shared<QueryAccessor>(accessor, prefix, query);
    };
    const auto authorising = [&accessor](const std::string& prefix,
                                         const std::string& token) {
        // Not everything ion points at needs a token of ours. Google's
        // Photorealistic 3D Tiles come back as a URL that already carries
        // what it needs and no accessToken beside it; putting an empty
        // bearer on those requests is how they came back as errors nothing
        // could parse.
        if (token.empty()) {
            return;
        }
        accessor = std::make_shared<AuthorisingAccessor>(accessor, prefix, token);
    };
    Cesium3DTilesSelection::TilesetExternals externals{
        accessor, std::make_shared<RendererResources>(renderer, impl_->skipped),
        impl_->async, std::make_shared<CesiumUtility::CreditSystem>()};

    impl_->credit_system = externals.pCreditSystem;
    impl_->provider = options.provider;
    impl_->imagery = options.provider == Provider::open && options.imagery.has_value();

    Cesium3DTilesSelection::TilesetOptions tileset_options;
    tileset_options.maximumScreenSpaceError = options.maximum_screen_space_error;
    tileset_options.ellipsoid = Ellipsoid::WGS84;
    // Fog culls what is hazy far away; the renderer draws no haze.
    tileset_options.enableFogCulling = false;

    if (options.provider != Provider::open) {
        const std::string reason =
            why_not(options.provider, options.ion_token, options.google_key);
        if (!reason.empty()) {
            throw std::runtime_error(reason);
        }
    }
    switch (options.provider) {
    case Provider::open: {
        // The Copernicus DEM, built into tiles here: Cesium Native streams
        // quantized-mesh and 3D Tiles, and nothing serves the DEM as either.
        auto loader = std::make_unique<DemLoader>(options.region, std::move(heights),
                                                  impl_->imagery, impl_->failures);
        auto root = std::make_unique<Tile>(
            loader->make_tile(QuadtreeTileID(0, 0, 0), -1000.0, 9000.0));
        impl_->tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
            externals, std::move(loader), std::move(root), tileset_options);
        break;
    }
    case Provider::ion: {
        // Cesium World Terrain is ion asset 1, and Bing Maps Aerial - the
        // imagery ion drapes on it - asset 2. Both are asked for by name and
        // then fetched as ordinary tilesets; see ion_endpoint above for why.
        const IonEndpoint terrain =
            ion_endpoint(ion_world_terrain_asset, options.ion_token);
        authorising(host_of(terrain.url), terrain.access_token);
        externals.pAssetAccessor = accessor;
        impl_->tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
            externals, terrain.url, tileset_options);
        const IonEndpoint imagery =
            ion_endpoint(ion_aerial_imagery_asset, options.ion_token);
        impl_->from_provider = terrain.attributions;
        impl_->from_provider.insert(impl_->from_provider.end(),
                                    imagery.attributions.begin(),
                                    imagery.attributions.end());
        impl_->tileset->getOverlays().add(
            CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlay>(
                new CesiumRasterOverlays::BingMapsRasterOverlay(
                    "ion imagery", imagery.url, imagery.key,
                    imagery.style.empty()
                        ? CesiumRasterOverlays::BingMapsStyle::AERIAL
                        : imagery.style)));
        break;
    }
    case Provider::google: {
        // Photorealistic 3D Tiles carry their own imagery, so nothing is
        // draped on them. A Google key reaches them directly; an ion token
        // reaches the same tiles through ion's asset 2275207.
        if (!options.google_key.empty()) {
            const std::string root =
                "https://tile.googleapis.com/v1/3dtiles/root.json?key=" +
                options.google_key;
            carrying(host_of(root), query_of(root));
            externals.pAssetAccessor = accessor;
            impl_->tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
                externals, root, tileset_options);
        } else {
            const IonEndpoint google =
                ion_endpoint(google_photorealistic_asset, options.ion_token);
            impl_->from_provider = google.attributions;
            authorising(host_of(google.url), google.access_token);
            carrying(host_of(google.url), query_of(google.url));
            externals.pAssetAccessor = accessor;
            impl_->tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
                externals, google.url, tileset_options);
        }
        break;
    }
    }

    if (impl_->imagery && options.imagery) {
        const Imagery& imagery = *options.imagery;
        CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions wmts;
        wmts.format = imagery.format;
        wmts.layer = imagery.layer;
        wmts.style = imagery.style;
        wmts.tileMatrixSetID = imagery.tile_matrix_set;
        wmts.maximumLevel = imagery.maximum_level;
        wmts.credit = imagery.credit;
        wmts.projection = CesiumGeospatial::GeographicProjection(Ellipsoid::WGS84);
        impl_->tileset->getOverlays().add(
            CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlay>(
                new CesiumRasterOverlays::WebMapTileServiceRasterOverlay(
                    "imagery", imagery.url, {}, wmts)));
    }
}

namespace {

// Cesium Native's credits are HTML; the HUD draws plain text. Tags are taken
// out and the text between them kept, which is what a credit says.
std::string without_tags(const std::string& html) {
    std::string out;
    bool inside = false;
    for (const char c : html) {
        if (c == '<') {
            inside = true;
        } else if (c == '>') {
            inside = false;
        } else if (!inside) {
            out += c;
        }
    }
    // One space between words, and none either end.
    std::string tidy;
    bool space = true;
    for (const char c : out) {
        const bool blank = c == ' ' || c == '\t' || c == '\n' || c == '\r';
        if (blank) {
            space = true;
            continue;
        }
        if (space && !tidy.empty()) {
            tidy += ' ';
        }
        space = false;
        tidy += c;
    }
    return tidy;
}

} // namespace

std::vector<std::optional<double>> TerrainTiles::heights_at(
    const std::vector<world::Geodetic>& places) {
    std::vector<CesiumGeospatial::Cartographic> ask;
    ask.reserve(places.size());
    for (const world::Geodetic& g : places) {
        ask.push_back(CesiumGeospatial::Cartographic::fromDegrees(
            g.longitude_deg, g.latitude_deg, 0.0));
    }
    // **Asking once is not enough, and the answer says nothing about that.**
    // A sample reports success whether or not the tiles beneath it had
    // arrived: at Boston and Anchorage it answered, repeatably, with a
    // surface tens of kilometres below the ellipsoid, while Denver and Las
    // Vegas - whose two runway ends sit in one whole-degree cell rather than
    // straddling two, so the same call had twice as long to load - answered
    // properly. So it is asked again until two answers running agree, which
    // is the only thing that says the tiles it needed were there.
    constexpr int asks = 8;
    constexpr double settled_m = 0.01;
    std::vector<std::optional<double>> out(places.size(), std::nullopt);
    std::vector<std::optional<double>> before;
    for (int ask_round = 0; ask_round < asks; ++ask_round) {
        auto asked = impl_->tileset->sampleHeightMostDetailed(ask);
        // The answer needs tiles, which need the workers, whose results are
        // taken up here: waiting on the future alone would wait for ever.
        constexpr int rounds = 1200; // at 50 ms, a minute at most
        for (int round = 0;
             round < rounds && !asked.isReady() && !platform::stop_requested(); ++round) {
            impl_->tileset->loadTiles();
            impl_->async.dispatchMainThreadTasks();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (platform::stop_requested()) {
            return out; // told to stop: what is known, and no more waiting
        }
        if (!asked.isReady()) {
            continue; // not this time; the loading above carries on regardless
        }
        const Cesium3DTilesSelection::SampleHeightResult result =
            asked.waitInMainThread();
        out.assign(places.size(), std::nullopt);
        for (std::size_t i = 0; i < out.size() && i < result.positions.size(); ++i) {
            if (i < result.sampleSuccess.size() && result.sampleSuccess[i]) {
                out[i] = result.positions[i].height;
            }
        }
        // **A place that has not answered is not a place that has settled.**
        // Comparing "no answer" with "no answer" and calling them equal
        // would stop the asking on the second round, which is the least
        // loading time this can give - and a cell whose terrain is merely
        // slow would be reported as having no surface. So every place must
        // have a height, and two rounds running must agree on it.
        bool all_answered = true;
        for (const std::optional<double>& height : out) {
            if (!height) {
                all_answered = false;
                break;
            }
        }
        if (all_answered && !before.empty()) {
            bool same = true;
            for (std::size_t i = 0; i < out.size(); ++i) {
                if (!before[i] || std::abs(*out[i] - *before[i]) > settled_m) {
                    same = false;
                    break;
                }
            }
            if (same) {
                return out;
            }
        }
        before = out;
    }
    return out;
}

Provider TerrainTiles::provider() const {
    return impl_->provider;
}

std::vector<std::string> TerrainTiles::credits() const {
    std::vector<std::string> out;
    for (const std::string& html : impl_->from_provider) {
        std::string text = without_tags(html);
        if (!text.empty() && std::find(out.begin(), out.end(), text) == out.end()) {
            out.push_back(std::move(text));
        }
    }
    if (impl_->credit_system) {
        const CesiumUtility::CreditsSnapshot& snapshot =
            impl_->credit_system->getSnapshot();
        for (const CesiumUtility::Credit& credit : snapshot.currentCredits) {
            std::string text = without_tags(impl_->credit_system->getHtml(credit));
            if (!text.empty() &&
                std::find(out.begin(), out.end(), text) == out.end()) {
                out.push_back(std::move(text));
            }
        }
    }
    return out;
}

Imagery open_imagery() {
    Imagery imagery;
    imagery.url =
        "https://tiles.maps.eox.at/wmts/1.0.0/{Layer}/{Style}/{TileMatrixSet}/"
        "{TileMatrix}/{TileRow}/{TileCol}.jpg";
    imagery.layer = "s2cloudless";
    imagery.style = "default";
    imagery.tile_matrix_set = "WGS84";
    imagery.format = "image/jpeg";
    // Level 13 is 9.5 m a pixel at the equator, as fine as the 10 m mosaic.
    imagery.maximum_level = 13;
    imagery.credit = "Sentinel-2 cloudless - https://s2maps.eu by EOX IT Services GmbH "
                     "(Contains modified Copernicus Sentinel data 2016)";
    return imagery;
}

TerrainTiles::~TerrainTiles() {
    // **Every transfer still going is given up first.** What follows waits
    // for the tileset's loads and for every request in flight, and a tile
    // server that answers and then never finishes - a body a byte at a time
    // defeats every stall timeout there is - kept that wait going for ever:
    // the program never ended, and held its cache open while it did not
    // (a tail, and tests/cmake/ion_stalled.cmake). Nothing is waiting for
    // what they would bring, so each ends at once as a failed request.
    impl_->closing->store(true);
    // Cesium Native finishes destroying a tileset on the main thread, freeing
    // each tile's meshes as it goes.
    auto destroyed = impl_->tileset->getAsyncDestructionCompleteEvent();
    impl_->tileset.reset();
    while (!destroyed.isReady()) {
        impl_->async.dispatchMainThreadTasks();
        std::this_thread::yield();
    }
    // And its workers stop here, with it, whatever still holds them - once no
    // request is in flight, what they were doing is done, and what that left
    // for the main thread has run, which may have given them more. Work left
    // waiting when they stop would never run, and what it holds - Cesium
    // Native's imagery among it - never be freed.
    for (;;) {
        const bool idle = impl_->workers->idle() && *impl_->requests == 0;
        bool dispatched = false;
        while (impl_->async.dispatchOneMainThreadTask()) {
            dispatched = true;
        }
        if (idle && !dispatched) {
            break;
        }
        std::this_thread::yield();
    }
    impl_->workers->stop();
}

std::vector<Draw> TerrainTiles::update(const Camera& camera, int width, int height,
                                       bool complete) {
    const Mat3& axes = camera.world_from_camera;
    const glm::dvec3 up(axes.m[3], axes.m[4], axes.m[5]);
    const glm::dvec3 direction(-axes.m[6], -axes.m[7], -axes.m[8]);
    const double aspect = static_cast<double>(width) / static_cast<double>(height);
    const double vertical = camera.vertical_fov_rad;
    const double horizontal = 2.0 * std::atan(std::tan(vertical / 2.0) * aspect);
    const Cesium3DTilesSelection::ViewState view(
        glm::dvec3(camera.position.x, camera.position.y, camera.position.z), direction,
        up, glm::dvec2(width, height), horizontal, vertical, Ellipsoid::WGS84);

    auto& tileset = *impl_->tileset;
    auto& group = tileset.getDefaultViewGroup();

    // **"Every tile this view needs" is a finite thing only for the open
    // provider.** Its terrain is built here, over a region, and stops at the
    // DEM's own spacing, so waiting for all of it makes the same command draw
    // the same terrain on every machine - which is what a shot is for.
    //
    // A streamed provider has no such end: it covers the Earth and refines
    // until it runs out of levels, and what arrives depends on the network
    // and on the provider. Waiting for all of it ran past 25 minutes. So it
    // is given a settling instead - rounds of loading until what is drawn
    // stops growing, and never more than the cap - and its frame is not
    // claimed to be the same everywhere, because it cannot be. What is
    // claimed of it is that it drew terrain, and that its attribution is on
    // it.
    const bool finite = impl_->provider == Provider::open;
    if (complete && !finite) {
        // Refinement arrives in waves - a level loads, and asking again
        // asks for the one below it - so "settled" has to mean quiet for a
        // while, not quiet for a moment: at half a second it stopped at
        // level 2 where waiting for everything reaches level 12.
        constexpr int rounds = 900;    // at 50 ms, three quarters of a minute
        constexpr int steady_for = 60; // three seconds unchanged
        std::size_t was = 0;
        std::size_t was_deepest = 0;
        std::size_t was_held = 0;
        int steady = 0;
        // A program told to stop does not wait for its terrain to settle.
        for (int round = 0; round < rounds && !platform::stop_requested(); ++round) {
            const auto& loading = tileset.updateViewGroup(group, {view}, 0.0f);
            tileset.loadTiles();
            // What the workers finished has to be taken up here: the offline
            // wait does this itself, and without it nothing ever arrives.
            impl_->async.dispatchMainThreadTasks();
            std::size_t drawn = 0;
            std::size_t deepest = 0;
            for (const auto& tile : loading.tilesToRenderThisFrame) {
                if (tile->getContent().getRenderContent() == nullptr) {
                    continue;
                }
                ++drawn;
                if (const auto* id = std::get_if<QuadtreeTileID>(&tile->getTileID())) {
                    deepest = std::max<std::size_t>(deepest, id->level);
                }
            }
            // How many tiles the tileset holds, which keeps rising while it
            // is still refining. What is drawn can sit still for a moment
            // part-way down, and the level cannot be watched at all for a
            // tileset whose tiles are not a quadtree - Google's are not, and
            // reported level 0 throughout while it was still coming.
            const auto held =
                static_cast<std::size_t>(tileset.getNumberOfTilesLoaded());
            steady = drawn == was && deepest == was_deepest && held == was_held
                         ? steady + 1
                         : 0;
            was = drawn;
            was_deepest = deepest;
            was_held = held;
            if (drawn > 0 && steady >= steady_for) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    const Cesium3DTilesSelection::ViewUpdateResult& result =
        complete && finite ? tileset.updateViewGroupOffline(group, {view})
                           : tileset.updateViewGroup(group, {view}, 0.0f);
    if (!(complete && finite)) {
        tileset.loadTiles();
    }

    std::vector<Draw> draws;
    impl_->counts.drawn = 0;
    impl_->counts.deepest = 0;
    impl_->counts.without_imagery = 0;
    for (const auto& tile : result.tilesToRenderThisFrame) {
        const auto* content = tile->getContent().getRenderContent();
        if (content == nullptr) {
            continue;
        }
        const auto* drawn =
            static_cast<const DrawnTile*>(content->getRenderResources());
        if (drawn == nullptr) {
            continue;
        }
        draws.insert(draws.end(), drawn->draws.begin(), drawn->draws.end());
        ++impl_->counts.drawn;
        if (impl_->imagery && !drawn->imagery) {
            ++impl_->counts.without_imagery;
        }
        if (const auto* id = std::get_if<QuadtreeTileID>(&tile->getTileID())) {
            impl_->counts.deepest =
                std::max<std::size_t>(impl_->counts.deepest, id->level);
        }
    }
    impl_->counts.loaded = static_cast<std::size_t>(tileset.getNumberOfTilesLoaded());
    impl_->counts.failed = impl_->failures.load();
    impl_->counts.skipped = impl_->skipped.load();
    return draws;
}

TerrainTiles::Counts TerrainTiles::counts() const {
    return impl_->counts;
}

} // namespace glideslope::gfx
