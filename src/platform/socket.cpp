// The parts of an address that are the same on every system: what one looks
// like written down, and how to read one back. No system headers here, so
// that both implementations of the socket share exactly this.

#include "platform/socket.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

namespace glideslope::platform {
namespace {

// A decimal number from `text` at `at`, of at most `digits` digits, moving
// `at` past it. Returns false for no digits, too many, or a leading zero on
// a longer number - "01" is not how a number is written.
bool decimal(const std::string& text, std::size_t& at, int digits, unsigned& out) {
    const std::size_t start = at;
    unsigned value = 0;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
        value = value * 10 + static_cast<unsigned>(text[at] - '0');
        ++at;
        if (static_cast<int>(at - start) > digits) {
            return false;
        }
    }
    if (at == start) {
        return false;
    }
    if (text[start] == '0' && at - start > 1) {
        return false;
    }
    out = value;
    return true;
}

// Four dotted decimals into four bytes.
bool v4_of(const std::string& text, std::uint8_t* out) {
    std::size_t at = 0;
    for (int i = 0; i < 4; ++i) {
        if (i > 0) {
            if (at >= text.size() || text[at] != '.') {
                return false;
            }
            ++at;
        }
        unsigned part = 0;
        if (!decimal(text, at, 3, part) || part > 255) {
            return false;
        }
        out[i] = static_cast<std::uint8_t>(part);
    }
    return at == text.size();
}

// An IPv6 address into sixteen bytes, with "::" standing for as many zero
// groups as are missing.
bool v6_of(const std::string& text, std::uint8_t* out) {
    std::vector<std::uint16_t> before;
    std::vector<std::uint16_t> after;
    bool seen_gap = false;
    std::size_t at = 0;
    std::vector<std::uint16_t>* into = &before;

    if (text.size() >= 2 && text[0] == ':' && text[1] == ':') {
        seen_gap = true;
        into = &after;
        at = 2;
    } else if (!text.empty() && text[0] == ':') {
        return false; // a single leading colon is not a gap
    }

    while (at < text.size()) {
        // A group of one to four hexadecimal digits.
        std::uint32_t group = 0;
        const std::size_t start = at;
        while (at < text.size() && std::isxdigit(static_cast<unsigned char>(text[at])) != 0) {
            const char c = text[at];
            const unsigned digit = c <= '9' ? static_cast<unsigned>(c - '0')
                                            : static_cast<unsigned>((c | 0x20) - 'a' + 10);
            group = group * 16 + digit;
            ++at;
            if (at - start > 4) {
                return false;
            }
        }
        if (at == start) {
            return false;
        }
        into->push_back(static_cast<std::uint16_t>(group));
        if (at == text.size()) {
            break;
        }
        if (text[at] != ':') {
            return false;
        }
        ++at;
        if (at < text.size() && text[at] == ':') {
            if (seen_gap) {
                return false; // only one gap
            }
            seen_gap = true;
            into = &after;
            ++at;
            if (at == text.size()) {
                break;
            }
        } else if (at == text.size()) {
            return false; // a trailing single colon
        }
    }

    const std::size_t groups = before.size() + after.size();
    if (seen_gap ? groups > 7 : groups != 8) {
        return false;
    }
    // **The bound the writes below depend on, said where they are.** Every
    // group is two bytes and there are sixteen to fill, so neither half may
    // hold more than eight: `byte = 16 - after.size() * 2` underflows a
    // `size_t` if it does, and the loop after it writes off the end of a
    // caller's address. The guard above already makes that unreachable - a
    // gap allows at most seven groups and no gap means exactly eight, all of
    // them in `before` - but it says so through a ternary several lines up,
    // which is a long way to carry an invariant that costs one line to state.
    // GCC at -O2 cannot follow it either, and says so.
    if (before.size() > 8 || after.size() > 8) {
        return false;
    }
    std::memset(out, 0, 16);
    std::size_t byte = 0;
    for (const std::uint16_t group : before) {
        out[byte++] = static_cast<std::uint8_t>(group >> 8);
        out[byte++] = static_cast<std::uint8_t>(group & 0xFFu);
    }
    byte = 16 - after.size() * 2;
    for (const std::uint16_t group : after) {
        out[byte++] = static_cast<std::uint8_t>(group >> 8);
        out[byte++] = static_cast<std::uint8_t>(group & 0xFFu);
    }
    return true;
}

} // namespace

bool Address::operator==(const Address& other) const {
    return size == other.size && port == other.port &&
           std::equal(bytes, bytes + size, other.bytes);
}

std::string Address::text() const {
    char buffer[64];
    if (is_v4()) {
        std::snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u:%u", bytes[0], bytes[1],
                      bytes[2], bytes[3], port);
        return buffer;
    }
    if (is_v6()) {
        // Every group, written out. The "::" shorthand is not used: a
        // shorthand is for reading, and this is for saying exactly what the
        // address is.
        std::string out = "[";
        for (int i = 0; i < 8; ++i) {
            char group[8];
            std::snprintf(group, sizeof(group), "%x",
                          (static_cast<unsigned>(bytes[i * 2]) << 8) |
                              static_cast<unsigned>(bytes[i * 2 + 1]));
            out += (i > 0 ? ":" : "") + std::string(group);
        }
        std::snprintf(buffer, sizeof(buffer), "]:%u", port);
        return out + buffer;
    }
    return "(no address)";
}

Address loopback_v4(std::uint16_t port) {
    Address out;
    out.size = 4;
    out.bytes[0] = 127;
    out.bytes[3] = 1;
    out.port = port;
    return out;
}

Address loopback_v6(std::uint16_t port) {
    Address out;
    out.size = 16;
    out.bytes[15] = 1;
    out.port = port;
    return out;
}

std::optional<Address> address_of(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }
    Address out;
    std::string host;
    std::string port;
    if (text[0] == '[') {
        const std::size_t close = text.find(']');
        if (close == std::string::npos || close + 1 >= text.size() ||
            text[close + 1] != ':') {
            return std::nullopt;
        }
        host = text.substr(1, close - 1);
        port = text.substr(close + 2);
        if (!v6_of(host, out.bytes)) {
            return std::nullopt;
        }
        out.size = 16;
    } else {
        const std::size_t colon = text.rfind(':');
        if (colon == std::string::npos) {
            return std::nullopt;
        }
        host = text.substr(0, colon);
        port = text.substr(colon + 1);
        if (!v4_of(host, out.bytes)) {
            return std::nullopt;
        }
        out.size = 4;
    }
    std::size_t at = 0;
    unsigned number = 0;
    if (!decimal(port, at, 5, number) || at != port.size() || number > 65535) {
        return std::nullopt;
    }
    out.port = static_cast<std::uint16_t>(number);
    return out;
}

} // namespace glideslope::platform
