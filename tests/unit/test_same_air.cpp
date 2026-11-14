// The same air and the same ground on the server and every client
// (src/frontend/same_air.hpp, src/net/told.hpp; REQUIREMENTS.md 6.3).

#include "harness.hpp"

#include "frontend/same_air.hpp"
#include "net/messages.hpp"
#include "net/told.hpp"
#include "sim/fixed_step.hpp"
#include "world/digest.hpp"
#include "world/metar.hpp"
#include "world/weather.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

using glideslope::test::check;

namespace {

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return {v.data(), v.size()};
}

// A report with everything a report can have: gusts, wind shear, turbulence
// given, a microburst, and a forecast above it with levels and near-ground
// winds - so that a field dropped on the way is a difference in the air.
glideslope::world::WeatherReport a_full_report(const std::string& metar) {
    glideslope::world::WeatherReport r;
    r.surface.metar = glideslope::world::parse_metar(metar);
    r.surface.latitude_deg = -33.9461;
    r.surface.longitude_deg = 151.1772;
    r.surface.elevation_m = 6.0;
    r.turbulence_severity = 3;
    r.air_seed = glideslope::world::air_seed_of(r.surface.metar);
    glideslope::world::Microburst b;
    b.latitude_deg = -33.95;
    b.longitude_deg = 151.18;
    b.radius_m = 1200.0;
    b.downdraught_mps = 9.0;
    b.start_s = 30.0;
    b.duration_s = 600.0;
    r.microbursts = {b};
    glideslope::world::WindsAloft a;
    a.latitude_deg = r.surface.latitude_deg;
    a.longitude_deg = r.surface.longitude_deg;
    a.time = "2026-10-02T06:00";
    a.levels = {{1000.0, 110.0, 4.0, 9.0, 18.0},
                {925.0, 780.0, 7.0, 14.0, 13.0},
                {850.0, 1490.0, 9.0, 18.0, 8.5},
                {700.0, 3010.0, 12.0, 25.0, -1.0}};
    a.near_ground = {{10.0, 3.0, 6.0}, {80.0, 4.0, 8.0}, {120.0, 4.5, 8.5}, {180.0, 5.0, 9.0}};
    r.aloft = a;
    return r;
}

// Hills, so that the air rising over the ground differs from place to place.
double hills(double lat, double lon) {
    return 150.0 + 120.0 * std::sin(lat * 400.0) * std::cos(lon * 300.0);
}

// What the server sends of `report`, through the wire's bytes, into `told`.
void send(const glideslope::world::WeatherReport& report, double changed_at_s, double blend_s,
          glideslope::net::Told& told, glideslope::frontend::HeardAir& client) {
    const glideslope::frontend::WeatherSaid said =
        glideslope::frontend::weather_said(&report, changed_at_s, blend_s);
    check(said.weather.aloft_follows && said.aloft, "a report with a forecast says one follows");
    check(told.hear(all_of(glideslope::net::write(said.weather))) ==
              glideslope::net::Told::Heard::nothing,
          "a weather whose forecast follows is not taken alone");
    check(told.hear(all_of(glideslope::net::write(*said.aloft))) ==
              glideslope::net::Told::Heard::weather,
          "and is taken with its forecast");
    client.heard(*told.weather(), told.aloft());
}

} // namespace

// **The weather a server sends is the air it flies, on the client too.** A
// server flies a full report, and fifteen minutes in a second one blends in
// over five; the client hears each through the wire's bytes and flies what it
// made of them. The air is compared at every one of 3 places, 6 heights and
// 8 times - before the change, through its blend and after - 144 points, each
// to the last bit: wind in three axes, temperature, pressure, turbulence and
// the wind at 20 ft.
GLIDESLOPE_TEST(the_weather_a_server_sends_is_flown_to_the_last_bit_by_its_clients) {
    double now_s = 0.0;
    const auto clock = [&now_s] { return now_s; };
    const auto server_air = std::make_shared<glideslope::world::ReportedWeather>(
        a_full_report("YSSY 020600Z 34018G30KT 9999 FEW030 24/12 Q1012 WS R34L"), nullptr,
        300.0, hills);
    glideslope::frontend::SessionClocked server(server_air, clock);
    glideslope::net::Told told;
    glideslope::frontend::HeardAir client(nullptr, hills, clock);
    send(server_air->report(), 0.0, 300.0, told, client);
    check(client.air() != nullptr, "the client flies a weather");

    const std::vector<std::array<double, 2>> places{
        {-33.9461, 151.1772}, {-33.95, 151.18}, {-33.80, 151.30}};
    const std::vector<double> heights{2.0, 9.0, 60.0, 300.0, 1200.0, 2800.0};
    const std::vector<double> times{10.0, 100.0, 900.0, 960.0, 1050.0, 1150.0, 1200.0, 2000.0};
    std::size_t compared = 0;
    bool changed = false;
    for (const double t : times) {
        if (!changed && t >= 900.0) {
            // **The change**, on the session's clock, as the server makes it.
            changed = true;
            now_s = 900.0;
            server_air->update(a_full_report("YSSY 020630Z 20025KT 9999 BKN020 19/15 Q1004"),
                               now_s);
            send(server_air->report(), now_s, 300.0, told, client);
            check(told.weathers() == 2, "the client has heard two weathers");
        }
        now_s = t;
        for (const auto& p : places) {
            for (const double h : heights) {
                // Each asked at a time of its own: the session's is used.
                const glideslope::sim::Conditions a = server.at(p[0], p[1], h, 1.0);
                const glideslope::sim::Conditions b = client.air()->at(p[0], p[1], h, 77.0);
                check(a.wind_north_mps == b.wind_north_mps && a.wind_east_mps == b.wind_east_mps &&
                          a.wind_down_mps == b.wind_down_mps &&
                          a.temperature_offset_c == b.temperature_offset_c &&
                          a.sea_level_pressure_hpa == b.sea_level_pressure_hpa &&
                          a.turbulence_severity == b.turbulence_severity &&
                          a.wind_at_20ft_mps == b.wind_at_20ft_mps,
                      "the air at " + std::to_string(h) + " m, " + std::to_string(t) +
                          " s is the server's");
                ++compared;
            }
        }
    }
    check(compared == 3 * 6 * 8, "every point was compared: " + std::to_string(compared));
    // **And the change is in it**: the air after the blend is not the air
    // before the change, so that the comparison above was of two weathers.
    glideslope::frontend::HeardAir first(nullptr, hills, clock);
    glideslope::net::Told told_first;
    send(a_full_report("YSSY 020600Z 34018G30KT 9999 FEW030 24/12 Q1012 WS R34L"), 0.0, 300.0,
         told_first, first);
    now_s = 2000.0;
    check(first.air()->at(-33.80, 151.30, 1200.0, 0.0).wind_east_mps !=
              client.air()->at(-33.80, 151.30, 1200.0, 0.0).wind_east_mps,
          "the second weather is other air than the first");
    std::printf("  %zu points of the air, each the server's to the last bit\n", compared);
}

// **A client joining mid-blend blends from the same weather.** The server
// blends a second report in over 300 s from 900 s; at 1000 s a client joins.
// It is told what the server blends from (world::ReportedWeather::
// blending_from) and then the new one, and a flight made from those two -
// the weather before the newest (net::Told::before), then the newest - flies
// the server's air to the last bit at every one of 3 places, 6 heights and 5
// times through the rest of the blend and after it. One made from the newest
// alone does not, which is what made this a tail. Past the blend, nothing is
// blended from.
GLIDESLOPE_TEST(a_client_joining_mid_blend_is_told_what_it_blends_from_and_flies_the_servers_air) {
    double now_s = 0.0;
    const auto clock = [&now_s] { return now_s; };
    const glideslope::world::WeatherReport first =
        a_full_report("YSSY 020600Z 34018G30KT 9999 FEW030 24/12 Q1012 WS R34L");
    const auto server_air =
        std::make_shared<glideslope::world::ReportedWeather>(first, nullptr, 300.0, hills);
    glideslope::frontend::SessionClocked server(server_air, clock);
    check(server_air->blending_from(10.0) == nullptr, "a first weather blends from nothing");
    now_s = 900.0;
    server_air->update(a_full_report("YSSY 020630Z 20025KT 9999 BKN020 19/15 Q1004"), now_s);
    now_s = 1000.0;
    const glideslope::world::WeatherReport* from = server_air->blending_from(now_s);
    check(from != nullptr && from->surface.metar.raw == first.surface.metar.raw,
          "mid-blend, the server blends from the first weather");
    check(server_air->blending_from(1200.0) == nullptr, "and past the blend, from none");

    glideslope::net::Told told;
    glideslope::frontend::HeardAir as_told(nullptr, hills, clock);
    send(*from, 0.0, 300.0, told, as_told);
    send(server_air->report(), 900.0, 300.0, told, as_told);
    check(told.stations().at(0).before && told.stations().at(0).before->metar == first.surface.metar.raw,
          "the weather before the newest is kept");
    // A flight made since, from what was kept: the one before, then the newest.
    glideslope::frontend::HeardAir made_since(nullptr, hills, clock);
    made_since.heard(*told.stations().at(0).before, told.stations().at(0).before_aloft);
    made_since.heard(*told.weather(), told.aloft());
    glideslope::frontend::HeardAir newest_alone(nullptr, hills, clock);
    newest_alone.heard(*told.weather(), told.aloft());

    std::size_t compared = 0;
    std::size_t newest_alone_differs = 0;
    const auto same = [](const glideslope::sim::Conditions& a,
                         const glideslope::sim::Conditions& b) {
        return a.wind_north_mps == b.wind_north_mps && a.wind_east_mps == b.wind_east_mps &&
               a.wind_down_mps == b.wind_down_mps &&
               a.temperature_offset_c == b.temperature_offset_c &&
               a.sea_level_pressure_hpa == b.sea_level_pressure_hpa &&
               a.turbulence_severity == b.turbulence_severity &&
               a.wind_at_20ft_mps == b.wind_at_20ft_mps;
    };
    const std::vector<std::array<double, 2>> places{
        {-33.9461, 151.1772}, {-33.95, 151.18}, {-33.80, 151.30}};
    for (const double t : {1000.0, 1050.0, 1100.0, 1199.0, 1300.0}) {
        now_s = t;
        for (const auto& p : places) {
            for (const double h : {2.0, 9.0, 60.0, 300.0, 1200.0, 2800.0}) {
                const glideslope::sim::Conditions a = server.at(p[0], p[1], h, 0.0);
                const std::string where =
                    " at " + std::to_string(h) + " m, " + std::to_string(t) + " s";
                check(same(a, as_told.air()->at(p[0], p[1], h, 0.0)),
                      "the joining client's air is the server's" + where);
                check(same(a, made_since.air()->at(p[0], p[1], h, 0.0)),
                      "a flight made since flies the server's air" + where);
                if (t < 1200.0 && !same(a, newest_alone.air()->at(p[0], p[1], h, 0.0))) {
                    ++newest_alone_differs;
                }
                ++compared;
            }
        }
    }
    check(compared == 3 * 6 * 5, "every point was compared: " + std::to_string(compared));
    check(newest_alone_differs == 3 * 6 * 4,
          "the newest alone is other air at every point inside the blend: " +
              std::to_string(newest_alone_differs) + " of 72");
    std::printf("  %zu points of the air joined mid-blend, each the server's to the last bit; "
                "the newest alone differs at %zu of the 72 inside the blend\n",
                compared, newest_alone_differs);
}

// **A client's gusts are the server's at the server's moment, and only
// then.** A server flies 30 kt gusting 45; the client hears it through the
// wire's bytes. At 4 places, 4 heights under the 610 m the gusts fade by,
// and 41 moments half a second apart, the client's air at the server's
// moment is the server's to the last bit; and its air at a moment skewed by
// one step, two steps and five seconds - a clock as wrong as a clocks'
// difference settling, and as one not settled at all - differs at every one
// of the 656 points, by more than 1 m/s somewhere at five seconds. So what
// decides whether a predicting client meets the server's gusts is only the
// moment it asks for (sim::Prediction::session_time_s, held by
// every_step_a_client_flies_meets_the_air_at_the_moment_the_server_flies_it).
GLIDESLOPE_TEST(a_clients_gusts_are_the_servers_at_the_servers_moment_and_differ_at_a_skewed_one) {
    double now_s = 0.0;
    const auto server_clock = [&now_s] { return now_s; };
    double skew_s = 0.0;
    const auto client_clock = [&now_s, &skew_s] { return now_s + skew_s; };
    const auto server_air = std::make_shared<glideslope::world::ReportedWeather>(
        a_full_report("YSSY 020600Z 27030G45KT 9999 FEW030 20/10 Q1012"), nullptr, 300.0,
        hills);
    glideslope::frontend::SessionClocked server(server_air, server_clock);
    glideslope::net::Told told;
    glideslope::frontend::HeardAir client(nullptr, hills, client_clock);
    send(server_air->report(), 0.0, 300.0, told, client);

    const std::vector<std::array<double, 2>> places{
        {-33.9461, 151.1772}, {-33.95, 151.18}, {-33.90, 151.10}, {-33.80, 151.30}};
    const std::vector<double> heights{9.0, 60.0, 200.0, 450.0};
    const double step = 1.0 / static_cast<double>(glideslope::sim::steps_per_second);
    struct Skew {
        double s;
        const char* what;
    };
    const std::vector<Skew> skews{{0.0, "the server's moment"},
                                  {step, "one step out"},
                                  {2.0 * step, "two steps out"},
                                  {5.0, "five seconds out"}};
    std::size_t walked = 0;
    for (const Skew& k : skews) {
        skew_s = k.s;
        std::size_t points = 0;
        std::size_t differ = 0;
        double largest_mps = 0.0;
        for (int i = 0; i <= 40; ++i) {
            now_s = 600.0 + 0.5 * i;
            for (const auto& p : places) {
                for (const double h : heights) {
                    const glideslope::sim::Conditions a = server.at(p[0], p[1], h, 0.0);
                    const glideslope::sim::Conditions b = client.air()->at(p[0], p[1], h, 0.0);
                    const double d = std::hypot(a.wind_north_mps - b.wind_north_mps,
                                                a.wind_east_mps - b.wind_east_mps,
                                                a.wind_down_mps - b.wind_down_mps);
                    largest_mps = std::max(largest_mps, d);
                    if (d > 0.0) {
                        ++differ;
                    }
                    ++points;
                }
            }
        }
        check(points == 41 * 4 * 4, std::string(k.what) + ": every point was walked");
        walked += points;
        if (k.s == 0.0) {
            check(differ == 0, "at the server's moment the client's gusts are the server's: " +
                                   std::to_string(differ) + " points differ");
        } else {
            check(differ == points, std::string(k.what) + ", the gusts differ at " +
                                        std::to_string(differ) + " of " +
                                        std::to_string(points) + " points, not all");
        }
        if (k.s >= 5.0) {
            check(largest_mps > 1.0, "five seconds out the wind differs by at most " +
                                         std::to_string(largest_mps) + " m/s, not over 1");
        }
        std::printf("  %s: %zu of %zu points differ, by %.4f m/s at most\n", k.what, differ,
                    points, largest_mps);
    }
    check(walked == 4 * 656, "four clocks of 656 points: " + std::to_string(walked));
}

// **Still air is said, and flown as none.** A server with no weather sends an
// empty METAR and nothing else; a client hearing it flies no weather - as the
// server does - and a client that had a weather flies the standard atmosphere
// with no wind, because an aircraft keeps the last weather it was given.
GLIDESLOPE_TEST(still_air_is_said_as_an_empty_metar_and_flown_as_no_weather) {
    const glideslope::frontend::WeatherSaid still =
        glideslope::frontend::weather_said(nullptr, 0.0, 300.0);
    check(still.weather.metar.empty() && !still.aloft && !still.weather.aloft_follows,
          "still air is an empty METAR with no forecast");
    glideslope::net::Told told;
    check(told.hear(all_of(glideslope::net::write(still.weather))) ==
              glideslope::net::Told::Heard::weather,
          "still air reads");
    glideslope::frontend::HeardAir fresh(nullptr, {}, [] { return 0.0; });
    fresh.heard(*told.weather(), told.aloft());
    check(fresh.air() == nullptr && fresh.reported(-33.9, 151.2) == nullptr,
          "a client that hears still air first flies no weather");

    glideslope::frontend::HeardAir had(nullptr, {}, [] { return 0.0; });
    glideslope::net::Told told2;
    const glideslope::world::WeatherReport windy =
        a_full_report("YSSY 020600Z 34030KT 9999 24/12 Q1012");
    send(windy, 0.0, 0.0, told2, had);
    check(had.air() != nullptr && had.air()->at(-33.9, 151.2, 500.0, 0.0).wind_east_mps != 0.0,
          "a client that heard a wind flies it");
    had.heard(still.weather, std::nullopt);
    const glideslope::sim::Conditions calm = had.air()->at(-33.9, 151.2, 500.0, 0.0);
    check(had.reported(-33.9, 151.2) == nullptr && calm.wind_north_mps == 0.0 && calm.wind_east_mps == 0.0 &&
              calm.wind_down_mps == 0.0 && calm.sea_level_pressure_hpa == 1013.25,
          "and then still air flies the standard atmosphere with no wind");
}

// **A forecast with no weather before it is no weather**, and a weather
// whose forecast follows is not taken until it has come.
GLIDESLOPE_TEST(a_weather_is_taken_whole_or_not_at_all) {
    const glideslope::world::WeatherReport r = a_full_report("YSSY 020600Z 34018KT 9999 Q1012");
    const glideslope::frontend::WeatherSaid full = glideslope::frontend::weather_said(&r, 5.0, 60.0);
    glideslope::net::Told told;
    check(told.hear(all_of(glideslope::net::write(*full.aloft))) ==
              glideslope::net::Told::Heard::nothing,
          "a forecast alone is dropped");
    check(!told.weather() && told.weathers() == 0, "and is no weather");
    check(told.hear(all_of(glideslope::net::write(full.weather))) ==
              glideslope::net::Told::Heard::nothing,
          "a weather saying its forecast follows waits for it");
    check(!told.weather(), "and is not taken alone");
    check(told.hear(all_of(glideslope::net::write(*full.aloft))) ==
              glideslope::net::Told::Heard::weather,
          "the forecast after it completes it");
    check(told.weather() && told.aloft() && told.weathers() == 1 &&
              told.weather()->changed_at_s == 5.0 && told.weather()->blend_s == 60.0,
          "the whole weather, as said");
}

// **A METAR longer than a `WEATHER` carries is cut, and flown as cut**, by
// the server as by its clients: cut on the wire alone, the server would fly a
// report its clients never heard.
GLIDESLOPE_TEST(a_metar_too_long_to_send_is_cut_to_whole_words_and_flown_as_cut) {
    std::string metar = "KDEN 021753Z 16012G22KT 10SM FEW080 SCT150 BKN250 24/02 A3002 RMK AO2";
    while (metar.size() <= glideslope::net::most_metar_bytes + 40) {
        metar += " SLP120 T02440017";
    }
    glideslope::world::WeatherReport r;
    r.surface.metar = glideslope::world::parse_metar(metar);
    check(r.surface.metar.raw.size() > glideslope::net::most_metar_bytes, "the report is too long");
    const glideslope::world::WeatherReport cut = glideslope::frontend::fit_to_send(r);
    check(cut.surface.metar.raw.size() <= glideslope::net::most_metar_bytes, "cut, it fits");
    check(metar.rfind(cut.surface.metar.raw, 0) == 0, "and is the report's first whole words");
    check(cut.surface.metar.wind_speed_kt == r.surface.metar.wind_speed_kt,
          "with its wind still read");
    const glideslope::world::WeatherReport same = glideslope::frontend::fit_to_send(cut);
    check(same.surface.metar.raw == cut.surface.metar.raw, "a report that fits is left as it is");
}

// **The ground said is this build's**, and either file it is made from
// changing changes it. The hash is held against one worked here from the two
// files' own SHA-256s as `docs/TRANSPORT.md` says, so that what the document
// says is what is sent.
GLIDESLOPE_TEST(the_collision_ground_said_is_the_builds_coverage_strips_and_rules) {
    const glideslope::net::TerrainDataset ours = glideslope::frontend::collision_dataset(data());
    check(ours.sha256.size() == glideslope::net::sha256_bytes, "a whole SHA-256");
    const auto file_hash = [](const std::filesystem::path& f) {
        std::ifstream in(f, std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(in), {}};
        return glideslope::world::sha256_hex(std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
    };
    const std::string lines = "coverage.txt " + file_hash(data() / "dem" / "coverage.txt") +
                              "\nstrips.csv " + file_hash(data() / "runways" / "strips.csv") +
                              "\nground rules 2\n";
    check(glideslope::frontend::hash_hex(ours) ==
              glideslope::world::sha256_hex(std::span<const std::uint8_t>(
                  reinterpret_cast<const std::uint8_t*>(lines.data()), lines.size())),
          "the hash is the document's: SHA-256 over the two files' and the rules' lines");
    check(ours.name.size() <= glideslope::net::most_name_bytes &&
              ours.version.size() <= glideslope::net::most_name_bytes,
          "its name and version fit the message");
    check(glideslope::frontend::same_ground(ours, glideslope::frontend::collision_dataset(data())),
          "and it is the same every time");
    const std::filesystem::path copy =
        std::filesystem::temp_directory_path() / "glideslope_same_air_ground";
    for (const std::string file : {"dem/coverage.txt", "runways/strips.csv"}) {
        std::filesystem::remove_all(copy);
        std::filesystem::create_directories(copy / "dem");
        std::filesystem::create_directories(copy / "runways");
        std::filesystem::copy_file(data() / "dem" / "coverage.txt", copy / "dem" / "coverage.txt");
        std::filesystem::copy_file(data() / "runways" / "strips.csv",
                                   copy / "runways" / "strips.csv");
        check(glideslope::frontend::same_ground(
                  ours, glideslope::frontend::collision_dataset(copy)),
              "copied, it is the same ground");
        std::ofstream(copy / file, std::ios::app | std::ios::binary) << "\n";
        check(!glideslope::frontend::same_ground(
                  ours, glideslope::frontend::collision_dataset(copy)),
              "a byte more in " + file + " is other ground");
    }
    std::filesystem::remove_all(copy);
    std::printf("  %s\n", glideslope::frontend::describe(ours).c_str());
}

// **A METAR a client cannot read is refused, and the air before it kept**:
// a server of this version never sends one, and one that does must not end
// the client - parse_metar throws, and nothing above the air would catch it.
GLIDESLOPE_TEST(a_weather_whose_metar_cannot_be_read_is_refused_and_the_air_before_it_kept) {
    glideslope::frontend::HeardAir air(nullptr, {}, [] { return 0.0; });
    glideslope::net::Told told;
    send(a_full_report("YSSY 020600Z 27030KT 9999 24/12 Q1012"), 0.0, 0.0, told, air);
    const double east = air.air()->at(-33.9, 151.2, 500.0, 0.0).wind_east_mps;
    check(east > 10.0, "the first weather is flown");
    glideslope::net::Weather bad;
    bad.metar = "not a report at all";
    bad.latitude_deg = -33.9;
    bad.longitude_deg = 151.2;
    std::string why;
    check(!air.heard(bad, std::nullopt, &why), "a METAR that cannot be read is refused");
    check(!why.empty(), "with why: " + why);
    check(air.air()->at(-33.9, 151.2, 500.0, 0.0).wind_east_mps == east,
          "and the weather before it is still flown");
    glideslope::frontend::HeardAir fresh(nullptr, {}, [] { return 0.0; });
    check(!fresh.heard(bad, std::nullopt) && fresh.air() == nullptr,
          "refused first, there is no weather at all");
}

namespace {

// Runway 24 at 55.97 N 37.41 E, 3,000 m long, and its other end, 06.
std::vector<glideslope::world::RunwayEnd> runway_24_and_06() {
    constexpr double radians = 3.14159265358979323846 / 180.0;
    constexpr double metres_per_degree = 111319.49;
    glideslope::world::RunwayEnd end24;
    end24.airport = "UUEE";
    end24.ident = "24";
    end24.latitude_deg = 55.97;
    end24.longitude_deg = 37.41;
    end24.heading_deg = 240.0;
    end24.length_m = 3000.0;
    glideslope::world::RunwayEnd end06 = end24;
    end06.ident = "06";
    end06.heading_deg = 60.0;
    end06.latitude_deg = 55.97 + 3000.0 * std::cos(240.0 * radians) / metres_per_degree;
    end06.longitude_deg = 37.41 + 3000.0 * std::sin(240.0 * radians) /
                                      (metres_per_degree * std::cos(55.97 * radians));
    return {end24, end06};
}

glideslope::world::WeatherReport at_uuee(const std::string& metar) {
    glideslope::world::WeatherReport r;
    r.surface.metar = glideslope::world::parse_metar(metar);
    r.surface.latitude_deg = 55.97;
    r.surface.longitude_deg = 37.41;
    return r;
}

// What a server tells a client joining now of `air`, through the wire's bytes:
// the weather, then the runways' conditions.
void joins(const glideslope::world::ReportedWeather& air, double changed_at_s, double now_s,
           glideslope::net::Told& told, glideslope::frontend::HeardAir& client) {
    const glideslope::frontend::WeatherSaid said =
        glideslope::frontend::weather_said(&air.report(), changed_at_s, 0.0);
    check(told.hear(all_of(glideslope::net::write(said.weather))) ==
              glideslope::net::Told::Heard::weather,
          "the weather is heard");
    check(client.heard(*told.weather(), told.aloft()), "and flown");
    const glideslope::net::RunwayConditions rc =
        glideslope::frontend::runway_conditions_said(air, changed_at_s, now_s);
    check(told.hear(all_of(glideslope::net::write(rc))) ==
              glideslope::net::Told::Heard::runway_conditions,
          "the runways' conditions are heard");
    check(client.conditions_heard(*told.runway_conditions()), "and laid on the weather flown");
}

} // namespace

// **A client joining ten minutes after the rain flies the wet runway the
// server does, and one with no runways of its own the server's icy one.** A
// server flies rain at a station, then from 600 s a dry report: its runways
// stay wet until 2,400 s (world::still_wet_after_rain_s). A client joining at
// 1,200 s is told only the dry report - the rain it never heard - and the
// runways' conditions (`RUNWAY_CONDITIONS`): at the station, at 1,200 s and
// 2,399 s, its runway is the server's code 5, and at 2,401 s both are dry. A
// second server flies ice reported on runway 24, its runways laid
// (frontend::StationRunways): a client that has fetched no runways is told
// them and flies code 1 on 24 as the server does, and 6 off it. Told the
// weather alone, without the conditions, the joining client is dry - what it
// flew before this.
GLIDESLOPE_TEST(a_client_joining_after_the_rain_flies_the_wet_runway_the_server_does) {
    const std::string rain = "METAR UUEE 140600Z 18005KT 6000 -RA BKN015 04/02 Q1012";
    const std::string dry = "METAR UUEE 140700Z 18005KT 9999 FEW030 04/01 Q1013";
    glideslope::world::ReportedWeather server(at_uuee(rain), nullptr, 0.0);
    server.update(at_uuee(dry), 600.0);
    double now_s = 1200.0;
    const auto clock = [&now_s] { return now_s; };
    glideslope::net::Told told;
    glideslope::frontend::HeardAir client(nullptr, {}, clock);
    joins(server, 600.0, now_s, told, client);
    glideslope::frontend::HeardAir weather_alone(nullptr, {}, clock);
    weather_alone.heard(*told.weather(), told.aloft());

    std::size_t compared = 0;
    for (const auto& [t, code] : std::vector<std::pair<double, int>>{
             {1200.0, 5}, {2399.0, 5}, {2401.0, 6}}) {
        now_s = t;
        const int theirs = server.at(55.97, 37.41, 150.0, t).runway_condition;
        const int ours = client.air()->at(55.97, 37.41, 150.0, 0.0).runway_condition;
        check(theirs == code && ours == code,
              "at " + std::to_string(t) + " s the server's runway is " + std::to_string(theirs) +
                  " and the joining client's " + std::to_string(ours) + ", both to be " +
                  std::to_string(code));
        ++compared;
    }
    now_s = 1200.0;
    check(weather_alone.air()->at(55.97, 37.41, 150.0, 0.0).runway_condition == 6,
          "told the weather alone, the joining client's runway is dry");

    // Ice on 24, laid by the server; the client has no runways at all.
    const std::string icy = "METAR UUEE 140800Z 18005KT 9999 FEW030 M02/M04 Q1013 R24/7901//";
    glideslope::world::ReportedWeather icy_server(at_uuee(icy), nullptr, 0.0);
    glideslope::frontend::StationRunways runways([] { return runway_24_and_06(); });
    std::string why;
    while (!runways.lay(icy_server, &why)) {
        check(why.empty(), "the runways are had: " + why);
        std::this_thread::yield();
    }
    glideslope::net::Told told_icy;
    glideslope::frontend::HeardAir icy_client(nullptr, {}, clock);
    joins(icy_server, 0.0, 1200.0, told_icy, icy_client);
    const double along_24 = 55.97 - 0.004; // 0.4 km down 24, south-west of its threshold
    const double on_24_lon = 37.41 - 0.004 * std::tan(60.0 * 3.14159265358979323846 / 180.0) /
                                         std::cos(55.97 * 3.14159265358979323846 / 180.0);
    for (const auto& [lat, lon, code] : std::vector<std::tuple<double, double, int>>{
             {along_24, on_24_lon, 1}, {55.97 + 0.01, 37.41, 6}}) {
        const int theirs = icy_server.at(lat, lon, 150.0, 1200.0).runway_condition;
        const int ours = icy_client.air()->at(lat, lon, 150.0, 0.0).runway_condition;
        check(theirs == code && ours == code,
              "the server's runway is " + std::to_string(theirs) + " and the client's " +
                  std::to_string(ours) + ", both to be " + std::to_string(code));
        ++compared;
    }
    check(compared == 5, "every moment and place compared");
}

// **The runways a reported state lies on are never waited for.** A source
// held until the test lets it go: `lay` returns at once, not laid, as often
// as it is asked, and the air's runway is the weather's alone (dry); let go,
// the runways are laid the next time it is asked and the runway is icy. One
// that fails lays nothing, says why, and is not asked again within the hour.
GLIDESLOPE_TEST(the_runways_a_reported_state_lies_on_are_laid_when_had_and_never_waited_for) {
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    std::atomic<int> loads{0};
    glideslope::frontend::StationRunways runways([released, &loads] {
        ++loads;
        released.wait();
        return runway_24_and_06();
    });
    glideslope::world::ReportedWeather air(
        at_uuee("METAR UUEE 140800Z 18005KT 9999 FEW030 M02/M04 Q1013 R24/7901//"), nullptr,
        0.0);
    const double lat = 55.97 - 0.004;
    const double lon = 37.41 - 0.004 * std::tan(60.0 * 3.14159265358979323846 / 180.0) /
                                   std::cos(55.97 * 3.14159265358979323846 / 180.0);
    std::size_t asked = 0;
    for (; asked < 1000; ++asked) {
        check(!runways.lay(air), "not laid while the source holds them");
    }
    check(air.at(lat, lon, 150.0, 0.0).runway_condition == 6,
          "until they come, the state lies on no runway");
    release.set_value();
    while (!runways.lay(air)) {
        std::this_thread::yield();
    }
    check(loads == 1, "fetched once, for all those asks: " + std::to_string(loads.load()));
    check(air.at(lat, lon, 150.0, 0.0).runway_condition == 1 && air.runways_laid(),
          "laid once they came: ice on 24");
    check(!runways.lay(air), "and not laid again");

    std::atomic<int> tries{0};
    {
        glideslope::frontend::StationRunways failing(
            [&tries]() -> std::vector<glideslope::world::RunwayEnd> {
                ++tries;
                throw std::runtime_error("nothing listens");
            });
        glideslope::world::ReportedWeather other(
            at_uuee("METAR UUEE 140800Z 18005KT 9999 FEW030 M02/M04 Q1013 R24/7901//"), nullptr,
            0.0);
        std::string why;
        while (why.empty()) {
            check(!failing.lay(other, &why), "a source that fails lays nothing");
            std::this_thread::yield();
        }
        check(why.find("nothing listens") != std::string::npos, "and says why: " + why);
        std::string again;
        check(!failing.lay(other, &again) && again.empty(), "once");
    } // let go: whatever it began is waited for here
    check(tries == 1, "and is not asked again within the hour: asked " +
                          std::to_string(tries.load()) + " times");
    check(asked == 1000, "asked a thousand times while held");
}

// **Runways that could not be had are asked for again once the wait is
// over, and each failure is said.** A source that fails, waited out at once
// (`retry_after` of nought, where a server waits an hour): asked as `lay` is
// asked again, it is tried again - three failures, each said - and once it
// answers, the runways are laid. With the hour, it is tried once in the
// same asks (the_runways_a_reported_state_lies_on_are_laid_when_had_and_never_waited_for).
GLIDESLOPE_TEST(runways_that_could_not_be_had_are_asked_for_again_after_the_wait_and_each_failure_said) {
    std::atomic<int> tries{0};
    glideslope::frontend::StationRunways flaky(
        [&tries]() -> std::vector<glideslope::world::RunwayEnd> {
            if (++tries <= 3) {
                throw std::runtime_error("nothing listens, try " + std::to_string(tries.load()));
            }
            return runway_24_and_06();
        },
        std::chrono::steady_clock::duration::zero());
    glideslope::world::ReportedWeather air(
        at_uuee("METAR UUEE 140800Z 18005KT 9999 FEW030 M02/M04 Q1013 R24/7901//"), nullptr,
        0.0);
    std::vector<std::string> said;
    for (;;) {
        std::string why;
        if (flaky.lay(air, &why)) {
            break;
        }
        if (!why.empty()) {
            said.push_back(why);
        }
        std::this_thread::yield();
    }
    check(said.size() == 3, "each of the three failures said: " + std::to_string(said.size()));
    for (std::size_t i = 0; i < said.size(); ++i) {
        check(said[i].find("try " + std::to_string(i + 1)) != std::string::npos,
              "failure " + std::to_string(i + 1) + " said as itself: " + said[i]);
    }
    check(tries == 4 && air.runways_laid(), "tried four times, and laid at the fourth");
}

// **A client flies the server's stations to the last bit, through every
// change of them.** A server flies stations in slots (world::StationSlots),
// told each slot's weather through the wire's bytes as glideslope_server
// tells it (net::Weather's slot and stations' change), heard by net::Told
// and flown by frontend::HeardAir. From 0 s, YSSY in slot 0 and YSWS in slot
// 1; at 600 s a new report of YSWS; at 900 s YSWS let go for YSRI and YWLM
// found, in slots 1 and 2; at 1500 s slot 0 emptied. Each blends over 120 s.
// The air is compared at 4 places between and beyond them, 3 heights and 12
// times - through every blend and after - 144 points, each to the last bit.
GLIDESLOPE_TEST(a_client_flies_the_servers_stations_to_the_last_bit_through_every_change) {
    double now_s = 0.0;
    const auto clock = [&now_s] { return now_s; };
    constexpr double blend_s = 120.0;
    const auto at = [](const std::string& metar, double lat, double lon) {
        glideslope::world::WeatherReport r = a_full_report(metar);
        r.surface.latitude_deg = lat;
        r.surface.longitude_deg = lon;
        r.aloft->latitude_deg = lat;
        r.aloft->longitude_deg = lon;
        r.microbursts.clear();
        return r;
    };
    const auto server = std::make_shared<glideslope::world::StationSlots>(nullptr);
    glideslope::frontend::SessionClocked server_air(server, clock);
    glideslope::net::Told told;
    glideslope::frontend::HeardAir client(nullptr, {}, clock);
    // A slot's weather, from now, as glideslope_server's Fleet sets and says it.
    const auto set = [&](std::size_t slot, std::optional<glideslope::world::WeatherReport> r) {
        server->set(slot, r, now_s, blend_s, now_s);
        const glideslope::frontend::WeatherSaid said = glideslope::frontend::weather_said(
            r ? &*r : nullptr, now_s, blend_s, static_cast<std::uint8_t>(slot),
            server->set_changed_at_s());
        (void)told.hear(all_of(glideslope::net::write(said.weather)));
        if (said.aloft) {
            (void)told.hear(all_of(glideslope::net::write(*said.aloft)));
        }
        check(told.last_station() == slot, "heard for its slot");
        client.heard(*told.weather(), told.aloft());
    };
    set(0, at("YSSY 102100Z 28016G28KT CAVOK 20/13 Q1015", -33.946, 151.173));
    set(1, at("YSWS 102100Z 22006KT 9999 16/11 Q1012", -33.888, 150.715));

    const std::vector<std::array<double, 2>> places{
        {-33.93, 151.06}, {-33.917, 150.944}, {-33.70, 150.80}, {-33.0, 151.7}};
    const std::vector<double> heights{9.0, 300.0, 1200.0};
    const std::vector<double> times{10.0,   600.0,  650.0,  800.0,  900.0,  960.0,
                                    1100.0, 1500.0, 1530.0, 1580.0, 1700.0, 2000.0};
    std::size_t compared = 0;
    std::size_t changes = 0;
    for (const double t : times) {
        now_s = t;
        if (t == 600.0) {
            set(1, at("YSWS 102130Z 24012KT 9999 17/11 Q1013", -33.888, 150.715));
            ++changes;
        }
        if (t == 900.0) {
            set(1, at("YSRI 102100Z 01003KT 9999 18/11 Q1015", -33.601, 150.777));
            set(2, at("YWLM 102100Z 27005KT CAVOK 18/14 Q1016", -32.794, 151.836));
            ++changes;
        }
        if (t == 1500.0) {
            set(0, std::nullopt);
            ++changes;
        }
        for (const auto& p : places) {
            for (const double h : heights) {
                const glideslope::sim::Conditions a = server_air.at(p[0], p[1], h, 1.0);
                const glideslope::sim::Conditions b = client.air()->at(p[0], p[1], h, 77.0);
                check(a.wind_north_mps == b.wind_north_mps && a.wind_east_mps == b.wind_east_mps &&
                          a.wind_down_mps == b.wind_down_mps &&
                          a.temperature_offset_c == b.temperature_offset_c &&
                          a.sea_level_pressure_hpa == b.sea_level_pressure_hpa &&
                          a.turbulence_severity == b.turbulence_severity &&
                          a.wind_at_20ft_mps == b.wind_at_20ft_mps &&
                          a.runway_condition == b.runway_condition,
                      "the air at " + std::to_string(p[0]) + ", " + std::to_string(h) + " m, " +
                          std::to_string(t) + " s is the server's");
                ++compared;
            }
        }
    }
    check(compared == 4 * 3 * 12 && changes == 3,
          "every point compared through every change: " + std::to_string(compared));
    check(told.stations().size() == 3 && client.stations()->slots().size() == 3 &&
              !client.stations()->slots()[0],
          "three slots heard, the first emptied");
    std::printf("  %zu points of the air of three slots, each the server's to the last bit\n",
                compared);
}

// **A client joining while the server's stations change flies the server's
// air to the last bit.** A server flies stations in slots, each set as
// glideslope_server's Fleet::set_slot sets it (world::StationSlots::set, the
// moment of the change its own): from 0 s YSSY in rain in slot 0 and YSWS in
// slot 1; at 600 s a new report of YSWS; at 650 s YSWS let go for YSRI, and
// YWLM and UUEE - its runway 24 iced, laid on its runways - found in slots 2
// and 3; at 700 s, while that change still blends, YWLM let go and UUEE let
// go for YSCB; at 710 s a new report of YSSY, the rain over and its runways
// still wet. Each blends over 120 s. A client there from the start is told
// each change as it comes. One joining at 715 s - inside both changes'
// blends and YSWS's own, so that slot 1 is told three weathers: built so,
// and checked - is told what glideslope_server tells a joining client
// (frontend::weather_said_on_joining, and then each slot's runways), through
// the wire's bytes, net::Told and frontend::HeardAir. The air of both is
// compared with the server's at 5 places (one on UUEE's runway 24, one on
// YSSY's), 3 heights and 10 times from the joining to past every blend's
// end, 150 points each, to the last bit; the ice and the wet runway are
// among them, counted.
GLIDESLOPE_TEST(a_client_joining_while_the_servers_stations_change_flies_the_servers_air) {
    double now_s = 0.0;
    const auto clock = [&now_s] { return now_s; };
    constexpr double blend_s = 120.0;
    const auto at = [](const std::string& metar, double lat, double lon) {
        glideslope::world::WeatherReport r = a_full_report(metar);
        r.surface.latitude_deg = lat;
        r.surface.longitude_deg = lon;
        r.aloft->latitude_deg = lat;
        r.aloft->longitude_deg = lon;
        r.microbursts.clear();
        return r;
    };
    const auto server = std::make_shared<glideslope::world::StationSlots>(nullptr);
    glideslope::frontend::SessionClocked server_air(server, clock);
    glideslope::net::Told there_told;
    glideslope::frontend::HeardAir there(nullptr, {}, clock);
    const auto hear = [](glideslope::net::Told& told, glideslope::frontend::HeardAir& air,
                         const std::vector<std::uint8_t>& body) {
        const glideslope::net::Told::Heard heard = told.hear(all_of(body));
        if (heard == glideslope::net::Told::Heard::weather) {
            check(air.heard(*told.weather(), told.aloft()), "a weather flown");
        } else if (heard == glideslope::net::Told::Heard::runway_conditions) {
            check(air.conditions_heard(*told.stations().at(told.last_station()).runway_conditions),
                  "runways laid on the weather they are for");
        }
    };
    // A slot's weather, from now, as glideslope_server's Fleet::set_slot sets
    // it, and as it tells a client already there: the weather, then its
    // runways - laid on UUEE's, as the server's StationRunways lays them.
    std::size_t slot_count = 0;
    const auto set = [&](std::size_t slot, std::optional<glideslope::world::WeatherReport> r) {
        slot_count = std::max(slot_count, slot + 1);
        server->set(slot, r, now_s, blend_s, now_s);
        const glideslope::frontend::WeatherSaid said = glideslope::frontend::weather_said(
            r ? &*r : nullptr, now_s, blend_s, static_cast<std::uint8_t>(slot),
            server->set_changed_at_s());
        hear(there_told, there, glideslope::net::write(said.weather));
        if (said.aloft) {
            hear(there_told, there, glideslope::net::write(*said.aloft));
        }
        if (const auto& s = server->slots()[slot]) {
            s->lay_runways(runway_24_and_06());
            hear(there_told, there,
                 glideslope::net::write(glideslope::frontend::runway_conditions_said(
                     *s, now_s, now_s, static_cast<std::uint8_t>(slot))));
        }
    };
    const auto uuee = [] {
        glideslope::world::WeatherReport r =
            at_uuee("METAR UUEE 140800Z 18005KT 9999 FEW030 M02/M04 Q1013 R24/7901//");
        r.air_seed = glideslope::world::air_seed_of(r.surface.metar);
        return r;
    };
    set(0, at("YSSY 102100Z 28016G28KT 4000 RA BKN010 20/13 Q1015", -33.946, 151.173));
    set(1, at("YSWS 102100Z 22006KT 9999 16/11 Q1012", -33.888, 150.715));
    now_s = 600.0;
    set(1, at("YSWS 102130Z 24012KT 9999 17/11 Q1013", -33.888, 150.715));
    now_s = 650.0;
    set(1, at("YSRI 102100Z 01003KT 9999 18/11 Q1015", -33.601, 150.777));
    set(2, at("YWLM 102100Z 27005KT CAVOK 18/14 Q1016", -32.794, 151.836));
    set(3, uuee());
    now_s = 700.0;
    set(2, std::nullopt);
    set(3, at("YSCB 102100Z 33020KT CAVOK 12/02 Q1019", -35.307, 149.195));
    now_s = 710.0;
    set(0, at("YSSY 102130Z 18025KT CAVOK 19/13 Q1011", -33.946, 151.173));

    // **Joining mid-change**, as built: inside the blends of both changes,
    // and of YSWS's new report.
    now_s = 715.0;
    check(server->set_changed_at_s() == 700.0 && now_s < 650.0 + blend_s &&
              now_s < 600.0 + blend_s,
          "joined inside the blends of the changes at 650 s and 700 s, and of YSWS's report");
    glideslope::net::Told joiner_told;
    glideslope::frontend::HeardAir joiner(nullptr, {}, clock);
    for (const std::vector<std::uint8_t>& body :
         glideslope::frontend::weather_said_on_joining(*server, slot_count, blend_s, now_s)) {
        hear(joiner_told, joiner, body);
    }
    // Then each slot's runways, as the server tells them after the weather.
    for (std::size_t i = 0; i < server->slots().size(); ++i) {
        if (const auto& s = server->slots()[i]) {
            hear(joiner_told, joiner,
                 glideslope::net::write(glideslope::frontend::runway_conditions_said(
                     *s, s->changed_at_s(), now_s, static_cast<std::uint8_t>(i))));
        }
    }

    const double uuee_lat = 55.97 - 0.004;
    const double uuee_lon = 37.41 - 0.004 * std::tan(60.0 * 3.14159265358979323846 / 180.0) /
                                        std::cos(55.97 * 3.14159265358979323846 / 180.0);
    const std::vector<std::array<double, 2>> places{{-33.946, 151.173},
                                                    {-33.75, 150.80},
                                                    {-33.25, 151.40},
                                                    {-34.60, 150.0},
                                                    {uuee_lat, uuee_lon}};
    const std::vector<double> heights{9.0, 150.0, 1200.0};
    const std::vector<double> times{715.0, 720.0, 730.0, 750.0, 770.0,
                                    790.0, 800.0, 819.0, 850.0, 1000.0};
    const auto same = [](const glideslope::sim::Conditions& a,
                         const glideslope::sim::Conditions& b) {
        return a.wind_north_mps == b.wind_north_mps && a.wind_east_mps == b.wind_east_mps &&
               a.wind_down_mps == b.wind_down_mps &&
               a.temperature_offset_c == b.temperature_offset_c &&
               a.sea_level_pressure_hpa == b.sea_level_pressure_hpa &&
               a.turbulence_severity == b.turbulence_severity &&
               a.wind_at_20ft_mps == b.wind_at_20ft_mps && a.gust_factor_kt == b.gust_factor_kt &&
               a.runway_condition == b.runway_condition;
    };
    std::size_t compared = 0;
    std::size_t joiner_same = 0;
    std::size_t there_same = 0;
    std::size_t iced = 0;
    std::size_t wet = 0;
    for (const double t : times) {
        now_s = t;
        for (const auto& p : places) {
            for (const double h : heights) {
                const glideslope::sim::Conditions a = server_air.at(p[0], p[1], h, 1.0);
                const bool j = same(a, joiner.air()->at(p[0], p[1], h, 77.0));
                const bool e = same(a, there.air()->at(p[0], p[1], h, 77.0));
                joiner_same += j ? 1U : 0U;
                there_same += e ? 1U : 0U;
                iced += a.runway_condition == 1 ? 1U : 0U;
                wet += a.runway_condition == glideslope::sim::wet_runway ? 1U : 0U;
                ++compared;
            }
        }
    }
    std::printf("  %zu points from the joining at 715 s to 1000 s: the joiner's air the "
                "server's at %zu, the client there from the start's at %zu; %zu on ice, %zu "
                "wet\n",
                compared, joiner_same, there_same, iced, wet);
    check(joiner_told.stations().at(1).weathers >= 3,
          "slot 1 told three weathers or more on joining: " +
              std::to_string(joiner_told.stations().at(1).weathers));
    check(compared == 5 * 3 * 10, "every point compared: " + std::to_string(compared));
    check(iced > 0 && wet > 0, "the ice and the wet runway among them");
    check(there_same == compared, "the client there from the start flies the server's air");
    check(joiner_same == compared, "the joiner flies the server's air at every point");
    check(joiner.stations() && joiner.stations()->slots().size() == 4 &&
              !joiner.stations()->slots()[2],
          "the joiner flies four slots, the third emptied");
}

// **The stations flown keep only the changes that still reach the air**,
// however long the session. Stations changed every 30 s for 3,000 s - a
// slot given another station in turn - blending over 120 s: the changes
// kept, counted after every change, are never more than those within a
// blend and blend_kept_s (60 s) of the newest, (120 + 60) / 30 + 1 = 7, and
// reach it; and a client joining is told only those whose blend has not
// ended - the newest and the 120 / 30 = 4 before it - 5 changes' slots.
GLIDESLOPE_TEST(the_stations_flown_keep_only_the_changes_that_still_reach_the_air) {
    constexpr double blend_s = 120.0;
    constexpr double every_s = 30.0;
    glideslope::world::StationSlots slots(nullptr);
    const std::vector<std::pair<std::string, std::array<double, 2>>> stations{
        {"YSSY", {-33.946, 151.173}}, {"YSWS", {-33.888, 150.715}},
        {"YSRI", {-33.601, 150.777}}, {"YWLM", {-32.794, 151.836}}};
    const auto report = [&](std::size_t k) {
        glideslope::world::WeatherReport r = a_full_report(
            stations[k % stations.size()].first + " 102100Z 28016KT CAVOK 20/13 Q1015");
        r.surface.latitude_deg = stations[k % stations.size()].second[0];
        r.surface.longitude_deg = stations[k % stations.size()].second[1];
        r.aloft.reset();
        r.microbursts.clear();
        return r;
    };
    slots.set(0, report(0), 0.0, blend_s, 0.0);
    std::size_t most = 0;
    std::size_t changes = 0;
    double now_s = 0.0;
    for (std::size_t k = 1; now_s < 3000.0; ++k) {
        now_s = static_cast<double>(k) * every_s;
        slots.set(0, report(k), now_s, blend_s, now_s);
        ++changes;
        std::size_t kept = 0;
        for (const glideslope::world::StationSlots::Stations* c = &slots.air(); c != nullptr;
             c = c->from.get()) {
            ++kept;
        }
        most = std::max(most, kept);
    }
    const auto bound = static_cast<std::size_t>((blend_s + glideslope::world::blend_kept_s) /
                                                every_s) +
                       1;
    check(changes == 100, "a hundred changes: " + std::to_string(changes));
    check(most == bound, "the changes kept never more than " + std::to_string(bound) +
                             ", and reaching it: " + std::to_string(most));
    // Joined at the moment of the newest change: one weather a change.
    std::size_t weathers = 0;
    for (const std::vector<std::uint8_t>& body :
         glideslope::frontend::weather_said_on_joining(slots, 1, blend_s, now_s)) {
        weathers += glideslope::net::kind_of(all_of(body)) == glideslope::net::Message::weather
                        ? 1U
                        : 0U;
    }
    const auto told = static_cast<std::size_t>(blend_s / every_s) + 1;
    check(weathers == told, "a joiner told the " + std::to_string(told) +
                                " changes whose blend has not ended: " +
                                std::to_string(weathers));
    std::printf("  %zu changes: at most %zu kept, %zu told to a joiner\n", changes, most,
                weathers);
}

// **The air at a moment is the same whenever it is asked** - flown forward
// past a blend's end and then again from before it, as a client puts its
// prediction right (sim::Prediction). A server flies two stations, then a
// third found at 100 s and a new report of the first at 110 s, over 60 s;
// its air at every second from 90 to 200 s is kept. A client told the same
// flies to 400 s, past both blends' ends, and then again from 90 s: at every
// one of those 111 seconds, at 3 places and 2 heights, its air is the
// server's to the last bit. Before 2026-11-11's review the blend was let go
// when asked past its end, and a step flown again flew the new air alone.
GLIDESLOPE_TEST(a_client_flying_again_from_before_a_blends_end_flies_the_servers_air) {
    double now_s = 0.0;
    const auto clock = [&now_s] { return now_s; };
    constexpr double blend_s = 60.0;
    const auto at = [](const std::string& metar, double lat, double lon) {
        glideslope::world::WeatherReport r = a_full_report(metar);
        r.surface.latitude_deg = lat;
        r.surface.longitude_deg = lon;
        r.microbursts.clear();
        return r;
    };
    const auto server = std::make_shared<glideslope::world::StationSlots>(nullptr);
    glideslope::net::Told told;
    glideslope::frontend::HeardAir client(nullptr, {}, clock);
    const auto set = [&](std::size_t slot, glideslope::world::WeatherReport r) {
        server->set(slot, r, now_s, blend_s, now_s);
        const glideslope::frontend::WeatherSaid said = glideslope::frontend::weather_said(
            &r, now_s, blend_s, static_cast<std::uint8_t>(slot), server->set_changed_at_s());
        (void)told.hear(all_of(glideslope::net::write(said.weather)));
        if (said.aloft) {
            (void)told.hear(all_of(glideslope::net::write(*said.aloft)));
        }
        client.heard(*told.weather(), told.aloft());
    };
    set(0, at("YSSY 102100Z 28016KT CAVOK 20/13 Q1015", -33.946, 151.173));
    set(1, at("YSWS 102100Z 22012KT 9999 16/11 Q1012", -33.888, 150.715));
    const std::vector<std::array<double, 2>> places{
        {-33.93, 151.06}, {-33.917, 150.944}, {-33.75, 150.85}};
    const std::vector<double> heights{300.0, 1200.0};
    std::vector<glideslope::sim::Conditions> server_said;
    for (int t = 90; t <= 200; ++t) {
        now_s = t;
        if (t == 100) {
            set(2, at("YSRI 102100Z 01015KT 9999 18/11 Q1015", -33.601, 150.777));
        }
        if (t == 110) {
            set(0, at("YSSY 102130Z 18025KT CAVOK 19/13 Q1011", -33.946, 151.173));
        }
        for (const auto& p : places) {
            for (const double h : heights) {
                server_said.push_back(server->at(p[0], p[1], h, now_s));
            }
        }
    }
    // The client flies on, past both blends' ends ...
    for (int t = 201; t <= 400; ++t) {
        for (const auto& p : places) {
            (void)client.air()->at(p[0], p[1], 300.0, 0.0);
        }
        now_s = t;
    }
    // ... and then again from 90 s.
    std::size_t compared = 0;
    for (int t = 90; t <= 200; ++t) {
        now_s = t;
        for (const auto& p : places) {
            for (const double h : heights) {
                const glideslope::sim::Conditions a = server_said[compared];
                const glideslope::sim::Conditions b = client.air()->at(p[0], p[1], h, 0.0);
                check(a.wind_north_mps == b.wind_north_mps && a.wind_east_mps == b.wind_east_mps &&
                          a.wind_down_mps == b.wind_down_mps &&
                          a.temperature_offset_c == b.temperature_offset_c &&
                          a.sea_level_pressure_hpa == b.sea_level_pressure_hpa,
                      "flown again at " + std::to_string(t) + " s, the server's air");
                ++compared;
            }
        }
    }
    check(compared == 111 * 3 * 2, "every point compared: " + std::to_string(compared));
    std::printf("  %zu points flown again from before the blends' ends, each the server's\n",
                compared);
}
