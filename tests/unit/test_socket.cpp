#include "harness.hpp"

#include "platform/socket.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <vector>

using glideslope::platform::Address;
using glideslope::platform::address_of;
using glideslope::platform::UdpSocket;
using glideslope::test::check;

namespace {

std::span<const std::uint8_t> all_of(const std::vector<std::uint8_t>& v) {
    return std::span<const std::uint8_t>(v.data(), v.size());
}

// Waits a little for a datagram, because a loopback send is quick but not
// instant. Answers how many bytes arrived, or 0 if none did.
std::size_t wait_for(UdpSocket& socket, std::span<std::uint8_t> into, Address& from) {
    for (int i = 0; i < 200; ++i) {
        const std::size_t got = socket.receive(into, from);
        if (got > 0) {
            return got;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

} // namespace

// **An address written down and read back is the same address**, both
// families, and the shorthands a person writes are understood.
GLIDESLOPE_TEST(an_address_written_down_and_read_back_is_the_same_address) {
    struct Case {
        std::string text;
        std::string reads_back_as; // empty: the same text
    };
    const std::vector<Case> cases{
        {"0.0.0.0:0", ""},
        {"127.0.0.1:26000", ""},
        {"255.255.255.255:65535", ""},
        {"1.2.3.4:1", ""},
        {"[::1]:26000", "[0:0:0:0:0:0:0:1]:26000"},
        {"[::]:1", "[0:0:0:0:0:0:0:0]:1"},
        {"[2001:db8::1]:443", "[2001:db8:0:0:0:0:0:1]:443"},
        {"[2001:db8:0:0:0:0:0:1]:443", ""},
        {"[fe80::1234:5678:9abc:def0]:9", "[fe80:0:0:0:1234:5678:9abc:def0]:9"},
    };
    std::size_t walked = 0;
    for (const Case& one : cases) {
        const auto got = address_of(one.text);
        check(got.has_value(), one.text + " is an address");
        const std::string want = one.reads_back_as.empty() ? one.text : one.reads_back_as;
        check(got->text() == want,
              one.text + " reads back as " + got->text() + ", not " + want);
        // And what it reads back as reads back as itself.
        const auto again = address_of(got->text());
        check(again.has_value() && *again == *got,
              "what " + one.text + " writes as is itself an address");
        ++walked;
    }
    check(walked == cases.size(), "every address was walked: " + std::to_string(walked));
    check(walked == 9, "there are nine addresses in this walk, not " +
                           std::to_string(walked));

    check(glideslope::platform::loopback_v4(26000).text() == "127.0.0.1:26000",
          "the IPv4 loopback is 127.0.0.1");
    check(glideslope::platform::loopback_v6(26000).text() == "[0:0:0:0:0:0:0:1]:26000",
          "the IPv6 loopback is ::1");
}

// **Everything that is not an address is refused**, and a host name most of
// all: nothing here resolves, so a name is not an address.
GLIDESLOPE_TEST(anything_that_is_not_an_address_is_refused) {
    const std::vector<std::string> nots{
        "",
        "127.0.0.1",                  // no port
        ":26000",                     // no host
        "127.0.0.1:",                 // no port after the colon
        "127.0.0.1:65536",            // a port there is none of
        "127.0.0.1:-1",               // not a number
        "256.0.0.1:1",                // a byte there is none of
        "1.2.3:1",                    // three parts
        "1.2.3.4.5:1",                // five parts
        "01.2.3.4:1",                 // a leading zero is not how a number is written
        "1.2.3.4:007",                // nor on a port
        "localhost:26000",            // a name, which this does not resolve
        "glideslope.example:26000",   // nor this one
        "[::1]",                      // no port
        "[::1:26000",                 // no bracket
        "::1:26000",                  // no brackets at all
        "[1:2:3:4:5:6:7:8:9]:1",      // nine groups
        "[1:2:3:4:5:6:7]:1",          // seven, with no gap
        "[1::2::3]:1",                // two gaps
        "[12345::1]:1",               // a group of five digits
        "[:1]:1",                     // a lone leading colon
        "[1:]:1",                     // a lone trailing colon
        "[xyz::1]:1",                 // not hexadecimal
    };
    std::size_t walked = 0;
    for (const std::string& one : nots) {
        check(!address_of(one).has_value(),
              "\"" + one + "\" was read as an address and should not have been");
        ++walked;
    }
    check(walked == nots.size(), "every one was walked: " + std::to_string(walked));
    check(walked == 23, "there are 23 of them, not " + std::to_string(walked));
}

// **A datagram sent to the loopback arrives, whole, and says where it came
// from.**
GLIDESLOPE_TEST(a_datagram_sent_to_the_loopback_arrives_whole_and_says_where_from) {
    auto listener = UdpSocket::bound(0);
    check(listener.has_value(), "a socket binds to whatever port is going");
    check(listener->port() != 0, "and is told which port it got");

    auto sender = UdpSocket::bound(0);
    check(sender.has_value(), "and so does another");
    check(sender->port() != listener->port(), "and they are not the same port");

    // Nothing has been sent, so nothing is waiting: the ordinary answer, not
    // an error.
    std::vector<std::uint8_t> into(glideslope::platform::largest_datagram);
    Address from;
    check(listener->receive(std::span<std::uint8_t>(into.data(), into.size()), from) == 0,
          "a socket nobody has written to has nothing waiting");

    const std::vector<std::uint8_t> sent{'G', 'L', 'D', 'S', 1, 3, 0xFF, 0x00, 0x7F};
    check(sender->send(glideslope::platform::loopback_v4(listener->port()), all_of(sent)),
          "a datagram is sent to the loopback");

    const std::size_t got =
        wait_for(*listener, std::span<std::uint8_t>(into.data(), into.size()), from);
    check(got == sent.size(),
          "it arrives whole: " + std::to_string(got) + " bytes of " +
              std::to_string(sent.size()));
    check(std::equal(sent.begin(), sent.end(), into.begin()), "and byte for byte");
    check(from.is_v4(), "and says it came from an IPv4 address");
    check(from.port == sender->port(),
          "and from the port that sent it: " + std::to_string(from.port) + ", not " +
              std::to_string(sender->port()));
}

// **A datagram larger than the smallest path is refused rather than
// fragmented.** 1232 bytes is IPv6's obligatory 1280 less its headers, so
// nothing this sends is ever broken up on the way.
GLIDESLOPE_TEST(a_datagram_too_large_for_the_smallest_path_is_refused) {
    auto sender = UdpSocket::bound(0);
    check(sender.has_value(), "a socket binds");
    const Address to = glideslope::platform::loopback_v4(9);

    const std::vector<std::uint8_t> big(glideslope::platform::largest_datagram + 1, 'x');
    check(!sender->send(to, all_of(big)),
          "a datagram of " + std::to_string(big.size()) + " bytes is refused");

    const std::vector<std::uint8_t> most(glideslope::platform::largest_datagram, 'x');
    check(sender->send(to, all_of(most)),
          "and one of " + std::to_string(most.size()) + " is not");

    // An address that is not one is refused rather than sent into the dark.
    const std::vector<std::uint8_t> small{1, 2, 3};
    check(!sender->send(Address{}, all_of(small)),
          "a datagram to an address that is not one is refused");
}

// **A socket asked for a particular port gets that port**, and a second
// socket asking for the same one is refused rather than quietly sharing it.
GLIDESLOPE_TEST(a_socket_gets_the_port_it_asks_for_and_no_two_share_one) {
    auto first = UdpSocket::bound(0);
    check(first.has_value(), "a socket binds to any port");
    const std::uint16_t port = first->port();

    auto same = UdpSocket::bound(port);
    check(!same.has_value(),
          "a second socket on port " + std::to_string(port) + " is refused");

    // Once the first is gone the port can be had, and asking for it by number
    // gives that number back.
    first.reset();
    auto again = UdpSocket::bound(port);
    if (again.has_value()) {
        check(again->port() == port,
              "a socket asked for port " + std::to_string(port) + " got " +
                  std::to_string(again->port()));
    }
}
