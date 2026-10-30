#include "gfx/fetches.hpp"

#include "platform/http.hpp"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/CachingAssetAccessor.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/ICacheDatabase.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace glideslope::gfx {

namespace {

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

// **Every GET answered**, for the cache above it (gfx/fetches.hpp): one that
// came back with no response comes back with unanswered_fetch_status, and
// nothing in it, instead. Anything else, and every other verb, which the
// cache passes straight through, is left as it came.
class AnsweredAccessor final : public CesiumAsync::IAssetAccessor {
public:
    explicit AnsweredAccessor(std::shared_ptr<CesiumAsync::IAssetAccessor> network)
        : network_(std::move(network)) {}

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    get(const CesiumAsync::AsyncSystem& async, const std::string& url,
        const std::vector<THeader>& headers) override {
        return network_->get(async, url, headers)
            .thenImmediately(
                [](std::shared_ptr<CesiumAsync::IAssetRequest>&& request)
                    -> std::shared_ptr<CesiumAsync::IAssetRequest> {
                    if (request != nullptr && request->response() == nullptr) {
                        return std::make_shared<Unanswered>(std::move(request));
                    }
                    return std::move(request);
                });
    }

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    request(const CesiumAsync::AsyncSystem& async, const std::string& verb,
            const std::string& url, const std::vector<THeader>& headers,
            const std::span<const std::byte>& body) override {
        return network_->request(async, verb, url, headers, body);
    }

    void tick() noexcept override {
        network_->tick();
    }

private:
    class Response final : public CesiumAsync::IAssetResponse {
    public:
        uint16_t statusCode() const override {
            return static_cast<uint16_t>(unanswered_fetch_status);
        }
        std::string contentType() const override {
            return {};
        }
        const CesiumAsync::HttpHeaders& headers() const override {
            return headers_;
        }
        std::span<const std::byte> data() const override {
            return {};
        }

    private:
        CesiumAsync::HttpHeaders headers_;
    };

    // The request that got no answer, answered.
    class Unanswered final : public CesiumAsync::IAssetRequest {
    public:
        explicit Unanswered(std::shared_ptr<CesiumAsync::IAssetRequest> asked)
            : asked_(std::move(asked)) {}
        const std::string& method() const override {
            return asked_->method();
        }
        const std::string& url() const override {
            return asked_->url();
        }
        const CesiumAsync::HttpHeaders& headers() const override {
            return asked_->headers();
        }
        const CesiumAsync::IAssetResponse* response() const override {
            return &response_;
        }

    private:
        std::shared_ptr<CesiumAsync::IAssetRequest> asked_;
        Response response_;
    };

    std::shared_ptr<CesiumAsync::IAssetAccessor> network_;
};

} // namespace

std::shared_ptr<CesiumAsync::IAssetAccessor>
platform_fetches(std::shared_ptr<const std::atomic<bool>> closing) {
    return std::make_shared<PlatformAccessor>(std::move(closing));
}

std::shared_ptr<CesiumAsync::IAssetAccessor>
cached_fetches(std::shared_ptr<CesiumAsync::IAssetAccessor> network,
               std::shared_ptr<CesiumAsync::ICacheDatabase> cache) {
    return std::make_shared<CesiumAsync::CachingAssetAccessor>(
        spdlog::default_logger(),
        std::make_shared<AnsweredAccessor>(std::move(network)), std::move(cache));
}

} // namespace glideslope::gfx
