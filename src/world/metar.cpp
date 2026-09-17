#include "world/metar.hpp"

#include <regex>
#include <sstream>
#include <vector>

namespace glideslope::world {

namespace {

constexpr double knots_per_mps = 3600.0 / 1852.0;
constexpr double knots_per_kmh = 1.0 / 1.852;
constexpr double hpa_per_inhg = 33.8639;

double speed_in_knots(double value, const std::string& unit) {
    if (unit == "MPS") {
        return value * knots_per_mps;
    }
    if (unit == "KMH") {
        return value * knots_per_kmh;
    }
    return value;
}

// "M05" is -5.
double signed_whole(const std::string& text) {
    return text[0] == 'M' ? -std::stod(text.substr(1)) : std::stod(text);
}

// "0222" in a T remark is 22.2; "1039" is -3.9.
double signed_tenths(const std::string& text) {
    const double v = std::stod(text.substr(1)) / 10.0;
    return text[0] == '1' ? -v : v;
}

} // namespace

Metar parse_metar(std::string_view report) {
    std::istringstream in{std::string(report)};
    std::vector<std::string> words;
    for (std::string w; in >> w;) {
        words.push_back(w);
    }
    std::size_t i = 0;
    while (i < words.size() && (words[i] == "METAR" || words[i] == "SPECI")) {
        ++i;
    }

    static const std::regex station(R"([A-Z][A-Z0-9]{3})");
    static const std::regex time(R"((\d{2})(\d{2})(\d{2})Z)");
    static const std::regex wind(R"((\d{3}|VRB)(\d{2,3})(?:G(\d{2,3}))?(KT|MPS|KMH))");
    static const std::regex varies(R"((\d{3})V(\d{3}))");
    static const std::regex temperatures(R"((M?\d{2})/(M?\d{2})?)");
    static const std::regex qnh(R"(Q(\d{4}))");
    static const std::regex altimeter(R"(A(\d{4}))");
    static const std::regex precise(R"(T([01]\d{3})([01]\d{3}))");

    Metar m;
    std::smatch match;
    if (i >= words.size() || !std::regex_match(words[i], match, station)) {
        throw MetarError("a METAR must start with its station: " + std::string(report));
    }
    m.station = words[i++];
    if (i >= words.size() || !std::regex_match(words[i], match, time)) {
        throw MetarError("a METAR's station must be followed by its time: " +
                         std::string(report));
    }
    m.day = std::stoi(match[1]);
    m.hour = std::stoi(match[2]);
    m.minute = std::stoi(match[3]);
    ++i;

    bool remarks = false;
    for (; i < words.size(); ++i) {
        const std::string& w = words[i];
        if (w == "RMK") {
            remarks = true;
            continue;
        }
        if (!remarks && (w == "BECMG" || w == "TEMPO" || w == "NOSIG")) {
            // A trend: what follows is forecast, until the remarks.
            while (i + 1 < words.size() && words[i + 1] != "RMK") {
                ++i;
            }
            continue;
        }
        if (remarks) {
            if (std::regex_match(w, match, precise)) {
                m.temperature_c = signed_tenths(match[1]);
                m.dewpoint_c = signed_tenths(match[2]);
            }
            continue;
        }
        if (!m.wind_speed_kt && std::regex_match(w, match, wind)) {
            const std::string unit = match[4];
            m.wind_speed_kt = speed_in_knots(std::stod(match[2]), unit);
            if (match[1] != "VRB") {
                m.wind_from_deg = std::stod(match[1]);
            }
            if (match[3].matched) {
                m.gust_kt = speed_in_knots(std::stod(match[3]), unit);
            }
        } else if (m.wind_speed_kt && !m.wind_varies_from_deg &&
                   std::regex_match(w, match, varies)) {
            m.wind_varies_from_deg = std::stod(match[1]);
            m.wind_varies_to_deg = std::stod(match[2]);
        } else if (!m.temperature_c && std::regex_match(w, match, temperatures)) {
            m.temperature_c = signed_whole(match[1]);
            if (match[2].matched) {
                m.dewpoint_c = signed_whole(match[2]);
            }
        } else if (!m.qnh_hpa && std::regex_match(w, match, qnh)) {
            m.qnh_hpa = std::stod(match[1]);
        } else if (!m.qnh_hpa && std::regex_match(w, match, altimeter)) {
            m.qnh_hpa = std::stod(match[1]) / 100.0 * hpa_per_inhg;
        }
    }
    return m;
}

} // namespace glideslope::world
