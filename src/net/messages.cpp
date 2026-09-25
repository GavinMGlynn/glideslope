#include "net/messages.hpp"

#include <cmath>
#include <cstddef>
#include <utility>

namespace glideslope::net {
namespace {

// **A reader that refuses a number that is not one.** Eight bytes on the
// wire can say NaN or infinity as easily as they can say a latitude, and
// nothing above this layer would notice: a NaN position spreads through the
// floating origin and the terrain query, and an infinite duration never
// ends. So the refusal is here, in the one place every floating-point field
// of every message goes through, rather than at each field - where a field
// added later would be a field nobody remembered to check.
//
// It wraps a `Reader` rather than deriving from one, and forwards only what
// the messages here actually read: a field wanting anything else is a
// compile error, to be answered by adding it below on purpose, rather than a
// way round the check that nobody sees. Everything it does forward is
// unchanged, including the rule that a broken reader stays broken and
// answers zero.
class MessageReader {
public:
    explicit MessageReader(std::span<const std::uint8_t> in) : r_(in) {}

    std::uint8_t u8() { return r_.u8(); }
    std::uint64_t u64() { return r_.u64(); }

    double f64() {
        const double v = r_.f64();
        if (!std::isfinite(v)) {
            not_a_number_ = true;
            return 0.0;
        }
        return v;
    }

    std::string text(std::size_t most) { return r_.text(most); }
    std::vector<std::uint8_t> bytes(std::size_t n) { return r_.bytes(n); }

    bool ok() const { return r_.ok() && !not_a_number_; }
    bool done() const { return r_.done() && !not_a_number_; }

private:
    Reader r_;
    bool not_a_number_ = false;
};

// Every message begins with its kind.
Writer begin_message(Message kind) {
    Writer w;
    w.u8(static_cast<std::uint8_t>(kind));
    return w;
}

// A reader over `body`, positioned after the kind byte, or a broken reader
// if the body is not this kind.
MessageReader after_kind(std::span<const std::uint8_t> body, Message kind,
                         bool& is_kind) {
    MessageReader r(body);
    is_kind = r.u8() == static_cast<std::uint8_t>(kind) && r.ok();
    return r;
}

// A count that the protocol will accept. A count beyond `most` breaks the
// reader rather than being trimmed, because a sender saying more than this
// protocol allows is not one to guess at.
std::size_t count(MessageReader& r, std::size_t most, bool& ok) {
    const std::uint8_t n = r.u8();
    if (!r.ok() || n > most) {
        ok = false;
        return 0;
    }
    return n;
}

void write_microbursts(Writer& w, const std::vector<Microburst>& m) {
    w.u8(static_cast<std::uint8_t>(m.size()));
    for (const Microburst& b : m) {
        w.f64(b.latitude_deg);
        w.f64(b.longitude_deg);
        w.f64(b.radius_m);
        w.f64(b.downdraught_mps);
        w.f64(b.start_s);
        w.f64(b.duration_s);
    }
}

bool read_microbursts(MessageReader& r, std::vector<Microburst>& out) {
    bool ok = true;
    const std::size_t n = count(r, most_microbursts, ok);
    if (!ok) {
        return false;
    }
    out.clear();
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        Microburst b;
        b.latitude_deg = r.f64();
        b.longitude_deg = r.f64();
        b.radius_m = r.f64();
        b.downdraught_mps = r.f64();
        b.start_s = r.f64();
        b.duration_s = r.f64();
        out.push_back(b);
    }
    return r.ok();
}

} // namespace

bool known_message(std::uint8_t kind) {
    switch (static_cast<Message>(kind)) {
    case Message::lobby:
    case Message::session:
    case Message::weather:
    case Message::aircraft:
    case Message::terrain_dataset:
    case Message::watch:
    case Message::controller_swap:
    case Message::weather_aloft:
        return true;
    }
    return false;
}

bool known_controller(std::uint8_t controller) {
    switch (static_cast<Controller>(controller)) {
    case Controller::nobody:
    case Controller::person:
    case Controller::ai:
        return true;
    }
    return false;
}

std::optional<Message> kind_of(std::span<const std::uint8_t> body) {
    if (body.empty() || !known_message(body[0])) {
        return std::nullopt;
    }
    return static_cast<Message>(body[0]);
}

// ---- the lobby ----------------------------------------------------------

std::vector<std::uint8_t> write(const Lobby& m) {
    Writer w = begin_message(Message::lobby);
    w.u8(m.players_allowed);
    w.u8(static_cast<std::uint8_t>(m.slots.size()));
    for (const Lobby::Slot& s : m.slots) {
        w.u8(s.index);
        w.u8(static_cast<std::uint8_t>(s.controller));
        w.text(s.name);
    }
    return w.take();
}

bool read(std::span<const std::uint8_t> body, Lobby& out) {
    bool is_kind = false;
    MessageReader r = after_kind(body, Message::lobby, is_kind);
    if (!is_kind) {
        return false;
    }
    Lobby got;
    got.players_allowed = r.u8();
    bool ok = r.ok();
    const std::size_t n = count(r, most_slots, ok);
    if (!ok) {
        return false;
    }
    got.slots.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        Lobby::Slot s;
        s.index = r.u8();
        if (s.index >= most_slots) {
            return false;
        }
        const std::uint8_t controller = r.u8();
        if (!known_controller(controller)) {
            return false;
        }
        s.controller = static_cast<Controller>(controller);
        s.name = r.text(most_name_bytes);
        got.slots.push_back(std::move(s));
    }
    // A player count this protocol does not allow is not a lobby.
    if (!r.done() || got.players_allowed < 1 || got.players_allowed > most_slots) {
        return false;
    }
    out = std::move(got);
    return true;
}

// ---- the session --------------------------------------------------------

std::vector<std::uint8_t> write(const Session& m) {
    Writer w = begin_message(Message::session);
    w.u64(m.id);
    w.text(m.name);
    w.u64(m.began_unix_ms);
    w.f64(m.simulation_time_s);
    return w.take();
}

bool read(std::span<const std::uint8_t> body, Session& out) {
    bool is_kind = false;
    MessageReader r = after_kind(body, Message::session, is_kind);
    if (!is_kind) {
        return false;
    }
    Session got;
    got.id = r.u64();
    got.name = r.text(most_name_bytes);
    got.began_unix_ms = r.u64();
    got.simulation_time_s = r.f64();
    if (!r.done()) {
        return false;
    }
    out = std::move(got);
    return true;
}

// ---- the weather --------------------------------------------------------

std::vector<std::uint8_t> write(const Weather& m) {
    Writer w = begin_message(Message::weather);
    w.text(m.metar);
    w.f64(m.latitude_deg);
    w.f64(m.longitude_deg);
    w.f64(m.elevation_m);
    w.u8(m.turbulence_severity ? 1u : 0u);
    w.u8(m.turbulence_severity ? *m.turbulence_severity : 0u);
    w.u64(m.air_seed);
    write_microbursts(w, m.microbursts);
    return w.take();
}

bool read(std::span<const std::uint8_t> body, Weather& out) {
    bool is_kind = false;
    MessageReader r = after_kind(body, Message::weather, is_kind);
    if (!is_kind) {
        return false;
    }
    Weather got;
    got.metar = r.text(most_metar_bytes);
    got.latitude_deg = r.f64();
    got.longitude_deg = r.f64();
    got.elevation_m = r.f64();
    const std::uint8_t has_turbulence = r.u8();
    const std::uint8_t severity = r.u8();
    // **The severity byte is held to nought when the flag says there is
    // none**, because a byte nobody reads is a byte that can carry anything.
    if (!r.ok() || has_turbulence > 1 || severity > 7 ||
        (has_turbulence == 0 && severity != 0)) {
        return false;
    }
    if (has_turbulence == 1) {
        got.turbulence_severity = severity;
    }
    got.air_seed = r.u64();
    if (!read_microbursts(r, got.microbursts) || !r.done()) {
        return false;
    }
    out = std::move(got);
    return true;
}

// ---- the forecast above the station -------------------------------------

std::vector<std::uint8_t> write(const WeatherAloft& m) {
    Writer w = begin_message(Message::weather_aloft);
    w.text(m.time);
    w.u8(static_cast<std::uint8_t>(m.levels.size()));
    for (const AloftLevel& l : m.levels) {
        w.f64(l.pressure_hpa);
        w.f64(l.height_m);
        w.f64(l.wind_north_mps);
        w.f64(l.wind_east_mps);
        w.f64(l.temperature_c);
    }
    w.u8(static_cast<std::uint8_t>(m.near_ground.size()));
    for (const NearGroundWind& g : m.near_ground) {
        w.f64(g.height_m);
        w.f64(g.wind_north_mps);
        w.f64(g.wind_east_mps);
    }
    return w.take();
}

bool read(std::span<const std::uint8_t> body, WeatherAloft& out) {
    bool is_kind = false;
    MessageReader r = after_kind(body, Message::weather_aloft, is_kind);
    if (!is_kind) {
        return false;
    }
    WeatherAloft got;
    got.time = r.text(most_time_bytes);
    bool ok = r.ok();
    const std::size_t levels = count(r, most_levels, ok);
    if (!ok) {
        return false;
    }
    got.levels.reserve(levels);
    for (std::size_t i = 0; i < levels; ++i) {
        AloftLevel l;
        l.pressure_hpa = r.f64();
        l.height_m = r.f64();
        l.wind_north_mps = r.f64();
        l.wind_east_mps = r.f64();
        l.temperature_c = r.f64();
        got.levels.push_back(l);
    }
    const std::size_t near = count(r, most_near_ground, ok);
    if (!ok) {
        return false;
    }
    got.near_ground.reserve(near);
    for (std::size_t i = 0; i < near; ++i) {
        NearGroundWind g;
        g.height_m = r.f64();
        g.wind_north_mps = r.f64();
        g.wind_east_mps = r.f64();
        got.near_ground.push_back(g);
    }
    if (!r.done()) {
        return false;
    }
    out = std::move(got);
    return true;
}

// ---- an aircraft's definition -------------------------------------------

std::vector<std::uint8_t> write(const AircraftDefinition& m) {
    Writer w = begin_message(Message::aircraft);
    w.u8(m.aircraft);
    w.text(m.id);
    w.text(m.model);
    return w.take();
}

bool read(std::span<const std::uint8_t> body, AircraftDefinition& out) {
    bool is_kind = false;
    MessageReader r = after_kind(body, Message::aircraft, is_kind);
    if (!is_kind) {
        return false;
    }
    AircraftDefinition got;
    got.aircraft = r.u8();
    if (!r.ok()) {
        return false;
    }
    got.id = r.text(most_name_bytes);
    got.model = r.text(most_name_bytes);
    if (!r.done()) {
        return false;
    }
    out = std::move(got);
    return true;
}

// ---- the terrain dataset ------------------------------------------------

std::vector<std::uint8_t> write(const TerrainDataset& m) {
    Writer w = begin_message(Message::terrain_dataset);
    w.text(m.name);
    w.text(m.version);
    w.bytes(m.sha256);
    return w.take();
}

bool read(std::span<const std::uint8_t> body, TerrainDataset& out) {
    bool is_kind = false;
    MessageReader r = after_kind(body, Message::terrain_dataset, is_kind);
    if (!is_kind) {
        return false;
    }
    TerrainDataset got;
    got.name = r.text(most_name_bytes);
    got.version = r.text(most_name_bytes);
    got.sha256 = r.bytes(sha256_bytes);
    if (!r.done()) {
        return false;
    }
    out = std::move(got);
    return true;
}

// ---- which aircraft is watched -------------------------------------------

std::vector<std::uint8_t> write(const Watch& m) {
    Writer w = begin_message(Message::watch);
    w.u8(m.aircraft);
    return w.take();
}

bool read(std::span<const std::uint8_t> body, Watch& out) {
    bool is_kind = false;
    MessageReader r = after_kind(body, Message::watch, is_kind);
    if (!is_kind) {
        return false;
    }
    Watch got;
    got.aircraft = r.u8();
    if (!r.done()) {
        return false;
    }
    out = got;
    return true;
}

// ---- a controller swap --------------------------------------------------

std::vector<std::uint8_t> write(const ControllerSwap& m) {
    Writer w = begin_message(Message::controller_swap);
    w.u8(m.aircraft);
    w.u8(static_cast<std::uint8_t>(m.to));
    w.f64(m.at_simulation_time_s);
    return w.take();
}

bool read(std::span<const std::uint8_t> body, ControllerSwap& out) {
    bool is_kind = false;
    MessageReader r = after_kind(body, Message::controller_swap, is_kind);
    if (!is_kind) {
        return false;
    }
    ControllerSwap got;
    got.aircraft = r.u8();
    if (!r.ok()) {
        return false;
    }
    const std::uint8_t to = r.u8();
    if (!r.ok() || !known_controller(to)) {
        return false;
    }
    got.to = static_cast<Controller>(to);
    got.at_simulation_time_s = r.f64();
    if (!r.done()) {
        return false;
    }
    out = std::move(got);
    return true;
}

} // namespace glideslope::net
