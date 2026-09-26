#include "net/inside.hpp"

namespace glideslope::net {

bool known_inside(std::uint8_t kind) {
    switch (static_cast<Inside>(kind)) {
    case Inside::reliable:
    case Inside::inputs:
    case Inside::state:
    case Inside::ping:
    case Inside::pong:
    case Inside::leaving:
        return true;
    }
    return false;
}

bool is_leaving(std::span<const std::uint8_t> body) {
    return body.size() == 1 && body[0] == static_cast<std::uint8_t>(Inside::leaving);
}

std::vector<std::uint8_t> knock(Inside kind, std::uint64_t token) {
    std::vector<std::uint8_t> out;
    out.reserve(knock_size);
    out.push_back(static_cast<std::uint8_t>(kind));
    for (std::size_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((token >> (i * 8)) & 0xFFu));
    }
    return out;
}

std::optional<std::uint64_t> knock_token(Inside kind,
                                         std::span<const std::uint8_t> body) {
    if (body.size() != knock_size || body[0] != static_cast<std::uint8_t>(kind)) {
        return std::nullopt;
    }
    std::uint64_t token = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        token |= static_cast<std::uint64_t>(body[i + 1]) << (i * 8);
    }
    return token;
}

} // namespace glideslope::net
