#include "world/runways.hpp"

#include <cstdlib>
#include <map>

namespace glideslope::world {

namespace {

// One line of a CSV file: fields separated by commas, a field in double
// quotes may hold commas, and "" inside one is a quote.
std::vector<std::string> fields_of(std::string_view line) {
    std::vector<std::string> out(1);
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                out.back() += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                out.back() += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            out.emplace_back();
        } else if (c != '\r') {
            out.back() += c;
        }
    }
    return out;
}

// A number, or NaN for an empty field or one that is not a number.
double number_of(const std::string& field) {
    if (field.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    char* end = nullptr;
    const double v = std::strtod(field.c_str(), &end);
    return *end == '\0' ? v : std::numeric_limits<double>::quiet_NaN();
}

} // namespace

std::vector<RunwayEnd> read_runways(std::string_view csv) {
    std::size_t at = 0;
    const auto next_line = [&](std::string_view& line) {
        if (at >= csv.size()) {
            return false;
        }
        const std::size_t end = csv.find('\n', at);
        line = csv.substr(at, end == std::string_view::npos ? csv.size() - at : end - at);
        at = end == std::string_view::npos ? csv.size() : end + 1;
        return true;
    };
    std::string_view line;
    if (!next_line(line)) {
        throw RunwayError("the runways file is empty");
    }
    std::map<std::string, std::size_t> column;
    const std::vector<std::string> header = fields_of(line);
    for (std::size_t i = 0; i < header.size(); ++i) {
        column[header[i]] = i;
    }
    const char* const wanted[] = {"airport_ident", "length_ft", "surface", "closed"};
    for (const char* name : wanted) {
        if (!column.contains(name)) {
            throw RunwayError(std::string("the runways file has no column ") + name);
        }
    }
    for (const char* end : {"le_", "he_"}) {
        for (const char* what :
             {"ident", "latitude_deg", "longitude_deg", "elevation_ft", "heading_degT"}) {
            if (!column.contains(std::string(end) + what)) {
                throw RunwayError(std::string("the runways file has no column ") + end + what);
            }
        }
    }

    std::vector<RunwayEnd> out;
    while (next_line(line)) {
        const std::vector<std::string> f = fields_of(line);
        const auto field = [&](const std::string& name) -> const std::string& {
            static const std::string none;
            const std::size_t i = column.at(name);
            return i < f.size() ? f[i] : none;
        };
        if (field("closed") == "1") {
            continue;
        }
        const double length_m = number_of(field("length_ft")) * 0.3048;
        for (const std::string end : {"le_", "he_"}) {
            RunwayEnd r;
            r.airport = field("airport_ident");
            r.ident = field(end + "ident");
            r.latitude_deg = number_of(field(end + "latitude_deg"));
            r.longitude_deg = number_of(field(end + "longitude_deg"));
            r.elevation_ft = number_of(field(end + "elevation_ft"));
            r.heading_deg = number_of(field(end + "heading_degT"));
            r.length_m = length_m;
            r.surface = field("surface");
            if (r.airport.empty() || r.ident.empty() || r.ident.starts_with('H') ||
                !(r.latitude_deg >= -90.0 && r.latitude_deg <= 90.0) ||
                !(r.longitude_deg >= -180.0 && r.longitude_deg <= 180.0) ||
                !(r.heading_deg >= 0.0 && r.heading_deg <= 360.0) || !(r.length_m > 0.0)) {
                continue;
            }
            out.push_back(std::move(r));
        }
    }
    return out;
}

std::vector<RunwayEnd> runways_at(const std::vector<RunwayEnd>& all, std::string_view airport) {
    std::vector<RunwayEnd> out;
    for (const RunwayEnd& r : all) {
        if (r.airport == airport) {
            out.push_back(r);
        }
    }
    return out;
}

sim::Runway as_runway(const RunwayEnd& end, double elevation_ft) {
    sim::Runway r;
    r.name = end.airport + " " + end.ident;
    r.threshold_lat_deg = end.latitude_deg;
    r.threshold_lon_deg = end.longitude_deg;
    r.elevation_ft = elevation_ft;
    r.heading_deg = end.heading_deg;
    r.length_m = end.length_m;
    return r;
}

} // namespace glideslope::world
