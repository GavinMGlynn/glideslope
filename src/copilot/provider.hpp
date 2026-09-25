#pragma once

// A language model to plan with: OpenAI's or Anthropic's, asked over HTTPS
// with the user's own key (REQUIREMENTS.md section 5: "the player's own key").
//
// **What a model says is only ever text**, and the planner reads it as a
// flight plan or refuses it (copilot/planner.hpp). Nothing a model answers
// reaches a control surface except as a plan the autopilot flies.
//
// **Recorded answers**, for tests and CI, which have no key: a `Post` that
// records every request and its answer to a file, and one that plays them
// back. Played back, a request must be byte for byte the one recorded - a
// prompt that has changed since is refused rather than answered with what
// was said to another - and no header is recorded, so no key is.

#include "platform/http.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace glideslope::copilot {

struct ProviderError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// An HTTP POST: `http_post`, or a recording of it, or a playback.
using Post = std::function<platform::HttpResponse(const platform::HttpRequest& request,
                                                  const std::string& body)>;

// The platform's own.
Post http_post();

// Every exchange through `post` written to `file`, one JSON object a line:
// the URL, the request's body, and the answer's status and body. Headers are
// not written, so neither is a key.
Post recording(Post post, std::filesystem::path file);

// The exchanges `file` holds, played back in order. Throws ProviderError for
// a request to another URL or with another body than the one recorded next,
// or for one more than were recorded.
Post playback(const std::filesystem::path& file);

// One turn of a conversation: "user" or "assistant", and what was said.
struct Turn {
    std::string role;
    std::string text;
};

class Provider {
public:
    virtual ~Provider() = default;
    // "openai" or "anthropic", and the model asked.
    virtual std::string name() const = 0;
    virtual std::string model() const = 0;
    // What the model says next, told `instructions` and the conversation so
    // far. Throws ProviderError for an answer that is not one: an error
    // status, the service's own error, or a body that does not hold text.
    virtual std::string answer(const std::string& instructions,
                               const std::vector<Turn>& conversation) = 0;
};

// The providers, each with the model it asks by default. OpenAI's is a dated
// snapshot, so that the same request is answered by the same model from one
// year to the next. Anthropic's is the model's name as Anthropic gives it,
// with no snapshot yet chosen: none can be until there is a key to list them
// with. Either way a recording holds the model asked, in the request it must
// match.
inline constexpr const char* default_openai_model = "gpt-5.5-2026-04-23";
inline constexpr const char* default_anthropic_model = "claude-sonnet-5";

// **A provider with no key is refused**, not faked: throws ProviderError
// naming the key it needs and where it is read from. `key` may be empty only
// for a playback, which sends nothing anywhere.
std::unique_ptr<Provider> make_provider(const std::string& name, const std::string& key,
                                        const std::string& model, Post post,
                                        bool played_back = false);

} // namespace glideslope::copilot
