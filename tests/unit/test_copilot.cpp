#include "harness.hpp"

#include "copilot/planner.hpp"
#include "copilot/provider.hpp"
#include "world/json.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

using glideslope::copilot::Post;
using glideslope::copilot::ProviderError;
using glideslope::test::check;
using glideslope::test::fail;
using glideslope::world::Json;

namespace {

glideslope::platform::HttpResponse answered(int status, const std::string& body) {
    glideslope::platform::HttpResponse r;
    r.status = status;
    r.body.assign(body.begin(), body.end());
    return r;
}

// What was sent through a stand-in Post, and what it answered.
struct Sent {
    glideslope::platform::HttpRequest request;
    std::string body;
};

// A Post that keeps what it is sent and answers from `answers` in turn.
Post stand_in(std::vector<Sent>& sent, std::vector<glideslope::platform::HttpResponse> answers) {
    auto next = std::make_shared<std::size_t>(0);
    auto kept = std::make_shared<std::vector<glideslope::platform::HttpResponse>>(std::move(answers));
    return [&sent, next, kept](const glideslope::platform::HttpRequest& request,
                               const std::string& body) {
        sent.push_back({request, body});
        return (*kept)[std::min((*next)++, kept->size() - 1)];
    };
}

std::string header(const glideslope::platform::HttpRequest& r, const std::string& name) {
    for (const auto& [n, v] : r.headers) {
        if (n == name) {
            return v;
        }
    }
    return {};
}

std::string openai_answer(const std::string& text) {
    return glideslope::world::write_json(Json::make_object(
        {{"choices", Json::make_array({Json::make_object(
                         {{"message", Json::make_object({{"role", Json::make_string("assistant")},
                                                         {"content", Json::make_string(text)}})}})})}}));
}

std::string anthropic_answer(const std::string& text) {
    return glideslope::world::write_json(Json::make_object(
        {{"content", Json::make_array({Json::make_object(
                         {{"type", Json::make_string("text")}, {"text", Json::make_string(text)}})})}}));
}

std::filesystem::path scratch(const std::string& name) {
    const auto dir = std::filesystem::path(GLIDESLOPE_TEST_DOWNLOADS_DIR) / "test-scratch";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::filesystem::remove(path);
    return path;
}

// Sydney's runways 16R and 34L, and a request to fly from them.
glideslope::copilot::PlanRequest sydney(const std::string& command) {
    glideslope::copilot::PlanRequest r;
    r.command = command;
    r.aircraft = "c172p";
    r.aircraft_name = "Cessna 172P Skyhawk";
    r.approach_kts = 62;
    r.climb_kts = 74;
    r.cruise_kts = 105;
    r.airport = "YSSY";
    glideslope::world::RunwayEnd a;
    a.airport = "YSSY";
    a.ident = "16R";
    a.latitude_deg = -33.929401;
    a.longitude_deg = 151.171997;
    a.elevation_ft = 8;
    a.heading_deg = 168;
    a.length_m = 3962;
    glideslope::world::RunwayEnd b = a;
    b.ident = "34L";
    b.latitude_deg = -33.964298;
    b.longitude_deg = 151.181;
    b.elevation_ft = 14;
    b.heading_deg = 348;
    r.runways = {a, b};
    return r;
}

const std::string good_plan = "aircraft c172p\n"
                              "runway 34L -33.964298 151.181000 14 348 3962\n"
                              "takeoff 800\n"
                              "waypoint CLIMB -33.92 151.19 3000 80\n"
                              "orbit CBD -33.8688 151.2093 1500 3000 90 0 left\n";

// A provider that answers from a script, for the planner's tests.
class Scripted : public glideslope::copilot::Provider {
public:
    explicit Scripted(std::vector<std::string> answers) : answers_(std::move(answers)) {}
    std::string name() const override {
        return "scripted";
    }
    std::string model() const override {
        return "none";
    }
    std::string answer(const std::string&,
                       const std::vector<glideslope::copilot::Turn>& conversation) override {
        conversations.push_back(conversation);
        return answers_.at(std::min(asked_++, answers_.size() - 1));
    }
    std::vector<std::vector<glideslope::copilot::Turn>> conversations;

private:
    std::vector<std::string> answers_;
    std::size_t asked_ = 0;
};

} // namespace

GLIDESLOPE_TEST(each_provider_is_asked_as_its_api_has_it_and_its_answer_read) {
    for (const std::string name : {"openai", "anthropic"}) {
        std::vector<Sent> sent;
        const bool openai = name == "openai";
        auto provider = glideslope::copilot::make_provider(
            name, "the-key", "", stand_in(sent, {answered(200, openai ? openai_answer("a plan")
                                                                      : anthropic_answer("a plan"))}));
        const std::string said = provider->answer(
            "the instructions", {{"user", "take off"}, {"assistant", "no"}, {"user", "again"}});
        check(said == "a plan", name + "'s answer read: " + said);
        check(sent.size() == 1, name + " asked once");
        const Json body = glideslope::world::parse_json(sent[0].body);
        if (openai) {
            check(sent[0].request.url == "https://api.openai.com/v1/chat/completions" &&
                      header(sent[0].request, "Authorization") == "Bearer the-key" &&
                      header(sent[0].request, "Content-Type") == "application/json",
                  "OpenAI: its URL, and the key as a bearer token");
            check(body.at("model").string() == glideslope::copilot::default_openai_model,
                  "OpenAI: its dated default model");
            const auto& m = body.at("messages").array();
            check(m.size() == 4 && m[0].at("role").string() == "system" &&
                      m[0].at("content").string() == "the instructions" &&
                      m[1].at("role").string() == "user" && m[2].at("role").string() == "assistant" &&
                      m[3].at("content").string() == "again",
                  "OpenAI: the instructions as the system message, then every turn in order");
        } else {
            check(sent[0].request.url == "https://api.anthropic.com/v1/messages" &&
                      header(sent[0].request, "x-api-key") == "the-key" &&
                      header(sent[0].request, "anthropic-version") == "2023-06-01",
                  "Anthropic: its URL, its key header and its version");
            check(body.at("model").string() == glideslope::copilot::default_anthropic_model &&
                      body.at("system").string() == "the instructions" &&
                      body.at("max_tokens").number() > 0,
                  "Anthropic: its model, the instructions as system, and a token limit");
            const auto& m = body.at("messages").array();
            check(m.size() == 3 && m[0].at("role").string() == "user" &&
                      m[2].at("content").string() == "again",
                  "Anthropic: every turn in order");
        }

        // An error is the service's own words, and a key is never among them.
        std::vector<Sent> refused_sent;
        auto refusing = glideslope::copilot::make_provider(
            name, "the-key", "a-model",
            stand_in(refused_sent,
                     {answered(401, "{\"error\":{\"message\":\"Incorrect API key provided: "
                                    "the-key\"}}")}));
        try {
            (void)refusing->answer("i", {{"user", "u"}});
            fail(name + ": an error status was taken for an answer");
        } catch (const ProviderError& e) {
            const std::string why = e.what();
            // The service echoed the key; the error says what it said, less that.
            check(why.find("401") != std::string::npos &&
                      why.find("Incorrect API key provided: [the key]") != std::string::npos &&
                      why.find("the-key") == std::string::npos,
                  name + ": the error said as the service said it, less the key: " + why);
        }
        check(glideslope::world::parse_json(refused_sent[0].body).at("model").string() == "a-model",
              name + ": the model asked for is the one asked");
    }
}

GLIDESLOPE_TEST(a_provider_with_no_key_is_refused_and_one_played_back_needs_none) {
    std::vector<Sent> sent;
    std::size_t refused = 0;
    for (const std::string name : {"openai", "anthropic"}) {
        try {
            (void)glideslope::copilot::make_provider(name, "", "", stand_in(sent, {answered(200, "")}));
            fail(name + " was made with no key");
        } catch (const ProviderError& e) {
            check(std::string(e.what()).find(name == "openai" ? "openai-key" : "anthropic-key") !=
                      std::string::npos,
                  name + ": refused, saying where its key is read from: " + e.what());
            ++refused;
        }
        (void)glideslope::copilot::make_provider(name, "", "", stand_in(sent, {answered(200, "")}),
                                                 true);
    }
    try {
        (void)glideslope::copilot::make_provider("gemini", "k", "", stand_in(sent, {answered(200, "")}));
        fail("a provider that is not one was made");
    } catch (const ProviderError&) {
        ++refused;
    }
    check(refused == 3 && sent.empty(), "both providers without a key, and one that is not a "
                                        "provider, refused, and nothing sent");
}

GLIDESLOPE_TEST(a_recording_plays_back_only_the_requests_it_recorded_and_holds_no_key) {
    const auto file = scratch("copilot-recording.jsonl");
    std::vector<Sent> sent;
    const Post recorded = glideslope::copilot::recording(
        stand_in(sent, {answered(200, "first"), answered(429, "second")}), file);
    glideslope::platform::HttpRequest request;
    request.url = "https://api.example/v1";
    request.headers = {{"Authorization", "Bearer secret-key-123"}};
    (void)recorded(request, "one");
    (void)recorded(request, "two");

    std::ifstream in(file, std::ios::binary);
    const std::string text(std::istreambuf_iterator<char>(in), {});
    check(text.find("secret-key-123") == std::string::npos && text.find("Authorization") == std::string::npos,
          "the recording holds no header, so no key");

    const Post played = glideslope::copilot::playback(file);
    const auto first = played(request, "one");
    const auto second = played(request, "two");
    check(first.status == 200 && std::string(first.body.begin(), first.body.end()) == "first" &&
              second.status == 429 && std::string(second.body.begin(), second.body.end()) == "second",
          "each answer played back, status and body, in order");
    std::size_t refused = 0;
    try {
        (void)played(request, "three");
        fail("an exchange past the recording was answered");
    } catch (const ProviderError&) {
        ++refused;
    }
    const Post again = glideslope::copilot::playback(file);
    try {
        (void)again(request, "not one");
        fail("a request other than the one recorded was answered");
    } catch (const ProviderError& e) {
        check(std::string(e.what()).find("recorded again") != std::string::npos,
              std::string("refused, saying it must be recorded again: ") + e.what());
        ++refused;
    }
    glideslope::platform::HttpRequest elsewhere = request;
    elsewhere.url = "https://api.example/v2";
    try {
        (void)glideslope::copilot::playback(file)(elsewhere, "one");
        fail("a request to another URL was answered");
    } catch (const ProviderError&) {
        ++refused;
    }
    check(refused == 3, "one too many, another body and another URL: three refused");
}

GLIDESLOPE_TEST(a_plan_from_words_is_checked_and_refused_back_to_the_model_until_it_can_be_flown) {
    // Every way a plan is refused, each told back to the model, which then
    // answers with the plan that can be flown.
    const std::vector<std::pair<std::string, std::string>> wrong{
        {"take off and climb", "line 1: no command \"take\""},
        {"aircraft pa28\nrunway 34L -33.964298 151.181000 14 348 3962\ntakeoff 800\n"
         "waypoint A -33.92 151.19 3000 80\n",
         "for aircraft pa28, not c172p"},
        {"aircraft c172p\nstart -33.9 151.2 3000 0 90\nwaypoint A -33.92 151.19 3000 80\n",
         "does not take off"},
        {"aircraft c172p\nrunway 34L -33.964 151.181 14 348 3962\ntakeoff 800\n"
         "waypoint A -33.92 151.19 3000 80\n",
         "not one of the runway lines given"},
        {"aircraft c172p\nrunway 07 -33.943699 151.164001 16 74 2530\ntakeoff 800\n"
         "waypoint A -33.92 151.19 3000 80\n",
         "not one of the runway lines given"},
        {"aircraft c172p\nrunway 34L -33.964298 151.181000 14 348 3962\ntakeoff 800\n"
         "waypoint A -33.92 151.19 400 80\n",
         "below 514 ft"},
        {"aircraft c172p\nrunway 34L -33.964298 151.181000 14 348 3962\ntakeoff 800\n"
         "waypoint A -33.92 151.19 3000 50\n",
         "outside 62 to 126 kt"},
        {"aircraft c172p\nrunway 34L -33.964298 151.181000 14 348 3962\ntakeoff 800\n"
         "orbit A -33.92 151.19 3000 3000 140 1 left\n",
         "outside 62 to 126 kt"},
        {"aircraft c172p\nrunway 34L -33.964298 151.181000 14 348 3962\ntakeoff 800\n"
         "waypoint MELBOURNE -37.8136 144.9631 3000 100\n",
         "more than 200"},
    };
    std::size_t covered = 0;
    for (const auto& [plan, why] : wrong) {
        Scripted model({plan, good_plan});
        const auto planned = glideslope::copilot::plan_from_words(
            model, sydney("take off, climb to 3,000 ft and orbit the CBD"));
        check(planned.attempts == 2 && planned.refused.size() == 1 &&
                  planned.refused[0].find(why) != std::string::npos,
              "refused for \"" + why + "\", not: " +
                  (planned.refused.empty() ? std::string("nothing") : planned.refused[0]));
        check(model.conversations.size() == 2 && model.conversations[1].size() == 3 &&
                  model.conversations[1][1].role == "assistant" &&
                  model.conversations[1][1].text == plan &&
                  model.conversations[1][2].text.find(why) != std::string::npos,
              "and the refusal told back to the model after its own answer: " + why);
        check(planned.plan.takeoff && planned.plan.takeoff->runway.name == "34L" &&
                  planned.plan.waypoints.size() == 2 && planned.plan.waypoints[1].orbit,
              "then the plan that can be flown is taken");
        ++covered;
    }
    check(covered == wrong.size() && covered == 9, "nine ways wrong, every one refused");

    // A fence round the plan is not the plan's; a plan that is right first
    // time is taken first time; and the request carries the command, the
    // aircraft's speeds and every runway line.
    Scripted fenced({"```\n" + good_plan + "```\n"});
    const auto first = glideslope::copilot::plan_from_words(fenced, sydney("orbit the CBD"));
    check(first.attempts == 1 && first.text == good_plan, "a fenced plan taken, less its fence");
    const std::string& asked = fenced.conversations[0][0].text;
    for (const char* part : {"The pilot says: orbit the CBD", "c172p", "62 kt on the approach",
                             "from 62 to 126 kt", "An orbit's radius must be at least",
                             "It stands at YSSY",
                             "runway 16R -33.929401 151.171997 8 168 3962",
                             "runway 34L -33.964298 151.181000 14 348 3962"}) {
        check(asked.find(part) != std::string::npos,
              std::string("the request says \"") + part + "\":\n" + asked);
    }

    // **A runway end with no elevation** is not offered, and a plan taking off
    // from it is refused: its heights could not be checked against it.
    glideslope::copilot::PlanRequest with_unknown = sydney("take off");
    glideslope::world::RunwayEnd unknown = with_unknown.runways[0];
    unknown.ident = "07";
    unknown.latitude_deg = -33.943699;
    unknown.longitude_deg = 151.164001;
    unknown.heading_deg = 74;
    unknown.elevation_ft = std::numeric_limits<double>::quiet_NaN();
    with_unknown.runways.push_back(unknown);
    check(glideslope::copilot::planning_request(with_unknown).find("runway 07") == std::string::npos,
          "an end with no elevation is not offered");
    Scripted from_unknown({"aircraft c172p\nrunway 07 -33.943699 151.164001 0 74 3962\n"
                           "takeoff 800\nwaypoint A -33.92 151.19 3000 80\n",
                           good_plan});
    const auto planned_unknown = glideslope::copilot::plan_from_words(from_unknown, with_unknown);
    check(planned_unknown.refused.size() == 1 &&
              planned_unknown.refused[0].find("not one of the runway lines given") != std::string::npos,
          "a plan taking off from it is refused");
    glideslope::copilot::PlanRequest none_known = with_unknown;
    none_known.runways = {unknown};
    Scripted never_asked({good_plan});
    try {
        (void)glideslope::copilot::plan_from_words(never_asked, none_known);
        fail("a plan was asked for where no runway says its elevation");
    } catch (const ProviderError& e) {
        check(std::string(e.what()).find("says its elevation") != std::string::npos &&
                  never_asked.conversations.empty(),
              std::string("refused before the model is asked: ") + e.what());
    }

    // Refused every time, it is an error saying why each was.
    Scripted stubborn({"take off"});
    try {
        (void)glideslope::copilot::plan_from_words(stubborn, sydney("take off"));
        fail("a plan refused three times was taken");
    } catch (const ProviderError& e) {
        check(std::string(e.what()).find("3 plans were each refused") != std::string::npos &&
                  stubborn.conversations.size() == 3,
              std::string("three answers, each refused, and said so: ") + e.what());
    }
}
