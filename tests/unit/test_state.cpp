#include "harness.hpp"

#include "net/inside.hpp"
#include "net/protocol.hpp"
#include "net/sealing.hpp"
#include "net/state.hpp"
#include "platform/socket.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <span>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

using glideslope::net::AircraftState;
using glideslope::net::Controller;
using glideslope::net::StatePacket;
using glideslope::net::read_state;
using glideslope::net::write_state;
using glideslope::test::check;

namespace {

// An aircraft whose every field is different from every other, so that a
// field written or read in the wrong order shows up rather than cancelling.
// Nothing here is zero: a flipped sign bit on a zero gives a value that
// compares equal to it, which would weaken the byte-change test below.
AircraftState an_aircraft(int n) {
    const double d = 1.0 + n;
    AircraftState a;
    a.index = static_cast<std::uint8_t>(n + 1);
    a.controller = n % 2 == 0 ? Controller::person : Controller::ai;
    // Earth's radius is about 6,378,137 m, so these are the size a real one is.
    a.x_m = 6378137.0 + d;
    a.y_m = -4517590.25 - d;
    a.z_m = 1234567.5 + d * 2.0;
    a.vx_mps = static_cast<float>(120.5 + d);
    a.vy_mps = static_cast<float>(-85.25 - d);
    a.vz_mps = static_cast<float>(3.125 + d);
    a.heading_deg = static_cast<float>(37.5 + d);
    a.pitch_deg = static_cast<float>(-4.25 - d);
    a.roll_deg = static_cast<float>(12.75 + d);
    return a;
}

StatePacket a_packet(std::size_t count) {
    StatePacket s;
    s.simulation_time_s = 1234.5;
    s.last_input_applied = 0xDEADBEEF;
    // Named, so the round trip carries it; `no_aircraft` when there is none
    // to name, which is the other thing it can hold.
    s.your_aircraft = count > 0 ? static_cast<std::uint8_t>(1)
                                : glideslope::net::no_aircraft;
    for (std::size_t i = 0; i < count; ++i) {
        s.aircraft.push_back(an_aircraft(static_cast<int>(i)));
    }
    return s;
}

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

} // namespace

// **A state packet written and read back is the same packet**, for every
// number of aircraft one can hold - nought to twenty, all twenty-one walked -
// and it is the size the code says it is.
GLIDESLOPE_TEST(a_state_packet_written_and_read_back_is_the_same_packet) {
    std::size_t walked = 0;
    for (std::size_t count = 0; count <= glideslope::net::most_aircraft_in_a_state;
         ++count) {
        const StatePacket sent = a_packet(count);
        const auto bytes = write_state(sent);
        check(bytes.has_value(), std::to_string(count) + " aircraft can be written");
        check(bytes->size() == glideslope::net::state_bytes(count),
              "and takes the " + std::to_string(glideslope::net::state_bytes(count)) +
                  " bytes it says, not " + std::to_string(bytes->size()));
        check((*bytes)[0] == static_cast<std::uint8_t>(glideslope::net::Inside::state),
              "and begins with its kind");
        const auto back = read_state(all_of(*bytes));
        check(back.has_value(), "and reads back");
        check(*back == sent, "as the same packet");
        ++walked;
    }
    check(walked == glideslope::net::most_aircraft_in_a_state + 1,
          "all 21 counts from nought to twenty were walked, not " +
              std::to_string(walked));

    // One more than it holds is not a packet, and is refused before any bytes
    // are written rather than making one nothing can read.
    check(!write_state(a_packet(glideslope::net::most_aircraft_in_a_state + 1))
               .has_value(),
          "twenty-one aircraft is refused");
}

// **A packet filled to its limits fits in one datagram**, with the envelope
// and the sealing in front of it. If it did not, the server would silently
// stop being able to say where everybody is on a full session.
GLIDESLOPE_TEST(a_state_packet_filled_to_its_limits_fits_in_one_datagram) {
    const auto bytes = write_state(a_packet(glideslope::net::most_aircraft_in_a_state));
    check(bytes.has_value(), "a full packet can be written");
    const std::size_t on_the_wire = glideslope::net::envelope_size +
                                    glideslope::net::sealing_overhead + bytes->size();
    check(on_the_wire <= glideslope::platform::largest_datagram,
          "twenty aircraft, sealed and enveloped, is " + std::to_string(on_the_wire) +
              " bytes, and a datagram holds " +
              std::to_string(glideslope::platform::largest_datagram));
    std::printf("a full state packet is %zu bytes on the wire, of %zu\n", on_the_wire,
                glideslope::platform::largest_datagram);
}

// **Every truncation of a state packet is refused** - every length from
// nothing to one byte short - and the reader never runs off the end.
GLIDESLOPE_TEST(every_truncation_of_a_state_packet_is_refused) {
    std::size_t walked = 0;
    for (std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{7},
                              glideslope::net::most_aircraft_in_a_state}) {
        const auto whole = write_state(a_packet(count));
        check(whole.has_value(), "the packet can be written");
        for (std::size_t length = 0; length < whole->size(); ++length) {
            const std::vector<std::uint8_t> cut(whole->begin(),
                                                whole->begin() +
                                                    static_cast<std::ptrdiff_t>(length));
            check(!read_state(all_of(cut)).has_value(),
                  "a packet cut to " + std::to_string(length) + " of " +
                      std::to_string(whole->size()) + " bytes is refused");
            ++walked;
        }
        // And one byte too many is refused as well: a packet is exactly its
        // own length.
        std::vector<std::uint8_t> extra = *whole;
        extra.push_back(0);
        check(!read_state(all_of(extra)).has_value(),
              "and a packet with a byte after it is refused");
    }
    check(walked > 0, "truncations were walked");
    std::printf("%zu truncations refused\n", walked);
}

// **Every single-byte change to a state packet is read exactly or refused.**
// Not "does not crash": if the reader takes it, what it read must write back
// to the very bytes it was given, so nothing is lost and nothing invented.
GLIDESLOPE_TEST(every_single_byte_change_to_a_state_packet_is_read_or_refused) {
    const auto whole = write_state(a_packet(3));
    check(whole.has_value(), "the packet can be written");
    std::size_t refused = 0;
    std::size_t read_back = 0;
    for (std::size_t at = 0; at < whole->size(); ++at) {
        for (int bit = 0; bit < 8; ++bit) {
            std::vector<std::uint8_t> changed = *whole;
            changed[at] = static_cast<std::uint8_t>(changed[at] ^ (1u << bit));
            const auto got = read_state(all_of(changed));
            if (!got) {
                ++refused;
                continue;
            }
            const auto again = write_state(*got);
            check(again.has_value(), "what was read can be written again");
            check(*again == changed,
                  "a change at byte " + std::to_string(at) + " bit " +
                      std::to_string(bit) +
                      " was read as something other than what was there");
            ++read_back;
        }
    }
    check(refused + read_back == whole->size() * 8,
          "every bit of every byte was walked");
    std::printf("%zu one-bit changes: %zu refused, %zu read exactly\n",
                whole->size() * 8, refused, read_back);
}

// **A number that is not one is refused, in every field that holds one.** The
// clock, the nine numbers of an aircraft, and the thirteen of the client's own
// motion - twenty-three fields - against a NaN and both infinities: sixty-nine
// cases, counted.
GLIDESLOPE_TEST(a_state_packet_refuses_a_nan_and_an_infinity_in_every_field) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double up = std::numeric_limits<double>::infinity();
    const std::vector<double> wrong = {nan, up, -up};

    // A packet with two aircraft and this client's own motion, so that every
    // numeric field there is can be set.
    const auto a_full_packet = [] {
        StatePacket s = a_packet(2);
        glideslope::net::OwnMotion m;
        m.x_m = -4646000.5;
        m.y_m = 2553000.25;
        m.z_m = -3534000.75;
        m.attitude = {0.5F, 0.5F, 0.5F, 0.5F};
        m.uvw_mps = {55.0F, 1.5F, -2.0F};
        m.pqr_radps = {0.01F, -0.02F, 0.03F};
        s.yours = m;
        return s;
    };

    // Setting one field of a packet, by number, so the walk can say how many
    // fields there are and check it covered them.
    const std::size_t fields = 23; // the clock; an aircraft's three positions,
                                   // three velocities and three angles; and the
                                   // own motion's three positions, four
                                   // quaternion terms, three velocities and
                                   // three rates
    std::size_t walked = 0;
    for (const double bad : wrong) {
        for (std::size_t field = 0; field < fields; ++field) {
            StatePacket s = a_full_packet();
            AircraftState& a = s.aircraft[1];
            glideslope::net::OwnMotion& m = *s.yours;
            const auto f = static_cast<float>(bad);
            switch (field) {
            case 0: s.simulation_time_s = bad; break;
            case 1: a.x_m = bad; break;
            case 2: a.y_m = bad; break;
            case 3: a.z_m = bad; break;
            case 4: a.vx_mps = f; break;
            case 5: a.vy_mps = f; break;
            case 6: a.vz_mps = f; break;
            case 7: a.heading_deg = f; break;
            case 8: a.pitch_deg = f; break;
            case 9: a.roll_deg = f; break;
            case 10: m.x_m = bad; break;
            case 11: m.y_m = bad; break;
            case 12: m.z_m = bad; break;
            case 13: case 14: case 15: case 16: m.attitude[field - 13] = f; break;
            case 17: case 18: case 19: m.uvw_mps[field - 17] = f; break;
            case 20: case 21: case 22: m.pqr_radps[field - 20] = f; break;
            default: check(false, "a field that is not one of the twenty-three"); break;
            }
            check(!write_state(s).has_value(),
                  "field " + std::to_string(field) + " refuses being written");

            // And a reader given those bytes anyway refuses them: a writer
            // that will not make one is not a defence against one arriving.
            auto bytes = *write_state(a_full_packet());
            const std::size_t header = glideslope::net::state_header_bytes;
            const std::size_t per = glideslope::net::state_per_aircraft_bytes;
            std::size_t at = 0;
            bool is_float = false;
            if (field == 0) {
                at = 1;
            } else if (field <= 9) {
                // The second aircraft, past its index, controller and condition.
                at = header + per + 3;
                if (field <= 3) {
                    at += (field - 1) * 8;
                } else {
                    at += 3 * 8 + (field - 4) * 4;
                    is_float = true;
                }
            } else {
                // The own motion, past both aircraft and its flag.
                at = header + 2 * per + 1;
                if (field <= 12) {
                    at += (field - 10) * 8;
                } else {
                    at += 3 * 8 + (field - 13) * 4;
                    is_float = true;
                }
            }
            if (is_float) {
                std::uint32_t bits = 0;
                const auto v = static_cast<float>(bad);
                std::memcpy(&bits, &v, sizeof(bits));
                for (std::size_t i = 0; i < 4; ++i) {
                    bytes[at + i] = static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFFu);
                }
            } else {
                std::uint64_t bits = 0;
                std::memcpy(&bits, &bad, sizeof(bits));
                for (std::size_t i = 0; i < 8; ++i) {
                    bytes[at + i] = static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFFu);
                }
            }
            check(!read_state(all_of(bytes)).has_value(),
                  "field " + std::to_string(field) +
                      " refuses being read as a number that is not one");
            ++walked;
        }
    }
    check(walked == wrong.size() * fields,
          "all " + std::to_string(wrong.size() * fields) + " cases were walked");
    check(walked == 69, "sixty-nine cases, not " + std::to_string(walked));
}

// **A state packet carries this client's own motion, and nothing but a flag
// when there is none**: written and read back the same, the size the code says,
// and a flag other than nought or one refused.
GLIDESLOPE_TEST(a_state_packet_carries_the_clients_own_motion) {
    StatePacket s = a_packet(3);
    glideslope::net::OwnMotion m;
    m.x_m = -4646000.5;
    m.y_m = 2553000.25;
    m.z_m = -3534000.75;
    m.attitude = {0.9F, 0.1F, -0.2F, 0.3F};
    m.uvw_mps = {55.0F, 1.5F, -2.0F};
    m.pqr_radps = {0.01F, -0.02F, 0.03F};
    s.yours = m;
    const auto with = write_state(s);
    check(with.has_value(), "a packet with the client's motion is written");
    check(with->size() == glideslope::net::state_bytes(3, true),
          "and is " + std::to_string(glideslope::net::state_bytes(3, true)) + " bytes, not " +
              std::to_string(with->size()));
    const auto back = read_state(all_of(*with));
    check(back.has_value() && *back == s, "and is read back the same");

    const auto without = write_state(a_packet(3));
    check(without.has_value() && without->size() == glideslope::net::state_bytes(3),
          "a packet without it carries its flag alone");
    std::size_t refused = 0;
    for (int flag = 2; flag < 256; ++flag) {
        auto bytes = *without;
        bytes[glideslope::net::state_bytes(3) - 1] = static_cast<std::uint8_t>(flag);
        if (!read_state(all_of(bytes))) {
            ++refused;
        }
    }
    check(refused == 254, "every flag but nought and one is refused: " +
                              std::to_string(refused) + " of 254");
    check(glideslope::net::state_bytes(20, true) <= 1218,
          "a full packet with the client's motion fits a datagram: " +
              std::to_string(glideslope::net::state_bytes(20, true)) + " bytes");
}

// **A controller this version does not know is not a packet.** All 256 bytes
// walked: three are controllers and 253 are not.
GLIDESLOPE_TEST(a_state_packet_with_a_controller_that_is_not_one_is_refused) {
    const auto whole = write_state(a_packet(1));
    check(whole.has_value(), "the packet can be written");
    // The controller is the second byte of the aircraft, which follows the
    // header and its index.
    const std::size_t at = glideslope::net::state_header_bytes + 1;
    std::size_t taken = 0;
    std::size_t refused = 0;
    for (int byte = 0; byte <= 255; ++byte) {
        std::vector<std::uint8_t> changed = *whole;
        changed[at] = static_cast<std::uint8_t>(byte);
        if (read_state(all_of(changed)).has_value()) {
            ++taken;
        } else {
            ++refused;
        }
    }
    check(taken == 3, "three bytes are controllers, not " + std::to_string(taken));
    check(refused == 253, "and 253 are not, not " + std::to_string(refused));
}

// **Every byte a condition could be, walked**: flying and wrecked are read,
// and the other 254 make the whole packet unreadable rather than being taken
// for one of them. A wreck is said and read back as a wreck.
GLIDESLOPE_TEST(a_state_packet_carries_a_wreck_and_refuses_a_condition_it_does_not_know) {
    StatePacket wreck = a_packet(2);
    wreck.aircraft[1].condition = glideslope::net::Condition::wrecked;
    const auto said = write_state(wreck);
    check(said.has_value(), "a packet with a wreck in it is written");
    const auto heard = read_state(*said);
    check(heard.has_value() && *heard == wreck, "and read back the same, the wreck a wreck");

    const std::size_t at = glideslope::net::state_header_bytes + 2; // the first aircraft's
    std::size_t taken = 0;
    std::size_t refused = 0;
    for (int value = 0; value < 256; ++value) {
        auto bytes = *write_state(a_packet(1));
        bytes[at] = static_cast<std::uint8_t>(value);
        if (read_state(bytes)) {
            ++taken;
        } else {
            ++refused;
        }
    }
    check(taken == 2, "two bytes are conditions, not " + std::to_string(taken));
    check(refused == 254, "and 254 are not, not " + std::to_string(refused));
    check(taken + refused == 256, "every byte was tried");
}

// **The document and the code agree about the state packet**, field for
// field, so that a client written from `TRANSPORT.md` reads what this writes.
GLIDESLOPE_TEST(the_transport_document_and_the_code_agree_about_the_state_packet) {
    const std::filesystem::path doc =
        std::filesystem::path(GLIDESLOPE_TEST_PROJECT_DIR) / "docs" / "TRANSPORT.md";
    std::ifstream in(doc);
    check(in.good(), "docs/TRANSPORT.md is there");
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const auto says = [&](const std::string& what) {
        return text.find(what) != std::string::npos;
    };

    check(says("### State updates"), "the document has a section for it");
    check(says("| `f32` | 4 | its IEEE-754 bits, written as a `u32`"),
          "and says how a float goes on the wire");
    const std::vector<std::string> fields = {
        "the simulation's clock, seconds since the session began",
        "the newest input sequence from this client the server has applied",
        "the number, as below, of this client's own aircraft",
        "how many aircraft follow",
        "the server's number for this aircraft",
        "who is flying it",
        "whether it is flying or a wreck",
        "its position, Earth-centred and Earth-fixed, metres",
        "its velocity in the same frame, metres a second",
        "its heading, degrees",
        "its pitch, degrees",
        "its roll, degrees",
    };
    std::size_t said = 0;
    for (const std::string& field : fields) {
        check(says(field), "the document names '" + field + "'");
        ++said;
    }
    check(said == fields.size(), "every field was looked for");
    check(says(std::to_string(glideslope::net::most_aircraft_in_a_state) +
               " aircraft"),
          "and says how many aircraft one can hold");
}
