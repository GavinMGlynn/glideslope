#pragma once

// The server's dashboard: what it shows about itself, gathered once and drawn
// either in the terminal or in a window (window.hpp).
//
// **The terminal and the window draw the same lines.** Neither reads the
// server's state: `run()` gathers a `Dashboard` and `dashboard_lines()` turns
// it into the text both of them show, so a number in the window is the number
// the terminal would have printed at that moment by construction - and
// `--window-dump` prints both, so that a test holds them to it.

#include <deque>
#include <string>
#include <vector>

namespace glideslope::server {

// One slot, and the connection in it if there is one.
struct DashboardSlot {
    int slot = 0;
    std::string who;      // the player's name, or "(open)"
    std::string ping;     // milliseconds, or "-"
    std::string in;       // bytes in, or "-"
    std::string out;      // bytes out, or "-"
    std::string address;  // the connection's address; empty with nobody on it
};

// Things that happened - arrivals and departures - which the terminal would
// otherwise scroll away. The most recent few are kept.
class Happenings {
public:
    static constexpr std::size_t kept = 8;
    void add(double up_s, const std::string& what);
    const std::deque<std::string>& lines() const { return lines_; }

private:
    std::deque<std::string> lines_;
};

struct Dashboard {
    std::string heading;  // the program and its version
    std::string traffic;  // the port, how long up, what has arrived
    std::vector<DashboardSlot> slots;
    std::vector<std::string> flying;  // one line per aircraft
    std::vector<std::string> happened;
    std::string footer;
};

// The dashboard as lines of text, which is all either the terminal or the
// window shows of it. A slot's row is always the same line, whether or not a
// drop button is drawn beside it.
std::vector<std::string> dashboard_lines(const Dashboard& d);

// Which line of `dashboard_lines` slot number `index` in `d.slots` is on.
std::size_t dashboard_slot_line(std::size_t index);

} // namespace glideslope::server
