#include "copilot/provider.hpp"

#include "world/json.hpp"

#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>

namespace glideslope::copilot {

namespace {

using world::Json;

std::string text_of(const platform::HttpResponse& r) {
    return std::string(r.body.begin(), r.body.end());
}

platform::HttpResponse response_of(int status, const std::string& body) {
    platform::HttpResponse r;
    r.status = status;
    r.body.assign(body.begin(), body.end());
    return r;
}

// `text` with `key` taken out wherever it appears: what a service or anything
// between says back is repeated, and a proxy's page may echo the request.
std::string without(std::string text, const std::string& key) {
    if (key.empty()) {
        return text;
    }
    for (std::size_t at = text.find(key); at != std::string::npos; at = text.find(key, at)) {
        text.replace(at, key.size(), "[the key]");
    }
    return text;
}

// The service's own words for what went wrong, where it says: both put an
// object `error` with a `message` in an error's body.
std::string error_in(const std::string& body) {
    try {
        const Json j = world::parse_json(body);
        if (j.kind() == Json::Kind::object) {
            if (const Json* e = j.find("error"); e && e->kind() == Json::Kind::object) {
                if (const Json* m = e->find("message"); m && m->kind() == Json::Kind::string) {
                    return m->string();
                }
            }
        }
    } catch (const world::JsonError&) {
    }
    return body.substr(0, 200);
}

platform::HttpRequest request_to(const std::string& url) {
    platform::HttpRequest r;
    r.url = url;
    r.user_agent = "glideslope (+https://github.com/GavinMGlynn/glideslope)";
    r.max_body = std::uint64_t{4} << 20;
    // A model takes its time; one that says nothing for this long has gone.
    r.stall_timeout_seconds = 300;
    return r;
}

// **OpenAI's Chat Completions**: the instructions as its system message, then
// the turns. https://platform.openai.com/docs/api-reference/chat
class OpenAi : public Provider {
public:
    OpenAi(std::string key, std::string model, Post post)
        : key_(std::move(key)), model_(std::move(model)), post_(std::move(post)) {}

    std::string name() const override {
        return "openai";
    }
    std::string model() const override {
        return model_;
    }

    std::string answer(const std::string& instructions,
                       const std::vector<Turn>& conversation) override {
        std::vector<Json> messages{Json::make_object(
            {{"role", Json::make_string("system")}, {"content", Json::make_string(instructions)}})};
        for (const Turn& t : conversation) {
            messages.push_back(Json::make_object(
                {{"role", Json::make_string(t.role)}, {"content", Json::make_string(t.text)}}));
        }
        const std::string body = world::write_json(Json::make_object(
            {{"model", Json::make_string(model_)}, {"messages", Json::make_array(messages)}}));
        platform::HttpRequest request = request_to("https://api.openai.com/v1/chat/completions");
        request.headers = {{"Content-Type", "application/json"},
                           {"Authorization", "Bearer " + key_}};
        const platform::HttpResponse r = post_(request, body);
        const std::string text = text_of(r);
        if (r.status != 200) {
            throw ProviderError("OpenAI answered " + std::to_string(r.status) + ": " +
                                without(error_in(text), key_));
        }
        try {
            const Json j = world::parse_json(text);
            const Json& choice = j.at("choices").array().at(0);
            return choice.at("message").at("content").string();
        } catch (const std::exception& e) {
            throw ProviderError(std::string("OpenAI's answer holds no text: ") + e.what());
        }
    }

private:
    std::string key_;
    std::string model_;
    Post post_;
};

// **Anthropic's Messages API**: the instructions as `system`, then the turns.
// https://docs.anthropic.com/en/api/messages
class Anthropic : public Provider {
public:
    Anthropic(std::string key, std::string model, Post post)
        : key_(std::move(key)), model_(std::move(model)), post_(std::move(post)) {}

    std::string name() const override {
        return "anthropic";
    }
    std::string model() const override {
        return model_;
    }

    std::string answer(const std::string& instructions,
                       const std::vector<Turn>& conversation) override {
        std::vector<Json> messages;
        for (const Turn& t : conversation) {
            messages.push_back(Json::make_object(
                {{"role", Json::make_string(t.role)}, {"content", Json::make_string(t.text)}}));
        }
        const std::string body = world::write_json(Json::make_object(
            {{"model", Json::make_string(model_)},
             {"max_tokens", Json::make_number(4096)},
             {"system", Json::make_string(instructions)},
             {"messages", Json::make_array(messages)}}));
        platform::HttpRequest request = request_to("https://api.anthropic.com/v1/messages");
        request.headers = {{"Content-Type", "application/json"},
                           {"x-api-key", key_},
                           {"anthropic-version", "2023-06-01"}};
        const platform::HttpResponse r = post_(request, body);
        const std::string text = text_of(r);
        if (r.status != 200) {
            throw ProviderError("Anthropic answered " + std::to_string(r.status) + ": " +
                                without(error_in(text), key_));
        }
        try {
            const Json j = world::parse_json(text);
            std::string out;
            for (const Json& block : j.at("content").array()) {
                if (block.at("type").string() == "text") {
                    out += block.at("text").string();
                }
            }
            if (out.empty()) {
                throw ProviderError("no text block");
            }
            return out;
        } catch (const std::exception& e) {
            throw ProviderError(std::string("Anthropic's answer holds no text: ") + e.what());
        }
    }

private:
    std::string key_;
    std::string model_;
    Post post_;
};

struct Recorded {
    std::string url;
    std::string request;
    int status = 0;
    std::string answer;
};

} // namespace

Post http_post() {
    return [](const platform::HttpRequest& request, const std::string& body) {
        return platform::http_post(request, body);
    };
}

Post recording(Post post, std::filesystem::path file) {
    auto lock = std::make_shared<std::mutex>();
    return [post = std::move(post), file = std::move(file),
            lock](const platform::HttpRequest& request, const std::string& body) {
        const platform::HttpResponse r = post(request, body);
        const std::string line = world::write_json(Json::make_object(
            {{"url", Json::make_string(request.url)},
             {"request", Json::make_string(body)},
             {"status", Json::make_number(r.status)},
             {"answer", Json::make_string(text_of(r))}}));
        const std::lock_guard<std::mutex> held(*lock);
        std::ofstream out(file, std::ios::binary | std::ios::app);
        out << line << '\n';
        if (!out) {
            throw ProviderError("cannot write the recording " + file.string());
        }
        return r;
    };
}

Post playback(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        throw ProviderError("cannot read the recording " + file.string());
    }
    auto recorded = std::make_shared<std::vector<Recorded>>();
    int number = 0;
    for (std::string line; std::getline(in, line);) {
        ++number;
        if (line.empty()) {
            continue;
        }
        try {
            const Json j = world::parse_json(line);
            recorded->push_back({j.at("url").string(), j.at("request").string(),
                                 static_cast<int>(j.at("status").number()),
                                 j.at("answer").string()});
        } catch (const world::JsonError& e) {
            throw ProviderError(file.string() + ": line " + std::to_string(number) + ": " +
                                e.what());
        }
    }
    auto next = std::make_shared<std::size_t>(0);
    return [recorded, next, file](const platform::HttpRequest& request, const std::string& body) {
        if (*next >= recorded->size()) {
            throw ProviderError(file.string() + " recorded " + std::to_string(recorded->size()) +
                                " exchanges, and this is one more");
        }
        const Recorded& r = (*recorded)[(*next)++];
        if (r.url != request.url || r.request != body) {
            throw ProviderError(file.string() + ": exchange " + std::to_string(*next) +
                                " was recorded for another request; the prompt has changed "
                                "since it was recorded, and it must be recorded again");
        }
        return response_of(r.status, r.answer);
    };
}

std::unique_ptr<Provider> make_provider(const std::string& name, const std::string& key,
                                        const std::string& model, Post post, bool played_back) {
    if (name != "openai" && name != "anthropic") {
        throw ProviderError("no provider \"" + name + "\": openai or anthropic");
    }
    if (key.empty() && !played_back) {
        throw ProviderError(
            name == "openai"
                ? "no OpenAI key: put yours in GLIDESLOPE_OPENAI_KEY or the file openai-key "
                  "in glideslope's config directory"
                : "no Anthropic key: put yours in GLIDESLOPE_ANTHROPIC_KEY or the file "
                  "anthropic-key in glideslope's config directory");
    }
    if (name == "openai") {
        return std::make_unique<OpenAi>(key, model.empty() ? default_openai_model : model,
                                        std::move(post));
    }
    return std::make_unique<Anthropic>(key, model.empty() ? default_anthropic_model : model,
                                       std::move(post));
}

} // namespace glideslope::copilot
