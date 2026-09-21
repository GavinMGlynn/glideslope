#pragma once

// UDP, as each system does it: BSD sockets on Linux and macOS, Winsock on
// Windows. One header, two implementations, and nothing above this knows
// which it has.
//
// **Nothing here blocks.** A socket is non-blocking from the moment it is
// made: `receive` answers at once with what was waiting, or with nothing.
// The simulation steps at a fixed rate and cannot afford to wait on a
// datagram that may never come.
//
// **Nothing here throws.** A socket that cannot be made is an empty optional;
// a send or a receive that fails says so and the caller decides. A network is
// a thing that fails, and every caller here has to handle it anyway.
//
// **An address is bytes, not a name.** Nothing here resolves a host name:
// that is a blocking call into the system's resolver, and it belongs where
// waiting is allowed.

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace glideslope::platform {

// Where a datagram came from or is going: an IPv4 or IPv6 address, held as
// the system holds it, and a port. Two addresses are the same when they name
// the same machine and port.
struct Address {
    // 4 bytes for IPv4, 16 for IPv6; `size` says which.
    std::uint8_t bytes[16]{};
    std::uint8_t size = 0; // 4 or 16; 0 for an address that is not one
    std::uint16_t port = 0;

    bool is_v4() const { return size == 4; }
    bool is_v6() const { return size == 16; }
    bool known() const { return size == 4 || size == 16; }
    bool operator==(const Address& other) const;

    // Written as a person reads it: `127.0.0.1:26000`, or `[::1]:26000`.
    std::string text() const;
};

// The loopback address of each family, for a test or a server told to listen
// only to its own machine.
Address loopback_v4(std::uint16_t port);
Address loopback_v6(std::uint16_t port);

// An address written as a person reads it, or nothing if it is not one. This
// parses; it does not resolve, so a host name is refused.
std::optional<Address> address_of(const std::string& text);

// The largest datagram this will send or receive. Chosen to sit inside the
// smallest MTU a path is obliged to carry, so that nothing here is
// fragmented: IPv6's minimum is 1280 bytes, less 40 of IPv6 header and 8 of
// UDP header.
inline constexpr std::size_t largest_datagram = 1232;

class UdpSocket {
public:
    // Binds a socket. `port` of 0 takes whatever the system gives, which
    // `port()` then reports. `v6` binds an IPv6 socket rather than an IPv4
    // one. Returns nothing if the socket could not be made or bound.
    static std::optional<UdpSocket> bound(std::uint16_t port, bool v6 = false);

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;
    ~UdpSocket();

    // Sends one datagram. False if it could not be sent at all; a datagram
    // that is sent may still never arrive, which is UDP's business and not
    // this function's.
    bool send(const Address& to, std::span<const std::uint8_t> datagram);

    // Takes one waiting datagram into `into`, and says how many bytes it was
    // and where it came from. Zero means nothing was waiting - which is the
    // ordinary answer, not an error. A datagram longer than `into` is
    // dropped rather than cut, because half a datagram is not a datagram.
    std::size_t receive(std::span<std::uint8_t> into, Address& from);

    // The port it is bound to.
    std::uint16_t port() const { return port_; }

private:
    UdpSocket() = default;

    // The system's handle, as an integer that both systems can hold. -1 is
    // none.
    std::int64_t handle_ = -1;
    std::uint16_t port_ = 0;
    bool v6_ = false;
};

} // namespace glideslope::platform
