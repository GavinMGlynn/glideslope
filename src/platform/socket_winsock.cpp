// UDP on Windows: Winsock. See src/platform/socket.hpp.
//
// **Winsock has to be started before it can be used**, once per process, and
// stopped as many times as it was started. A counter here does that at the
// first socket and undoes it with the last, so nothing above has to know.

#include "platform/socket.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstring>

namespace glideslope::platform {
namespace {

std::atomic<int> sockets_open{0};

// Starts Winsock for the first socket. False if it cannot be started.
bool winsock_up() {
    if (sockets_open.fetch_add(1) == 0) {
        WSADATA data{};
        if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            sockets_open.fetch_sub(1);
            return false;
        }
    }
    return true;
}

void winsock_down() {
    if (sockets_open.fetch_sub(1) == 1) {
        ::WSACleanup();
    }
}

int to_system(const Address& from, sockaddr_storage& out) {
    std::memset(&out, 0, sizeof(out));
    if (from.is_v4()) {
        auto* in4 = reinterpret_cast<sockaddr_in*>(&out);
        in4->sin_family = AF_INET;
        in4->sin_port = ::htons(from.port);
        std::memcpy(&in4->sin_addr, from.bytes, 4);
        return static_cast<int>(sizeof(sockaddr_in));
    }
    if (from.is_v6()) {
        auto* in6 = reinterpret_cast<sockaddr_in6*>(&out);
        in6->sin6_family = AF_INET6;
        in6->sin6_port = ::htons(from.port);
        std::memcpy(&in6->sin6_addr, from.bytes, 16);
        return static_cast<int>(sizeof(sockaddr_in6));
    }
    return 0;
}

Address from_system(const sockaddr_storage& from) {
    Address out;
    if (from.ss_family == AF_INET) {
        const auto* in4 = reinterpret_cast<const sockaddr_in*>(&from);
        out.size = 4;
        std::memcpy(out.bytes, &in4->sin_addr, 4);
        out.port = ::ntohs(in4->sin_port);
    } else if (from.ss_family == AF_INET6) {
        const auto* in6 = reinterpret_cast<const sockaddr_in6*>(&from);
        out.size = 16;
        std::memcpy(out.bytes, &in6->sin6_addr, 16);
        out.port = ::ntohs(in6->sin6_port);
    }
    return out;
}

} // namespace

std::optional<UdpSocket> UdpSocket::bound(std::uint16_t port, bool v6) {
    if (!winsock_up()) {
        return std::nullopt;
    }
    const SOCKET fd = ::socket(v6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == INVALID_SOCKET) {
        winsock_down();
        return std::nullopt;
    }
    u_long non_blocking = 1;
    if (::ioctlsocket(fd, FIONBIO, &non_blocking) != 0) {
        ::closesocket(fd);
        winsock_down();
        return std::nullopt;
    }

    sockaddr_storage me{};
    int length = 0;
    if (v6) {
        auto* in6 = reinterpret_cast<sockaddr_in6*>(&me);
        in6->sin6_family = AF_INET6;
        in6->sin6_addr = in6addr_any;
        in6->sin6_port = ::htons(port);
        length = static_cast<int>(sizeof(sockaddr_in6));
    } else {
        auto* in4 = reinterpret_cast<sockaddr_in*>(&me);
        in4->sin_family = AF_INET;
        in4->sin_addr.s_addr = ::htonl(INADDR_ANY);
        in4->sin_port = ::htons(port);
        length = static_cast<int>(sizeof(sockaddr_in));
    }
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&me), length) != 0) {
        ::closesocket(fd);
        winsock_down();
        return std::nullopt;
    }

    sockaddr_storage got{};
    int got_length = static_cast<int>(sizeof(got));
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&got), &got_length) != 0) {
        ::closesocket(fd);
        winsock_down();
        return std::nullopt;
    }

    UdpSocket socket;
    socket.handle_ = static_cast<std::int64_t>(fd);
    socket.v6_ = v6;
    socket.port_ = from_system(got).port;
    return socket;
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : handle_(other.handle_), port_(other.port_), v6_(other.v6_) {
    other.handle_ = -1;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        if (handle_ >= 0) {
            ::closesocket(static_cast<SOCKET>(handle_));
            winsock_down();
        }
        handle_ = other.handle_;
        port_ = other.port_;
        v6_ = other.v6_;
        other.handle_ = -1;
    }
    return *this;
}

UdpSocket::~UdpSocket() {
    if (handle_ >= 0) {
        ::closesocket(static_cast<SOCKET>(handle_));
        winsock_down();
    }
}

bool UdpSocket::send(const Address& to, std::span<const std::uint8_t> datagram) {
    if (handle_ < 0 || !to.known() || datagram.size() > largest_datagram) {
        return false;
    }
    sockaddr_storage them{};
    const int length = to_system(to, them);
    if (length == 0) {
        return false;
    }
    const int sent = ::sendto(static_cast<SOCKET>(handle_),
                              reinterpret_cast<const char*>(datagram.data()),
                              static_cast<int>(datagram.size()), 0,
                              reinterpret_cast<const sockaddr*>(&them), length);
    return sent == static_cast<int>(datagram.size());
}

std::size_t UdpSocket::receive(std::span<std::uint8_t> into, Address& from) {
    from = Address{};
    if (handle_ < 0 || into.empty()) {
        return 0;
    }
    sockaddr_storage them{};
    int length = static_cast<int>(sizeof(them));
    const int got = ::recvfrom(static_cast<SOCKET>(handle_),
                               reinterpret_cast<char*>(into.data()),
                               static_cast<int>(into.size()), 0,
                               reinterpret_cast<sockaddr*>(&them), &length);
    if (got == SOCKET_ERROR) {
        // WSAEWOULDBLOCK is nothing waiting, which is the ordinary answer.
        // WSAEMSGSIZE is a datagram too long for the buffer: Winsock has
        // already dropped the rest, and half a datagram is not a datagram.
        return 0;
    }
    if (got <= 0) {
        return 0;
    }
    from = from_system(them);
    return static_cast<std::size_t>(got);
}

} // namespace glideslope::platform
