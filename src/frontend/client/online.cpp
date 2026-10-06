#include "online.hpp"

#include "frontend/same_air.hpp"
#include "platform/paths.hpp"

#include "sim/fixed_step.hpp"
#include "world/geodesy.hpp"

#include <algorithm>
#include <cmath>
#include <span>

namespace glideslope::client {
namespace {

constexpr double inputs_every_s = 1.0 / 30.0;
constexpr double radians = 3.14159265358979323846 / 180.0;

// North-east-down about `origin`: a position less the origin, or a velocity
// as it is.
void to_local(const world::Ecef& origin, double x, double y, double z, bool position,
              double& n, double& e, double& d) {
    const world::Geodetic g = world::to_geodetic(origin);
    const double lat = g.latitude_deg * radians;
    const double lon = g.longitude_deg * radians;
    if (position) {
        x -= origin.x;
        y -= origin.y;
        z -= origin.z;
    }
    n = -std::sin(lat) * std::cos(lon) * x - std::sin(lat) * std::sin(lon) * y +
        std::cos(lat) * z;
    e = -std::sin(lon) * x + std::cos(lon) * y;
    d = -std::cos(lat) * std::cos(lon) * x - std::cos(lat) * std::sin(lon) * y -
        std::sin(lat) * z;
}

world::Ecef from_local(const world::Ecef& origin, double n, double e, double d) {
    const world::Geodetic g = world::to_geodetic(origin);
    const double lat = g.latitude_deg * radians;
    const double lon = g.longitude_deg * radians;
    return {origin.x - std::sin(lat) * std::cos(lon) * n - std::sin(lon) * e -
                std::cos(lat) * std::cos(lon) * d,
            origin.y - std::sin(lat) * std::sin(lon) * n + std::cos(lon) * e -
                std::cos(lat) * std::sin(lon) * d,
            origin.z + std::cos(lat) * n - std::sin(lat) * d};
}

} // namespace

sim::Motion motion_of(const net::OwnMotion& y) {
    sim::Motion m;
    m.location_ecef_m = {y.x_m, y.y_m, y.z_m};
    for (std::size_t i = 0; i < 4; ++i) {
        m.attitude_local[i] = static_cast<double>(y.attitude[i]);
    }
    for (std::size_t i = 0; i < 3; ++i) {
        m.uvw_mps[i] = static_cast<double>(y.uvw_mps[i]);
        m.pqr_radps[i] = static_cast<double>(y.pqr_radps[i]);
    }
    return m;
}

Online::Online(net::ClientSession session, std::function<double()> local_s)
    : session_(std::move(session)), local_s_(std::move(local_s)) {
    polled_s_ = local_s_();
    keeper_ = std::thread([this] { keep_while_away(); });
}

Online::~Online() {
    {
        const auto lock = held();
        stop_ = true;
    }
    stopping_.notify_all();
    keeper_.join();
}

void Online::keep_while_away() {
    auto lock = held();
    while (!stopping_.wait_for(lock, std::chrono::milliseconds(20),
                               [this] { return stop_.load(); })) {
        const double now = local_s_();
        if (now - polled_s_ < kept_after_s) {
            continue;
        }
        // **Kept, and nothing more**: the updates left waiting for the
        // frame loop - the newest five seconds of them, which is what the
        // session holds (net::ClientSession::most_states_kept) - heard in
        // order when it is back.
        session_.poll(now);
        ++times_kept_;
        longest_kept_s_ = std::max(longest_kept_s_, now - polled_s_);
    }
}

double Online::longest_kept_since_asked_s() {
    const auto lock = held();
    const double out = longest_kept_s_;
    longest_kept_s_ = 0.0;
    return out;
}

std::size_t Online::times_kept() const {
    const auto lock = held();
    return times_kept_;
}

void Online::poll_here(double local_s) {
    session_.poll(local_s);
    polled_s_ = local_s;
    noticed();
}

void Online::idle(double local_s) {
    const auto lock = held();
    poll_here(local_s);
    (void)session_.take_states();
}

void Online::leave() {
    const auto lock = held();
    session_.leave();
}

Standing Online::standing() const {
    const auto lock = held();
    return {session_.standing(), session_.let_go(), session_.quiet_when_let_go_s(),
            session_.went_back()};
}

void Online::stall() {
    const auto lock = held();
    session_.stall_until_let_go();
}

std::optional<Joined> Online::joined_by(const net::StatePacket& state) const {
    if (!state.yours || state.your_aircraft == net::no_aircraft) {
        return std::nullopt;
    }
    const auto found = session_.roster().find(state.your_aircraft);
    if (found == session_.roster().end()) {
        return std::nullopt;
    }
    Joined joined;
    joined.number = state.your_aircraft;
    joined.aircraft_id = found->second.id;
    joined.motion = motion_of(*state.yours);
    joined.server_steps = static_cast<std::uint64_t>(std::llround(
        state.simulation_time_s * static_cast<double>(sim::steps_per_second)));
    return joined;
}

sim::Controls Online::fly(double local_s, const sim::Controls& stick, Flight& flight) {
    const auto lock = held();
    // Anything the keeper heard of the session's standing - joined again -
    // is taken in before an input goes into it.
    noticed();
    if (clock_.known()) {
        session_now_s_ = clock_.now(local_s);
    }
    // **What is sent is what is flown**: rounded as the wire rounds it, so
    // that this client and the server fly the same numbers.
    if (local_s - sent_at_s_ >= inputs_every_s) {
        sent_at_s_ = local_s;
        ++sequence_;
        const net::ControlList sent = net::as_sent(stick.as_list());
        sent_ = sim::Controls::from_list(sent);
        sending_.add(sequence_, sent);
        const std::vector<std::uint8_t> packet = sending_.packet();
        session_.send_inputs(std::span<const std::uint8_t>(packet.data(), packet.size()));
        // The first, with nothing flown before it, is flown at once.
        if (sequence_ == 1) {
            flown(flight);
        }
    }
    return flying_;
}

void Online::flown(Flight& flight) {
    // Not behind the lock: `fly` calls it under it, and nothing here is the
    // keeper's.
    flying_ = sent_;
    flight.set_input_sequence(sequence_);
}

void Online::listen(double local_s) {
    const auto lock = held();
    poll_here(local_s);
    for (net::StatePacket& state : session_.take_states()) {
        arrived_.push_back(std::move(state));
    }
    arrived_s_ = local_s;
    arrived_in_ = session_.joined_again();
}

void Online::hear(Flight& flight) {
    {
        const auto lock = held();
        // **Joined again while the ticks were flown** - the keeper heard it -
        // what `listen` read is the old session's, and heard now as the new
        // one's it would name the old aircraft as this client's own, and set
        // the newest word's time from a restarted server's old clock, so
        // that every word of the new one was ignored as older.
        if (session_.joined_again() != arrived_in_) {
            arrived_.clear();
        }
        noticed();
        for (const net::StatePacket& state : arrived_) {
            heard(state, arrived_s_, flight);
        }
        arrived_.clear();
        // **For a test: the last update from before a take-over, heard again
        // just after it**, as the network reorders them - built, not waited
        // for (`--late-update-after-take-over`, as the command-line client's).
        if (hear_again_) {
            const net::StatePacket late = std::move(*hear_again_);
            hear_again_.reset();
            std::printf("glideslope: an update from before the take-over is heard again "
                        "after it\n");
            std::fflush(stdout);
            heard(late, arrived_s_, flight);
        }
        // **What the server says the session is** (REQUIREMENTS.md 6.3): the
        // ground it collides on, held once against this client's, and each
        // weather it flies, flown here in place of any of this client's own.
        const net::Told& told = session_.told();
        // **A session joined again is told afresh**, and what the old one
        // said is not taken for the new one's: each weather is counted with
        // the session it came in, and the ground compared again.
        if (session_.joined_again() != told_in_) {
            told_in_ = session_.joined_again();
            ground_compared_ = false;
        }
        if (!told.dataset()) {
            ground_compared_ = false;
        } else if (!ground_compared_) {
            ground_compared_ = true;
            const net::TerrainDataset ours =
                frontend::collision_dataset(platform::data_directory());
            if (!frontend::same_ground(*told.dataset(), ours)) {
                other_ground_ = "the server collides on " + frontend::describe(*told.dataset()) +
                                ", and this client's is " + frontend::describe(ours);
            } else {
                std::printf("glideslope: told the collision ground: %s, this client's too\n",
                            frontend::describe(ours).c_str());
                std::fflush(stdout);
            }
        }
        // Each flight counts its own: one made since - another aeroplane
        // taken over - is given the newest. **Not on other ground, nor before
        // the ground is known**: the reliable layer delivers in order and
        // the ground is said first, so a weather is always heard after it.
        const std::int64_t key = (static_cast<std::int64_t>(told_in_) << 32) |
                                 static_cast<std::int64_t>(told.weathers());
        if (told.weather() && ground_compared_ && other_ground_.empty() &&
            key != flight.server_weathers()) {
            std::string why;
            if (flight.heard_weather(*told.weather(), told.aloft(),
                                     [this] { return session_now_s_; }, key, why)) {
                weathers_flown_ = told.weathers();
                std::printf("glideslope: flying the server's weather (%d): %s\n", weathers_flown_,
                            told.weather()->metar.empty() ? "still air"
                                                           : told.weather()->metar.c_str());
            } else {
                std::printf("glideslope: refused the server's weather, and flies the last: %s\n",
                            why.c_str());
            }
            std::fflush(stdout);
        }
    }
    // **Put right with the session let go**: a correction replays the
    // inputs since over the collision DEM, which may fetch a tile, and a
    // replay in a sanitized build is long by itself. Held across them, the
    // lock kept the keeper waiting on it, and the session silent - what the
    // keeper is for. What follows is the frame loop's alone.
    if (!own_word_) {
        return;
    }
    const OwnWord word = *own_word_;
    own_word_.reset();
    if (word.adopt) {
        flight.adopt(word.motion);
        return;
    }
    const bool known = flight.clocks_known();
    const auto c =
        flight.reconcile(word.motion, word.last_applied, word.steps_into, word.server_steps,
                         word.engine_stopped);
    double& worst = known ? worst_known_m_ : worst_learning_m_;
    worst = std::max(worst, c.moved_m);
    ++corrections_;
    ++frames_heard_own_;
    worst_correction_m_ = std::max(worst_correction_m_, c.moved_m);
    if (c.snapped) {
        ++snapped_;
    } else {
        corrected_ = true;
    }
}

void Online::noticed() {
    // **Gone back to the old session**: nothing to start again - the same
    // keys, the same aircraft, the server's count of this client's inputs
    // where it was - only the input sent last, for `flown_since_going_back`.
    if (session_.went_back() != went_back_seen_) {
        went_back_seen_ = session_.went_back();
        back_at_ = sequence_;
    }
    if (session_.joined_again() == joined_again_seen_) {
        return;
    }
    joined_again_seen_ = session_.joined_again();
    rejoining_ = true;
    mine_ = net::no_aircraft;
    taken_.reset();
    taken_at_.reset();
    applied_ = 0;
    own_ai_flying_ = false;
    resuming_ = false;
    // The server forgot what the old session rode along in.
    watching_ = net::no_aircraft;
    watched_.clear();
    // **And its clock may have started again**: a server restarted with the
    // same key (`--store`) lets every session go, and counts simulated time
    // from nought. Nothing old is kept to compare with - no update of the
    // old session can open under the new one's keys to be reordered past
    // it - so the clock, the newest word and everything drawn start afresh.
    old_session_s_ = newest_heard_s_;
    newest_heard_s_.reset();
    reconciled_s_.reset();
    own_word_.reset();
    clock_ = net::SessionClock{};
    origin_.reset();
    shown_.clear();
    wrecked_.clear();
    ai_.clear();
}

void Online::heard(const net::StatePacket& state, double local_s, Flight& flight) {
    // The last update naming the aircraft that is its own, kept for a test
    // that hears it again after a take-over.
    if (late_after_take_over_ && state.yours && mine_ != net::no_aircraft &&
        state.your_aircraft == mine_ &&
        (!reconciled_s_ || state.simulation_time_s > *reconciled_s_)) {
        before_take_over_ = state;
    }
    clock_.heard(state.simulation_time_s, local_s);
    if (!newest_heard_s_ || state.simulation_time_s > *newest_heard_s_) {
        newest_heard_s_ = state.simulation_time_s;
    }
    // Its own aircraft as this update has it, for its copilot to be told.
    if (!own_heard_ || state.simulation_time_s > own_heard_->first) {
        for (const net::AircraftState& a : state.aircraft) {
            if (a.index == mine_) {
                own_heard_ = std::pair{state.simulation_time_s, a};
            }
        }
    }
    // The watched aircraft's controls, by the time they were true.
    if (state.watched && state.watched->aircraft == watching_) {
        watched_[state.simulation_time_s] = *state.watched;
        ++watched_heard_;
        while (watched_.size() > 64) {
            watched_.erase(watched_.begin());
        }
    }
    // **Joined again, its own is whatever the server now says it is**, the
    // old number or another: made by the caller from this update's motion,
    // as a take-over is. Until the server has said, nothing is put right.
    if (rejoining_ && state.yours && state.your_aircraft != net::no_aircraft &&
        (!reconciled_s_ || state.simulation_time_s > *reconciled_s_)) {
        if (auto joined = joined_by(state)) {
            rejoining_ = false;
            mine_ = state.your_aircraft;
            joined->again = true;
            taken_ = std::move(joined);
            taken_at_ = sequence_;
            taken_back_ = false;
            rejoined_ = true;
            reconciled_s_ = state.simulation_time_s;
            own_word_.reset();
            for (const net::AircraftState& a : state.aircraft) {
                if (a.index == mine_) {
                    own_ai_flying_ = a.controller == net::Controller::ai;
                }
            }
            return;
        }
    }
    // **Another aircraft is this client's own now**: one it took over. The
    // flight is not put right by it - it is another aircraft - but made it,
    // by the caller, from this update's motion. Only by the newest word: one
    // the network held back from before the take-over names the aircraft
    // given up, and would take that over again (the command-line client did,
    // through 200 ms of jitter, 2026-09-26).
    if (state.yours && mine_ != net::no_aircraft && state.your_aircraft != mine_ &&
        state.your_aircraft != net::no_aircraft &&
        (!reconciled_s_ || state.simulation_time_s > *reconciled_s_)) {
        if (auto joined = joined_by(state)) {
            mine_ = state.your_aircraft;
            taken_ = std::move(joined);
            taken_at_ = sequence_;
            taken_back_ = false;
            rejoined_ = false;
            resuming_ = false;
            reconciled_s_ = state.simulation_time_s;
            own_word_.reset();
            send_watch(net::no_aircraft);
            if (late_after_take_over_ && before_take_over_) {
                hear_again_ = std::move(before_take_over_);
                before_take_over_.reset();
            }
            for (const net::AircraftState& a : state.aircraft) {
                if (a.index == mine_) {
                    own_ai_flying_ = a.controller == net::Controller::ai;
                }
            }
            return;
        }
    }
    // **Its own, from the newest word only**: an update older than one
    // already used would put it back to where the newer one had moved it
    // from, with the inputs since already let go.
    // **A take-over or a join not yet taken up by the caller takes the newest
    // word** - the flight is still the aircraft left behind, and put right by
    // the one taken it snapped by the distance between them, 433 m in a
    // pass of 250 ms (PROJECT_STATUS.md, 2026-09-30); and the caller builds
    // it from the motion given here, which the newest word says best.
    if (taken_ && state.yours && state.your_aircraft == mine_ &&
        state.simulation_time_s > *reconciled_s_) {
        if (auto newer = joined_by(state)) {
            newer->again = taken_->again;
            taken_ = std::move(newer);
            reconciled_s_ = state.simulation_time_s;
            own_word_.reset();
            // Who flies it, as the word that gave it was read; and what it
            // says of the clocks' difference, which is the connection's and
            // not the aircraft's, as every older word of a frame's does.
            for (const net::AircraftState& a : state.aircraft) {
                if (a.index == mine_) {
                    own_ai_flying_ = a.controller == net::Controller::ai;
                }
            }
            flight.hear_clock(state.last_input_applied, state.yours->steps_into_input,
                              static_cast<std::uint64_t>(std::llround(
                                  state.simulation_time_s *
                                  static_cast<double>(sim::steps_per_second))));
        }
    } else if (state.yours && state.your_aircraft == mine_ &&
        (!reconciled_s_ || state.simulation_time_s > *reconciled_s_)) {
        // **Who flies it**, by the same newest word: handed to the AI, it is
        // no longer predicted; taken back, it is put where this update says
        // and predicted from there, as at a take-over.
        for (const net::AircraftState& a : state.aircraft) {
            if (a.index != mine_) {
                continue;
            }
            const bool ai = a.controller == net::Controller::ai;
            if (ai != own_ai_flying_) {
                own_ai_flying_ = ai;
                switched_ = true;
                if (!ai) {
                    resuming_ = true;
                    taken_back_ = true;
                    rejoined_ = false;
                    taken_at_ = sequence_;
                }
            }
        }
        // **The first word since joining is where it is**, not a correction:
        // a machine slow to build its flight after joining heard nothing of
        // it for seconds while the server flew it on, and was then put right
        // by the whole way flown - 46 m, too far to hide.
        if (own_ai_flying_) {
            // The AI's: drawn from the updates, below, and not predicted.
            reconciled_s_ = state.simulation_time_s;
            own_word_.reset();
        } else if (!reconciled_s_ || resuming_ || (own_word_ && own_word_->adopt)) {
            // Put there by `hear` from the newest word, when the words
            // heard together are those waiting since it joined.
            reconciled_s_ = state.simulation_time_s;
            resuming_ = false;
            own_word_ = OwnWord{motion_of(*state.yours), 0, 0, 0, true, std::nullopt};
        } else {
            // **Put right from the newest of the words heard together**, by
            // `hear`, once they are all heard; each older one says only the
            // clocks' difference.
            reconciled_s_ = state.simulation_time_s;
            const std::uint64_t server_steps = static_cast<std::uint64_t>(std::llround(
                state.simulation_time_s * static_cast<double>(sim::steps_per_second)));
            if (own_word_) {
                flight.hear_clock(own_word_->last_applied, own_word_->steps_into,
                                  own_word_->server_steps);
            }
            const auto own = std::find_if(
                state.aircraft.begin(), state.aircraft.end(),
                [&](const net::AircraftState& a) { return a.index == state.your_aircraft; });
            own_word_ = OwnWord{motion_of(*state.yours), state.last_input_applied,
                                state.yours->steps_into_input, server_steps, false,
                                own != state.aircraft.end() &&
                                        own->condition == net::Condition::engine_stopped
                                    ? std::optional<int>(own->stopped_engine)
                                    : std::nullopt};
            if (own_words_ == 0) {
                first_own_word_s_ = state.simulation_time_s;
            }
            last_own_word_s_ = state.simulation_time_s;
            ++own_words_;
        }
    }
    // **Everybody else, to be drawn behind the clock**, and what the server
    // says of this client's own - kept whoever flies it, so that handed to
    // the AI it is drawn from the updates at once, not from two updates on.
    if (state.yours && state.your_aircraft == mine_) {
        applied_ = std::max(applied_, state.last_input_applied);
    }
    for (const net::AircraftState& a : state.aircraft) {
        if (!origin_) {
            origin_ = world::Ecef{a.x_m, a.y_m, a.z_m};
        }
        net::RemoteState r;
        r.time_s = state.simulation_time_s;
        to_local(*origin_, a.x_m, a.y_m, a.z_m, true, r.north_m, r.east_m, r.down_m);
        to_local(*origin_, static_cast<double>(a.vx_mps), static_cast<double>(a.vy_mps),
                 static_cast<double>(a.vz_mps), false, r.north_mps, r.east_mps, r.down_mps);
        r.heading_deg = static_cast<double>(a.heading_deg);
        r.pitch_deg = static_cast<double>(a.pitch_deg);
        r.roll_deg = static_cast<double>(a.roll_deg);
        r.wrecked = a.condition == net::Condition::wrecked;
        shown_[a.index].received(r);
        wrecked_[a.index] = a.condition == net::Condition::wrecked;
        ai_[a.index] = a.controller == net::Controller::ai;
    }
    // One no longer in the updates is no longer in the sky.
    for (auto it = shown_.begin(); it != shown_.end();) {
        const bool still = std::any_of(state.aircraft.begin(), state.aircraft.end(),
                                       [&](const net::AircraftState& a) {
                                           return a.index == it->first;
                                       });
        it = still ? std::next(it) : shown_.erase(it);
    }
}

std::vector<Other> Online::others(double local_s) {
    const auto lock = held();
    std::vector<Other> out;
    if (!clock_.known() || !origin_) {
        return out;
    }
    const double now = clock_.now(local_s);
    for (auto& [number, shown] : shown_) {
        // Its own is drawn as any other only while the AI flies it.
        if (!shown.known() || (number == mine_ && !own_ai_flying_)) {
            continue;
        }
        const net::RemoteState at = shown.at(now);
        Other o;
        o.number = number;
        const auto found = session_.roster().find(number);
        if (found != session_.roster().end()) {
            o.aircraft_id = found->second.id;
        }
        o.centre = from_local(*origin_, at.north_m, at.east_m, at.down_m);
        o.heading_deg = at.heading_deg;
        o.pitch_deg = at.pitch_deg;
        o.roll_deg = at.roll_deg;
        o.north_mps = at.north_mps;
        o.east_mps = at.east_mps;
        o.down_mps = at.down_mps;
        // How its path moves, per second of this machine's clock.
        const std::array<double, 3> path = shown.path_velocity();
        const world::Ecef v = world::ned_to_ecef(world::to_geodetic(*origin_), path[0], path[1],
                                                 path[2]);
        o.path_mps = {v.x * clock_.rate(), v.y * clock_.rate(), v.z * clock_.rate()};
        o.ai_flying = ai_[number];
        o.wrecked = wrecked_[number];
        out.push_back(o);
    }
    return out;
}

void Online::watch(std::uint8_t number) {
    const auto lock = held();
    send_watch(number);
}

void Online::send_watch(std::uint8_t number) {
    watching_ = number;
    watched_.clear();
    net::Watch w;
    w.aircraft = number;
    const std::vector<std::uint8_t> body = net::write(w);
    session_.send_message(std::span<const std::uint8_t>(body.data(), body.size()));
}

std::optional<net::Watched> Online::watched_controls(double local_s) const {
    if (!clock_.known() || watched_.empty()) {
        return std::nullopt;
    }
    const double at = clock_.now(local_s) - net::shown_behind_s;
    const auto after = watched_.lower_bound(at);
    if (after == watched_.end()) {
        // Past the newest: held where it was, as a gauge would be.
        return watched_.rbegin()->second;
    }
    if (after == watched_.begin()) {
        // **Before the oldest kept: held there**, as past the newest. A client
        // whose clock runs more than the 64 kept behind them - a slow debug
        // build on a Windows runner did - would otherwise show no controls at
        // all, for as long as it ran.
        return watched_.begin()->second;
    }
    const auto before = std::prev(after);
    const double span = after->first - before->first;
    const double k = span > 0.0 ? (at - before->first) / span : 0.0;
    const auto mix = [k](double a, double b) { return a + k * (b - a); };
    net::Watched out = before->second;
    out.aileron = mix(before->second.aileron, after->second.aileron);
    out.elevator = mix(before->second.elevator, after->second.elevator);
    out.rudder = mix(before->second.rudder, after->second.rudder);
    out.throttle = mix(before->second.throttle, after->second.throttle);
    out.flaps = mix(before->second.flaps, after->second.flaps);
    return out;
}

void Online::take_over(std::uint8_t number) {
    const auto lock = held();
    net::ControllerSwap swap;
    swap.aircraft = number;
    swap.to = net::Controller::person;
    const std::vector<std::uint8_t> body = net::write(swap);
    session_.send_message(std::span<const std::uint8_t>(body.data(), body.size()));
}

void Online::send_route(net::CopilotRoute route) {
    const auto lock = held();
    route.aircraft = mine_;
    const std::vector<std::uint8_t> body = net::write(route);
    session_.send_message(std::span<const std::uint8_t>(body.data(), body.size()));
}

void Online::hand_over(bool to_ai) {
    const auto lock = held();
    net::ControllerSwap swap;
    swap.aircraft = mine_;
    swap.to = to_ai ? net::Controller::ai : net::Controller::person;
    const std::vector<std::uint8_t> body = net::write(swap);
    session_.send_message(std::span<const std::uint8_t>(body.data(), body.size()));
}

std::optional<Joined> Online::taken_over() {
    std::optional<Joined> out;
    out.swap(taken_);
    return out;
}

} // namespace glideslope::client
