#include "gfx/terrain_tiles.hpp"

#include "gfx/terrain_colour.hpp"
#include "platform/http.hpp"

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/TileLoadResult.h>
#include <Cesium3DTilesSelection/TileRefine.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetContentLoader.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/CachingAssetAccessor.h>
#include <CesiumAsync/SqliteCache.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumGeometry/Axis.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeospatial/BoundingRegion.h>
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

#include <spdlog/spdlog.h>

#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <atomic>
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
class PlatformAccessor final : public CesiumAsync::IAssetAccessor {
public:
    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    get(const CesiumAsync::AsyncSystem& async, const std::string& url,
        const std::vector<THeader>& headers) override {
        return request(async, "GET", url, headers, {});
    }

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    request(const CesiumAsync::AsyncSystem& async, const std::string& verb,
            const std::string& url, const std::vector<THeader>& headers,
            const std::span<const std::byte>& body) override {
        const bool plain = verb == "GET" && headers.empty() && body.empty();
        return async.runInWorkerThread(
            [verb, url, plain]() -> std::shared_ptr<CesiumAsync::IAssetRequest> {
                auto request = std::make_shared<Request>(verb, url);
                if (!plain) {
                    std::fprintf(stderr,
                                 "glideslope: %s %s refused: only plain GETs are "
                                 "made\n",
                                 verb.c_str(), url.c_str());
                    return request;
                }
                try {
                    platform::HttpRequest q;
                    q.url = url;
                    q.user_agent =
                        "glideslope (+https://github.com/GavinMGlynn/glideslope)";
                    request->set(platform::http_get(q));
                } catch (const platform::HttpError& e) {
                    std::fprintf(stderr, "glideslope: %s: %s\n", url.c_str(), e.what());
                }
                return request;
            });
    }

    void tick() noexcept override {}

private:
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
struct LoadedTile {
    std::vector<std::pair<Mesh, Placement>> meshes;
};

struct DrawnTile {
    std::vector<Draw> draws;
    bool imagery = false; // an imagery tile is attached
};

// An imagery tile: its pixels once decoded, then its texture on the GPU.
struct RasterPixels {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct RasterTexture {
    TextureId texture = no_texture;
};

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
        const auto colour = primitive.attributes.find("COLOR_0");
        if (colour == primitive.attributes.end() ||
            !read_colours(gltf, colour->second, mesh.vertices)) {
            std::array<float, 4> base{0.8f, 0.8f, 0.8f, 1.0f};
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
    });
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
        for (const auto& [mesh, placement] : loaded->meshes) {
            if (!mesh.indices.empty()) {
                drawn->draws.push_back({renderer_.add_mesh(mesh), placement});
            }
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
        }
    }

    // Imagery: each tile decoded to RGBA on a worker thread, uploaded as a
    // texture on the main thread, and put on the terrain tiles it covers, with
    // the scale and offset that take a tile's coordinates into its part of it.
    void* prepareRasterInLoadThread(CesiumImage::ImageAsset& image,
                                    const std::any& /*rendererOptions*/) override {
        if (image.bytesPerChannel != 1 ||
            (image.channels != 3 && image.channels != 4) || image.width <= 0 ||
            image.height <= 0) {
            ++skipped_;
            return nullptr;
        }
        auto pixels = std::make_unique<RasterPixels>();
        pixels->width = image.width;
        pixels->height = image.height;
        const auto count = static_cast<std::size_t>(image.width) *
                           static_cast<std::size_t>(image.height);
        pixels->rgba.resize(count * 4);
        const auto channels = static_cast<std::size_t>(image.channels);
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t c = 0; c < 3; ++c) {
                pixels->rgba[i * 4 + c] =
                    static_cast<std::uint8_t>(image.pixelData[i * channels + c]);
            }
            pixels->rgba[i * 4 + 3] =
                channels == 4 ? static_cast<std::uint8_t>(image.pixelData[i * 4 + 3])
                              : 255;
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

struct TerrainTiles::Impl {
    std::shared_ptr<std::atomic<int>> requests = std::make_shared<std::atomic<int>>(0);
    std::atomic<std::size_t> failures{0};
    std::atomic<std::size_t> skipped{0};
    bool imagery = false;
    std::shared_ptr<WorkerPool> workers;
    CesiumAsync::AsyncSystem async;
    std::unique_ptr<Cesium3DTilesSelection::Tileset> tileset;
    Counts counts;

    explicit Impl(int threads)
        : workers(std::make_shared<WorkerPool>(threads)), async(workers) {}
};

TerrainTiles::TerrainTiles(Renderer& renderer, const TerrainOptions& options,
                           world::HeightSource heights)
    : impl_(std::make_unique<Impl>(options.worker_threads)) {
    // What is fetched, through the platform's HTTPS, kept in Cesium Native's
    // SQLite cache for as long as its caching headers allow.
    std::shared_ptr<CesiumAsync::IAssetAccessor> accessor =
        std::make_shared<PlatformAccessor>();
    if (!options.cache_file.empty()) {
        std::error_code error;
        std::filesystem::create_directories(options.cache_file.parent_path(), error);
        accessor = std::make_shared<CesiumAsync::CachingAssetAccessor>(
            spdlog::default_logger(), accessor,
            std::make_shared<CesiumAsync::SqliteCache>(spdlog::default_logger(),
                                                       options.cache_file.string()));
    }
    accessor = std::make_shared<CountingAccessor>(accessor, impl_->requests);
    Cesium3DTilesSelection::TilesetExternals externals{
        accessor, std::make_shared<RendererResources>(renderer, impl_->skipped),
        impl_->async, std::make_shared<CesiumUtility::CreditSystem>()};

    impl_->imagery = options.imagery.has_value();
    auto loader = std::make_unique<DemLoader>(options.region, std::move(heights),
                                              impl_->imagery, impl_->failures);
    auto root = std::make_unique<Tile>(
        loader->make_tile(QuadtreeTileID(0, 0, 0), -1000.0, 9000.0));

    Cesium3DTilesSelection::TilesetOptions tileset_options;
    tileset_options.maximumScreenSpaceError = options.maximum_screen_space_error;
    tileset_options.ellipsoid = Ellipsoid::WGS84;
    // Fog culls what is hazy far away; the renderer draws no haze.
    tileset_options.enableFogCulling = false;
    impl_->tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
        externals, std::move(loader), std::move(root), tileset_options);

    if (options.imagery) {
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
    const Cesium3DTilesSelection::ViewUpdateResult& result =
        complete ? tileset.updateViewGroupOffline(group, {view})
                 : tileset.updateViewGroup(group, {view}, 0.0f);
    if (!complete) {
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
