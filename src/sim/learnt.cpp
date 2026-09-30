#include "sim/learnt.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace glideslope::sim {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double degrees = 180.0 / pi;
constexpr double feet_per_metre = 3.280839895013123;
constexpr double mps_per_fps = 0.3048;

// As sim::Lander's, and tools/rl/landing.py's: the metres in a degree at the
// threshold.
double metres_per_degree_latitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111132.92 - 559.82 * std::cos(2.0 * lat) + 1.175 * std::cos(4.0 * lat) -
           0.0023 * std::cos(6.0 * lat);
}

double metres_per_degree_longitude(double latitude_deg) {
    const double lat = latitude_deg / degrees;
    return 111412.84 * std::cos(lat) - 93.5 * std::cos(3.0 * lat) +
           0.118 * std::cos(5.0 * lat);
}

struct Where {
    double along_m;  // before the threshold, positive
    double across_m; // right of the centreline, positive
    double above_m;  // above the threshold's elevation
};

Where where(const LandingReadings& r, const Runway& rw) {
    const double north_m = (r.values[0] - rw.threshold_lat_deg) *
                           metres_per_degree_latitude(rw.threshold_lat_deg);
    const double east_m = (r.values[1] - rw.threshold_lon_deg) *
                          metres_per_degree_longitude(rw.threshold_lat_deg);
    const double h = rw.heading_deg / degrees;
    const double past_m = east_m * std::sin(h) + north_m * std::cos(h);
    const double across_m = east_m * std::cos(h) - north_m * std::sin(h);
    const double above_m = (r.values[2] - rw.elevation_ft) / feet_per_metre;
    return {-past_m, across_m, above_m};
}

std::vector<double> numbers(const std::string& text, const std::string& what) {
    std::istringstream in(text);
    std::vector<double> out;
    std::string word;
    while (in >> word) {
        try {
            std::size_t used = 0;
            out.push_back(std::stod(word, &used));
            // std::stod reads "nan" and "inf" as numbers; a weight that is
            // one would fly every step as NaN.
            if (used != word.size() || !std::isfinite(out.back())) {
                throw std::invalid_argument(word);
            }
        } catch (const std::exception&) {
            throw std::runtime_error("the policy's " + what + " has '" + word +
                                     "', which is not a finite number");
        }
    }
    return out;
}

// A count or a size: a whole number from `least` to `most`, so that nothing
// is truncated or wrapped on the way to an integer.
std::size_t whole(double v, double least, double most, const std::string& what) {
    if (!(v >= least && v <= most) || v != std::floor(v)) {
        throw std::runtime_error("the policy's " + what + " is " + std::to_string(v) +
                                 ", not a whole number from " + std::to_string(least) +
                                 " to " + std::to_string(most));
    }
    return static_cast<std::size_t>(v);
}

// The widest layer a policy may have: far wider than any trained here, and
// small enough that a file cannot ask for memory it should not.
constexpr double widest_layer = 4096.0;

} // namespace

LearntPolicy LearntPolicy::read(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("cannot read the policy " + file.string());
    }
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty() && line.front() != '#') {
            lines.push_back(line);
        }
    }
    const auto fail = [&](const std::string& why) {
        return std::runtime_error(file.string() + ": " + why);
    };
    if (lines.empty() || lines.front() != "glideslope-policy 1") {
        throw fail("not a glideslope-policy 1 file");
    }
    std::map<std::string, std::string> kv;
    std::size_t i = 1;
    for (; i < lines.size(); ++i) {
        const auto space = lines[i].find(' ');
        const std::string key = lines[i].substr(0, space);
        kv[key] = space == std::string::npos ? "" : lines[i].substr(space + 1);
        if (key == "layers") {
            ++i;
            break;
        }
    }
    const auto one = [&](const std::string& key) {
        const auto found = kv.find(key);
        if (found == kv.end()) {
            throw fail("no " + key);
        }
        const std::vector<double> v = numbers(found->second, key);
        if (v.size() != 1) {
            throw fail(key + " is not one number");
        }
        return v.front();
    };
    LearntPolicy p;
    if (kv.count("aircraft") == 0 || kv.count("task") == 0 || kv["task"] != "landing") {
        throw fail("not a landing policy for a named aircraft");
    }
    p.aircraft = kv["aircraft"];
    // A decision from every step to every ten seconds.
    p.decision_steps = static_cast<int>(whole(one("decision_steps"), 1.0, 1200.0, "decision_steps"));
    p.vref_kts = one("vref_kts");
    p.flaps = one("flaps");
    p.glidepath_deg = one("glidepath_deg");
    p.aim_m = one("aim_m");
    if (whole(one("observations"), 1.0, widest_layer, "observations") != observations ||
        whole(one("actions"), 1.0, widest_layer, "actions") != actions) {
        throw fail("its observations and actions are not the " +
                   std::to_string(observations) + " and " + std::to_string(actions) +
                   " the simulation makes and takes");
    }
    p.obs_mean = numbers(kv["obs_mean"], "obs_mean");
    p.obs_scale = numbers(kv["obs_scale"], "obs_scale");
    if (p.obs_mean.size() != observations || p.obs_scale.size() != observations) {
        throw fail("obs_mean and obs_scale are not one number an observation");
    }
    p.obs_clip = one("obs_clip");
    if (!(p.obs_clip > 0.0)) {
        throw fail("obs_clip is not above nothing");
    }
    const std::size_t count = whole(one("layers"), 1.0, 64.0, "layers");
    std::size_t inputs = observations;
    for (std::size_t l = 0; l < count; ++l) {
        if (i >= lines.size()) {
            throw fail("fewer layers than it says");
        }
        std::istringstream head(lines[i++]);
        std::string word, n_in, n_out, activation, extra;
        Layer layer;
        head >> word >> n_in >> n_out >> activation;
        if (word != "layer" || (head >> extra)) {
            throw fail("layer " + std::to_string(l) + "'s heading is not 'layer IN OUT ACTIVATION'");
        }
        const std::vector<double> in_n = numbers(n_in, "layer size");
        const std::vector<double> out_n = numbers(n_out, "layer size");
        if (in_n.size() != 1 || out_n.size() != 1) {
            throw fail("layer " + std::to_string(l) + " does not say its sizes");
        }
        layer.inputs = whole(in_n.front(), 1.0, widest_layer, "layer size");
        layer.outputs = whole(out_n.front(), 1.0, widest_layer, "layer size");
        if (layer.inputs != inputs || (activation != "tanh" && activation != "linear")) {
            throw fail("layer " + std::to_string(l) + " does not follow the one before");
        }
        layer.tanh = activation == "tanh";
        for (std::size_t o = 0; o < layer.outputs; ++o) {
            if (i >= lines.size()) {
                throw fail("layer " + std::to_string(l) + " is cut short");
            }
            const std::vector<double> row = numbers(lines[i++], "weights");
            if (row.size() != layer.inputs) {
                throw fail("a row of layer " + std::to_string(l) + " has " +
                           std::to_string(row.size()) + " weights, not " +
                           std::to_string(layer.inputs));
            }
            layer.weights.insert(layer.weights.end(), row.begin(), row.end());
        }
        if (i >= lines.size() || lines[i].rfind("bias ", 0) != 0) {
            throw fail("layer " + std::to_string(l) + " has no bias");
        }
        layer.bias = numbers(lines[i++].substr(5), "bias");
        if (layer.bias.size() != layer.outputs) {
            throw fail("layer " + std::to_string(l) + "'s bias is the wrong length");
        }
        inputs = layer.outputs;
        p.layers.push_back(std::move(layer));
    }
    if (p.layers.empty() || inputs != actions) {
        throw fail("its last layer does not give " + std::to_string(actions) + " actions");
    }
    return p;
}

std::array<double, LearntPolicy::actions>
LearntPolicy::act(const std::vector<double>& observation) const {
    if (observation.size() != observations) {
        throw std::invalid_argument("an observation of the wrong size");
    }
    std::vector<double> x(observations);
    for (std::size_t k = 0; k < observations; ++k) {
        x[k] = std::clamp((observation[k] - obs_mean[k]) * obs_scale[k], -obs_clip, obs_clip);
    }
    for (const Layer& layer : layers) {
        std::vector<double> y(layer.outputs);
        for (std::size_t o = 0; o < layer.outputs; ++o) {
            // Summed in the order tools/rl/policy_file.py sums: the bias,
            // then each weight in turn.
            double total = layer.bias[o];
            for (std::size_t k = 0; k < layer.inputs; ++k) {
                total += layer.weights[o * layer.inputs + k] * x[k];
            }
            y[o] = layer.tanh ? std::tanh(total) : total;
        }
        x = std::move(y);
    }
    std::array<double, actions> out{};
    for (std::size_t k = 0; k < actions; ++k) {
        out[k] = std::clamp(x[k], -1.0, 1.0);
    }
    return out;
}

const std::array<const char*, LandingReadings::count>& LandingReadings::names() {
    static const std::array<const char*, count> n{
        "position/lat-geod-deg", "position/long-gc-deg", "position/h-sl-ft",
        "attitude/phi-rad",      "attitude/theta-rad",   "attitude/psi-rad",
        "velocities/p-rad_sec",  "velocities/q-rad_sec", "velocities/r-rad_sec",
        "velocities/vc-kts",     "velocities/v-north-fps", "velocities/v-east-fps",
        "velocities/v-down-fps", "aero/alpha-rad",       "aero/beta-rad",
        "gear/wow",              "velocities/vtrue-fps"};
    return n;
}

LandingReadings LandingReadings::of(const Aircraft& aircraft) {
    LandingReadings r;
    for (std::size_t k = 0; k < count; ++k) {
        r.values[k] = aircraft.property(names()[k]);
    }
    return r;
}

std::vector<double> landing_observation(const LandingReadings& r, const Runway& runway,
                                        const LearntPolicy& policy,
                                        const std::array<double, LearntPolicy::actions>& previous,
                                        double integral) {
    const Where w = where(r, runway);
    const double h = runway.heading_deg / degrees;
    const double glidepath_m = (w.along_m + policy.aim_m) * std::tan(policy.glidepath_deg / degrees);
    const double heading_error = std::remainder(r.values[5] - h, 2.0 * pi);
    const double vn = r.values[10] * mps_per_fps;
    const double ve = r.values[11] * mps_per_fps;
    const double v_along = ve * std::sin(h) + vn * std::cos(h);
    const double v_across = ve * std::cos(h) - vn * std::sin(h);
    const double climb = -r.values[12] * mps_per_fps;
    const double track = std::atan2(ve, vn);
    const double drift = std::remainder(track - r.values[5], 2.0 * pi);
    const double vt = r.values[16] * mps_per_fps;
    const double air = r.values[5] + r.values[14];
    const double wind_n = vn - vt * std::cos(air);
    const double wind_e = ve - vt * std::sin(air);
    const double wind_across = wind_e * std::cos(h) - wind_n * std::sin(h);
    const double wind_along = wind_e * std::sin(h) + wind_n * std::cos(h);
    return {w.along_m / 1000.0,
            w.across_m / 30.0,
            (w.above_m - glidepath_m) / 10.0,
            w.above_m / 30.0,
            heading_error,
            v_across / 5.0,
            v_along / 30.0,
            climb / 3.0,
            (r.values[9] - policy.vref_kts) / 10.0,
            r.values[3],
            r.values[4],
            r.values[6],
            r.values[7],
            r.values[8],
            r.values[13],
            r.values[14],
            r.values[15] > 0.5 ? 1.0 : 0.0,
            previous[0],
            previous[1],
            previous[2],
            previous[3],
            drift,
            wind_across / 5.0,
            wind_along / 5.0,
            integral / 300.0};
}

Controls landing_controls(const std::array<double, LearntPolicy::actions>& action,
                          const LearntPolicy& policy) {
    Controls c;
    c.elevator = std::clamp(action[0], -1.0, 1.0);
    c.aileron = std::clamp(action[1], -1.0, 1.0);
    c.rudder = std::clamp(action[2], -1.0, 1.0);
    c.throttle = (std::clamp(action[3], -1.0, 1.0) + 1.0) / 2.0;
    c.mixture = 1.0;
    c.propeller = 1.0;
    c.flaps = policy.flaps;
    c.gear = 1.0;
    c.pitch_trim = 0.0;
    c.left_brake = 0.0;
    c.right_brake = 0.0;
    return c;
}

double remember_drift(double integral, double across_m, const LearntPolicy& policy) {
    constexpr double memory_s = 10.0;
    const double dt = static_cast<double>(policy.decision_steps) / 120.0;
    return integral * std::exp(-dt / memory_s) + across_m * dt;
}

namespace {

// A pilot's hand: full travel in a second, as sim::Controller moves one.
bool towards(double& from, double to) {
    constexpr double step = 1.0 / 120.0;
    from += std::clamp(to - from, -step, step);
    return from == to;
}

bool towards(Controls& from, const Controls& to) {
    bool met = true;
    for (auto [control, wanted] :
         {std::pair{&from.aileron, to.aileron}, std::pair{&from.elevator, to.elevator},
          std::pair{&from.rudder, to.rudder}, std::pair{&from.throttle, to.throttle},
          std::pair{&from.mixture, to.mixture}, std::pair{&from.flaps, to.flaps},
          std::pair{&from.left_brake, to.left_brake},
          std::pair{&from.right_brake, to.right_brake},
          std::pair{&from.pitch_trim, to.pitch_trim},
          std::pair{&from.propeller, to.propeller}, std::pair{&from.gear, to.gear},
          std::pair{&from.speedbrake, to.speedbrake}}) {
        met = towards(*control, wanted) && met;
    }
    return met;
}

} // namespace

LearntLander::LearntLander(const Aircraft& aircraft, const Runway& runway,
                           std::shared_ptr<const LearntPolicy> policy,
                           const ApproachSpeeds& speeds)
    : a_(aircraft), runway_(runway), policy_(std::move(policy)),
      rollout_(aircraft, runway, speeds, policy_ ? policy_->glidepath_deg : 3.0) {
    if (!policy_) {
        throw std::invalid_argument("a learnt lander needs a policy");
    }
    // A policy flies the aircraft it was trained on, and no other.
    if (policy_->aircraft != aircraft.figures().model) {
        throw std::invalid_argument("the learnt landing is the " + policy_->aircraft +
                                    "'s, and this is the " + aircraft.figures().model);
    }
}

Controls LearntLander::fly() {
    if (stage_ != Stage::flying) {
        const Controls rolling = rollout_.fly();
        if (easing_) {
            easing_ = !towards(held_, rolling);
        } else {
            held_ = rolling;
        }
        if (rollout_.stage() == Lander::Stage::stopped) {
            stage_ = Stage::stopped;
        }
        return held_;
    }
    // The rollout's lander watches, to know where she touched.
    rollout_.watch();
    const LandingReadings r = LandingReadings::of(a_);
    if (!touched_ && r.values[15] > 0.5) {
        const Where w = where(r, runway_);
        touched_ = true;
        touchdown_sink_fpm_ = -a_.property("velocities/h-dot-fps") * 60.0;
        touchdown_across_m_ = w.across_m;
        touchdown_along_m_ = -w.along_m;
    }
    if (touched_) {
        stage_ = Stage::rollout;
        rollout_.resume(held_.throttle);
        easing_ = true;
        return fly();
    }
    if (steps_ % policy_->decision_steps == 0) {
        integral_ = remember_drift(integral_, where(r, runway_).across_m, *policy_);
        const std::array<double, LearntPolicy::actions> action =
            policy_->act(landing_observation(r, runway_, *policy_, previous_, integral_));
        held_ = landing_controls(action, *policy_);
        previous_ = action;
        ++decisions_;
    }
    ++steps_;
    return held_;
}

} // namespace glideslope::sim
