#include "harness.hpp"

#include "copilot/copilot.hpp"
#include "copilot/planner.hpp"
#include "copilot/provider.hpp"
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
        {&running, "turn left now", "none of keep, glide, waypoint or orbit"},
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
