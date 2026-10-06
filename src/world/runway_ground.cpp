#include "world/runway_ground.hpp"

#include "world/geodesy.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <string>
#include <cmath>
#include <map>
#include <numbers>
#include <set>
#include <utility>
#include <tuple>

namespace glideslope::world {

namespace {

constexpr double to_rad = std::numbers::pi / 180.0;
constexpr double to_deg = 180.0 / std::numbers::pi;
constexpr double metres_per_foot = 0.3048;

// The index's cells: a sixteenth of a degree each way.
constexpr std::int64_t cells_per_degree = 16;
constexpr std::int64_t cell_rows = 180 * cells_per_degree;
constexpr std::int64_t cell_columns = 360 * cells_per_degree;
constexpr double cells_per_degree_d = static_cast<double>(cells_per_degree);

std::int64_t row_of(double latitude_deg) {
    return std::clamp(static_cast<std::int64_t>(std::floor(latitude_deg * cells_per_degree_d)),
                      -cell_rows / 2, cell_rows / 2 - 1);
}

std::int64_t column_of(std::int64_t unwrapped) {
    return ((unwrapped + cell_columns / 2) % cell_columns + cell_columns) % cell_columns;
}

std::int64_t key_of(std::int64_t row, std::int64_t column) {
    return (row + cell_rows / 2) * cell_columns + column;
}

std::array<double, 3> on_ellipsoid(double latitude_deg, double longitude_deg) {
    const Ecef e = to_ecef({latitude_deg, longitude_deg, 0.0});
    return {e.x, e.y, e.z};
}

double dot(const double* a, const std::array<double, 3>& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// 0 at 0, 1 at 1, flat at both.
double smoothstep(double x) {
    x = std::clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

} // namespace

RunwaySurfaces::RunwaySurfaces(std::vector<RunwayStrip> strips) {
    std::sort(strips.begin(), strips.end(), [](const RunwayStrip& a, const RunwayStrip& b) {
        return std::tie(a.airport, a.le_ident, a.he_ident, a.le_latitude_deg, a.le_longitude_deg,
                        a.he_latitude_deg, a.he_longitude_deg) <
               std::tie(b.airport, b.le_ident, b.he_ident, b.le_latitude_deg, b.le_longitude_deg,
                        b.he_latitude_deg, b.he_longitude_deg);
    });
    runways_.reserve(strips.size());
    for (RunwayStrip& s : strips) {
        Runway r{};
        // The frame: at the middle of the two ends, on the ellipsoid.
        const Ecef le = to_ecef({s.le_latitude_deg, s.le_longitude_deg, 0.0});
        const Ecef he = to_ecef({s.he_latitude_deg, s.he_longitude_deg, 0.0});
        const Geodetic middle =
            to_geodetic({(le.x + he.x) / 2.0, (le.y + he.y) / 2.0, (le.z + he.z) / 2.0});
        const double lat = middle.latitude_deg * to_rad;
        const double lon = middle.longitude_deg * to_rad;
        const Ecef origin = to_ecef({middle.latitude_deg, middle.longitude_deg, 0.0});
        r.origin[0] = origin.x;
        r.origin[1] = origin.y;
        r.origin[2] = origin.z;
        r.east[0] = -std::sin(lon);
        r.east[1] = std::cos(lon);
        r.east[2] = 0.0;
        r.north[0] = -std::sin(lat) * std::cos(lon);
        r.north[1] = -std::sin(lat) * std::sin(lon);
        r.north[2] = std::cos(lat);
        const auto local = [&](const Ecef& p, double& x, double& y) {
            const std::array<double, 3> d{p.x - r.origin[0], p.y - r.origin[1],
                                          p.z - r.origin[2]};
            x = dot(r.east, d);
            y = dot(r.north, d);
        };
        double he_x = 0.0;
        double he_y = 0.0;
        local(le, r.le_x, r.le_y);
        local(he, he_x, he_y);
        r.length_m = std::hypot(he_x - r.le_x, he_y - r.le_y);
        if (!(r.length_m >= 1.0 && r.length_m <= longest_runway_m)) {
            continue;
        }
        r.along_x = (he_x - r.le_x) / r.length_m;
        r.along_y = (he_y - r.le_y) / r.length_m;
        const double width_m =
            s.width_m >= 3.0 && s.width_m <= 150.0 ? s.width_m : default_runway_width_m;
        r.half_width_m = width_m / 2.0;
        r.strip = std::move(s);

        // **Into the index**: every cell the rectangle and its shoulder may
        // reach, by a circle round it - generous by the ellipsoid's
        // smallest radius of curvature, and every longitude near a pole.
        const auto index = static_cast<std::uint32_t>(runways_.size());
        runways_.push_back(std::move(r));
        const Runway& in = runways_.back();
        const double reach_m =
            std::hypot(in.length_m / 2.0 + runway_shoulder_m, in.half_width_m + runway_shoulder_m) +
            1.0;
        const double d_lat = reach_m / 6335439.0 * to_deg;
        const double south = middle.latitude_deg - d_lat;
        const double north = middle.latitude_deg + d_lat;
        const double nearest_pole = std::max(std::abs(south), std::abs(north));
        std::int64_t west_column = 0;
        std::int64_t east_column = cell_columns - 1;
        bool all_round = true;
        if (nearest_pole < 89.0) {
            const double d_lon = reach_m / (Wgs84::a * std::cos(nearest_pole * to_rad)) * to_deg;
            west_column = static_cast<std::int64_t>(
                std::floor((middle.longitude_deg - d_lon) * cells_per_degree_d));
            east_column = static_cast<std::int64_t>(
                std::floor((middle.longitude_deg + d_lon) * cells_per_degree_d));
            all_round = false;
        }
        for (std::int64_t row = row_of(south); row <= row_of(north); ++row) {
            for (std::int64_t c = west_column; c <= east_column; ++c) {
                std::vector<std::uint32_t>& cell =
                    cells_[key_of(row, all_round ? c : column_of(c))];
                if (cell.empty() || cell.back() != index) {
                    cell.push_back(index);
                }
            }
        }
    }

    // **Ties**: each pair of runways sharing a cell, once, in the first's
    // frame - where their centrelines cross, and where an end of either is
    // within reach of the other, at the closest point to it.
    std::set<std::pair<std::uint32_t, std::uint32_t>> tried;
    std::vector<std::uint32_t> parent(runways_.size());
    for (std::uint32_t i = 0; i < parent.size(); ++i) {
        parent[i] = i;
    }
    const auto root = [&](std::uint32_t i) {
        while (parent[i] != i) {
            i = parent[i] = parent[parent[i]];
        }
        return i;
    };
    std::vector<std::int64_t> keys;
    keys.reserve(cells_.size());
    for (const auto& [key, cell] : cells_) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    for (const std::int64_t key : keys) {
        const std::vector<std::uint32_t>& cell = cells_.at(key);
        for (std::size_t m = 0; m < cell.size(); ++m) {
            for (std::size_t k = m + 1; k < cell.size(); ++k) {
                const std::uint32_t a = cell[m];
                const std::uint32_t b = cell[k];
                if (!tried.insert({a, b}).second) {
                    continue;
                }
                Runway& ra = runways_[a];
                Runway& rb = runways_[b];
                const auto local = [&](double lat, double lon, double& x, double& y) {
                    const std::array<double, 3> p = on_ellipsoid(lat, lon);
                    const std::array<double, 3> d{p[0] - ra.origin[0], p[1] - ra.origin[1],
                                                  p[2] - ra.origin[2]};
                    x = dot(ra.east, d);
                    y = dot(ra.north, d);
                };
                double b_le_x = 0.0;
                double b_le_y = 0.0;
                double b_he_x = 0.0;
                double b_he_y = 0.0;
                local(rb.strip.le_latitude_deg, rb.strip.le_longitude_deg, b_le_x, b_le_y);
                local(rb.strip.he_latitude_deg, rb.strip.he_longitude_deg, b_he_x, b_he_y);
                const double rx = ra.along_x * ra.length_m;
                const double ry = ra.along_y * ra.length_m;
                const double sx = b_he_x - b_le_x;
                const double sy = b_he_y - b_le_y;
                const double b_length = std::hypot(sx, sy);
                const double reach = ra.half_width_m + rb.half_width_m + runway_shoulder_m;
                std::vector<std::pair<double, double>> found;
                // One tie, unless another of this pair is within 20 m of it
                // along both: two ties at one place say one thing twice.
                const auto tie = [&](double t, double u) {
                    for (const auto& [t0, u0] : found) {
                        if (std::abs(t - t0) * ra.length_m < 20.0 &&
                            std::abs(u - u0) * b_length < 20.0) {
                            return;
                        }
                    }
                    found.emplace_back(t, u);
                };
                const double denominator = rx * sy - ry * sx;
                const double qx = b_le_x - ra.le_x;
                const double qy = b_le_y - ra.le_y;
                // Where the centrelines cross - unless parallel, or as near
                // as makes no crossing.
                if (std::abs(denominator) >= 1e-6 * ra.length_m * b_length) {
                    const double t = (qx * sy - qy * sx) / denominator;
                    const double u = (qx * ry - qy * rx) / denominator;
                    if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
                        tie(t, u);
                    }
                }
                // Each end of a, against b's centreline...
                for (const double t : {0.0, 1.0}) {
                    const double ex = ra.le_x + t * rx - b_le_x;
                    const double ey = ra.le_y + t * ry - b_le_y;
                    const double u =
                        std::clamp((ex * sx + ey * sy) / (b_length * b_length), 0.0, 1.0);
                    if (std::hypot(ex - u * sx, ey - u * sy) <= reach) {
                        tie(t, u);
                    }
                }
                // ...and each end of b, against a's.
                for (const double u : {0.0, 1.0}) {
                    const double ex = b_le_x + u * sx - ra.le_x;
                    const double ey = b_le_y + u * sy - ra.le_y;
                    const double t = std::clamp((ex * rx + ey * ry) / (ra.length_m * ra.length_m),
                                                0.0, 1.0);
                    if (std::hypot(ex - t * rx, ey - t * ry) <= reach) {
                        tie(t, u);
                    }
                }
                for (const auto& [t, u] : found) {
                    ra.ties.push_back({b, t, u});
                    rb.ties.push_back({a, u, t});
                }
                if (!found.empty()) {
                    parent[root(a)] = root(b);
                }
            }
        }
    }
    std::map<std::uint32_t, std::vector<std::uint32_t>> groups;
    for (std::uint32_t i = 0; i < runways_.size(); ++i) {
        if (!runways_[i].ties.empty()) {
            groups[root(i)].push_back(i);
        }
    }
    for (Runway& r : runways_) {
        std::sort(r.ties.begin(), r.ties.end(), [](const Tie& x, const Tie& y) {
            return x.other != y.other ? x.other < y.other : x.t < y.t;
        });
    }
    for (std::uint32_t i = 0; i < runways_.size(); ++i) {
        runways_[i].group =
            runways_[i].ties.empty() ? std::vector<std::uint32_t>{i} : groups.at(root(i));
    }
}

std::span<const std::uint32_t> RunwaySurfaces::reaching(double latitude_deg,
                                                     double longitude_deg) const {
    const auto c = static_cast<std::int64_t>(std::floor(longitude_deg * cells_per_degree_d));
    const auto found = cells_.find(key_of(row_of(latitude_deg), column_of(c)));
    if (found == cells_.end()) {
        return {};
    }
    return found->second;
}

RunwaySurfaces::Placed RunwaySurfaces::place(std::size_t i, double latitude_deg,
                                             double longitude_deg) const {
    const Runway& r = runways_[i];
    const std::array<double, 3> p = on_ellipsoid(latitude_deg, longitude_deg);
    const std::array<double, 3> d{p[0] - r.origin[0], p[1] - r.origin[1], p[2] - r.origin[2]};
    // Behind the frame's horizon - the far side of the Earth - is nowhere near.
    if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > 1e8) {
        return {};
    }
    const double x = dot(r.east, d) - r.le_x;
    const double y = dot(r.north, d) - r.le_y;
    const double along = x * r.along_x + y * r.along_y;
    const double across = std::abs(x * r.along_y - y * r.along_x);
    const double beyond_end = std::max({-along, along - r.length_m, 0.0});
    const double beyond_side = std::max(across - r.half_width_m, 0.0);
    const double outside = std::hypot(beyond_end, beyond_side);
    Placed out;
    out.t = std::clamp(along / r.length_m, 0.0, 1.0);
    out.outside_m = outside;
    out.weight = outside == 0.0 ? 1.0 : smoothstep(1.0 - outside / runway_shoulder_m);
    return out;
}

std::vector<RunwayStrip> read_runway_strips(std::string_view text) {
    std::vector<RunwayStrip> out;
    std::size_t at = 0;
    int line_number = 0;
    bool header = false;
    while (at < text.size()) {
        const std::size_t end = text.find('\n', at);
        std::string_view line =
            text.substr(at, end == std::string_view::npos ? text.size() - at : end - at);
        at = end == std::string_view::npos ? text.size() : end + 1;
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::vector<std::string> f(1);
        for (const char c : line) {
            if (c == ',') {
                f.emplace_back();
            } else {
                f.back() += c;
            }
        }
        const auto refuse = [&](const std::string& why) {
            throw RunwayError("runway strips, line " + std::to_string(line_number) + ": " + why);
        };
        if (!header) {
            if (line != "airport,le_ident,he_ident,le_latitude_deg,le_longitude_deg,"
                        "le_elevation_ft,he_latitude_deg,he_longitude_deg,he_elevation_ft,"
                        "width_m") {
                refuse("not the header of a strips file");
            }
            header = true;
            continue;
        }
        if (f.size() != 10) {
            refuse("ten fields wanted, " + std::to_string(f.size()) + " found");
        }
        // A number; NaN for an empty field where `may_be_empty`.
        const auto number = [&](const std::string& field, bool may_be_empty) {
            if (field.empty() && may_be_empty) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            char* stop = nullptr;
            const double v = std::strtod(field.c_str(), &stop);
            if (field.empty() || *stop != '\0' || !std::isfinite(v)) {
                refuse("\"" + field + "\" is not a number");
            }
            return v;
        };
        RunwayStrip s;
        s.airport = f[0];
        s.le_ident = f[1];
        s.he_ident = f[2];
        s.le_latitude_deg = number(f[3], false);
        s.le_longitude_deg = number(f[4], false);
        s.le_elevation_ft = number(f[5], true);
        s.he_latitude_deg = number(f[6], false);
        s.he_longitude_deg = number(f[7], false);
        s.he_elevation_ft = number(f[8], true);
        s.width_m = number(f[9], true);
        if (s.airport.empty() || std::abs(s.le_latitude_deg) > 90.0 ||
            std::abs(s.he_latitude_deg) > 90.0 || std::abs(s.le_longitude_deg) > 180.0 ||
            std::abs(s.he_longitude_deg) > 180.0) {
            refuse("no airport, or a place off the Earth");
        }
        out.push_back(std::move(s));
    }
    if (!header) {
        throw RunwayError("runway strips: no header");
    }
    return out;
}

std::shared_ptr<const RunwaySurfaces> runway_surfaces(const std::filesystem::path& data) {
    static std::mutex lock;
    static std::map<std::filesystem::path, std::shared_ptr<const RunwaySurfaces>> made;
    const std::lock_guard<std::mutex> held(lock);
    std::shared_ptr<const RunwaySurfaces>& surfaces = made[data];
    if (!surfaces) {
        const std::filesystem::path path = data / "runways" / "strips.csv";
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            throw RunwayError("cannot read " + path.string());
        }
        surfaces = std::make_shared<const RunwaySurfaces>(
            read_runway_strips(std::string(std::istreambuf_iterator<char>(in), {})));
    }
    return surfaces;
}

CollisionGround::CollisionGround(std::shared_ptr<Dem> dem,
                                 std::shared_ptr<const RunwaySurfaces> runways,
                                 double tolerance_m)
    : dem_(std::move(dem)), runways_(std::move(runways)), tolerance_m_(tolerance_m) {}

CollisionGround::Surface CollisionGround::fit_line(std::size_t runway) {
    const RunwaySurfaces::Runway& r = runways_->at(runway);
    // **The fit to the DEM**: least squares through its heights every
    // fit_spacing_m along the centreline, end to end, in t from 0 to 1.
    const RunwayStrip& s = r.strip;
    const auto samples = static_cast<int>(std::ceil(r.length_m / fit_spacing_m)) + 1;
    double sum_t = 0.0;
    double sum_h = 0.0;
    double sum_tt = 0.0;
    double sum_th = 0.0;
    const Ecef le = to_ecef({s.le_latitude_deg, s.le_longitude_deg, 0.0});
    const Ecef he = to_ecef({s.he_latitude_deg, s.he_longitude_deg, 0.0});
    for (int k = 0; k < samples; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(samples - 1);
        const Geodetic g = to_geodetic(
            {le.x + t * (he.x - le.x), le.y + t * (he.y - le.y), le.z + t * (he.z - le.z)});
        const double h = dem_->height_above_geoid(g.latitude_deg, g.longitude_deg);
        sum_t += t;
        sum_h += h;
        sum_tt += t * t;
        sum_th += t * h;
    }
    const double n = static_cast<double>(samples);
    const double slope = (n * sum_th - sum_t * sum_h) / (n * sum_tt - sum_t * sum_t);
    const double fit_le = (sum_h - slope * sum_t) / n;
    return {fit_le, fit_le + slope, false};
}

CollisionGround::Surface CollisionGround::own_line(std::size_t runway) {
    const RunwayStrip& s = runways_->at(runway).strip;
    const Surface fit = fit_line(runway);
    const double fit_le = fit.le_m;
    const double fit_he = fit.he_m;
    const double file_le = s.le_elevation_ft * metres_per_foot;
    const double file_he = s.he_elevation_ft * metres_per_foot;
    if (std::isfinite(file_le) && std::isfinite(file_he) &&
        std::abs(file_le - fit_le) <= tolerance_m_ &&
        std::abs(file_he - fit_he) <= tolerance_m_) {
        return {file_le, file_he, true};
    }
    return {fit_le, fit_he, false};
}

const CollisionGround::Surface& CollisionGround::surface(std::size_t runway) {
    const auto found = surfaces_.find(runway);
    if (found != surfaces_.end()) {
        return found->second;
    }
    // **The runway's group, all at once**: each one's own line, then the
    // least change to their ends' heights that makes every tie meet
    // - x = x0 - C^T (C C^T)^-1 C x0, for C the ties' rows - solved by
    // Gaussian elimination with partial pivoting, in the group's order.
    const std::vector<std::uint32_t>& group = runways_->at(runway).group;
    std::vector<Surface> own;
    own.reserve(group.size());
    for (const std::uint32_t i : group) {
        own.push_back(own_line(i));
    }
    std::vector<Surface> lines = own;
    const auto member = [&](std::uint32_t i) {
        return static_cast<std::size_t>(
            std::lower_bound(group.begin(), group.end(), i) - group.begin());
    };
    // One row for each tie, a < b: its coefficients on the le and he
    // heights of a and of b, in the unknowns' order (2 per runway).
    struct Row {
        std::size_t a, b;
        double a_le, a_he, b_le, b_he;
    };
    std::vector<Row> constraints;
    for (const std::uint32_t i : group) {
        for (const RunwaySurfaces::Tie& c : runways_->at(i).ties) {
            if (i < c.other) {
                constraints.push_back({member(i), member(c.other), 1.0 - c.t, c.t, -(1.0 - c.other_t),
                                -c.other_t});
            }
        }
    }
    const auto solve = [&] {
        if (constraints.empty()) {
            return;
        }
        const std::size_t m = constraints.size();
        const auto x0 = [&](std::size_t k, bool he) { return he ? lines[k].he_m : lines[k].le_m; };
        // M = C C^T, and the misses C x0.
        std::vector<double> matrix(m * m, 0.0);
        std::vector<double> miss(m, 0.0);
        const auto coefficient = [&](const Row& row, std::size_t k, bool he) {
            double c = 0.0;
            if (row.a == k) {
                c += he ? row.a_he : row.a_le;
            }
            if (row.b == k) {
                c += he ? row.b_he : row.b_le;
            }
            return c;
        };
        for (std::size_t p = 0; p < m; ++p) {
            miss[p] = constraints[p].a_le * x0(constraints[p].a, false) + constraints[p].a_he * x0(constraints[p].a, true) +
                      constraints[p].b_le * x0(constraints[p].b, false) + constraints[p].b_he * x0(constraints[p].b, true);
            for (std::size_t q = 0; q < m; ++q) {
                double sum = 0.0;
                for (const std::size_t k : {constraints[p].a, constraints[p].b}) {
                    for (const bool he : {false, true}) {
                        sum += coefficient(constraints[p], k, he) * coefficient(constraints[q], k, he);
                    }
                }
                matrix[p * m + q] = sum;
            }
            // Three runways meeting at one place make one tie follow from
            // the other two; this keeps M invertible, and moves the answer
            // by far less than a millimetre.
            matrix[p * m + p] += 1e-9;
        }
        std::vector<double> y = miss;
        for (std::size_t col = 0; col < m; ++col) {
            std::size_t pivot = col;
            for (std::size_t r = col + 1; r < m; ++r) {
                if (std::abs(matrix[r * m + col]) > std::abs(matrix[pivot * m + col])) {
                    pivot = r;
                }
            }
            if (pivot != col) {
                for (std::size_t c = 0; c < m; ++c) {
                    std::swap(matrix[col * m + c], matrix[pivot * m + c]);
                }
                std::swap(y[col], y[pivot]);
            }
            for (std::size_t r = col + 1; r < m; ++r) {
                const double f = matrix[r * m + col] / matrix[col * m + col];
                for (std::size_t c = col; c < m; ++c) {
                    matrix[r * m + c] -= f * matrix[col * m + c];
                }
                y[r] -= f * y[col];
            }
        }
        for (std::size_t col = m; col-- > 0;) {
            for (std::size_t c = col + 1; c < m; ++c) {
                y[col] -= matrix[col * m + c] * y[c];
            }
            y[col] /= matrix[col * m + col];
        }
        // x = x0 - C^T y.
        for (std::size_t p = 0; p < m; ++p) {
            lines[constraints[p].a].le_m -= constraints[p].a_le * y[p];
            lines[constraints[p].a].he_m -= constraints[p].a_he * y[p];
            lines[constraints[p].b].le_m -= constraints[p].b_le * y[p];
            lines[constraints[p].b].he_m -= constraints[p].b_he * y[p];
        }
    };
    solve();
    // **A group the ties would move too far** - an end more than the
    // tolerance from its own line, which a wrong elevation in the file does,
    // even where each runway's own ends were near their fit - is made from
    // the fits to the DEM instead, every runway of it, and tied again.
    double moved = 0.0;
    for (std::size_t k = 0; k < group.size(); ++k) {
        moved = std::max({moved, std::abs(lines[k].le_m - own[k].le_m),
                          std::abs(lines[k].he_m - own[k].he_m)});
    }
    if (moved > tolerance_m_) {
        for (std::size_t k = 0; k < group.size(); ++k) {
            lines[k] = fit_line(group[k]);
        }
        solve();
    }
    for (std::size_t k = 0; k < group.size(); ++k) {
        surfaces_.emplace(group[k], lines[k]);
    }
    return surfaces_.at(runway);
}

double CollisionGround::height_above_geoid(double latitude_deg, double longitude_deg) {
    const std::span<const std::uint32_t> reaching =
        runways_->reaching(latitude_deg, longitude_deg);
    // The runways that reach the place, and how far outside the nearest's
    // rectangle it is.
    placed_.clear();
    double nearest_m = std::numeric_limits<double>::infinity();
    for (const std::uint32_t i : reaching) {
        const RunwaySurfaces::Placed p = runways_->place(i, latitude_deg, longitude_deg);
        if (p.weight > 0.0) {
            placed_.emplace_back(i, p);
            nearest_m = std::min(nearest_m, p.outside_m);
        }
    }
    double weights = 0.0;
    double weighted = 0.0;
    for (const auto& [i, p] : placed_) {
        const double further_m = p.outside_m - nearest_m;
        const double weight =
            further_m == 0.0 ? p.weight
                             : p.weight * smoothstep(1.0 - further_m / runway_overlap_band_m);
        if (weight > 0.0) {
            const Surface& s = surface(i);
            weights += weight;
            weighted += weight * (s.le_m + p.t * (s.he_m - s.le_m));
        }
    }
    if (weights == 0.0) {
        return dem_->height_above_geoid(latitude_deg, longitude_deg);
    }
    if (weights >= 1.0) {
        return weighted / weights;
    }
    return (1.0 - weights) * dem_->height_above_geoid(latitude_deg, longitude_deg) + weighted;
}

double CollisionGround::height_above_ellipsoid(double latitude_deg, double longitude_deg) {
    const Geoid* geoid = dem_->geoid();
    if (geoid == nullptr) {
        throw DemError("heights above the ellipsoid need the geoid");
    }
    return height_above_geoid(latitude_deg, longitude_deg) +
           geoid->undulation(latitude_deg, longitude_deg);
}

Water CollisionGround::water(double latitude_deg, double longitude_deg) {
    for (const std::uint32_t i : runways_->reaching(latitude_deg, longitude_deg)) {
        if (runways_->place(i, latitude_deg, longitude_deg).weight == 1.0) {
            return Water::none;
        }
    }
    return dem_->water(latitude_deg, longitude_deg);
}

} // namespace glideslope::world
