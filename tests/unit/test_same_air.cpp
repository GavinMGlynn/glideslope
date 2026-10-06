// The same air and the same ground on the server and every client
// (src/frontend/same_air.hpp, src/net/told.hpp; REQUIREMENTS.md 6.3).

#include "harness.hpp"

#include "frontend/same_air.hpp"
#include "net/messages.hpp"
#include "net/told.hpp"
#include "world/digest.hpp"
#include "world/metar.hpp"
#include "world/weather.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
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
    check(fresh.air() == nullptr && fresh.reported() == nullptr,
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
    check(had.reported() == nullptr && calm.wind_north_mps == 0.0 && calm.wind_east_mps == 0.0 &&
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
