// impair - a UDP relay that makes a network worse, for the network checks.
//
//   glideslope_impair LISTEN_PORT SERVER_HOST:PORT --delay MS --jitter MS
//                     --loss PERCENT --seed N [--until-input-ends] [--seconds S]
//
// Clients send to LISTEN_PORT as if it were the server; each client gets a
// socket of its own towards the server, so the server still sees each one at
// an address of its own - which is how it tells sessions apart. Every datagram,
// either way, is dropped with probability PERCENT, and otherwise delivered
// after DELAY milliseconds plus up to JITTER more, drawn evenly - so datagrams
// can arrive out of order, as they do. The draws come from one generator seeded
// with N, so a run can be repeated.
//
// It stops after S seconds (600 unless given) or, with `--until-input-ends`,
// when its standard input ends - which, last in a test's pipeline, is when the
// server before it has gone - once it has delivered everything the server sent
// before it went; and says what it did: how many datagrams each
// way, and how many it dropped; and, its input ended, what it took from the
// server after that and delivered to the clients, on standard error. With
// `--until-input-ends`, stopping for the time instead is a failure, and it
// exits 1; so is the time running out with something the server sent still
// held for a client after the input ended. What it reads from standard input it passes
// on to standard error a whole line at a time, so that the program before it
// can still be heard, and its lines are not torn by others written to the
// same standard error.
//
// With `--gap MS --every S`, everything from the server is dropped for MS
// milliseconds once every S seconds as well: a hole in the updates a client
// must draw across, made rather than hoped for from random loss.
//
// With `--forge-refusal-after N`, once N datagrams have come from the server,
// a session is made quiet and a forger refuses it - built, not waited for.
// **It is for one client**: the hold is of everything the server sends, to
// every client, and the first initiation from any of them ends it.
// Everything from the server is held, not dropped, and every datagram a
// client sends meanwhile is answered with a `BAD_HANDSHAKE` that the server
// never sent: from the relay's own port, which is the server's address as
// the clients see it - what a forger who can write the server's address on a
// datagram sends. It ends on the event a test waits for, not a time: the
// first handshake initiation from a client - that client trying to join
// again - which is passed on to the server, and then everything held is
// delivered, in the order it came, and nothing more is forged. Once only. It
// says on standard error how many it forged and held, and whether the hold
// ended so; with the hold never ended, everything still held is let go at
// the end as never delivered. It holds at most `most_held` datagrams; any
// past that are dropped, counted and said, so that a hold nobody ends cannot
// grow without bound.
//
// With `--hold-until-let-go-after N`, the same hold begins after N datagrams
// from the server, and nothing is forged: **the server lets the session go
// for real, with its last updates held on the way.** While the hold lasts
// everything a client sends is dropped, so the server hears nothing - except
// that the relay sends it, every quarter of a second, the last sealed
// datagram the client sent before the hold, which the server has already
// opened: a replay, which it drops in silence while it has the session, and
// refuses `BAD_HANDSHAKE` once it has let it go. **That refusal is the event**:
// from it, the server's refusals are passed on and what the client sends goes
// through again, so the client hears the server's own refusals and believes
// one. Its initiation ends the hold as above, and what was held - updates
// sealed before the session was let go - is delivered after it. It says on
// standard error whether the server let the session go during the hold, how
// many it dropped from the client meanwhile and how many it held.
//
// It is what `tc netem` does, without needing to be root or on Linux, so that
// the same check runs on every CI platform.

#include "net/protocol.hpp"
#include "platform/closed_pipes.hpp"
#include "platform/socket.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

// Set by the thread that reads standard input, when it ends. Not the main
// thread's to own: that thread is still waiting on its read when main returns.
std::atomic<bool> input_ended{false};

struct Held {
    std::chrono::steady_clock::time_point due;
    bool to_server = false;
    glideslope::platform::Address to;     // the client, when going back
    std::string client;                   // which client's upstream socket
    std::vector<std::uint8_t> bytes;
};

double number(const char* text) {
    return std::strtod(text, nullptr);
}

} // namespace

int main(int argc, char** argv) {
    // Last in a pipeline or not, what it says to a reader that has gone must
    // not end it (platform/closed_pipes.hpp).
    glideslope::platform::outlive_closed_pipes();
    if (argc < 3) {
        std::fprintf(stderr, "usage: glideslope_impair LISTEN_PORT SERVER_HOST:PORT --delay MS "
                             "--jitter MS --loss PERCENT --seed N [--until-input-ends] "
                             "[--seconds S] [--gap MS --every S] "
                             "[--forge-refusal-after N | --hold-until-let-go-after N]\n");
        return 2;
    }
    const auto listen_port = static_cast<std::uint16_t>(std::atoi(argv[1]));
    const auto server = glideslope::platform::address_of(argv[2]);
    if (!server) {
        std::fprintf(stderr, "impair: not an address: %s\n", argv[2]);
        return 2;
    }
    double delay_ms = 0.0, jitter_ms = 0.0, loss = 0.0, seconds = 600.0;
    double gap_ms = 0.0, gap_every_s = 0.0;
    // How many datagrams from the server before the hold and the forging
    // begin; 0 is never.
    std::uint64_t forge_after = 0;
    // `--hold-until-let-go-after`: the same hold, forging nothing (above).
    bool until_let_go = false;
    unsigned seed = 1;
    bool until_input_ends = false;
    for (int i = 3; i < argc; i += 2) {
        const std::string flag = argv[i];
        if (flag == "--until-input-ends") {
            until_input_ends = true;
            --i;
            continue;
        }
        if (i + 1 >= argc) {
            std::fprintf(stderr, "impair: %s wants a value\n", flag.c_str());
            return 2;
        }
        if (flag == "--delay") delay_ms = number(argv[i + 1]);
        else if (flag == "--jitter") jitter_ms = number(argv[i + 1]);
        else if (flag == "--loss") loss = number(argv[i + 1]) / 100.0;
        else if (flag == "--seed") seed = static_cast<unsigned>(std::strtoul(argv[i + 1], nullptr, 10));
        else if (flag == "--seconds") seconds = number(argv[i + 1]);
        else if (flag == "--gap") gap_ms = number(argv[i + 1]);
        else if (flag == "--every") gap_every_s = number(argv[i + 1]);
        else if (flag == "--forge-refusal-after")
            forge_after = std::strtoull(argv[i + 1], nullptr, 10);
        else if (flag == "--hold-until-let-go-after") {
            forge_after = std::strtoull(argv[i + 1], nullptr, 10);
            until_let_go = true;
        } else {
            std::fprintf(stderr, "impair: no option %s\n", flag.c_str());
            return 2;
        }
    }
    auto front = glideslope::platform::UdpSocket::bound(listen_port);
    if (!front) {
        std::fprintf(stderr, "impair: cannot listen on port %u\n", static_cast<unsigned>(listen_port));
        return 1;
    }
    std::printf("impair: %u -> %s, delay %.0f ms, jitter %.0f ms, loss %.1f%%, seed %u\n",
                static_cast<unsigned>(listen_port), argv[2], delay_ms, jitter_ms, loss * 100.0, seed);
    std::fflush(stdout);

    std::mt19937_64 random(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    // One socket towards the server for each client, and who it is for.
    std::map<std::string, std::unique_ptr<glideslope::platform::UdpSocket>> upstream;
    std::map<std::string, glideslope::platform::Address> client_of;
    std::vector<Held> held;
    std::uint64_t up = 0, down = 0, dropped_up = 0, dropped_down = 0;

    // Standard input read to its end on a thread of its own, because reading
    // it waits and the relaying must not.
    if (until_input_ends) {
        std::thread([] {
            // Passed on to standard error, so that a test that fails can
            // show what the program before this one in its pipeline said -
            // and that a test can read, so **a whole line in one write**. The
            // clients before it write their own lines to the same standard
            // error, and a line passed on a character at a time (stderr is
            // unbuffered) had one of theirs written into its middle: "aircraft
            // 2 not taken over: aircraft 2 is a pclient ad014e2b: aircraft 2
            // handed to the AI\nlayer's", on CI's windows-clang (2026-09-29),
            // and the test that looked for the refusal did not find it. One
            // write of a line is not split by another writer's: POSIX says so
            // of a pipe up to PIPE_BUF; on Windows the C runtime's fwrite
            // makes one _write of a line under 4096 bytes, and a pipe keeps
            // one write whole in practice, though nothing documents it.
            std::string line;
            for (int c; (c = std::fgetc(stdin)) != EOF;) {
                line += static_cast<char>(c);
                if (c == '\n') {
                    std::fwrite(line.data(), 1, line.size(), stderr);
                    std::fflush(stderr);
                    line.clear();
                }
            }
            if (!line.empty()) {
                std::fwrite(line.data(), 1, line.size(), stderr);
                std::fflush(stderr);
            }
            input_ended = true;
        }).detach();
    }
    const auto began = std::chrono::steady_clock::now();
    std::vector<std::uint8_t> buffer(glideslope::platform::largest_datagram);

    std::uint64_t gapped = 0;
    // **The forger's hold** (`--forge-refusal-after`): what the server sent
    // while it lasts, and what was forged.
    enum class Forging { not_yet, holding, over };
    Forging forging = Forging::not_yet;
    std::uint64_t from_server = 0, forged = 0;
    std::vector<Held> held_back;
    constexpr std::size_t most_held = 4096;
    std::uint64_t past_the_cap = 0;
    std::chrono::steady_clock::time_point hold_began{};
    double hold_lasted_s = 0.0;
    std::vector<std::uint8_t> refusal;
    {
        glideslope::net::Writer w = glideslope::net::begin(glideslope::net::Type::refusal);
        w.u8(static_cast<std::uint8_t>(glideslope::net::Refusal::bad_handshake));
        refusal = w.take();
    }
    const auto is_a = [](glideslope::net::Type type, const std::uint8_t* data, std::size_t n) {
        glideslope::net::Reader r(std::span<const std::uint8_t>(data, n));
        glideslope::net::Envelope envelope;
        glideslope::net::Refusal why{};
        return glideslope::net::read_envelope(r, envelope, why) && envelope.type == type;
    };
    const auto is_initiation = [&is_a](const std::uint8_t* data, std::size_t n) {
        return is_a(glideslope::net::Type::handshake_initiation, data, n);
    };
    // **Held until the server lets go** (`--hold-until-let-go-after`): the
    // replay that asks the server whether it still has the session, whose
    // client it is, when it last went and how often; whether the server has
    // refused it; and how many of the client's datagrams were dropped first.
    std::vector<std::uint8_t> probe;
    std::string probe_client;
    auto probed_at = std::chrono::steady_clock::now();
    std::uint64_t probes = 0, dropped_holding = 0, released = 0;
    bool let_go_seen = false;
    const auto gaps_from = std::chrono::steady_clock::now();
    const auto hold = [&](bool to_server, const glideslope::platform::Address& to,
                          const std::string& client, const std::uint8_t* data, std::size_t n) {
        // In a gap, nothing from the server gets through.
        if (!to_server && gap_ms > 0.0 && gap_every_s > 0.0) {
            const double into_s = std::fmod(
                std::chrono::duration<double>(std::chrono::steady_clock::now() - gaps_from).count(),
                gap_every_s);
            if (into_s >= gap_every_s - gap_ms / 1000.0) {
                ++gapped;
                return;
            }
        }
        // The server's refusal, while held until it lets go, is the let-go
        // itself: passed on, not held.
        const bool refusal_passed =
            until_let_go && is_a(glideslope::net::Type::refusal, data, n);
        if (!to_server && forging == Forging::holding && refusal_passed) {
            let_go_seen = true;
        }
        if (!to_server && forging == Forging::holding && !refusal_passed) {
            if (held_back.size() >= most_held) {
                ++past_the_cap;
                return;
            }
            held_back.push_back({std::chrono::steady_clock::now(), false, to, client,
                                 std::vector<std::uint8_t>(data, data + n)});
            return;
        }
        if (unit(random) < loss) {
            ++(to_server ? dropped_up : dropped_down);
            return;
        }
        const double after_ms = delay_ms + jitter_ms * unit(random);
        held.push_back({std::chrono::steady_clock::now() +
                            std::chrono::microseconds(static_cast<long long>(after_ms * 1000.0)),
                        to_server, to, client, std::vector<std::uint8_t>(data, data + n)});
    };

    // **What the server sent before it went is still passed on.** Its input
    // ending is the server gone, but not its last words: a server that drops
    // a client sends its goodbye and, with nobody left, stops at once
    // (--until-empty). The relay once stopped on the first pass that saw its
    // input ended, and a goodbye still waiting in its socket, or held for the
    // pass after, was never delivered: the dropped client never heard it, and
    // the test of the window client dropped by the operator failed on CI
    // four times for it (2026-09-30). The server's send comes before its exit,
    // and its exit before its output ends; so once the input is seen ended,
    // one more pass takes everything already come from the server, and the
    // relay stops only when all of it towards the clients has been delivered,
    // each at its own delay. What comes from the clients from then on has no
    // server to go to, and is let go.
    //
    // **Whether that pass took everything is said, not assumed.** A send on
    // loopback is in the receiver's socket when it returns in practice, not
    // by guarantee: Linux can defer loopback delivery to ksoftirqd on a
    // loaded machine, and macOS hands lo0's input to a thread of its own. So
    // the relay counts what it took from the server once its input was seen
    // ended and what it delivered to the clients after that, and says both
    // on standard error - where a test reads the programs' words - so that a
    // client that never heard the server's last words shows whether they
    // ever reached the relay.
    bool ending = false;
    std::uint64_t taken_after_end = 0, delivered_after_end = 0;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        const double up_s = std::chrono::duration<double>(now - began).count();
        if (up_s >= seconds) {
            break;
        }
        // Read before this pass's receives, so that the pass takes
        // everything the server sent before its input ended.
        const bool ended_before_this_pass = input_ended;
        bool busy = false;
        glideslope::platform::Address from;
        // From the clients, towards the server.
        for (std::size_t got; (got = front->receive(buffer, from)) > 0;) {
            if (ending) {
                continue;
            }
            busy = true;
            const std::string client = from.text();
            if (!upstream.count(client)) {
                auto socket = glideslope::platform::UdpSocket::bound(0);
                if (!socket) {
                    std::fprintf(stderr, "impair: no socket towards the server for %s\n",
                                 client.c_str());
                    continue;
                }
                upstream[client] =
                    std::make_unique<glideslope::platform::UdpSocket>(std::move(*socket));
                client_of[client] = from;
            }
            if (forging == Forging::holding) {
                if (is_initiation(buffer.data(), got)) {
                    // A client trying to join again: the hold is over, and
                    // what was held goes on its way, in the order it came.
                    forging = Forging::over;
                    hold_lasted_s = std::chrono::duration<double>(
                                        std::chrono::steady_clock::now() - hold_began)
                                        .count();
                    for (Held& h : held_back) {
                        h.due = std::chrono::steady_clock::now();
                        held.push_back(std::move(h));
                    }
                    released = held_back.size();
                    held_back.clear();
                } else if (until_let_go) {
                    // Until the server has let the session go, it hears
                    // nothing from the client; after, everything.
                    if (!let_go_seen) {
                        ++dropped_holding;
                        continue;
                    }
                } else {
                    (void)front->send(from, std::span<const std::uint8_t>(refusal.data(),
                                                                          refusal.size()));
                    ++forged;
                }
            }
            if (until_let_go && forging == Forging::not_yet &&
                is_a(glideslope::net::Type::sealed, buffer.data(), got)) {
                probe.assign(buffer.data(), buffer.data() + got);
                probe_client = client;
            }
            hold(true, *server, client, buffer.data(), got);
        }
        // The replay that asks whether the server still has the session.
        if (until_let_go && forging == Forging::holding && !let_go_seen && !probe.empty() &&
            now - probed_at >= std::chrono::milliseconds(250)) {
            probed_at = now;
            (void)upstream[probe_client]->send(
                *server, std::span<const std::uint8_t>(probe.data(), probe.size()));
            ++probes;
        }
        // From the server, back towards each client.
        for (auto& [client, socket] : upstream) {
            for (std::size_t got; (got = socket->receive(buffer, from)) > 0;) {
                busy = true;
                if (ended_before_this_pass) {
                    ++taken_after_end;
                }
                ++from_server;
                if (forging == Forging::not_yet && forge_after > 0 &&
                    from_server > forge_after) {
                    forging = Forging::holding;
                    hold_began = std::chrono::steady_clock::now();
                }
                hold(false, client_of[client], client, buffer.data(), got);
            }
        }
        // What is due, delivered. Each datagram is due by its own delay, so one
        // can overtake another; those falling due in one pass go in the order
        // they came. (Windows sleeps longer than asked, which makes a pass
        // longer there - harsher than stated, never kinder.)
        for (auto it = held.begin(); it != held.end();) {
            if (it->due > now) {
                ++it;
                continue;
            }
            const std::span<const std::uint8_t> bytes(it->bytes.data(), it->bytes.size());
            if (it->to_server) {
                (void)upstream[it->client]->send(*server, bytes);
                ++up;
            } else {
                (void)front->send(it->to, bytes);
                ++down;
                if (ended_before_this_pass) {
                    ++delivered_after_end;
                }
            }
            it = held.erase(it);
            busy = true;
        }
        if (ended_before_this_pass) {
            if (ending) {
                // A pass after the last one that could take anything from
                // the server: only what is still held towards a client keeps
                // the relay running, until it falls due.
                std::erase_if(held, [](const Held& h) { return h.to_server; });
                if (held.empty()) {
                    break;
                }
            }
            ending = true;
            busy = true;
        }
        if (!busy) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }
    std::printf("impair: forwarded %llu to the server and %llu back; dropped %llu and %llu\n",
                static_cast<unsigned long long>(up), static_cast<unsigned long long>(down),
                static_cast<unsigned long long>(dropped_up),
                static_cast<unsigned long long>(dropped_down));
    std::printf("impair: %llu dropped in gaps\n", static_cast<unsigned long long>(gapped));
    // **What the forger did**, on standard error, where a test reads the
    // programs' words.
    if (forge_after > 0 && until_let_go) {
        std::fprintf(stderr,
                     "impair: held the server's datagrams; the server %s after %llu replays; "
                     "%llu dropped from the client meanwhile; the hold %s; %llu held delivered "
                     "after it, %llu dropped past its cap\n",
                     let_go_seen ? "let the session go" : "never let the session go",
                     static_cast<unsigned long long>(probes),
                     static_cast<unsigned long long>(dropped_holding),
                     forging == Forging::over    ? "ended on a client's initiation"
                     : forging == Forging::holding ? "never ended"
                                                   : "never began",
                     static_cast<unsigned long long>(released),
                     static_cast<unsigned long long>(past_the_cap));
        std::fflush(stderr);
    } else if (forge_after > 0) {
        if (forging == Forging::over) {
            std::fprintf(stderr,
                         "impair: forged %llu refusals while holding the server's datagrams; "
                         "the hold ended on a client's initiation after %.1f s; %llu dropped "
                         "past its cap of %zu\n",
                         static_cast<unsigned long long>(forged), hold_lasted_s,
                         static_cast<unsigned long long>(past_the_cap), most_held);
        } else {
            std::fprintf(stderr,
                         "impair: forged %llu refusals; the hold %s, %llu from the server "
                         "never delivered and %llu dropped past its cap\n",
                         static_cast<unsigned long long>(forged),
                         forging == Forging::holding ? "never ended: no client tried to join again"
                                                     : "never began",
                         static_cast<unsigned long long>(held_back.size()),
                         static_cast<unsigned long long>(past_the_cap));
        }
        std::fflush(stderr);
    }
    // **Given up on, not ended**: asked to stop when its input ends, it
    // stopped for the time instead - the program before it in the pipeline
    // was still running, which that program's own exit code, once it goes,
    // may not say. So this one says it.
    const bool gave_up = until_input_ends && !input_ended;
    if (gave_up) {
        std::printf("impair: gave up after %.0f s with its input still open\n", seconds);
    }
    // **Its time run out with the server's last words still held** is a
    // failure too: they were never delivered, and nothing else would say so.
    const auto still_held = static_cast<unsigned long long>(
        std::count_if(held.begin(), held.end(), [](const Held& h) { return !h.to_server; }));
    const bool lost_last_words = ending && still_held > 0;
    if (ending) {
        std::fprintf(stderr,
                     "impair: after its input ended, took %llu from the server and delivered "
                     "%llu to the clients; %llu still held when it stopped\n",
                     static_cast<unsigned long long>(taken_after_end),
                     static_cast<unsigned long long>(delivered_after_end), still_held);
        std::fflush(stderr);
    }
    if (lost_last_words) {
        std::printf("impair: its time ran out with %llu held for the clients after its input "
                    "ended\n",
                    still_held);
    }
    std::fflush(stdout);
    // Out without the runtime's tidying up, which can wait on standard
    // input's lock - held by the reading thread, if the input has not ended.
    std::_Exit(gave_up || lost_last_words ? 1 : 0);
}
