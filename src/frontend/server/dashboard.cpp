#include "frontend/server/dashboard.hpp"

#include <cstdio>

namespace glideslope::server {

void Happenings::add(double up_s, const std::string& what) {
    char at[32];
    std::snprintf(at, sizeof at, "%6.0f s  ", up_s);
    lines_.push_back(std::string(at) + what);
    while (lines_.size() > kept) {
        lines_.pop_front();
    }
}

namespace {

// Lines before the first slot's row: the heading, the traffic, a blank line
// and the column titles.
constexpr std::size_t lines_before_slots = 4;

} // namespace

std::size_t dashboard_slot_line(std::size_t index) {
    return lines_before_slots + index;
}

std::vector<std::string> dashboard_lines(const Dashboard& d) {
    std::vector<std::string> out;
    out.push_back(d.heading);
    out.push_back(d.traffic);
    out.emplace_back();
    out.emplace_back("  slot  who              ping     in     out");
    for (const DashboardSlot& s : d.slots) {
        char row[128];
        std::snprintf(row, sizeof row, "  %-4d  %-15s  %4s  %6s  %6s", s.slot, s.who.c_str(),
                      s.ping.c_str(), s.in.c_str(), s.out.c_str());
        out.emplace_back(row);
    }
    if (!d.flying.empty()) {
        out.emplace_back();
        out.emplace_back("  flying           latitude    longitude     ft agl");
        for (const std::string& line : d.flying) {
            out.push_back(line);
        }
    }
    out.emplace_back();
    out.emplace_back("  lately");
    if (d.happened.empty()) {
        out.emplace_back("  (nothing yet)");
    }
    for (const std::string& line : d.happened) {
        out.push_back("  " + line);
    }
    out.emplace_back();
    out.push_back(d.footer);
    return out;
}

} // namespace glideslope::server
