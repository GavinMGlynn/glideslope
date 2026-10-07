#include "harness.hpp"

#include "net/messages.hpp"
#include "net/reliable.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

using glideslope::net::AircraftDefinition;
using glideslope::net::AloftLevel;
using glideslope::net::Controller;
using glideslope::net::ControllerSwap;
using glideslope::net::CopilotRoute;
using glideslope::net::RouteWaypoint;
using glideslope::net::Lobby;
using glideslope::net::Message;
using glideslope::net::Microburst;
using glideslope::net::NearGroundWind;
using glideslope::net::Session;
using glideslope::net::TerrainDataset;
using glideslope::net::Weather;
using glideslope::net::WeatherAloft;
using glideslope::test::check;

namespace {

// One filled-in example of each kind, so that every field is on the wire in
// every test below. Nothing here is a default: a field left at its default
// would pass a round trip that never wrote it.
Lobby a_lobby() {
    Lobby m;
    m.players_allowed = 4;
    m.slots = {{0, Controller::person, "Gavin"},
               {1, Controller::ai, "Mike Bravo"},
               {2, Controller::nobody, ""},
               {3, Controller::person, "a name with spaces"}};
    return m;
}

Session a_session() {
    Session m;
    m.id = 0x0123456789ABCDEFull;
    m.name = "Sydney, evening";
    m.began_unix_ms = 1790000000000ull;
    m.simulation_time_s = 1234.5;
    return m;
}

Weather a_weather() {
    Weather m;
    m.metar = "YSSY 220000Z 09012KT 9999 FEW030 SCT200 22/14 Q1017";
    m.latitude_deg = -33.9461;
    m.longitude_deg = 151.1772;
    m.elevation_m = 6.0;
    m.turbulence_severity = 3;
    m.air_seed = 0xFEEDFACECAFEBEEFull;
    m.microbursts = {{-33.9, 151.2, 1500.0, 12.0, 60.0, 900.0}};
    m.changed_at_s = 600.0;
    m.blend_s = 300.0;
    m.aloft_follows = true;
    return m;
}

WeatherAloft a_weather_aloft() {
    WeatherAloft m;
    m.time = "2026-09-22T00:00";
    m.levels = {{1000.0, 110.0, 3.5, -2.0, 21.5}, {925.0, 780.0, 6.0, -1.0, 16.0}};
    m.near_ground = {{10.0, 3.0, -1.5}, {80.0, 5.0, -2.5}};
    return m;
}

AircraftDefinition an_aircraft() {
    AircraftDefinition m;
    m.aircraft = 6;
    m.id = "f35b";
    m.model = "f35b";
    return m;
}

TerrainDataset a_dataset() {
    TerrainDataset m;
    m.name = "Copernicus GLO-30";
    m.version = "2023_1";
    m.sha256.assign(glideslope::net::sha256_bytes, 0);
    for (std::size_t i = 0; i < m.sha256.size(); ++i) {
        m.sha256[i] = static_cast<std::uint8_t>(i * 7 + 1);
    }
    return m;
}

ControllerSwap a_swap() {
    ControllerSwap m;
    m.aircraft = 5;
    m.to = Controller::ai;
    m.at_simulation_time_s = 987.25;
    return m;
}

// A glide to an orbit over a field, by way of a waypoint: every field there
// is, the orbit's among them.
CopilotRoute a_route() {
    CopilotRoute m;
    m.aircraft = 3;
    m.glide_kts = 68.0;
    RouteWaypoint to;
    to.name = "KURNELL";
    to.latitude_deg = -34.0125;
    to.longitude_deg = 151.2095;
    to.altitude_ft = 1500.0;
    to.airspeed_kts = 90.0;
    RouteWaypoint round = to;
    round.name = "YSSY";
    round.latitude_deg = -33.9461;
    round.longitude_deg = 151.1772;
    round.altitude_ft = 21.0;
    round.airspeed_kts = 68.0;
    round.orbit = RouteWaypoint::Orbit{1500.0, 2, true};
    m.waypoints = {to, round};
    return m;
}

bool same_route(const CopilotRoute& a, const CopilotRoute& b) {
    if (a.aircraft != b.aircraft || a.glide_kts != b.glide_kts ||
        a.waypoints.size() != b.waypoints.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.waypoints.size(); ++i) {
        const RouteWaypoint& x = a.waypoints[i];
        const RouteWaypoint& y = b.waypoints[i];
        if (x.name != y.name || x.latitude_deg != y.latitude_deg ||
            x.longitude_deg != y.longitude_deg || x.altitude_ft != y.altitude_ft ||
            x.airspeed_kts != y.airspeed_kts || x.orbit.has_value() != y.orbit.has_value()) {
            return false;
        }
        if (x.orbit && (x.orbit->radius_m != y.orbit->radius_m ||
                        x.orbit->turns != y.orbit->turns || x.orbit->right != y.orbit->right)) {
            return false;
        }
    }
    return true;
}

// Each kind, as bytes, with a reader that says whether a body is that kind
// and a check that what came back is what went out.
struct Kind {
    Message kind;
    std::string name;
    std::vector<std::uint8_t> bytes;
    // Reads `body` as this kind; true if it read and matched the example.
    std::function<bool(std::span<const std::uint8_t>)> reads_back;
    // Reads `body` as this kind at all, whatever it holds.
    std::function<bool(std::span<const std::uint8_t>)> reads;
};

std::vector<Kind> every_kind() {
    std::vector<Kind> out;

    const Lobby lobby = a_lobby();
    out.push_back({Message::lobby, "lobby", glideslope::net::write(lobby),
                   [lobby](std::span<const std::uint8_t> b) {
                       Lobby got;
                       if (!glideslope::net::read(b, got)) return false;
                       if (got.players_allowed != lobby.players_allowed) return false;
                       if (got.slots.size() != lobby.slots.size()) return false;
                       for (std::size_t i = 0; i < got.slots.size(); ++i) {
                           if (got.slots[i].index != lobby.slots[i].index) return false;
                           if (got.slots[i].controller != lobby.slots[i].controller) return false;
                           if (got.slots[i].name != lobby.slots[i].name) return false;
                       }
                       return true;
                   },
                   [](std::span<const std::uint8_t> b) {
                       Lobby got;
                       return glideslope::net::read(b, got);
                   }});

    const Session session = a_session();
    out.push_back({Message::session, "session", glideslope::net::write(session),
                   [session](std::span<const std::uint8_t> b) {
                       Session got;
                       return glideslope::net::read(b, got) && got.id == session.id &&
                              got.name == session.name &&
                              got.began_unix_ms == session.began_unix_ms &&
                              got.simulation_time_s == session.simulation_time_s;
                   },
                   [](std::span<const std::uint8_t> b) {
                       Session got;
                       return glideslope::net::read(b, got);
                   }});

    const Weather weather = a_weather();
    out.push_back({Message::weather, "weather", glideslope::net::write(weather),
                   [weather](std::span<const std::uint8_t> b) {
                       Weather got;
                       if (!glideslope::net::read(b, got)) return false;
                       if (got.metar != weather.metar) return false;
                       if (got.latitude_deg != weather.latitude_deg) return false;
                       if (got.longitude_deg != weather.longitude_deg) return false;
                       if (got.elevation_m != weather.elevation_m) return false;
                       if (got.turbulence_severity != weather.turbulence_severity) return false;
                       if (got.air_seed != weather.air_seed) return false;
                       if (got.changed_at_s != weather.changed_at_s) return false;
                       if (got.blend_s != weather.blend_s) return false;
                       if (got.aloft_follows != weather.aloft_follows) return false;
                       if (got.microbursts.size() != weather.microbursts.size()) return false;
                       for (std::size_t i = 0; i < got.microbursts.size(); ++i) {
                           if (got.microbursts[i].latitude_deg != weather.microbursts[i].latitude_deg) return false;
                           if (got.microbursts[i].longitude_deg != weather.microbursts[i].longitude_deg) return false;
                           if (got.microbursts[i].radius_m != weather.microbursts[i].radius_m) return false;
                           if (got.microbursts[i].downdraught_mps != weather.microbursts[i].downdraught_mps) return false;
                           if (got.microbursts[i].start_s != weather.microbursts[i].start_s) return false;
                           if (got.microbursts[i].duration_s != weather.microbursts[i].duration_s) return false;
                       }
                       return true;
                   },
                   [](std::span<const std::uint8_t> b) {
                       Weather got;
                       return glideslope::net::read(b, got);
                   }});

    const WeatherAloft aloft = a_weather_aloft();
    out.push_back({Message::weather_aloft, "weather_aloft",
                   glideslope::net::write(aloft),
                   [aloft](std::span<const std::uint8_t> b) {
                       WeatherAloft got;
                       if (!glideslope::net::read(b, got)) return false;
                       if (got.time != aloft.time) return false;
                       if (got.levels.size() != aloft.levels.size()) return false;
                       for (std::size_t i = 0; i < got.levels.size(); ++i) {
                           if (got.levels[i].pressure_hpa != aloft.levels[i].pressure_hpa) return false;
                           if (got.levels[i].height_m != aloft.levels[i].height_m) return false;
                           if (got.levels[i].wind_north_mps != aloft.levels[i].wind_north_mps) return false;
                           if (got.levels[i].wind_east_mps != aloft.levels[i].wind_east_mps) return false;
                           if (got.levels[i].temperature_c != aloft.levels[i].temperature_c) return false;
                       }
                       if (got.near_ground.size() != aloft.near_ground.size()) return false;
                       for (std::size_t i = 0; i < got.near_ground.size(); ++i) {
                           if (got.near_ground[i].height_m != aloft.near_ground[i].height_m) return false;
                           if (got.near_ground[i].wind_north_mps != aloft.near_ground[i].wind_north_mps) return false;
                           if (got.near_ground[i].wind_east_mps != aloft.near_ground[i].wind_east_mps) return false;
                       }
                       return true;
                   },
                   [](std::span<const std::uint8_t> b) {
                       WeatherAloft got;
                       return glideslope::net::read(b, got);
                   }});

    const AircraftDefinition aircraft = an_aircraft();
    out.push_back({Message::aircraft, "aircraft", glideslope::net::write(aircraft),
                   [aircraft](std::span<const std::uint8_t> b) {
                       AircraftDefinition got;
                       return glideslope::net::read(b, got) && got.aircraft == aircraft.aircraft &&
                              got.id == aircraft.id && got.model == aircraft.model;
                   },
                   [](std::span<const std::uint8_t> b) {
                       AircraftDefinition got;
                       return glideslope::net::read(b, got);
                   }});

    const TerrainDataset dataset = a_dataset();
    out.push_back({Message::terrain_dataset, "terrain_dataset",
                   glideslope::net::write(dataset),
                   [dataset](std::span<const std::uint8_t> b) {
                       TerrainDataset got;
                       return glideslope::net::read(b, got) && got.name == dataset.name &&
                              got.version == dataset.version && got.sha256 == dataset.sha256;
                   },
                   [](std::span<const std::uint8_t> b) {
                       TerrainDataset got;
                       return glideslope::net::read(b, got);
                   }});

    const ControllerSwap swap = a_swap();
    out.push_back({Message::controller_swap, "controller_swap",
                   glideslope::net::write(swap),
                   [swap](std::span<const std::uint8_t> b) {
                       ControllerSwap got;
                       return glideslope::net::read(b, got) && got.aircraft == swap.aircraft &&
                              got.to == swap.to &&
                              got.at_simulation_time_s == swap.at_simulation_time_s;
                   },
                   [](std::span<const std::uint8_t> b) {
                       ControllerSwap got;
                       return glideslope::net::read(b, got);
                   }});

    glideslope::net::Watch watch;
    watch.aircraft = 6;
    out.push_back({Message::watch, "watch", glideslope::net::write(watch),
                   [watch](std::span<const std::uint8_t> b) {
                       glideslope::net::Watch got;
                       return glideslope::net::read(b, got) && got.aircraft == watch.aircraft;
                   },
                   [](std::span<const std::uint8_t> b) {
                       glideslope::net::Watch got;
                       return glideslope::net::read(b, got);
                   }});

    const CopilotRoute route = a_route();
    out.push_back({Message::copilot_route, "copilot_route", glideslope::net::write(route),
                   [route](std::span<const std::uint8_t> b) {
                       CopilotRoute got;
                       return glideslope::net::read(b, got) && same_route(got, route);
                   },
                   [](std::span<const std::uint8_t> b) {
                       CopilotRoute got;
                       return glideslope::net::read(b, got);
                   }});

    glideslope::net::TakeOverRefused refused;
    refused.aircraft = 3;
    out.push_back({Message::take_over_refused, "take_over_refused",
                   glideslope::net::write(refused),
                   [refused](std::span<const std::uint8_t> b) {
                       glideslope::net::TakeOverRefused got;
                       return glideslope::net::read(b, got) && got.aircraft == refused.aircraft;
                   },
                   [](std::span<const std::uint8_t> b) {
                       glideslope::net::TakeOverRefused got;
                       return glideslope::net::read(b, got);
                   }});

    glideslope::net::LearntLandingRefused learnt;
    learnt.aircraft = 5;
    learnt.why = "not at the learnt landing's gate: YSSY 16R: 2.9 miles out";
    out.push_back({Message::learnt_landing_refused, "learnt_landing_refused",
                   glideslope::net::write(learnt),
                   [learnt](std::span<const std::uint8_t> b) {
                       glideslope::net::LearntLandingRefused got;
                       return glideslope::net::read(b, got) && got.aircraft == learnt.aircraft &&
                              got.why == learnt.why;
                   },
                   [](std::span<const std::uint8_t> b) {
                       glideslope::net::LearntLandingRefused got;
                       return glideslope::net::read(b, got);
                   }});
    return out;
}

} // namespace

// **Every message writes and reads back exactly what went in.** Each example
// has every field set to something that is not its default, so a field the
// writer forgot cannot pass.
GLIDESLOPE_TEST(every_message_writes_and_reads_back_what_went_into_it) {
    // **Every kind there is**, not every kind remembered: the walks below go
    // through `every_kind`, and a kind added to the code and not to it would
    // be walked by none of them.
    std::size_t known = 0;
    for (int kind = 0; kind < 256; ++kind) {
        if (glideslope::net::known_message(static_cast<std::uint8_t>(kind))) {
            ++known;
        }
    }
    check(every_kind().size() == known,
          std::to_string(every_kind().size()) + " kinds are walked, and the code knows " +
              std::to_string(known));
    std::size_t walked = 0;
    for (const Kind& k : every_kind()) {
        check(!k.bytes.empty(), k.name + " writes something");
        check(k.bytes[0] == static_cast<std::uint8_t>(k.kind),
              k.name + " begins with its own kind byte");
        const auto said = glideslope::net::kind_of(k.bytes);
        check(said.has_value() && *said == k.kind,
              k.name + "'s body says which kind it is");
        check(k.reads_back(k.bytes), k.name + " reads back what went in");
        std::printf("  %-16s %zu bytes\n", k.name.c_str(), k.bytes.size());
        ++walked;
    }
    // **The space this walked, stated**: the six kinds the item names, the
    // forecast, which aircraft a client watches, a copilot's route, and a
    // take-over refused.
    check(walked == 11, "eleven kinds were walked, not " + std::to_string(walked));
}

// **A body of one kind is never read as another.** All eighty-one pairs are
// tried, because a reader that accepted another kind's bytes would hand a
// caller a message it never sent.
GLIDESLOPE_TEST(no_message_reads_as_a_kind_it_is_not) {
    const std::vector<Kind> kinds = every_kind();
    std::size_t pairs = 0;
    std::size_t refused = 0;
    for (const Kind& wrote : kinds) {
        for (const Kind& reads_as : kinds) {
            const bool got = reads_as.reads(wrote.bytes);
            if (wrote.kind == reads_as.kind) {
                check(got, wrote.name + " reads as itself");
            } else {
                check(!got, wrote.name + " must not read as " + reads_as.name);
                ++refused;
            }
            ++pairs;
        }
    }
    check(pairs == 121, "all hundred and twenty-one pairs were tried, not " + std::to_string(pairs));
    check(refused == 110, "a hundred and ten of them are refused, not " + std::to_string(refused));
}

// **Every truncation of every message is refused.** A datagram can arrive
// short; a reader that walked off the end of one would be a hole.
GLIDESLOPE_TEST(every_truncation_of_every_message_is_refused) {
    std::size_t walked = 0;
    for (const Kind& k : every_kind()) {
        for (std::size_t n = 0; n < k.bytes.size(); ++n) {
            const std::span<const std::uint8_t> cut(k.bytes.data(), n);
            check(!k.reads(cut), k.name + " cut to " + std::to_string(n) +
                                     " bytes must be refused");
            ++walked;
        }
    }
    check(walked > 400, "every prefix of every message was tried: " +
                            std::to_string(walked));
    std::printf("  refused %zu truncations\n", walked);
}

// **Every message with one byte longer than it should be is refused**, so
// that a reader cannot be fed a message with something hidden after it.
GLIDESLOPE_TEST(every_message_with_anything_trailing_is_refused) {
    std::size_t walked = 0;
    for (const Kind& k : every_kind()) {
        std::vector<std::uint8_t> longer = k.bytes;
        longer.push_back(0);
        check(!k.reads(longer), k.name + " with a byte after it must be refused");
        ++walked;
    }
    check(walked == 11, "every kind was tried");
}

// **Every single-byte change to every message either reads or is refused,
// and never anything else.** This is the whole space: each byte, each of the
// 255 other values it could hold. Nothing here may crash, and nothing may
// read as a kind it is not.
GLIDESLOPE_TEST(every_single_byte_change_to_every_message_is_read_or_refused) {
    const std::vector<Kind> kinds = every_kind();
    std::size_t walked = 0;
    std::size_t accepted = 0;
    std::size_t refused = 0;
    for (const Kind& k : kinds) {
        for (std::size_t at = 0; at < k.bytes.size(); ++at) {
            for (int v = 0; v < 256; ++v) {
                const auto value = static_cast<std::uint8_t>(v);
                if (value == k.bytes[at]) {
                    continue;
                }
                std::vector<std::uint8_t> changed = k.bytes;
                changed[at] = value;
                if (k.reads(changed)) {
                    ++accepted;
                } else {
                    ++refused;
                }
                ++walked;
            }
        }
    }
    std::size_t space = 0;
    for (const Kind& k : kinds) {
        space += k.bytes.size() * 255;
    }
    check(walked == space, "every single-byte change was walked: " +
                               std::to_string(walked) + " of " + std::to_string(space));
    check(refused > 0, "some of them are refused");
    check(accepted > 0, "and some of them still read");
    std::printf("  %zu single-byte changes: %zu read, %zu refused\n", walked, accepted,
                refused);
}

// **The kinds and the controllers this version knows are exactly these.** A
// ninth kind or a fourth controller would be one `docs/TRANSPORT.md` does not
// describe.
GLIDESLOPE_TEST(the_message_kinds_and_controllers_are_the_ones_the_document_names) {
    std::size_t kinds = 0;
    for (int v = 0; v < 256; ++v) {
        if (glideslope::net::known_message(static_cast<std::uint8_t>(v))) {
            ++kinds;
        }
    }
    check(kinds == 11, "eleven kinds are known, not " + std::to_string(kinds));

    std::size_t controllers = 0;
    for (int v = 0; v < 256; ++v) {
        if (glideslope::net::known_controller(static_cast<std::uint8_t>(v))) {
            ++controllers;
        }
    }
    check(controllers == 3, "three controllers are known, not " +
                                std::to_string(controllers));
    // And a swap knows one more: the learnt landing (since version 8).
    std::size_t swap_controllers = 0;
    for (int v = 0; v < 256; ++v) {
        if (glideslope::net::known_swap_controller(static_cast<std::uint8_t>(v))) {
            ++swap_controllers;
        }
    }
    check(swap_controllers == 4 &&
              glideslope::net::known_swap_controller(
                  static_cast<std::uint8_t>(Controller::learnt_landing)) &&
              !glideslope::net::known_controller(
                  static_cast<std::uint8_t>(Controller::learnt_landing)),
          "a swap knows four controllers, the learnt landing the fourth, not " +
              std::to_string(swap_controllers));

    // A body whose first byte is not a kind is not a message at all.
    std::size_t not_a_kind = 0;
    for (int v = 0; v < 256; ++v) {
        const std::vector<std::uint8_t> body{static_cast<std::uint8_t>(v)};
        if (!glideslope::net::kind_of(body).has_value()) {
            ++not_a_kind;
        }
    }
    check(not_a_kind == 245, "two hundred and forty-five first bytes are no kind, not " +
                                 std::to_string(not_a_kind));
    check(!glideslope::net::kind_of({}).has_value(), "and an empty body is none");
}

// **The six messages the item names go through the reliable layer, and each
// arrives exactly once and in order, under every pattern of loss.** This is
// the item's own verification. The layer was already held to it with opaque
// bodies; this holds it with the six real ones, so that what is proved is
// that these six go through, not that something does.
//
// The channel loses what it is told to: `lose` is a bit per datagram in the
// order they are put on the wire, counting both directions. Twelve bits is
// 4,096 patterns, and datagrams past the twelfth all arrive, so every
// exchange finishes and the test can say what came out.
GLIDESLOPE_TEST(every_reliable_message_arrives_exactly_once_and_in_order_under_loss) {
    const std::vector<Kind> kinds = every_kind();
    check(kinds.size() == 11,
          "the six the item names, the forecast, a watch, a route and two refusals");

    constexpr int mask_width = 12;
    constexpr std::uint32_t patterns = 1u << mask_width;
    std::uint32_t walked = 0;
    std::uint32_t with_loss = 0;
    std::uint64_t worst_datagrams = 0;

    for (std::uint32_t lose = 0; lose < patterns; ++lose) {
        glideslope::net::Reliable sender;
        glideslope::net::Reliable receiver;
        for (const Kind& k : kinds) {
            check(sender.send(k.bytes), "a message is queued");
        }

        std::vector<std::vector<std::uint8_t>> arrived;
        int on_the_wire = 0;
        std::uint64_t put_on_the_wire = 0;
        std::uint64_t lost = 0;
        const auto carry = [&](glideslope::net::Reliable& from,
                               glideslope::net::Reliable& to, double now_s,
                               bool collect) {
            for (const std::vector<std::uint8_t>& datagram : from.to_send(now_s)) {
                const bool drop = on_the_wire < mask_width &&
                                  (lose & (1u << on_the_wire)) != 0;
                ++on_the_wire;
                ++put_on_the_wire;
                if (drop) {
                    ++lost;
                    continue;
                }
                for (std::vector<std::uint8_t>& got :
                     to.received(std::span<const std::uint8_t>(datagram.data(),
                                                               datagram.size()))) {
                    if (collect) {
                        arrived.push_back(std::move(got));
                    }
                }
            }
        };

        bool finished = false;
        double now_s = 0.0;
        for (int round = 0; round < 400; ++round) {
            carry(sender, receiver, now_s, true);
            carry(receiver, sender, now_s, false);
            if (sender.in_flight() == 0 && arrived.size() == kinds.size()) {
                finished = true;
                break;
            }
            now_s += glideslope::net::retry_after_s;
        }

        if (!finished || arrived.size() != kinds.size()) {
            glideslope::test::fail(
                "with loss pattern " + std::to_string(lose) + ", " +
                std::to_string(arrived.size()) + " of eleven messages arrived" +
                (finished ? "" : " and it never finished"));
        }
        // In order, once each, and each one still itself: the bytes are not
        // merely equal, they are read back as the message they were written
        // from, which is what a receiver will actually do with them.
        for (std::size_t i = 0; i < kinds.size(); ++i) {
            if (arrived[i] != kinds[i].bytes || !kinds[i].reads_back(arrived[i])) {
                glideslope::test::fail(
                    "with loss pattern " + std::to_string(lose) + ", message " +
                    std::to_string(i) + " should be " + kinds[i].name +
                    " and did not read back as it");
            }
        }
        if (lost > 0) {
            ++with_loss;
        }
        worst_datagrams = std::max(worst_datagrams, put_on_the_wire);
        ++walked;
    }

    check(walked == patterns, "every pattern of loss was walked: " +
                                  std::to_string(walked) + " of " +
                                  std::to_string(patterns));
    check(patterns == 4096, "there are 4,096 patterns over twelve datagrams");
    // Eleven messages with nothing lost are over in twelve datagrams -
    // eleven out and one acknowledgement back - so every bit of a pattern
    // touches one, and only the empty pattern loses nothing: 4,095 do.
    check(with_loss == 4095, "4,095 of the patterns lost something, not " +
                                 std::to_string(with_loss));
    std::printf("  11 messages through 4,096 loss patterns; worst took %llu datagrams\n",
                static_cast<unsigned long long>(worst_datagrams));
}

// **The document and the code say the same thing about the messages.**
// `docs/TRANSPORT.md` is written so that a third party could build a client
// from it alone, which is worth nothing if it drifts from what this end
// actually sends. Every kind, every controller and every limit is read back
// out of the document and held against the code.
GLIDESLOPE_TEST(the_transport_document_and_the_code_agree_about_the_messages) {
    const std::filesystem::path doc =
        std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "docs" / "TRANSPORT.md";
    std::ifstream in(doc);
    check(in.good(), "docs/TRANSPORT.md is there");
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto says = [&](const std::string& what) {
        return text.find(what) != std::string::npos;
    };

    // Every kind, by value and by name.
    const std::vector<std::pair<Message, std::string>> kinds{
        {Message::lobby, "LOBBY"},
        {Message::session, "SESSION"},
        {Message::weather, "WEATHER"},
        {Message::aircraft, "AIRCRAFT"},
        {Message::terrain_dataset, "TERRAIN_DATASET"},
        {Message::controller_swap, "CONTROLLER_SWAP"},
        {Message::weather_aloft, "WEATHER_ALOFT"},
        {Message::watch, "WATCH"},
        {Message::copilot_route, "COPILOT_ROUTE"},
        {Message::take_over_refused, "TAKE_OVER_REFUSED"},
        {Message::learnt_landing_refused, "LEARNT_LANDING_REFUSED"}};
    std::size_t walked = 0;
    for (const auto& [kind, name] : kinds) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "`%02X`", static_cast<unsigned>(kind));
        check(says(std::string("| ") + buf + " | `" + name + "` |"),
              "the document gives " + name + " as " + buf);
        check(glideslope::net::known_message(static_cast<std::uint8_t>(kind)),
              name + " is a kind the code knows");
        ++walked;
    }
    check(walked == 11, "every kind was walked");

    // Every controller, by value and by name.
    const std::vector<std::pair<Controller, std::string>> controllers{
        {Controller::nobody, "NOBODY"},
        {Controller::person, "PERSON"},
        {Controller::ai, "AI"},
        {Controller::learnt_landing, "LEARNT_LANDING"}};
    std::size_t said = 0;
    for (const auto& [controller, name] : controllers) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "`%02X`", static_cast<unsigned>(controller));
        check(says(std::string("| ") + buf + " | `" + name + "` |"),
              "the document gives " + name + " as " + buf);
        check(glideslope::net::known_swap_controller(static_cast<std::uint8_t>(controller)),
              name + " is a controller the code knows");
        ++said;
    }
    check(said == 4, "every controller was walked");

    // Every limit the code enforces is a number the document states.
    const std::vector<std::pair<std::size_t, std::string>> limits{
        {glideslope::net::most_slots, "at most 4"},
        {glideslope::net::most_levels, "at most 24"},
        {glideslope::net::most_near_ground, "at most 4"},
        {glideslope::net::most_microbursts, "at most 16"},
        {glideslope::net::most_metar_bytes, "at most 256 bytes"},
        {glideslope::net::most_name_bytes, "at most 64 bytes"},
        {glideslope::net::most_time_bytes, "at most 32 bytes"},
        {glideslope::net::sha256_bytes, "exactly 32 bytes"},
        {glideslope::net::most_route_waypoints, "at most 12"},
        {glideslope::net::most_waypoint_name_bytes, "at most 32 bytes"},
        {glideslope::net::most_reason_bytes, "cuts a longer one at 160 bytes"}};
    std::size_t held = 0;
    for (const auto& [value, phrase] : limits) {
        check(phrase.find(std::to_string(value)) != std::string::npos,
              "the phrase names the limit the code has: " + phrase);
        check(says(phrase), "the document says \"" + phrase + "\"");
        ++held;
    }
    check(held == 11, "every limit was walked");
    std::printf("  the document holds 11 kinds, 3 controllers and 11 limits\n");
}

// **Every message, filled to its limits, fits in one datagram.** Nothing
// here fragments: `UdpSocket::send` refuses a datagram over 1232 bytes, and
// the reliable layer would retransmit a message that cannot be sent for
// ever, so a limit that allows a body too large is a message that can never
// arrive.
//
// **This is written because it happened.** The weather was one message
// carrying the METAR and the forecast above it, with limits of a 1024-byte
// report, 64 pressure levels, 8 near-ground winds and 32 microbursts. At
// those limits its body was 5,419 bytes against the 1,218 left after the
// envelope and the reliable header - 4,201 over - and even at the figures
// this project really fetches, nineteen levels and four winds, a report with
// one microburst came to 1,235. The weather is now two messages, and these
// are the limits that make both fit.
GLIDESLOPE_TEST(every_message_filled_to_its_limits_fits_in_one_datagram) {
    const std::string longest_name(glideslope::net::most_name_bytes, 'n');
    std::size_t walked = 0;
    std::size_t largest = 0;
    std::string worst;

    const auto hold = [&](const std::string& name,
                          const std::vector<std::uint8_t>& bytes) {
        std::printf("  %-16s %5zu bytes of %zu\n", name.c_str(), bytes.size(),
                    glideslope::net::most_message_bytes);
        check(bytes.size() <= glideslope::net::most_message_bytes,
              name + " at its limits is " + std::to_string(bytes.size()) +
                  " bytes, which is more than the " +
                  std::to_string(glideslope::net::most_message_bytes) +
                  " a datagram leaves for a body");
        if (bytes.size() > largest) {
            largest = bytes.size();
            worst = name;
        }
        ++walked;
    };

    Lobby lobby;
    lobby.players_allowed = 4;
    for (std::size_t i = 0; i < glideslope::net::most_slots; ++i) {
        lobby.slots.push_back({static_cast<std::uint8_t>(i), Controller::person,
                               longest_name});
    }
    hold("lobby", glideslope::net::write(lobby));

    Session session;
    session.name = longest_name;
    hold("session", glideslope::net::write(session));

    Weather weather;
    weather.metar = std::string(glideslope::net::most_metar_bytes, 'M');
    weather.turbulence_severity = 7;
    weather.microbursts.assign(glideslope::net::most_microbursts, Microburst{});
    weather.aloft_follows = true;
    hold("weather", glideslope::net::write(weather));

    WeatherAloft aloft;
    aloft.time = std::string(glideslope::net::most_time_bytes, 'T');
    aloft.levels.assign(glideslope::net::most_levels, AloftLevel{});
    aloft.near_ground.assign(glideslope::net::most_near_ground, NearGroundWind{});
    hold("weather_aloft", glideslope::net::write(aloft));

    AircraftDefinition aircraft;
    aircraft.id = longest_name;
    aircraft.model = longest_name;
    hold("aircraft", glideslope::net::write(aircraft));

    TerrainDataset dataset;
    dataset.name = longest_name;
    dataset.version = longest_name;
    dataset.sha256.assign(glideslope::net::sha256_bytes, 0xAB);
    hold("terrain_dataset", glideslope::net::write(dataset));

    ControllerSwap swap;
    swap.to = Controller::ai;
    hold("controller_swap", glideslope::net::write(swap));

    CopilotRoute route;
    route.glide_kts = 68.0;
    RouteWaypoint longest;
    longest.name = std::string(glideslope::net::most_waypoint_name_bytes, 'W');
    longest.orbit = RouteWaypoint::Orbit{};
    route.waypoints.assign(glideslope::net::most_route_waypoints, longest);
    hold("copilot_route", glideslope::net::write(route));

    glideslope::net::LearntLandingRefused learnt;
    learnt.why = std::string(glideslope::net::most_reason_bytes, 'R');
    hold("learnt_landing_refused", glideslope::net::write(learnt));

    // **The space this walked, stated**: every kind there is but `WATCH` and
    // `TAKE_OVER_REFUSED`, a byte each with nothing to fill, and each one
    // filled rather than sampled.
    check(walked == 9, "every kind was filled to its limits, not " +
                           std::to_string(walked));
    // And the forecast this project really fetches fits, which is the case
    // that matters: nineteen pressure levels and four near-ground winds.
    WeatherAloft real;
    real.time = "2026-09-22T00:00";
    real.levels.assign(19, AloftLevel{});
    real.near_ground.assign(4, NearGroundWind{});
    const std::size_t really = glideslope::net::write(real).size();
    check(really <= glideslope::net::most_message_bytes,
          "the forecast this project fetches is " + std::to_string(really) +
              " bytes and must fit");
    std::printf("  the forecast actually fetched: %zu bytes; largest is %s at %zu\n",
                really, worst.c_str(), largest);
}

// **Every message the server accepts is named in `docs/THREATS.md`**, which
// is that document's own verification. A message kind that exists but is not
// named there is one whose defence nobody has had to think about.
GLIDESLOPE_TEST(every_message_the_server_accepts_is_named_in_the_threats_document) {
    const std::filesystem::path doc =
        std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "docs" / "THREATS.md";
    std::ifstream in(doc);
    check(in.good(), "docs/THREATS.md is there");
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto says = [&](const std::string& what) {
        return text.find(what) != std::string::npos;
    };

    // The four datagram types, which are what actually arrives on the wire.
    std::size_t walked = 0;
    for (const char* name : {"HANDSHAKE_INITIATION", "HANDSHAKE_RESPONSE", "SEALED",
                             "REFUSAL"}) {
        check(says(std::string("`") + name + "`"),
              std::string("THREATS.md names the datagram type ") + name);
        ++walked;
    }
    check(walked == 4, "all four datagram types were looked for");

    // And every reliable message kind the code knows, by name.
    const std::vector<std::pair<Message, std::string>> kinds{
        {Message::lobby, "LOBBY"},
        {Message::session, "SESSION"},
        {Message::weather, "WEATHER"},
        {Message::aircraft, "AIRCRAFT"},
        {Message::terrain_dataset, "TERRAIN_DATASET"},
        {Message::controller_swap, "CONTROLLER_SWAP"},
        {Message::weather_aloft, "WEATHER_ALOFT"},
        {Message::watch, "WATCH"},
        {Message::copilot_route, "COPILOT_ROUTE"},
        {Message::take_over_refused, "TAKE_OVER_REFUSED"},
        {Message::learnt_landing_refused, "LEARNT_LANDING_REFUSED"}};
    std::size_t named = 0;
    for (const auto& [kind, name] : kinds) {
        check(glideslope::net::known_message(static_cast<std::uint8_t>(kind)),
              name + " is a kind the code knows");
        check(says("`" + name + "`"), "THREATS.md names the message " + name);
        ++named;
    }
    // Every kind the code knows is in that list, so a new one cannot be added
    // without being named here and there.
    std::size_t known = 0;
    for (int v = 0; v < 256; ++v) {
        if (glideslope::net::known_message(static_cast<std::uint8_t>(v))) {
            ++known;
        }
    }
    check(named == known, "every kind the code knows was looked for: " +
                              std::to_string(named) + " of " + std::to_string(known));
    check(named == 11, "eleven kinds, not " + std::to_string(named));

    // **It says what it does not defend.** A threats document that only
    // listed defences would be the more dangerous for it.
    check(says("not built"), "it says which defences are not built");
    check(says("COMPLETION_PLAN.md"), "and points at the items that would build them");
    std::printf("  THREATS.md names 4 datagram types and %zu message kinds\n", named);
}

namespace {

std::uint64_t bits_of(double v) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

void put_bits(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint64_t bits) {
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[at + i] = static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFFu);
    }
}

// Every place `v`'s eight bytes appear in `bytes`. The test below insists on
// exactly one, which is what lets it find each field without writing the
// layout out a second time and getting it wrong.
std::vector<std::size_t> offsets_of(const std::vector<std::uint8_t>& bytes, double v) {
    std::vector<std::uint8_t> pattern(8, 0);
    put_bits(pattern, 0, bits_of(v));
    std::vector<std::size_t> out;
    for (std::size_t at = 0; at + 8 <= bytes.size(); ++at) {
        if (std::equal(pattern.begin(), pattern.end(),
                       bytes.begin() + static_cast<std::ptrdiff_t>(at))) {
            out.push_back(at);
        }
    }
    return out;
}

// **A value for one floating-point field**, distinct from every other and
// with a mantissa full of bits, so that its eight bytes cannot be mistaken
// for a run of eight anywhere else in the message - not in a name, not in a
// seed, and not in the halves of two fields beside each other.
double nth_number(std::size_t k) {
    const std::uint64_t bits = 0x3FF0000000000000ull + 0x0123456789ABCDull * (k + 1);
    double v = 0.0;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

// One kind, with every floating-point field it has holding a value of its
// own, so that each can be found in the encoded bytes and overwritten.
struct WithNumbers {
    std::string name;
    std::vector<std::uint8_t> bytes;
    std::vector<double> fields; // in the order they go on the wire
    std::function<bool(std::span<const std::uint8_t>)> reads;
};

// **The fields with a range of their own**, by the value each holds below,
// and the range: a `WEATHER`'s latitude and longitude, when it changed (not
// below nought, with no top) and its blend (0 to a day). A number outside
// is refused; inside, it reads.
struct Ranged {
    double value;
    double lowest;
    double highest;
};
std::vector<Ranged> ranged_fields;

std::vector<WithNumbers> every_kind_that_carries_a_number() {
    std::vector<WithNumbers> out;
    std::size_t k = 0;
    std::vector<double> fields;
    const auto next = [&k, &fields]() {
        const double v = nth_number(k++);
        fields.push_back(v);
        return v;
    };

    Session session = a_session();
    session.simulation_time_s = next();
    out.push_back({"session", glideslope::net::write(session), fields,
                   [](std::span<const std::uint8_t> b) {
                       Session got;
                       return glideslope::net::read(b, got);
                   }});
    fields.clear();

    // One microburst, so that each of its six fields appears once.
    Weather weather = a_weather();
    weather.latitude_deg = next();
    weather.longitude_deg = next();
    ranged_fields = {{weather.latitude_deg, -90.0, 90.0}, {weather.longitude_deg, -180.0, 180.0}};
    weather.elevation_m = next();
    Microburst burst;
    burst.latitude_deg = next();
    burst.longitude_deg = next();
    burst.radius_m = next();
    burst.downdraught_mps = next();
    burst.start_s = next();
    burst.duration_s = next();
    weather.microbursts = {burst};
    weather.changed_at_s = next();
    weather.blend_s = next();
    ranged_fields.push_back({weather.changed_at_s, 0.0, std::numeric_limits<double>::max()});
    ranged_fields.push_back({weather.blend_s, 0.0, glideslope::net::most_blend_s});
    out.push_back({"weather", glideslope::net::write(weather), fields,
                   [](std::span<const std::uint8_t> b) {
                       Weather got;
                       return glideslope::net::read(b, got);
                   }});
    fields.clear();

    // One pressure level and one near-ground wind, for the same reason.
    WeatherAloft aloft = a_weather_aloft();
    AloftLevel level;
    level.pressure_hpa = next();
    level.height_m = next();
    level.wind_north_mps = next();
    level.wind_east_mps = next();
    level.temperature_c = next();
    aloft.levels = {level};
    NearGroundWind wind;
    wind.height_m = next();
    wind.wind_north_mps = next();
    wind.wind_east_mps = next();
    aloft.near_ground = {wind};
    out.push_back({"weather_aloft", glideslope::net::write(aloft), fields,
                   [](std::span<const std::uint8_t> b) {
                       WeatherAloft got;
                       return glideslope::net::read(b, got);
                   }});
    fields.clear();

    ControllerSwap swap = a_swap();
    swap.at_simulation_time_s = next();
    out.push_back({"controller_swap", glideslope::net::write(swap), fields,
                   [](std::span<const std::uint8_t> b) {
                       ControllerSwap got;
                       return glideslope::net::read(b, got);
                   }});
    fields.clear();

    // A glide, and one waypoint that is an orbit, so that each field appears.
    CopilotRoute route = a_route();
    route.glide_kts = next();
    RouteWaypoint w = route.waypoints[1];
    w.latitude_deg = next();
    w.longitude_deg = next();
    w.altitude_ft = next();
    w.airspeed_kts = next();
    w.orbit->radius_m = next();
    route.waypoints = {w};
    out.push_back({"copilot_route", glideslope::net::write(route), fields,
                   [](std::span<const std::uint8_t> b) {
                       CopilotRoute got;
                       return glideslope::net::read(b, got);
                   }});
    return out;
}

} // namespace

// **Every floating-point field of every message refuses a NaN and an
// infinity.** Eight bytes on the wire can say "not a number" as easily as
// they can say a latitude, and nothing above this layer would notice: a NaN
// position spreads through the floating origin and the terrain query, and an
// infinite duration never ends. A number that is not one is refused exactly
// as a bad count or a trailing byte is.
//
// **The space, stated.** Five kinds of message carry twenty-seven
// floating-point fields between them:
//
//   - `SESSION`, one - the simulation's clock;
//   - `WEATHER`, five of its own and six per microburst;
//   - `WEATHER_ALOFT`, five per pressure level and three per near-ground
//     wind;
//   - `CONTROLLER_SWAP`, one;
//   - `COPILOT_ROUTE`, the glide's airspeed, four per waypoint and one more
//     per orbit.
//
// **The four kinds left out are named**: `LOBBY`, `AIRCRAFT`,
// `TERRAIN_DATASET` and `WATCH` carry no floating-point field at all - slot
// indices, names, a hash and a number - so there is nothing in them for this
// to walk.
//
// Each of the twenty-seven is overwritten with six bit patterns that are not
// a number and must be refused, and five that are numbers however extreme and
// must still read - except in the four fields with a range of their own (a
// `WEATHER`'s place, when it changed and its blend), where the largest and
// most negative doubles are outside (the most negative alone, for when it
// changed, which has no top) and must be refused: 162 refusals of NaN and
// infinity, 128 readings and 7 refusals out of range, each counted.
GLIDESLOPE_TEST(every_floating_point_field_of_every_message_refuses_a_nan_and_an_infinity) {
    // The bit patterns that are not a number. Both infinities, and NaNs
    // quiet and signalling, signed and with a payload, because a reader that
    // checked only for the one NaN a compiler happens to produce would let
    // the rest through.
    const std::vector<std::pair<std::uint64_t, std::string>> not_a_number{
        {0x7FF0000000000000ull, "+infinity"},
        {0xFFF0000000000000ull, "-infinity"},
        {0x7FF8000000000000ull, "a quiet NaN"},
        {0xFFF8000000000000ull, "a quiet NaN with its sign bit set"},
        {0x7FF0000000000001ull, "a signalling NaN"},
        {0x7FF8DEADBEEFCAFEull, "a NaN carrying a payload"}};
    // And the numbers that are numbers, however extreme. These must still
    // read, so that the refusal is of NaN and infinity and not of a range
    // somebody guessed at.
    const std::vector<std::pair<std::uint64_t, std::string>> a_number{
        {0x0000000000000000ull, "zero"},
        {0x8000000000000000ull, "negative zero"},
        {0x7FEFFFFFFFFFFFFFull, "the largest finite double"},
        {0xFFEFFFFFFFFFFFFFull, "the most negative finite double"},
        {0x0000000000000001ull, "the smallest subnormal"}};

    const std::vector<WithNumbers> kinds = every_kind_that_carries_a_number();
    check(kinds.size() == 5,
          "five of the ten kinds carry a floating-point field, not " +
              std::to_string(kinds.size()));

    std::size_t walked = 0;
    std::size_t refused = 0;
    std::size_t accepted = 0;
    std::size_t out_of_range = 0;
    check(ranged_fields.size() == 4, "four fields have ranges of their own");
    for (const WithNumbers& k : kinds) {
        check(k.reads(k.bytes), k.name + " reads as it stands");
        for (const double field : k.fields) {
            const std::vector<std::size_t> at = offsets_of(k.bytes, field);
            check(at.size() == 1,
                  k.name + "'s field " + std::to_string(walked) + " is at exactly one "
                  "place in its bytes, not " + std::to_string(at.size()));
            for (const auto& [bits, what] : not_a_number) {
                std::vector<std::uint8_t> changed = k.bytes;
                put_bits(changed, at[0], bits);
                check(!k.reads(changed),
                      k.name + " with " + what + " at byte " + std::to_string(at[0]) +
                          " must be refused");
                ++refused;
            }
            const auto range = std::find_if(ranged_fields.begin(), ranged_fields.end(),
                                            [&](const Ranged& r) { return r.value == field; });
            const bool ranged = k.name == "weather" && range != ranged_fields.end();
            for (const auto& [bits, what] : a_number) {
                std::vector<std::uint8_t> changed = k.bytes;
                put_bits(changed, at[0], bits);
                double v = 0.0;
                std::memcpy(&v, &bits, sizeof(v));
                const bool outside = ranged && (v < range->lowest || v > range->highest);
                if (outside) {
                    check(!k.reads(changed), k.name + " with " + what + " at byte " +
                                                 std::to_string(at[0]) +
                                                 " is outside its range and must be refused");
                    ++out_of_range;
                    continue;
                }
                check(k.reads(changed),
                      k.name + " with " + what + " at byte " + std::to_string(at[0]) +
                          " is a number and must still read");
                ++accepted;
            }
            ++walked;
        }
    }
    check(walked == 27, "twenty-seven floating-point fields were walked, not " +
                            std::to_string(walked));
    check(refused == 27 * 6, "162 numbers that are not numbers were refused, not " +
                                 std::to_string(refused));
    // Outside: both extremes for the place and the blend, the most negative
    // alone for when it changed.
    check(accepted == 27 * 5 - 7, "128 extreme numbers still read, not " +
                                      std::to_string(accepted));
    check(out_of_range == 7, "7 outside a ranged field's range were refused, not " +
                                 std::to_string(out_of_range));
    std::printf("  27 floating-point fields: %zu refused, %zu still read, %zu out of range\n",
                refused, accepted, out_of_range);
}

// **Every refusal `docs/TRANSPORT.md` names for a `WEATHER`**, each built
// on purpose: still air - an empty METAR - carrying a place (each of its
// three numbers), a turbulence severity, an air seed, a microburst or a
// forecast to follow; a forecast flag that is neither `00` nor `01`; a
// station past either pole or past 180 degrees either way; a change before
// nought; and a blend below nought or over a day. Fifteen cases, each
// refused; and still air bare, a full weather and one at every edge, read.
GLIDESLOPE_TEST(every_refusal_the_document_names_for_a_weather_is_refused) {
    const auto reads = [](const std::vector<std::uint8_t>& body) {
        Weather got;
        return glideslope::net::read(std::span<const std::uint8_t>(body.data(), body.size()), got);
    };
    Weather still;
    still.changed_at_s = 12.0;
    still.blend_s = 300.0;
    check(reads(glideslope::net::write(still)), "still air bare reads");
    check(reads(glideslope::net::write(a_weather())), "a full weather reads");
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> refused;
    const auto with = [&](const std::string& what, const auto& change) {
        Weather w = still;
        change(w);
        refused.emplace_back(what, glideslope::net::write(w));
    };
    with("still air with a latitude", [](Weather& w) { w.latitude_deg = 1.0; });
    with("still air with a longitude", [](Weather& w) { w.longitude_deg = 1.0; });
    with("still air with an elevation", [](Weather& w) { w.elevation_m = 1.0; });
    with("still air with turbulence", [](Weather& w) { w.turbulence_severity = 0; });
    with("still air with a seed", [](Weather& w) { w.air_seed = 1; });
    with("still air with a microburst", [](Weather& w) { w.microbursts = {Microburst{}}; });
    with("still air with a forecast to follow", [](Weather& w) { w.aloft_follows = true; });
    std::vector<std::uint8_t> flag = glideslope::net::write(a_weather());
    flag.back() = 2;
    refused.emplace_back("a forecast flag of 02", flag);
    const auto full = [&](const std::string& what, const auto& change) {
        Weather w = a_weather();
        change(w);
        refused.emplace_back(what, glideslope::net::write(w));
    };
    full("a latitude past the north pole", [](Weather& w) { w.latitude_deg = 90.5; });
    full("a latitude past the south pole", [](Weather& w) { w.latitude_deg = -90.5; });
    full("a longitude past 180 east", [](Weather& w) { w.longitude_deg = 180.5; });
    full("a longitude past 180 west", [](Weather& w) { w.longitude_deg = -180.5; });
    full("a change before the session began", [](Weather& w) { w.changed_at_s = -1.0; });
    full("a blend below nought", [](Weather& w) { w.blend_s = -1.0; });
    full("a blend over a day", [](Weather& w) { w.blend_s = 86400.5; });
    {
        Weather edge = a_weather();
        edge.latitude_deg = -90.0;
        edge.longitude_deg = 180.0;
        edge.blend_s = 86400.0;
        edge.changed_at_s = 0.0;
        check(reads(glideslope::net::write(edge)), "a weather at the very edges reads");
    }
    std::size_t walked = 0;
    for (const auto& [what, body] : refused) {
        check(!reads(body), what + " must be refused");
        ++walked;
    }
    check(walked == 15, "every refusal named was built: " + std::to_string(walked));
}

// **Every refusal `docs/TRANSPORT.md` names for a `COPILOT_ROUTE`**, each
// built on purpose rather than hoped for from the byte walks above: no
// waypoints, thirteen, a name of 33 bytes, an empty name, a name holding
// anything but letters, digits and underscores - a newline, which would
// smuggle in plan lines, a `#`, which would comment one out, an escape, which
// would reach the operator's terminal, and a space - a flag that is neither
// `00` nor `01` in each of its three places, and a glide airspeed that is not
// nought with no glide. And the one they are all changed from reads.
GLIDESLOPE_TEST(every_refusal_the_document_names_for_a_copilot_route_is_refused) {
    const auto reads = [](const std::vector<std::uint8_t>& body) {
        CopilotRoute got;
        return glideslope::net::read(std::span<const std::uint8_t>(body.data(), body.size()), got);
    };
    RouteWaypoint plain;
    plain.name = "NORTH";
    plain.latitude_deg = -33.8;
    plain.longitude_deg = 151.3;
    plain.altitude_ft = 3000.0;
    plain.airspeed_kts = 100.0;
    RouteWaypoint round = plain;
    round.orbit = RouteWaypoint::Orbit{1500.0, 1, true};
    CopilotRoute good;
    good.aircraft = 1;
    good.waypoints = {plain, round};
    check(reads(glideslope::net::write(good)), "the route every case is changed from reads");

    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> refused;
    const auto with = [&](const std::string& what, const auto& change) {
        CopilotRoute r = good;
        change(r);
        refused.emplace_back(what, glideslope::net::write(r));
    };
    with("no waypoints", [](CopilotRoute& r) { r.waypoints.clear(); });
    with("13 waypoints", [&](CopilotRoute& r) {
        r.waypoints.assign(glideslope::net::most_route_waypoints + 1, plain);
    });
    with("a 33-byte name", [](CopilotRoute& r) {
        r.waypoints[0].name = std::string(glideslope::net::most_waypoint_name_bytes + 1, 'N');
    });
    with("an empty name", [](CopilotRoute& r) { r.waypoints[0].name.clear(); });
    with("a name with a newline", [](CopilotRoute& r) {
        r.waypoints[0].name = "A\nwaypoint B -34 151 3000 100";
    });
    with("a name with a hash", [](CopilotRoute& r) { r.waypoints[0].name = "A#B"; });
    with("a name with an escape", [](CopilotRoute& r) { r.waypoints[0].name = "A\x1b[2J"; });
    with("a name with a space", [](CopilotRoute& r) { r.waypoints[0].name = "A B"; });
    // The flags, by their place: the glide's is byte 2; the first waypoint's
    // orbit flag follows its name and four f64s; the last byte is the
    // second's direction.
    const std::vector<std::uint8_t> bytes = glideslope::net::write(good);
    const std::size_t glide_flag = 2;
    const std::size_t first_orbit_flag = 12 + 2 + plain.name.size() + 4 * 8;
    for (const auto& [what, at] :
         std::vector<std::pair<std::string, std::size_t>>{{"a glide flag of 2", glide_flag},
                                                          {"an orbit flag of 2", first_orbit_flag},
                                                          {"a direction of 2", bytes.size() - 1}}) {
        std::vector<std::uint8_t> b = bytes;
        check(b[at] <= 1, what + ": the byte changed is a flag");
        b[at] = 2;
        refused.emplace_back(what, b);
    }
    {
        std::vector<std::uint8_t> b = bytes;
        check(b[glide_flag] == 0, "the good route glides not");
        b[glide_flag + 1 + 6] = 0xF0; // the airspeed's exponent: 1.0
        b[glide_flag + 1 + 7] = 0x3F;
        refused.emplace_back("a glide airspeed with no glide", b);
    }
    std::size_t walked = 0;
    for (const auto& [what, body] : refused) {
        check(!reads(body), "a route with " + what + " is refused");
        ++walked;
    }
    check(walked == 12 && refused.size() == 12, "all 12 refusals were tried, not " +
                                                    std::to_string(walked));
}

// **The aeroplane asked for, in the initiation's payload, reads back as
// itself, and every malformed one as none**: empty, a length of 0 or past 32,
// a length that is not the bytes', a byte outside the id's, a path. Every
// rule of `read_asked_aircraft` is here.
GLIDESLOPE_TEST(the_aeroplane_asked_for_reads_back_and_a_malformed_ask_reads_as_none) {
    using glideslope::net::read_asked_aircraft;
    using glideslope::net::write_asked_aircraft;
    const auto read = [](const std::vector<std::uint8_t>& v) {
        return read_asked_aircraft(std::span<const std::uint8_t>(v.data(), v.size()));
    };
    for (const std::string& id : std::vector<std::string>{"pa28", "737-300", "mosquito-fb6", "a_b",
                                 std::string(32, 'z')}) {
        check(read(write_asked_aircraft(id)) == id, "asked for " + id + ", read as it");
    }
    for (const std::string& id :
         std::vector<std::string>{"", "PA28", "../c172p", "pa 28", std::string(33, 'z')}) {
        check(write_asked_aircraft(id).empty(), "'" + id + "' is not written");
    }
    check(!read({}), "an empty payload asks for nothing");
    check(!read({0}), "a length of 0 is none");
    check(!read({3, 'p', 'a'}), "fewer bytes than the length is none");
    check(!read({2, 'p', 'a', '2'}), "a byte trailing is none");
    check(!read({4, 'p', 'a', '/', '8'}), "a byte outside an id's is none");
    std::vector<std::uint8_t> long_one{33};
    long_one.insert(long_one.end(), 33, 'z');
    check(!read(long_one), "past 32 bytes is none");
}

// **What the document says a reader of `LEARNT_LANDING_REFUSED` refuses**,
// each built: a reason past 160 bytes, and one with a byte outside printable
// ASCII - an escape, a newline, a DEL, a byte past 7E - since it is printed
// on the player's terminal. The longest allowed, an empty one and every
// printable byte read.
GLIDESLOPE_TEST(every_refusal_the_document_names_for_a_learnt_landing_refusal_is_refused) {
    const auto reads = [](const std::string& why) {
        glideslope::net::LearntLandingRefused m;
        m.aircraft = 1;
        m.why = why;
        // Written by hand past the writer, which cuts at the limit.
        std::vector<std::uint8_t> body{
            static_cast<std::uint8_t>(Message::learnt_landing_refused), 1,
            static_cast<std::uint8_t>(why.size() & 0xFF),
            static_cast<std::uint8_t>(why.size() >> 8)};
        body.insert(body.end(), why.begin(), why.end());
        glideslope::net::LearntLandingRefused got;
        return glideslope::net::read(std::span<const std::uint8_t>(body.data(), body.size()),
                                     got) &&
               got.why == why;
    };
    std::size_t cases = 0;
    const auto refused = [&](const std::string& why, const std::string& what) {
        check(!reads(why), what + " is refused");
        ++cases;
    };
    const auto read_back = [&](const std::string& why, const std::string& what) {
        check(reads(why), what + " reads");
        ++cases;
    };
    read_back(std::string(glideslope::net::most_reason_bytes, 'x'), "the longest reason");
    read_back("", "an empty reason");
    std::string printable;
    for (int c = 0x20; c <= 0x7E; ++c) {
        printable.push_back(static_cast<char>(c));
    }
    read_back(printable.substr(0, 95), "every printable byte");
    refused(std::string(glideslope::net::most_reason_bytes + 1, 'x'), "a reason of 161 bytes");
    std::size_t unprintable = 0;
    for (int c = 0; c < 256; ++c) {
        if (c >= 0x20 && c <= 0x7E) {
            continue;
        }
        refused(std::string("gate") + static_cast<char>(c), "byte " + std::to_string(c));
        ++unprintable;
    }
    check(unprintable == 161, "every unprintable byte was tried: " + std::to_string(unprintable));
    check(cases == 4 + 161, "every case was built: " + std::to_string(cases));
    // And the writer cuts rather than sending what the reader refuses.
    glideslope::net::LearntLandingRefused long_one;
    long_one.why = std::string(400, 'y');
    glideslope::net::LearntLandingRefused got;
    const std::vector<std::uint8_t> written = glideslope::net::write(long_one);
    check(glideslope::net::read(std::span<const std::uint8_t>(written.data(), written.size()),
                                got) &&
              got.why.size() == glideslope::net::most_reason_bytes,
          "a reason too long is cut to 160 bytes by the writer");
    // **The writer never writes what the reader refuses**: every byte there
    // is, in a reason, reads back - printable as itself, the rest as '?'.
    std::size_t bytes = 0;
    for (int c = 0; c < 256; ++c) {
        glideslope::net::LearntLandingRefused m;
        m.why = std::string("gate ") + static_cast<char>(c);
        const std::vector<std::uint8_t> body = glideslope::net::write(m);
        glideslope::net::LearntLandingRefused back;
        const bool read = glideslope::net::read(
            std::span<const std::uint8_t>(body.data(), body.size()), back);
        const char want = c >= 0x20 && c <= 0x7E ? static_cast<char>(c) : '?';
        check(read && back.why == std::string("gate ") + want,
              "byte " + std::to_string(c) + " written reads back as " + std::string(1, want));
        ++bytes;
    }
    check(bytes == 256, "every byte was written: " + std::to_string(bytes));
}
