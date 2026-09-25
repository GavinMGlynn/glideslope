#include "harness.hpp"

#include "net/inputs.hpp"
#include "net/messages.hpp"
#include "net/inside.hpp"
#include "net/protocol.hpp"
#include "net/state.hpp"
#include "net/reliable.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <span>
#include <string>
#include <vector>

using glideslope::test::check;

namespace {

// **Every parser that reads something off the wire.** A parser missing from
// this list is one nothing is fuzzing, so the count is stated and held.
struct Parser {
    std::string name;
    // Reads `bytes`. Whatever it answers is fine; what matters is that it
    // answers at all, without reading past the end or doing anything the
    // sanitizers object to.
    std::function<void(std::span<const std::uint8_t>)> read;
};

std::vector<Parser> every_parser() {
    std::vector<Parser> out;
    out.push_back({"envelope", [](std::span<const std::uint8_t> b) {
                       glideslope::net::Reader r(b);
                       glideslope::net::Envelope e;
                       glideslope::net::Refusal why{};
                       (void)glideslope::net::read_envelope(r, e, why);
                   }});
    out.push_back({"lobby", [](std::span<const std::uint8_t> b) {
                       glideslope::net::Lobby m;
                       (void)glideslope::net::read(b, m);
                   }});
    out.push_back({"session", [](std::span<const std::uint8_t> b) {
                       glideslope::net::Session m;
                       (void)glideslope::net::read(b, m);
                   }});
    out.push_back({"weather", [](std::span<const std::uint8_t> b) {
                       glideslope::net::Weather m;
                       (void)glideslope::net::read(b, m);
                   }});
    out.push_back({"weather_aloft", [](std::span<const std::uint8_t> b) {
                       glideslope::net::WeatherAloft m;
                       (void)glideslope::net::read(b, m);
                   }});
    out.push_back({"aircraft", [](std::span<const std::uint8_t> b) {
                       glideslope::net::AircraftDefinition m;
                       (void)glideslope::net::read(b, m);
                   }});
    out.push_back({"terrain_dataset", [](std::span<const std::uint8_t> b) {
                       glideslope::net::TerrainDataset m;
                       (void)glideslope::net::read(b, m);
                   }});
    out.push_back({"controller_swap", [](std::span<const std::uint8_t> b) {
                       glideslope::net::ControllerSwap m;
                       (void)glideslope::net::read(b, m);
                   }});
    out.push_back({"watch", [](std::span<const std::uint8_t> b) {
                       glideslope::net::Watch m;
                       (void)glideslope::net::read(b, m);
                   }});
    out.push_back({"reliable", [](std::span<const std::uint8_t> b) {
                       glideslope::net::Reliable r;
                       (void)r.received(b);
                   }});
    out.push_back({"inputs", [](std::span<const std::uint8_t> b) {
                       glideslope::net::InputReceiver r;
                       (void)r.received(b);
                   }});
    out.push_back({"state", [](std::span<const std::uint8_t> b) {
                       (void)glideslope::net::read_state(b);
                   }});
    // `kind_of` and `knock_token` are readers too, and the cheapest ones to
    // forget: both are one line and both index into a caller's bytes.
    out.push_back({"kind_of", [](std::span<const std::uint8_t> b) {
                       (void)glideslope::net::kind_of(b);
                   }});
    out.push_back({"knock", [](std::span<const std::uint8_t> b) {
                       (void)glideslope::net::knock_token(glideslope::net::Inside::ping,
                                                          b);
                       (void)glideslope::net::knock_token(glideslope::net::Inside::pong,
                                                          b);
                   }});
    return out;
}

// **The seed corpus**: one valid example of everything this project writes,
// and a few things it never would. Every seed goes through every parser, not
// only the one that made it, because a datagram arrives before anybody knows
// what it is.
std::vector<std::pair<std::string, std::vector<std::uint8_t>>> seeds() {
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> out;
    const auto add = [&](const std::string& name, std::vector<std::uint8_t> bytes) {
        out.push_back({name, std::move(bytes)});
    };

    // Every envelope type, as `begin` writes it.
    for (const auto& [type, name] :
         std::vector<std::pair<glideslope::net::Type, std::string>>{
             {glideslope::net::Type::handshake_initiation, "envelope-initiation"},
             {glideslope::net::Type::handshake_response, "envelope-response"},
             {glideslope::net::Type::sealed, "envelope-sealed"},
             {glideslope::net::Type::refusal, "envelope-refusal"}}) {
        glideslope::net::Writer w = glideslope::net::begin(type);
        w.u8(0);
        add(name, w.take());
    }

    // Every reliable message, filled in.
    glideslope::net::Lobby lobby;
    lobby.players_allowed = 4;
    lobby.slots = {{0, glideslope::net::Controller::person, "a pilot"},
                   {1, glideslope::net::Controller::ai, "an AI"}};
    add("lobby", glideslope::net::write(lobby));

    glideslope::net::Session session;
    session.id = 0x0102030405060708ull;
    session.name = "a session";
    session.began_unix_ms = 1790000000000ull;
    session.simulation_time_s = 12.5;
    add("session", glideslope::net::write(session));

    glideslope::net::Weather weather;
    weather.metar = "YSSY 220000Z 09012KT 9999 FEW030 22/14 Q1017";
    weather.latitude_deg = -33.9;
    weather.longitude_deg = 151.2;
    weather.elevation_m = 6.0;
    weather.turbulence_severity = 3;
    weather.air_seed = 0xFEEDFACEull;
    weather.microbursts = {{-33.9, 151.2, 1500.0, 12.0, 60.0, 900.0}};
    add("weather", glideslope::net::write(weather));

    glideslope::net::WeatherAloft aloft;
    aloft.time = "2026-09-22T00:00";
    aloft.levels = {{1000.0, 110.0, 3.5, -2.0, 21.5}};
    aloft.near_ground = {{10.0, 3.0, -1.5}};
    add("weather_aloft", glideslope::net::write(aloft));

    glideslope::net::AircraftDefinition aircraft;
    aircraft.aircraft = 1;
    aircraft.id = "c172p";
    aircraft.model = "c172p";
    add("aircraft", glideslope::net::write(aircraft));

    glideslope::net::TerrainDataset dataset;
    dataset.name = "Copernicus GLO-30";
    dataset.version = "2023_1";
    dataset.sha256.assign(glideslope::net::sha256_bytes, 0x5A);
    add("terrain_dataset", glideslope::net::write(dataset));

    glideslope::net::ControllerSwap swap;
    swap.aircraft = 2;
    swap.to = glideslope::net::Controller::ai;
    swap.at_simulation_time_s = 99.5;
    add("controller_swap", glideslope::net::write(swap));

    glideslope::net::Watch watch;
    watch.aircraft = 4;
    add("watch", glideslope::net::write(watch));

    // A reliable datagram, as the layer puts one on the wire.
    {
        glideslope::net::Reliable sender;
        const std::vector<std::uint8_t> body = glideslope::net::write(swap);
        (void)sender.send(body);
        for (const std::vector<std::uint8_t>& d : sender.to_send(0.0)) {
            add("reliable-datagram", d);
        }
    }

    // An input packet, full of frames.
    {
        glideslope::net::InputSender sender;
        for (std::uint32_t i = 1; i <= glideslope::net::redundancy; ++i) {
            glideslope::net::ControlList c{};
            for (std::size_t k = 0; k < c.size(); ++k) {
                c[k] = static_cast<double>((i + k) % 3) - 1.0;
            }
            sender.add(i, c);
        }
        add("inputs", sender.packet());
    }

    // And things this project never writes, which is what actually arrives
    // when somebody points something else at the port.
    add("empty", {});
    add("state", [] {
        glideslope::net::StatePacket s;
        s.simulation_time_s = 12.5;
        s.last_input_applied = 7;
        s.your_aircraft = 1;
        for (int i = 0; i < 3; ++i) {
            glideslope::net::AircraftState a;
            a.index = static_cast<std::uint8_t>(i);
            a.controller = glideslope::net::Controller::person;
            a.x_m = 6378137.0 + i;
            a.y_m = -4517590.0;
            a.z_m = 1234567.0;
            a.vx_mps = 120.0F;
            a.heading_deg = 90.0F;
            s.aircraft.push_back(a);
        }
        return *glideslope::net::write_state(s);
    }());
    add("one-byte", {0x00});
    add("all-ones", std::vector<std::uint8_t>(64, 0xFF));
    add("text", [] {
        const std::string s = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
        return std::vector<std::uint8_t>(s.begin(), s.end());
    }());
    add("largest-datagram", std::vector<std::uint8_t>(1232, 0xA5));
    return out;
}

} // namespace

// **Every network parser is fuzzed, under the sanitizers this build already
// runs.** The seed corpus is every datagram and message this project writes,
// plus things it never would, and each one goes through *every* parser -
// because a datagram arrives before anybody knows what it is, and a parser
// that is only ever shown its own output has not been tested at all.
//
// Every seed is also cut to every length and changed a byte at a time, and
// then mutated at random from a fixed seed so that a run is the same run
// twice. The build is compiled with `-fsanitize=address,undefined` and
// `-fno-sanitize-recover=all`, so reading past the end of any of these ends
// the test rather than passing it.
GLIDESLOPE_TEST(the_seed_corpus_goes_through_every_network_parser_under_sanitizers) {
    const std::vector<Parser> parsers = every_parser();
    const auto corpus = seeds();
    check(parsers.size() == 14, "fourteen parsers are fuzzed, not " +
                                    std::to_string(parsers.size()));
    check(corpus.size() == 20, "twenty seeds - four envelopes, eight messages, a reliable datagram, an input packet, a state packet and five things this project never writes - not " + std::to_string(corpus.size()));

    std::uint64_t calls = 0;

    // Every seed, whole, through every parser.
    for (const auto& [name, bytes] : corpus) {
        for (const Parser& p : parsers) {
            p.read(bytes);
            ++calls;
        }
    }

    // Every seed cut to every length, through every parser: a datagram can
    // arrive short, and a reader that walks off the end of one is a hole.
    for (const auto& [name, bytes] : corpus) {
        for (std::size_t n = 0; n <= bytes.size(); ++n) {
            const std::span<const std::uint8_t> cut(bytes.data(), n);
            for (const Parser& p : parsers) {
                p.read(cut);
                ++calls;
            }
        }
    }

    // Every seed with one byte changed, through every parser.
    for (const auto& [name, bytes] : corpus) {
        for (std::size_t at = 0; at < bytes.size(); ++at) {
            std::vector<std::uint8_t> changed = bytes;
            for (const std::uint8_t value : std::array<std::uint8_t, 6>{0x00, 0x01, 0x7F, 0x80, 0xFE, 0xFF}) {
                changed[at] = value;
                for (const Parser& p : parsers) {
                    p.read(changed);
                    ++calls;
                }
            }
        }
    }

    // And mutated at random, from a fixed seed so that a failure can be had
    // again: bytes changed, bytes dropped, bytes inserted, and pieces of two
    // seeds joined.
    std::mt19937_64 random(20260922);
    const auto pick = [&](std::size_t most) {
        return most == 0 ? std::size_t{0} : random() % most;
    };
    for (int round = 0; round < 4000; ++round) {
        const auto& [name, bytes] = corpus[pick(corpus.size())];
        std::vector<std::uint8_t> mutated = bytes;
        switch (random() % 4) {
        case 0:
            if (!mutated.empty()) {
                mutated[pick(mutated.size())] = static_cast<std::uint8_t>(random());
            }
            break;
        case 1:
            if (!mutated.empty()) {
                mutated.erase(mutated.begin() +
                              static_cast<std::ptrdiff_t>(pick(mutated.size())));
            }
            break;
        case 2:
            mutated.insert(mutated.begin() +
                               static_cast<std::ptrdiff_t>(pick(mutated.size() + 1)),
                           static_cast<std::uint8_t>(random()));
            break;
        default: {
            const auto& [other_name, other] = corpus[pick(corpus.size())];
            mutated.insert(mutated.end(), other.begin(), other.end());
            break;
        }
        }
        // Nothing this project sends is larger than a datagram, and nothing
        // larger will ever reach a parser.
        if (mutated.size() > glideslope::net::most_message_bytes + 64) {
            mutated.resize(glideslope::net::most_message_bytes + 64);
        }
        for (const Parser& p : parsers) {
            p.read(mutated);
            ++calls;
        }
    }

    check(calls > 100000, "the corpus was put through the parsers " +
                              std::to_string(calls) + " times");
    std::printf("  %zu seeds through %zu parsers: %llu reads, none out of bounds\n",
                corpus.size(), parsers.size(),
                static_cast<unsigned long long>(calls));
}

// **The corpus is written out**, so that it can be handed to a real fuzzer -
// libFuzzer or AFL - rather than only living inside this test.
GLIDESLOPE_TEST(the_seed_corpus_is_written_where_a_fuzzer_can_take_it) {
    const std::filesystem::path where =
        std::filesystem::path(GLIDESLOPE_TEST_SOURCE_DIR).parent_path() / "build" /
        "fuzz-corpus";
    std::filesystem::create_directories(where);
    std::size_t written = 0;
    for (const auto& [name, bytes] : seeds()) {
        const std::filesystem::path file = where / (name + ".bin");
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        check(out.good(), "the corpus file can be written: " + file.string());
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        ++written;
    }
    check(written == 20, "every seed was written, not " + std::to_string(written));
    std::printf("  wrote %zu seeds to %s\n", written, where.string().c_str());
}
