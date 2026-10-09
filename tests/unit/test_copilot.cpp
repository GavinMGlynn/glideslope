#include "harness.hpp"

#include "copilot/copilot.hpp"
#include "copilot/planner.hpp"
#include "copilot/provider.hpp"
#include "frontend/briefs.hpp"
#include "sim/catalogue.hpp"
#include "world/json.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <thread>
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

GLIDESLOPE_TEST(a_plan_from_the_ground_may_end_in_a_landing_on_a_runway_it_was_told_of_and_on_no_other) {
    // **Offered as the copilot is**: the command, and where it may land.
    auto request = sydney("take off, fly to Bankstown and land there");
    glideslope::world::RunwayEnd ysbk;
    ysbk.airport = "YSBK";
    ysbk.ident = "29C";
    ysbk.latitude_deg = -33.9268;
    ysbk.longitude_deg = 150.996002;
    ysbk.elevation_ft = 26;
    ysbk.heading_deg = 285;
    ysbk.length_m = 1100;
    request.fields = {request.runways[0], request.runways[1], ysbk};
    const std::string instructions = glideslope::copilot::planning_instructions();
    check(instructions.find("land NAME LATITUDE LONGITUDE ELEVATION_FT HEADING_DEG LENGTH_M") !=
                  std::string::npos &&
              instructions.find("`land`, last") != std::string::npos,
          "the planner is told how to land:\n" + instructions);
    const std::string asked = glideslope::copilot::planning_request(request);
    check(asked.find("Runways it may land on") != std::string::npos &&
              asked.find("YSBK runway 29C -33.926800 150.996002 26 285 1100") != std::string::npos &&
              asked.find("YSSY runway 16R -33.929401 151.171997 8 168 3962") != std::string::npos,
          "and the runways it may land on, as the copilot is:\n" + asked);

    const std::string from_34l = "aircraft c172p\n"
                                 "runway 34L -33.964298 151.181000 14 348 3962\n"
                                 "takeoff 800\n";
    const std::string to_bankstown = "waypoint EAST_OF_YSBK -33.93 151.05 1500 90\n";
    const std::string land_29c = "land YSBK_29C -33.926800 150.996002 26 285 1100\n";
    const std::string lands = from_34l + to_bankstown + land_29c;
    Scripted first_time({lands});
    const auto planned = glideslope::copilot::plan_from_words(first_time, request);
    check(planned.attempts == 1 && planned.plan.landing &&
              planned.plan.landing->name == "YSBK_29C" &&
              planned.plan.landing->heading_deg == 285.0 && planned.plan.waypoints.size() == 1,
          "a plan ending in a landing on a runway it was told of is taken, first time");

    // **Every way a landing is refused**, each told back and then the plan
    // that lands taken: the copilot's checks (copilot::landing_refusal), and
    // `land` last.
    auto no_approach = request;
    no_approach.approach_kts = 0;
    no_approach.slowest_kts = 62;
    auto told_none = request;
    told_none.fields.clear();
    auto unknown_height = request;
    unknown_height.fields.back().elevation_ft = std::numeric_limits<double>::quiet_NaN();
    struct Refusal {
        const glideslope::copilot::PlanRequest* request;
        std::string plan;
        std::string says;
    };
    const std::vector<Refusal> refusals{
        {&request, from_34l + land_29c + to_bankstown, "`land` is the last line"},
        {&request, from_34l + to_bankstown + "land YSBK_29C -33.9 151.0 26 285 1100\n",
         "none of the runways"},
        {&request, from_34l + to_bankstown + "land YSBK_11C -33.926800 150.996002 26 105 1100\n",
         "none of the runways"},
        {&request, from_34l + "orbit ROUND -33.93 151.05 1500 1500 90 0 left\n" + land_29c,
         "goes round for ever"},
        {&no_approach, lands, "no approach speed"},
        {&told_none, lands, "none of the runways"},
        {&unknown_height, lands, "none of the runways"},
    };
    std::size_t covered = 0;
    for (const Refusal& r : refusals) {
        Scripted model({r.plan, from_34l + to_bankstown});
        const auto then = glideslope::copilot::plan_from_words(model, *r.request);
        check(then.attempts == 2 && then.refused.size() == 1 &&
                  then.refused[0].find(r.says) != std::string::npos,
              "\"" + r.plan + "\" is refused, saying \"" + r.says + "\": " +
                  (then.refused.empty() ? std::string("it was taken") : then.refused[0]));
        check(model.conversations.size() == 2 &&
                  model.conversations[1][2].text.find(r.says) != std::string::npos,
              "and the refusal is told back to the model: " + r.says);
        ++covered;
    }
    check(covered == 7 && refusals.size() == 7, "all 7 ways a plan's landing is refused were tried");
    check(glideslope::copilot::planning_request(no_approach).find(
              "It has no approach speed, so a plan for it does not land") != std::string::npos &&
              glideslope::copilot::planning_request(no_approach).find("Runways it may land on") ==
                  std::string::npos,
          "an aircraft with no approach speed is told it does not land, and offered no runway");
    check(glideslope::copilot::planning_request(unknown_height).find("YSBK runway") ==
              std::string::npos,
          "a runway with no elevation is not offered to land on");
}

GLIDESLOPE_TEST(the_runways_a_plan_may_land_on_are_the_nearest_airports_whole_and_no_more_than_the_caps) {
    // **Synthetic airports** on the meridian, `km` north of the origin, each
    // with `ends` runway ends there: no downloaded data.
    using glideslope::world::RunwayEnd;
    const auto airport = [](const std::string& name, double km, int ends, bool elevation = true) {
        std::vector<RunwayEnd> out;
        for (int i = 0; i < ends; ++i) {
            RunwayEnd e;
            e.airport = name;
            e.ident = std::to_string(i + 1);
            e.latitude_deg = km / 111.2 + 0.0001 * i;
            e.longitude_deg = 0.0;
            e.elevation_ft = elevation ? 10.0 : std::numeric_limits<double>::quiet_NaN();
            e.heading_deg = 10.0 * i;
            e.length_m = 1000;
            out.push_back(e);
        }
        return out;
    };
    const auto joined = [](std::initializer_list<std::vector<RunwayEnd>> parts) {
        std::vector<RunwayEnd> out;
        for (const auto& p : parts) {
            out.insert(out.end(), p.begin(), p.end());
        }
        return out;
    };
    const auto offered = [](const std::vector<RunwayEnd>& fields) {
        std::vector<std::pair<std::string, int>> out;
        for (const RunwayEnd& e : fields) {
            if (out.empty() || out.back().first != e.airport) {
                out.emplace_back(e.airport, 0);
            }
            ++out.back().second;
        }
        return out;
    };
    const auto said = [](const std::vector<std::pair<std::string, int>>& airports) {
        std::string out;
        for (const auto& [name, n] : airports) {
            out += name + " (" + std::to_string(n) + ") ";
        }
        return out;
    };
    using glideslope::frontend::landing_fields;
    using glideslope::frontend::most_landing_airports;
    using glideslope::frontend::most_landing_fields;
    check(most_landing_airports == 4 && most_landing_fields == 24,
          "the caps are four airports and 24 runway ends");

    // Two big airports near - six ends each, as Sydney's and Bankstown's -
    // and a third, smaller one further: the third is offered. So is a
    // fourth; a fifth is past the airports' cap, one 60 km off past the
    // radius, and one whose only end has no elevation is not an airport to
    // land at.
    const auto home = airport("HOME", 0, 6);
    const auto all = joined({home, airport("BIG", 5, 6), airport("UNKNOWN", 10, 1, false),
                             airport("THIRD", 20, 2), airport("FOURTH", 25, 2),
                             airport("FIFTH", 30, 2), airport("FAR", 60, 2)});
    const auto near = offered(landing_fields(all, home));
    const std::vector<std::pair<std::string, int>> expected{
        {"HOME", 6}, {"BIG", 6}, {"THIRD", 2}, {"FOURTH", 2}};
    check(near == expected,
          "the nearest four airports, each whole: " + said(expected) + "- not " + said(near));

    // An airport with more ends than are left under the cap is passed over
    // whole, not cut short, and the next nearest offered.
    const auto crowded = offered(landing_fields(
        joined({home, airport("GIANT", 3, 20), airport("BIG", 5, 6), airport("THIRD", 20, 2)}),
        home));
    const std::vector<std::pair<std::string, int>> without_giant{
        {"HOME", 6}, {"BIG", 6}, {"THIRD", 2}};
    check(crowded == without_giant,
          "26 ends would pass the cap of 24, so GIANT is passed over: " + said(without_giant) +
              "- not " + said(crowded));

    // None where the airport says no elevation.
    check(landing_fields(all, airport("HOME", 0, 2, false)).empty(),
          "nothing offered from an airport with no elevation");
}

GLIDESLOPE_TEST(a_task_file_names_its_aircraft_airport_and_words_and_anything_else_is_refused) {
    // The server's own, as committed.
    std::ifstream in(std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "assets" / "tasks" /
                         "sydney-cbd-orbit.task",
                     std::ios::binary);
    check(static_cast<bool>(in), "the committed task is there to read");
    const glideslope::copilot::Task task =
        glideslope::copilot::parse_task(std::string(std::istreambuf_iterator<char>(in), {}));
    check(task.aircraft == "c172p" && task.airport == "YSSY" &&
              task.command == "take off, climb to 3,000 ft and orbit the CBD",
          "the committed task reads as the words the recordings were asked: " + task.command);
    // Each way of being wrong, each refused, saying what is wrong with it: an
    // unknown line, a key with no value, each of the three missing, each of
    // the three given twice, and a tab after a key.
    const std::vector<std::pair<std::string, std::string>> wrong{
        {"aircraft c172p\nairport YSSY\ntask go\nfly left\n", "is not aircraft, airport or task"},
        {"aircraft c172p\nairport\ntask go\n", "is not aircraft, airport or task"},
        {"airport YSSY\ntask go\n", "names its aircraft, its airport and its task"},
        {"aircraft c172p\ntask go\n", "names its aircraft, its airport and its task"},
        {"aircraft c172p\nairport YSSY\n", "names its aircraft, its airport and its task"},
        {"aircraft c172p\naircraft pa28\nairport YSSY\ntask go\n", "gives aircraft a second time"},
        {"aircraft c172p\nairport YSSY\nairport YSBK\ntask go\n", "gives airport a second time"},
        {"aircraft c172p\nairport YSSY\ntask go\ntask stay\n", "gives task a second time"},
        {"aircraft c172p\nairport\tYSSY\ntask go\n", "has a tab after its key"},
    };
    std::size_t refused = 0;
    for (const auto& [text, why] : wrong) {
        try {
            (void)glideslope::copilot::parse_task(text);
            fail("a task was read from: " + text);
        } catch (const ProviderError& e) {
            check(std::string(e.what()).find(why) != std::string::npos,
                  "the task \"" + text + "\" is refused saying \"" + why + "\", not: " + e.what());
            ++refused;
        }
    }
    check(refused == wrong.size(), "every wrong task refused: " + std::to_string(refused) +
                                       " of " + std::to_string(wrong.size()));
}

namespace {

// The Cessna the copilot is told of, and the pilot's task.
glideslope::copilot::Brief cessna_brief() {
    glideslope::copilot::Brief b;
    b.aircraft = "c172p";
    b.aircraft_name = "Cessna 172P Skyhawk";
    b.approach_kts = 62;
    b.climb_kts = 74;
    b.cruise_kts = 105;
    b.task = "follow the coast north to Palm Beach";
    return b;
}

// Off Bondi at 2,000 ft, heading north at 100 kt, over the sea.
glideslope::copilot::Situation off_bondi(bool engine_running) {
    glideslope::copilot::Situation now;
    now.seconds = 60;
    now.latitude_deg = -33.89;
    now.longitude_deg = 151.28;
    now.altitude_ft = 2000;
    now.ground_ft = 0;
    now.heading_deg = 0;
    now.airspeed_kts = 100;
    now.vertical_speed_fpm = 0;
    now.engine_running = engine_running;
    now.event = engine_running ? "a routine look" : "the engine has stopped";
    return now;
}

} // namespace

GLIDESLOPE_TEST(a_copilots_answer_is_read_as_keep_or_a_route_and_refused_wherever_it_cannot_be_flown) {
    const auto brief = cessna_brief();
    const auto running = off_bondi(true);
    const auto stopped = off_bondi(false);
    auto gliding = stopped;
    gliding.gliding_kts = 68;
    // Why an answer is refused, or empty when it is flown.
    const auto verdict = [&](const glideslope::copilot::Situation& now, const std::string& answer) {
        try {
            const auto change = glideslope::copilot::read_change(brief, now, answer);
            return glideslope::copilot::change_refusal(brief, now, change);
        } catch (const glideslope::sim::FlightPlanError& e) {
            return std::string(e.what());
        }
    };

    // Taken: keep, a route, a route in a fence, and with the engine stopped
    // a glide - or keep while one is flown.
    const auto kept = glideslope::copilot::read_change(brief, running, "keep\n");
    check(kept.keep && !kept.glide_kts, "`keep` alone keeps what is flown");
    const auto route = glideslope::copilot::read_change(
        brief, running,
        "```\nwaypoint MANLY -33.80 151.30 2000 100\n"
        "orbit BARRENJOEY -33.58 151.33 1500 2000 90 2 right\n```\n");
    check(!route.keep && !route.glide_kts && route.plan.waypoints.size() == 2 &&
              route.plan.start && route.plan.start->latitude_deg == -33.89 &&
              route.plan.waypoints[1].orbit && route.plan.waypoints[1].orbit->turns == 2 &&
              route.plan.waypoints[1].orbit->right,
          "a route, less its fence, is a plan flown from where the aircraft is");
    check(glideslope::copilot::change_refusal(brief, running, route).empty(),
          "and it may be flown");
    const auto glide = glideslope::copilot::read_change(
        brief, stopped, "glide 68\norbit YSSY -33.95 151.18 1500 1000 68 0 left\n");
    check(!glide.keep && glide.glide_kts == 68.0 &&
              glideslope::copilot::change_refusal(brief, stopped, glide).empty(),
          "with the engine stopped, a glide to a field is flown");
    check(verdict(gliding, "keep").empty(), "and kept while it is flown");
    check(verdict(stopped, "glide 68\nwaypoint YSSY_34L -33.96 151.18 14 60\n"
                           "orbit YSSY -33.95 151.18 800 14 60 0 left\n")
              .empty(),
          "a glide's heights and airspeeds are not flown, so neither is held to anything");

    // Refused: every way, each saying why. The space is these seventeen.
    std::string thirteen;
    for (int i = 0; i < 13; ++i) {
        thirteen += "waypoint W" + std::to_string(i) + " -33.80 151.30 2000 100\n";
    }
    struct Refusal {
        const glideslope::copilot::Situation* now;
        std::string answer;
        std::string says;
    };
    const std::vector<Refusal> refusals{
        {&running, "", "empty"},
        {&running, "turn left now", "none of keep, glide, waypoint, orbit or land"},
        {&running, "keep\nwaypoint A -33.80 151.30 2000 100", "an answer alone"},
        {&stopped, "waypoint A -33.80 151.30 2000 68\nglide 68", "the first line"},
        {&stopped, "glide sixty\nwaypoint A -33.80 151.30 2000 68", "the first line"},
        {&running, "glide 68\nwaypoint A -33.80 151.30 2000 68", "the engine is running"},
        {&stopped, "glide 90\nwaypoint A -33.80 151.30 2000 90", "a glide at 90 kt, outside 62 to 74"},
        {&stopped, "waypoint A -33.80 151.30 2000 68", "must begin with `glide"},
        {&stopped, "keep", "nothing glides"},
        {&running, "waypoint A -33.80 151.30 400 100", "below 500 ft"},
        {&running, "waypoint A -33.80 151.30 2000 140", "outside 62 to 126"},
        {&running, "waypoint A -35.80 151.30 2000 100", "more than 200"},
        {&running, "orbit A -33.80 151.30 500 2000 100 1 left", "radius"},
        {&stopped, "glide 68\norbit A -33.80 151.30 550 14 60 1 left", "too tight to glide round at 68"},
        // What could not be sent to a server (`COPILOT_ROUTE`): a name of
        // 33 letters, 13 waypoints, and round an orbit 256 times.
        {&running, "waypoint " + std::string(33, 'N') + " -33.80 151.30 2000 100",
         "letters, digits and underscores or fewer"},
        {&running, thirteen, "at most 12"},
        {&running, "orbit A -33.80 151.30 2000 2000 100 256 left", "at most 255"},
    };
    std::size_t covered = 0;
    for (const Refusal& r : refusals) {
        const std::string why = verdict(*r.now, r.answer);
        check(why.find(r.says) != std::string::npos,
              "\"" + r.answer + "\" is refused, saying \"" + r.says + "\": " +
                  (why.empty() ? "it was taken" : why));
        ++covered;
    }
    check(covered == 17 && refusals.size() == 17, "all 17 ways an answer is refused were tried");
}

// **A copilot's route may end in a landing** (`land`, last), on a runway it
// was told of - within 100 m of its threshold and 5 degrees of its heading -
// under power, by an aircraft with an approach speed, after no orbit flown
// for ever; and on nothing else. The space is the one taken and these eight.
GLIDESLOPE_TEST(a_copilots_route_may_end_in_a_landing_on_a_runway_it_was_told_of_and_on_no_other) {
    const auto brief = cessna_brief();
    auto no_approach = brief;
    no_approach.approach_kts = 0;
    auto running = off_bondi(true);
    glideslope::world::RunwayEnd yssy;
    yssy.airport = "YSSY";
    yssy.ident = "16R";
    yssy.latitude_deg = -33.929401;
    yssy.longitude_deg = 151.171997;
    yssy.elevation_ft = 8;
    yssy.heading_deg = 168;
    yssy.length_m = 3962;
    running.fields = {yssy};
    auto stopped = off_bondi(false);
    stopped.fields = running.fields;
    const auto told = glideslope::copilot::situation_text(brief, running);
    check(told.find("YSSY runway 16R") != std::string::npos,
          "the copilot is told the runway it may land on");
    check(glideslope::copilot::copilot_instructions().find(
              "land NAME LATITUDE LONGITUDE ELEVATION_FT HEADING_DEG LENGTH_M") !=
              std::string::npos,
          "and how to land on it");

    const std::string to_16r = "land YSSY_16R -33.929401 151.171997 8 168 3962\n";
    const auto landing = glideslope::copilot::read_change(
        brief, running, "waypoint NORTH_HEAD -33.82 151.29 2000 90\n" + to_16r);
    check(landing.plan.landing && landing.plan.landing->name == "YSSY_16R" &&
              landing.plan.landing->heading_deg == 168.0 &&
              glideslope::copilot::change_refusal(brief, running, landing).empty(),
          "a route ending in a landing on a runway it was told of is flown");
    check(glideslope::copilot::landing_field(running, *landing.plan.landing) ==
              &running.fields.front(),
          "and the runway it lands on is that one");

    const auto verdict = [&](const glideslope::copilot::Brief& b,
                             const glideslope::copilot::Situation& now, const std::string& answer) {
        try {
            const auto change = glideslope::copilot::read_change(b, now, answer);
            return glideslope::copilot::change_refusal(b, now, change);
        } catch (const glideslope::sim::FlightPlanError& e) {
            return std::string(e.what());
        }
    };
    struct Refusal {
        const glideslope::copilot::Brief* brief;
        const glideslope::copilot::Situation* now;
        std::string answer;
        std::string says;
    };
    const std::string north = "waypoint NORTH_HEAD -33.82 151.29 2000 90\n";
    const std::vector<Refusal> refusals{
        {&brief, &running, to_16r + north, "the last line"},
        {&brief, &running, north + "land YSSY_16R -33.939 151.172 8 168 3962", "none of the runways"},
        {&brief, &running, north + "land YSSY_34L -33.929401 151.171997 8 348 3962",
         "none of the runways"},
        {&brief, &stopped, "glide 68\n" + north + to_16r, "a glide does not land"},
        {&brief, &running, "orbit ROUND -33.82 151.29 2000 2000 90 0 left\n" + to_16r,
         "goes round for ever"},
        {&no_approach, &running, north + to_16r, "no approach speed"},
        {&brief, &running, north + "land YSSY-16R -33.929401 151.171997 8 168 3962",
         "letters, digits and underscores"},
        {&brief, &running, north + "land YSSY_16R -33.929401 151.171997 8 168", "land NAME"},
    };
    std::size_t covered = 0;
    for (const Refusal& r : refusals) {
        const std::string why = verdict(*r.brief, *r.now, r.answer);
        check(why.find(r.says) != std::string::npos,
              "\"" + r.answer + "\" is refused, saying \"" + r.says + "\": " +
                  (why.empty() ? "it was taken" : why));
        ++covered;
    }
    check(covered == 8 && refusals.size() == 8, "all 8 ways a landing is refused were tried");
}

GLIDESLOPE_TEST(a_copilots_answer_refused_is_told_back_to_the_model_until_one_can_be_flown) {
    const auto brief = cessna_brief();
    Scripted second_time({"waypoint A -33.80 151.30 400 100",
                          "waypoint A -33.80 151.30 2000 100"});
    const auto change = glideslope::copilot::decide(second_time, brief, off_bondi(true));
    check(change.attempts == 2 && change.refused.size() == 1 &&
              change.refused[0].find("below 500 ft") != std::string::npos,
          "the first answer refused, the second flown");
    check(second_time.conversations.size() == 2 &&
              second_time.conversations[1].size() == 3 &&
              second_time.conversations[1][2].text.find("below 500 ft") != std::string::npos,
          "the model is told why, and asked again");
    check(second_time.conversations[0][0].text.find("follow the coast north to Palm Beach") !=
                  std::string::npos &&
              second_time.conversations[0][0].text.find("a routine look") != std::string::npos,
          "it is told the pilot's task and why it is asked");

    Scripted stubborn({"turn left"});
    try {
        (void)glideslope::copilot::decide(stubborn, brief, off_bondi(true));
        fail("an answer refused three times was taken");
    } catch (const ProviderError& e) {
        check(stubborn.conversations.size() == 3 &&
                  std::string(e.what()).find("answers were each refused") != std::string::npos,
              std::string("three answers, each refused, and said so: ") + e.what());
    }
}

namespace {

// A provider that answers `keep` only once let go, and says whether it was
// let go or gave up waiting - a minute, far longer than the steps it waits
// for take on the slowest machine - after which it answers at once.
class Held : public glideslope::copilot::Provider {
public:
    std::string name() const override {
        return "held";
    }
    std::string model() const override {
        return "none";
    }
    std::string answer(const std::string&, const std::vector<glideslope::copilot::Turn>&) override {
        std::unique_lock lock(m_);
        if (!let_go_cv_.wait_for(lock, std::chrono::minutes(1), [this] { return let_go_; })) {
            // Given up on, it answers at once from then on: a copilot that
            // waited for it is seen to have been answered too soon, not hung.
            gave_up_ = true;
            let_go_ = true;
        }
        return "keep";
    }
    void let_go() {
        {
            std::lock_guard lock(m_);
            let_go_ = true;
        }
        let_go_cv_.notify_all();
    }
    bool gave_up() {
        std::lock_guard lock(m_);
        return gave_up_;
    }

private:
    std::mutex m_;
    std::condition_variable let_go_cv_;
    bool let_go_ = false;
    bool gave_up_ = false;
};

} // namespace

GLIDESLOPE_TEST(the_copilot_asks_on_a_thread_of_its_own_and_the_step_never_waits_for_the_model) {
    auto held = std::make_unique<Held>();
    Held& model = *held;
    glideslope::copilot::Copilot copilot(std::move(held), cessna_brief());
    check(copilot.ask(off_bondi(true)), "asked");
    check(copilot.asking(), "and a question is outstanding");
    // Ten simulated seconds of steps, each asking for the answer and asking
    // again, while the model has not answered: none waits, or the model is
    // never let go and gives up a minute later.
    constexpr int steps = 10 * 120;
    int stepped = 0;
    int asked_again = 0;
    int answered = 0;
    for (; stepped < steps; ++stepped) {
        answered += copilot.answered() ? 1 : 0;
        asked_again += copilot.ask(off_bondi(true)) ? 1 : 0;
    }
    model.let_go();
    check(stepped == steps && answered == 0 && asked_again == 0,
          "1200 steps with the model thinking: nothing answered, nothing asked again (" +
              std::to_string(answered) + " answered, " + std::to_string(asked_again) +
              " asked again)");
    // Picked up between steps once it has come.
    std::optional<glideslope::copilot::Change> change;
    while (!change) {
        change = copilot.answered();
        std::this_thread::yield();
    }
    check(change->keep && !copilot.asking(), "the answer is taken once, and nothing is outstanding");
    check(!model.gave_up(), "the model was let go by the steps, not given up on");
    check(!copilot.answered(), "and taken only once");
}

GLIDESLOPE_TEST(a_recording_played_back_but_its_numbers_answers_a_request_whose_figures_moved_and_no_other) {
    const auto file = scratch("copilot-numbers.jsonl");
    std::vector<Sent> sent;
    const Post recorded = glideslope::copilot::recording(stand_in(sent, {answered(200, "keep")}), file);
    glideslope::platform::HttpRequest request;
    request.url = "https://api.example/v1";
    (void)recorded(request, "at -33.8900 151.2800, 2000 ft, vertical speed -3 ft a minute");

    const auto body = [](const glideslope::platform::HttpResponse& r) {
        return std::string(r.body.begin(), r.body.end());
    };
    const auto moved = "at -33.8911 151.2799, 2004 ft, vertical speed 12.5 ft a minute";
    check(body(glideslope::copilot::playback(file, glideslope::copilot::Match::but_numbers)(
              request, moved)) == "keep",
          "its figures moved, its words the same: answered");
    std::size_t refused = 0;
    try {
        (void)glideslope::copilot::playback(file)(request, moved);
        fail("played back exactly, a request whose figures moved was answered");
    } catch (const ProviderError&) {
        ++refused;
    }
    try {
        (void)glideslope::copilot::playback(file, glideslope::copilot::Match::but_numbers)(
            request, "at -33.8900 151.2800, 2000 ft, the engine stopped");
        fail("a request in other words was answered");
    } catch (const ProviderError& e) {
        check(std::string(e.what()).find("recorded again") != std::string::npos, e.what());
        ++refused;
    }
    check(refused == 2, "moved figures played back exactly, and other words: both refused");
}

// **Runways nearby come nearest first**, and two at nearly the same distance
// swap places in a flight flown a little differently: the coast recording
// broke so (PROJECT_STATUS, 2026-10-02). Played back but its numbers, the
// same runways in the other order are the same question; another runway,
// or the route's waypoints in another order, are not.
GLIDESLOPE_TEST(a_recording_played_back_but_its_numbers_answers_the_same_runways_listed_in_another_order) {
    const auto brief = cessna_brief();
    const auto end = [](const std::string& airport, const std::string& ident, double lat,
                        double lon) {
        glideslope::world::RunwayEnd e;
        e.airport = airport;
        e.ident = ident;
        e.latitude_deg = lat;
        e.longitude_deg = lon;
        e.elevation_ft = 21;
        e.heading_deg = 154;
        e.length_m = 3962;
        return e;
    };
    const auto runway_34l = end("YSSY", "34L", -33.9615, 151.1797);
    const auto runway_16r = end("YSSY", "16R", -33.9310, 151.1690);
    const auto runway_29 = end("YSBK", "29", -33.9200, 150.9970);
    const auto waypoint = [](const std::string& name, double lat) {
        glideslope::sim::Waypoint w;
        w.name = name;
        w.latitude_deg = lat;
        w.longitude_deg = 151.28;
        w.altitude_ft = 2000;
        w.airspeed_kts = 100;
        return w;
    };
    const auto told = [&](std::vector<glideslope::world::RunwayEnd> fields,
                          std::vector<glideslope::sim::Waypoint> route) {
        auto now = off_bondi(true);
        now.fields = std::move(fields);
        now.route = std::move(route);
        // As a provider sends it: the situation, a string in a JSON body.
        return glideslope::world::write_json(Json::make_object(
            {{"input", Json::make_string(glideslope::copilot::situation_text(brief, now))}}));
    };
    const auto route = std::vector{waypoint("BONDI", -33.89), waypoint("MANLY", -33.80)};
    const auto file = scratch("copilot-runway-order.jsonl");
    std::vector<Sent> sent;
    glideslope::platform::HttpRequest request;
    request.url = "https://api.example/v1";
    (void)glideslope::copilot::recording(stand_in(sent, {answered(200, "keep")}), file)(
        request, told({runway_34l, runway_16r, runway_29}, route));
    const auto played = [&] {
        return glideslope::copilot::playback(file, glideslope::copilot::Match::but_numbers);
    };

    const auto swapped = told({runway_16r, runway_34l, runway_29}, route);
    const auto reply = played()(request, swapped);
    check(std::string(reply.body.begin(), reply.body.end()) == "keep",
          "the same runways in another order: answered");
    std::size_t refused = 0;
    const std::vector<std::pair<std::string, std::string>> others = {
        {"played back exactly", swapped},
        {"another runway in one's place", told({runway_16r, runway_29, runway_29}, route)},
        {"the route's waypoints in another order",
         told({runway_34l, runway_16r, runway_29}, {route[1], route[0]})}};
    for (const auto& [what, body] : others) {
        try {
            (void)(what == "played back exactly" ? glideslope::copilot::playback(file)
                                                 : played())(request, body);
            fail(what + ": answered");
        } catch (const ProviderError&) {
            ++refused;
        }
    }
    check(refused == others.size(), "each other question refused: " + std::to_string(refused) +
                                        " of " + std::to_string(others.size()));
}

// **A copilot going away gives up its question at once**: the model's
// request is abandoned (platform::HttpRequest::abandon) rather than waited
// out, so quitting is not held up by a model thinking. The stand-in service
// answers only when its request is abandoned, or after a minute, which is
// far longer than any machine takes to get there, and says which.
GLIDESLOPE_TEST(a_copilot_going_away_gives_up_its_question_at_once) {
    auto seen = std::make_shared<std::atomic<int>>(0); // 1 abandoned, 2 waited out
    auto asked = std::make_shared<std::atomic<bool>>(false);
    const Post never_answers = [seen, asked](const glideslope::platform::HttpRequest& request,
                                             const std::string&) {
        *asked = true;
        const auto until = std::chrono::steady_clock::now() + std::chrono::minutes(1);
        while (std::chrono::steady_clock::now() < until) {
            if (request.abandon != nullptr && request.abandon->load()) {
                *seen = 1;
                throw glideslope::platform::HttpError("abandoned");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        *seen = 2;
        return answered(200, openai_answer("keep"));
    };
    {
        glideslope::copilot::Copilot copilot(
            glideslope::copilot::make_provider("openai", "the-key", "", never_answers),
            cessna_brief());
        check(copilot.ask(off_bondi(true)), "asked");
        while (!*asked) {
            std::this_thread::yield();
        }
    }
    check(*seen == 1, "the request was abandoned as the copilot went, not waited out (" +
                          std::string(*seen == 2 ? "waited out" : "neither") + ")");
}

// **A plan or a route outside the speeds its aircraft holds clean is
// refused, and the model is told them**: an aircraft whose slowest is above
// its approach speed - a jet's approach speed is a flaps-down figure, and a
// plan is flown clean - and whose fastest is not a fifth over its cruise
// (sim::plan_speeds). Told to the planner and to the copilot, each saying
// why the slowest is not the approach speed; refused below the slowest and
// above the fastest by both, and taken at each. Where neither is given, the
// approach speed and a fifth over the cruise, as before.
GLIDESLOPE_TEST(a_plan_or_route_outside_the_speeds_its_aircraft_holds_clean_is_refused_and_the_model_told_them) {
    // The Cessna made a jet: 62 kt on the approach, but 80 the slowest and
    // 120, not 126, the fastest.
    glideslope::copilot::PlanRequest request = sydney("take off and orbit the CBD");
    request.slowest_kts = 80;
    request.fastest_kts = 120;
    const std::string asked = glideslope::copilot::planning_request(request);
    const std::string radius_at_80 =
        std::to_string(static_cast<int>(std::ceil(glideslope::sim::least_orbit_radius_m(80.0)))) +
        " m at 80 kt";
    for (const std::string& part : {std::string("62 kt on the approach"),
                                    std::string("every airspeed from 80 to 120 kt"),
                                    std::string("never slower than 80: a plan is flown clean, "
                                                "and the approach speed is for flaps down"),
                                    radius_at_80}) {
        check(asked.find(part) != std::string::npos,
              "the request says \"" + part + "\":\n" + asked);
    }
    const auto plan_at = [](int kts) {
        return "aircraft c172p\nrunway 34L -33.964298 151.181000 14 348 3962\ntakeoff 800\n"
               "waypoint CLIMB -33.92 151.19 3000 " + std::to_string(kts) + "\n";
    };
    for (const int kts : {70, 79, 121, 126}) {
        Scripted model({plan_at(kts), plan_at(100)});
        const auto planned = glideslope::copilot::plan_from_words(model, request);
        check(planned.refused.size() == 1 &&
                  planned.refused[0].find("outside 80 to 120 kt") != std::string::npos &&
                  model.conversations.size() == 2 &&
                  model.conversations[1][2].text.find("outside 80 to 120 kt") != std::string::npos,
              "a plan at " + std::to_string(kts) + " kt is refused and told back: " +
                  (planned.refused.empty() ? std::string("taken") : planned.refused[0]));
    }
    for (const int kts : {80, 120}) {
        Scripted model({plan_at(kts)});
        check(glideslope::copilot::plan_from_words(model, request).refused.empty(),
              "a plan at " + std::to_string(kts) + " kt is taken");
    }
    // Neither given: the approach speed and a fifth over the cruise.
    glideslope::copilot::PlanRequest unsaid = sydney("take off");
    Scripted at_approach({plan_at(62)});
    check(glideslope::copilot::plan_from_words(at_approach, unsaid).refused.empty() &&
              glideslope::copilot::planning_request(unsaid).find("never slower") ==
                  std::string::npos,
          "with none given, the approach speed is taken, and nothing said of a slowest");
    // **The radius the model is told is the one its plan is held to**: the
    // approach speed is rounded to whole knots where a request is filled
    // (frontend), so the least radius said at the slowest is the least the
    // plan reader allows at that speed - an orbit at exactly it is taken,
    // a metre tighter refused.
    {
        const double least = std::ceil(glideslope::sim::least_orbit_radius_m(80.0));
        const auto orbit_at = [&](double radius_m) {
            return "aircraft c172p\nrunway 34L -33.964298 151.181000 14 348 3962\ntakeoff 800\n"
                   "orbit CBD -33.8688 151.2093 " +
                   std::to_string(static_cast<int>(radius_m)) + " 3000 80 1 left\n";
        };
        Scripted told({orbit_at(least - 1), orbit_at(least)});
        const auto planned = glideslope::copilot::plan_from_words(told, request);
        check(planned.refused.size() == 1 &&
                  planned.refused[0].find("too tight") != std::string::npos,
              "an orbit a metre tighter than the radius told is refused, and one at it taken: " +
                  (planned.refused.empty() ? std::string("none refused") : planned.refused[0]));
    }
    // A slowest below the approach speed is the approach speed.
    unsaid.slowest_kts = 50;
    Scripted below({plan_at(55), plan_at(62)});
    check(glideslope::copilot::plan_from_words(below, unsaid).refused.size() == 1,
          "a slowest below the approach speed lets nothing below the approach through");

    // The copilot's routes, the same way: the server checks them with the
    // same function (frontend/server/main.cpp).
    glideslope::copilot::Brief brief = cessna_brief();
    brief.slowest_kts = 80;
    brief.fastest_kts = 120;
    const auto running = off_bondi(true);
    const std::string told = glideslope::copilot::situation_text(brief, running);
    check(told.find("Every airspeed from 80 to 120 kt, never slower than 80: a route is flown "
                    "clean, and the approach speed is for flaps down") != std::string::npos &&
              told.find(radius_at_80) != std::string::npos,
          "the copilot is told the slowest and fastest, and why:\n" + told);
    const auto route_at = [&](int kts) {
        const auto change = glideslope::copilot::read_change(
            brief, running, "waypoint MANLY -33.80 151.30 2000 " + std::to_string(kts) + "\n");
        return glideslope::copilot::change_refusal(brief, running, change);
    };
    for (const int kts : {70, 79, 121, 126}) {
        check(route_at(kts).find("outside 80 to 120 kt") != std::string::npos,
              "a route at " + std::to_string(kts) + " kt is refused: " + route_at(kts));
    }
    check(route_at(80).empty() && route_at(120).empty(), "at 80 and 120 kt it is flown");
}

// **Every aircraft can be planned by a model and routed by a copilot**: each
// the catalogue holds is briefed from the data as the client, the server and
// glideslope_cli brief it (frontend::brief_for, plan_request_for) - the
// 747-400 and the F-22A, which publish no stall speed, with no approach speed
// and their climb-away speeds measured from their models (2026-10-06) - and
// for each a plan taking off from Sydney's 34L at its slowest planned speed,
// to a waypoint and round its tightest orbit, is taken first time; a route at
// that speed is taken by its copilot; and with the engine stopped, a glide
// at the middle of the speeds it may glide at. Nothing is told "0 kt".
GLIDESLOPE_TEST(every_aircraft_is_planned_and_routed_from_its_own_speeds_with_or_without_an_approach_speed) {
    const auto data = std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
    const auto catalogue = glideslope::sim::read_catalogue(data);
    std::size_t planned = 0;
    std::size_t routed = 0;
    std::size_t glided = 0;
    std::vector<std::string> without_approach;
    for (const auto& entry : catalogue) {
        glideslope::copilot::PlanRequest request =
            glideslope::frontend::plan_request_for(data, entry.id);
        const glideslope::copilot::PlanRequest at_sydney = sydney("orbit the CBD");
        request.command = at_sydney.command;
        request.airport = at_sydney.airport;
        request.runways = at_sydney.runways;
        if (request.approach_kts <= 0.0) {
            without_approach.push_back(entry.id);
        }
        const double kts = glideslope::copilot::slowest_planned_kts(request);
        const double radius_m = std::ceil(glideslope::sim::least_orbit_radius_m(kts)) + 10.0;
        char plan[400];
        std::snprintf(plan, sizeof plan,
                      "aircraft %s\nrunway 34L -33.964298 151.181000 14 348 3962\ntakeoff 800\n"
                      "waypoint A -33.92 151.19 3000 %.0f\n"
                      "orbit B -33.8688 151.2093 %.0f 3000 %.0f 1 left\n",
                      entry.id.c_str(), kts, radius_m, kts);
        Scripted model({plan});
        const auto taken = glideslope::copilot::plan_from_words(model, request);
        const std::string& asked = model.conversations.at(0).at(0).text;
        const bool told_right =
            asked.find(" 0 kt") == std::string::npos &&
            asked.find(request.approach_kts > 0.0 ? "kt on the approach" : "kt climbing away") !=
                std::string::npos;
        check(told_right, entry.id + "'s request:\n" + asked);
        if (taken.attempts == 1 && taken.refused.empty() && told_right) {
            ++planned;
        } else {
            fail(entry.id + "'s plan was refused: " +
                 (taken.refused.empty() ? std::string("?") : taken.refused[0]));
        }

        glideslope::copilot::Brief brief = glideslope::frontend::brief_for(data, entry.id);
        brief.task = "orbit the CBD";
        const auto running = off_bondi(true);
        const std::string told = glideslope::copilot::situation_text(brief, running);
        check(told.find(" 0 kt") == std::string::npos, entry.id + " is told 0 kt:\n" + told);
        char route[200];
        std::snprintf(route, sizeof route, "orbit B -33.8688 151.2093 %.0f 3000 %.0f 1 left\n",
                      radius_m, kts);
        const auto change = glideslope::copilot::read_change(brief, running, route);
        const std::string why = glideslope::copilot::change_refusal(brief, running, change);
        check(why.empty(), entry.id + "'s route refused: " + why);
        routed += why.empty() ? 1U : 0U;

        const auto stopped = off_bondi(false);
        const auto glide = glideslope::copilot::glide_speeds(brief);
        const double glide_kts = std::round((glide.slowest_kts + glide.fastest_kts) / 2.0);
        char glide_route[200];
        std::snprintf(glide_route, sizeof glide_route,
                      "glide %.0f\norbit B -33.8688 151.2093 %.0f 21 %.0f 0 left\n", glide_kts,
                      std::ceil(glideslope::sim::least_orbit_radius_m(glide_kts)) + 10.0,
                      glide_kts);
        const auto gliding = glideslope::copilot::read_change(brief, stopped, glide_route);
        const std::string glide_why = glideslope::copilot::change_refusal(brief, stopped, gliding);
        check(glide_why.empty() && glide.slowest_kts > 0.0,
              entry.id + "'s glide at " + std::to_string(glide_kts) + " kt refused: " + glide_why);
        glided += glide_why.empty() ? 1U : 0U;
    }
    check(without_approach == std::vector<std::string>{"747-400", "f22"},
          "the 747-400 and the F-22A are the two with no approach speed");
    check(planned == catalogue.size() && routed == catalogue.size() &&
              glided == catalogue.size() && catalogue.size() == 16,
          "every aircraft of the 16 planned, routed and glided: " + std::to_string(planned) +
              ", " + std::to_string(routed) + ", " + std::to_string(glided) + " of " +
              std::to_string(catalogue.size()));
}
