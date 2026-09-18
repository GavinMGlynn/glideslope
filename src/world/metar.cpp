#include "world/metar.hpp"

#include <regex>
#include <sstream>
#include <vector>

namespace glideslope::world {

namespace {

constexpr double knots_per_mps = 3600.0 / 1852.0;
constexpr double knots_per_kmh = 1.0 / 1.852;
constexpr double hpa_per_inhg = 33.8639;
constexpr double metres_per_statute_mile = 1609.344;

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
    static const std::regex runway(R"((?:R|RWY)(\d{2}[LCR]?))");
    static const std::regex peak(R"((\d{3})(\d{2,3})/(\d{2})?(\d{2}))");
    static const std::regex shift_time(R"((\d{2})?(\d{2}))");
    static const std::regex metres(R"((\d{4})(NDV)?)");
    static const std::regex miles(R"(([MP])?(?:(\d+)|(\d+)/(\d+))SM)");
    static const std::regex whole_miles(R"(\d)");
    static const std::regex fraction_miles(R"((\d)/(\d{1,2})SM)");
    static const std::regex present(
        R"((\+|-|VC)?(MI|PR|BC|DR|BL|SH|TS|FZ)?((?:DZ|RA|SN|SG|IC|PL|GR|GS|UP|BR|FG|FU|VA|DU|SA|HZ|PY|PO|SQ|FC|SS|DS)*))");
    static const std::regex cloud(R"((FEW|SCT|BKN|OVC)(\d{3})(CB|TCU|///)?)");
    static const std::regex obscured(R"(VV(\d{3}))");

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
            } else if (w == "PK" && i + 2 < words.size() && words[i + 1] == "WND" &&
                       std::regex_match(words[i + 2], match, peak)) {
                Metar::PeakWind p;
                p.from_deg = std::stod(match[1]);
                p.speed_kt = std::stod(match[2]);
                p.hour = match[3].matched ? std::stoi(match[3]) : m.hour;
                p.minute = std::stoi(match[4]);
                m.peak_wind = p;
                i += 2;
            } else if (w == "WSHFT" && i + 1 < words.size() &&
                       std::regex_match(words[i + 1], match, shift_time)) {
                Metar::WindShift shift;
                shift.hour = match[1].matched ? std::stoi(match[1]) : m.hour;
                shift.minute = std::stoi(match[2]);
                ++i;
                if (i + 1 < words.size() && words[i + 1] == "FROPA") {
                    shift.frontal = true;
                    ++i;
                }
                m.wind_shift = shift;
            }
            continue;
        }
        if (w == "WS") {
            // WS ALL RWY; WS R27 or WS RWY27; WS TKOF RWY27 or WS LDG RWY27.
            std::size_t next = i + 1;
            if (next < words.size() &&
                (words[next] == "TKOF" || words[next] == "LDG")) {
                ++next;
            }
            if (next + 1 < words.size() && words[next] == "ALL" &&
                words[next + 1] == "RWY") {
                m.wind_shear_all_runways = true;
                i = next + 1;
            } else if (next < words.size() &&
                       std::regex_match(words[next], match, runway)) {
                m.wind_shear_runways.push_back(match[1]);
                i = next;
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
        } else if (w == "CAVOK") {
            m.cavok = true;
            m.visibility_m = 10000.0;
            m.visibility_or_more = true;
        } else if (!m.visibility_m && !m.temperature_c &&
                   std::regex_match(w, match, metres)) {
            // 9999 is 10 km or more.
            const double v = std::stod(match[1]);
            m.visibility_m = v == 9999.0 ? 10000.0 : v;
            m.visibility_or_more = v == 9999.0;
        } else if (!m.visibility_m && std::regex_match(w, match, miles)) {
            const double sm = match[2].matched
                                  ? std::stod(match[2])
                                  : std::stod(match[3]) / std::stod(match[4]);
            m.visibility_m = sm * metres_per_statute_mile;
            m.visibility_or_more = match[1] == "P";
            m.visibility_or_less = match[1] == "M";
        } else if (!m.visibility_m && std::regex_match(w, whole_miles) &&
                   i + 1 < words.size() &&
                   std::regex_match(words[i + 1], match, fraction_miles)) {
            // "1 1/2SM": a whole number of miles, then a fraction.
            m.visibility_m =
                (std::stod(w) + std::stod(match[1]) / std::stod(match[2])) *
                metres_per_statute_mile;
            ++i;
        } else if (std::regex_match(w, match, cloud)) {
            Metar::CloudLayer layer;
            const std::string cover = match[1];
            layer.cover = cover == "FEW"   ? Metar::CloudLayer::Cover::few
                          : cover == "SCT" ? Metar::CloudLayer::Cover::scattered
                          : cover == "BKN" ? Metar::CloudLayer::Cover::broken
                                           : Metar::CloudLayer::Cover::overcast;
            layer.base_ft = std::stod(match[2]) * 100.0;
            layer.cumulonimbus = match[3] == "CB";
            layer.towering_cumulus = match[3] == "TCU";
            m.clouds.push_back(layer);
        } else if (std::regex_match(w, match, obscured)) {
            Metar::CloudLayer layer;
            layer.cover = Metar::CloudLayer::Cover::obscured;
            layer.base_ft = std::stod(match[1]) * 100.0;
            m.clouds.push_back(layer);
        } else if (w == "SKC" || w == "CLR" || w == "NSC" || w == "NCD") {
            m.no_cloud = true;
        } else if (!m.temperature_c && m.clouds.empty() && !m.no_cloud &&
                   std::regex_match(w, match, present) &&
                   (match[2].matched || match[3].length() > 0)) {
            Metar::PresentWeather p;
            p.intensity = match[1] == "+" ? 1 : match[1] == "-" ? -1 : 0;
            p.vicinity = match[1] == "VC";
            p.descriptor = match[2];
            const std::string codes = match[3];
            for (std::size_t c = 0; c + 1 < codes.size(); c += 2) {
                p.phenomena.push_back(codes.substr(c, 2));
            }
            m.weather.push_back(p);
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
