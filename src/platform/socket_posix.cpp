// UDP on Linux and macOS: BSD sockets. See src/platform/socket.hpp.

#include "platform/socket.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace glideslope::platform {
namespace {

// The address as the system wants it. Returns the length used, or 0.
socklen_t to_system(const Address& from, sockaddr_storage& out) {
    std::memset(&out, 0, sizeof(out));
    if (from.is_v4()) {
        auto* in4 = reinterpret_cast<sockaddr_in*>(&out);
        in4->sin_family = AF_INET;
        in4->sin_port = htons(from.port);
        std::memcpy(&in4->sin_addr, from.bytes, 4);
        return static_cast<socklen_t>(sizeof(sockaddr_in));
    }
    if (from.is_v6()) {
        auto* in6 = reinterpret_cast<sockaddr_in6*>(&out);
        in6->sin6_family = AF_INET6;
        in6->sin6_port = htons(from.port);
        std::memcpy(&in6->sin6_addr, from.bytes, 16);
        return static_cast<socklen_t>(sizeof(sockaddr_in6));
    }
    return 0;
}

Address from_system(const sockaddr_storage& from) {
    Address out;
    if (from.ss_family == AF_INET) {
        const auto* in4 = reinterpret_cast<const sockaddr_in*>(&from);
        out.size = 4;
        std::memcpy(out.bytes, &in4->sin_addr, 4);
        out.port = ntohs(in4->sin_port);
    } else if (from.ss_family == AF_INET6) {
        const auto* in6 = reinterpret_cast<const sockaddr_in6*>(&from);
        out.size = 16;
        std::memcpy(out.bytes, &in6->sin6_addr, 16);
        out.port = ntohs(in6->sin6_port);
    }
    return out;
}

} // namespace

std::optional<UdpSocket> UdpSocket::bound(std::uint16_t port, bool v6) {
    const int fd = ::socket(v6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return std::nullopt;
    }
    // Non-blocking from the start: nothing above this may wait on a datagram.
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(fd);
        return std::nullopt;
    }

    sockaddr_storage me{};
    socklen_t length = 0;
    if (v6) {
        auto* in6 = reinterpret_cast<sockaddr_in6*>(&me);
        in6->sin6_family = AF_INET6;
        in6->sin6_addr = in6addr_any;
        in6->sin6_port = htons(port);
        length = static_cast<socklen_t>(sizeof(sockaddr_in6));
    } else {
        auto* in4 = reinterpret_cast<sockaddr_in*>(&me);
        in4->sin_family = AF_INET;
        in4->sin_addr.s_addr = htonl(INADDR_ANY);
        in4->sin_port = htons(port);
        length = static_cast<socklen_t>(sizeof(sockaddr_in));
    }
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&me), length) < 0) {
        ::close(fd);
        return std::nullopt;
    }

    // What port it actually got, which matters when it was asked for any.
    sockaddr_storage got{};
    socklen_t got_length = static_cast<socklen_t>(sizeof(got));
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&got), &got_length) < 0) {
        ::close(fd);
        return std::nullopt;
    }

    UdpSocket socket;
    socket.handle_ = fd;
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
            ::close(static_cast<int>(handle_));
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
        ::close(static_cast<int>(handle_));
    }
}

bool UdpSocket::send(const Address& to, std::span<const std::uint8_t> datagram) {
    if (handle_ < 0 || !to.known() || datagram.size() > largest_datagram) {
        return false;
    }
    sockaddr_storage them{};
    const socklen_t length = to_system(to, them);
    if (length == 0) {
        return false;
    }
    const ssize_t sent =
        ::sendto(static_cast<int>(handle_), datagram.data(), datagram.size(), 0,
                 reinterpret_cast<const sockaddr*>(&them), length);
    return sent == static_cast<ssize_t>(datagram.size());
}

std::size_t UdpSocket::receive(std::span<std::uint8_t> into, Address& from) {
    from = Address{};
    if (handle_ < 0 || into.empty()) {
        return 0;
    }
    sockaddr_storage them{};
    socklen_t length = static_cast<socklen_t>(sizeof(them));
    // MSG_TRUNC makes the answer the datagram's real length even when it did
    // not fit, so a long one can be told from one that just fitted and
    // dropped rather than cut. macOS has no MSG_TRUNC for datagrams; there a
    // datagram that exactly fills the buffer is indistinguishable from one
    // that was cut, which is why callers are given a buffer of
    // largest_datagram and anything longer is not ours anyway.
#ifdef MSG_TRUNC
    constexpr int flags = MSG_TRUNC;
#else
    constexpr int flags = 0;
#endif
    const ssize_t got =
        ::recvfrom(static_cast<int>(handle_), into.data(), into.size(), flags,
                   reinterpret_cast<sockaddr*>(&them), &length);
    if (got <= 0) {
        return 0; // nothing waiting, or a datagram of no bytes, which is nothing
    }
    if (static_cast<std::size_t>(got) > into.size()) {
        return 0; // too long to be ours; half a datagram is not a datagram
    }
    from = from_system(them);
    return static_cast<std::size_t>(got);
}

} // namespace glideslope::platform
