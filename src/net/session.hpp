#pragma once

// **A client's session with a server**: the handshake, the keys, and what
// arrives afterwards.
//
// This is the client half of the transport, in one place so that both
// frontends use the same one. It was written inside `glideslope_cli` first,
// which meant the client with the window could not connect to anything at
// all; a session belongs to the network, not to one program's `main`.
//
// **It owns its socket and nothing else.** It does not step an aircraft, does
// not draw and does not sleep: `poll()` is called as often as the caller
// likes and does what has arrived since the last one.
//
// **A handshake over UDP has to expect its first datagram to be lost**, and a
// server that is not listening yet answers nothing at all, so `connect()`
// resends the same initiation until it is answered or it gives up. The same
// initiation each time - `Noise_IK` makes one, and a second would be a second
// handshake, which a server treats as a duplicate.

#include "net/keys.hpp"
#include "net/sealing.hpp"
#include "net/state.hpp"
#include "platform/socket.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glideslope::net {

class ClientSession {
public:
    // **Completes a handshake with the server at `where`.** Nothing if the
    // address is not one, the key is not one, no socket can be opened, the
    // server refuses, or nothing answers within `give_up_after_s`.
    //
    // `resend_every_s` is how often the initiation goes out again while
    // nothing has come back.
    static std::optional<ClientSession> connect(const std::string& where,
                                                const std::string& key_hex,
                                                double give_up_after_s = 5.0,
                                                double resend_every_s = 0.25);

    // Read whatever has arrived: answer the server's knocking, and take in
    // any state update. `now_s` is the client's own clock, in seconds.
    void poll(double now_s);

    // The server's key, which is who this session is with.
    const PublicKey& theirs() const { return theirs_; }
    // How many of the server's knocks have been answered, and how many state
    // updates have arrived. What a client shows to say it is connected.
    int answered() const { return answered_; }
    int heard() const { return heard_; }

    // Where every aircraft was in the newest state update, and which of them
    // is this client's own - `no_aircraft` until the server says.
    const std::vector<AircraftState>& aircraft() const { return aircraft_; }
    std::uint8_t your_aircraft() const { return mine_; }
    double simulation_time_s() const { return clock_s_; }

    // Send this client's inputs, already packed by `InputSender::packet()`.
    void send_inputs(std::span<const std::uint8_t> packet);

    // Send the initiation once more, as a network that duplicates a datagram
    // would. A test flag's work; a server answers it with what it already
    // said.
    void send_the_initiation_again();

private:
    ClientSession() = default;

    std::unique_ptr<platform::UdpSocket> socket_;
    platform::Address server_;
    PublicKey theirs_;
    std::unique_ptr<Sealer> sealing_;
    std::unique_ptr<Unsealer> opening_;
    std::vector<std::uint8_t> initiation_;
    int answered_ = 0;
    int heard_ = 0;
    std::uint8_t mine_ = no_aircraft;
    double clock_s_ = 0.0;
    std::uint32_t applied_ = 0;
    std::vector<AircraftState> aircraft_;
};

} // namespace glideslope::net
