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
#include <span>
#include <string>
#include <utility>
#include <vector>

using glideslope::net::AircraftDefinition;
using glideslope::net::AloftLevel;
using glideslope::net::Controller;
using glideslope::net::ControllerSwap;
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
    // forecast, and which aircraft a client watches.
    check(walked == 8, "eight kinds were walked, not " + std::to_string(walked));
}

// **A body of one kind is never read as another.** All thirty-six pairs are
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
    check(pairs == 64, "all sixty-four pairs were tried, not " + std::to_string(pairs));
    check(refused == 56, "fifty-six of them are refused, not " + std::to_string(refused));
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
    check(walked == 8, "every kind was tried");
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
    check(kinds == 8, "eight kinds are known, not " + std::to_string(kinds));

    std::size_t controllers = 0;
    for (int v = 0; v < 256; ++v) {
        if (glideslope::net::known_controller(static_cast<std::uint8_t>(v))) {
            ++controllers;
        }
    }
    check(controllers == 3, "three controllers are known, not " +
                                std::to_string(controllers));

    // A body whose first byte is not a kind is not a message at all.
    std::size_t not_a_kind = 0;
    for (int v = 0; v < 256; ++v) {
        const std::vector<std::uint8_t> body{static_cast<std::uint8_t>(v)};
        if (!glideslope::net::kind_of(body).has_value()) {
            ++not_a_kind;
        }
    }
    check(not_a_kind == 248, "two hundred and forty-eight first bytes are no kind, not " +
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
    check(kinds.size() == 8, "the six the item names, the forecast, and a watch");

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
                std::to_string(arrived.size()) + " of seven messages arrived" +
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
    // Eight messages with nothing lost are over in nine datagrams - eight
    // out and one acknowledgement back - so a pattern whose set bits all lie
    // at 9 to 11 never touches anything. There are 2^3 = 8 of those,
    // including the empty one, which leaves 4,088 that do lose something.
    check(with_loss == 4088, "4,088 of the patterns lost something, not " +
                                 std::to_string(with_loss));
    std::printf("  8 messages through 4,096 loss patterns; worst took %llu datagrams\n",
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
        {Message::watch, "WATCH"}};
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
    check(walked == 8, "every kind was walked");

    // Every controller, by value and by name.
    const std::vector<std::pair<Controller, std::string>> controllers{
        {Controller::nobody, "NOBODY"},
        {Controller::person, "PERSON"},
        {Controller::ai, "AI"}};
    std::size_t said = 0;
    for (const auto& [controller, name] : controllers) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "`%02X`", static_cast<unsigned>(controller));
        check(says(std::string("| ") + buf + " | `" + name + "` |"),
              "the document gives " + name + " as " + buf);
        check(glideslope::net::known_controller(static_cast<std::uint8_t>(controller)),
              name + " is a controller the code knows");
        ++said;
    }
    check(said == 3, "every controller was walked");

    // Every limit the code enforces is a number the document states.
    const std::vector<std::pair<std::size_t, std::string>> limits{
        {glideslope::net::most_slots, "at most 4"},
        {glideslope::net::most_levels, "at most 24"},
        {glideslope::net::most_near_ground, "at most 4"},
        {glideslope::net::most_microbursts, "at most 16"},
        {glideslope::net::most_metar_bytes, "at most 256 bytes"},
        {glideslope::net::most_name_bytes, "at most 64 bytes"},
        {glideslope::net::most_time_bytes, "at most 32 bytes"},
        {glideslope::net::sha256_bytes, "exactly 32 bytes"}};
    std::size_t held = 0;
    for (const auto& [value, phrase] : limits) {
        check(phrase.find(std::to_string(value)) != std::string::npos,
              "the phrase names the limit the code has: " + phrase);
        check(says(phrase), "the document says \"" + phrase + "\"");
        ++held;
    }
    check(held == 8, "every limit was walked");
    std::printf("  the document holds 7 kinds, 3 controllers and 8 limits\n");
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

    // **The space this walked, stated**: every kind there is, and each one
    // filled rather than sampled.
    check(walked == 7, "every kind was filled to its limits, not " +
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
        {Message::watch, "WATCH"}};
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
    check(named == 8, "eight kinds, not " + std::to_string(named));

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
    weather.elevation_m = next();
    Microburst burst;
    burst.latitude_deg = next();
    burst.longitude_deg = next();
    burst.radius_m = next();
    burst.downdraught_mps = next();
    burst.start_s = next();
    burst.duration_s = next();
    weather.microbursts = {burst};
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
// **The space, stated.** Seven kinds of message carry nineteen
// floating-point fields between them:
//
//   - `SESSION`, one - the simulation's clock;
//   - `WEATHER`, three of its own and six per microburst;
//   - `WEATHER_ALOFT`, five per pressure level and three per near-ground
//     wind;
//   - `CONTROLLER_SWAP`, one.
//
// **The three kinds left out are named**: `LOBBY`, `AIRCRAFT` and
// `TERRAIN_DATASET` carry no floating-point field at all - slot indices,
// names and a hash - so there is nothing in them for this to walk.
//
// Each of the nineteen is overwritten with six bit patterns that are not a
// number and must be refused, and five that are numbers however extreme and
// must still read: 114 refusals and 95 readings, each counted.
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
    check(kinds.size() == 4,
          "four of the seven kinds carry a floating-point field, not " +
              std::to_string(kinds.size()));

    std::size_t walked = 0;
    std::size_t refused = 0;
    std::size_t accepted = 0;
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
            for (const auto& [bits, what] : a_number) {
                std::vector<std::uint8_t> changed = k.bytes;
                put_bits(changed, at[0], bits);
                check(k.reads(changed),
                      k.name + " with " + what + " at byte " + std::to_string(at[0]) +
                          " is a number and must still read");
                ++accepted;
            }
            ++walked;
        }
    }
    check(walked == 19, "nineteen floating-point fields were walked, not " +
                            std::to_string(walked));
    check(refused == 19 * 6, "114 numbers that are not numbers were refused, not " +
                                 std::to_string(refused));
    check(accepted == 19 * 5, "95 extreme numbers still read, not " +
                                  std::to_string(accepted));
    std::printf("  19 floating-point fields: %zu refused, %zu still read\n", refused,
                accepted);
}
