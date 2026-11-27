// The aircraft heard (src/audio/): her engines, the wind, her gear, flaps,
// touchdown and stall warning, and other aircraft by where they are - mixed
// from her flight and her sound set (assets/sound), and played on SDL's
// dummy audio driver, whose mix the device's tap hands back.
//
// Every situation is built, not waited for: a flight is flown a counted
// number of 120 Hz steps, each step's sound mixed for that step's time, and
// what is heard is measured on the mix tapped.

#include "harness.hpp"

#include "audio/device.hpp"
#include "audio/heard.hpp"
#include "audio/mixer.hpp"
#include "audio/sound_set.hpp"
#include "sim/aircraft.hpp"
#include "sim/catalogue.hpp"
#include "sim/learnt.hpp"
#include "sim/lift_peak.hpp"
#include "sim/terrain.hpp"
#include "sim/test_pilot.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

using glideslope::audio::Device;
using glideslope::audio::EngineHeard;
using glideslope::audio::Heard;
using glideslope::audio::Mixer;
using glideslope::audio::OtherHeard;
using glideslope::audio::SoundSet;
using glideslope::sim::CatalogueEntry;
using glideslope::test::check;

namespace {

constexpr int steps_per_second = 120;
constexpr double step_s = 1.0 / steps_per_second;

std::filesystem::path data() {
    return std::filesystem::path(GLIDESLOPE_TEST_DATA_DIR).parent_path();
}

// **SDL's dummy audio driver, asked for by name** and held to it: the mix is
// played on it, as a window client's is on a real one.
std::unique_ptr<Device> dummy_device() {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    auto device = std::make_unique<Device>();
    check(device->driver() == "dummy", "the device is on SDL's dummy driver, not " +
                                           device->driver());
    return device;
}

// One channel of an interleaved stereo mix, from frame `from` to `to`.
std::vector<double> channel(const std::vector<float>& mix, std::size_t c, std::size_t from,
                            std::size_t to) {
    std::vector<double> out;
    for (std::size_t f = from; f < to && 2 * f + c < mix.size(); ++f) {
        out.push_back(static_cast<double>(mix[2 * f + c]));
    }
    return out;
}

double rms(const std::vector<double>& x) {
    double sum = 0.0;
    for (const double v : x) {
        sum += v * v;
    }
    return x.empty() ? 0.0 : std::sqrt(sum / static_cast<double>(x.size()));
}

// Both channels, from frame `from` to `to`.
double rms_of(const std::vector<float>& mix, std::size_t from, std::size_t to) {
    return std::sqrt(0.5 * (std::pow(rms(channel(mix, 0, from, to)), 2) +
                            std::pow(rms(channel(mix, 1, from, to)), 2)));
}

// **The strongest frequency of a mix's left channel between `low` and `high`**:
// its last 65,536 frames - 1.37 s, bins of 0.73 Hz - Hann-windowed through a
// radix-2 FFT, the peak bin refined by a parabola through it and its
// neighbours' logarithms.
double peak_hz(const std::vector<float>& mix, double low, double high) {
    constexpr std::size_t n = 65536;
    const std::size_t frames = mix.size() / 2;
    check(frames >= n, "the mix holds 65,536 frames to measure");
    std::vector<std::complex<double>> a(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                                              static_cast<double>(n - 1));
        a[i] = w * static_cast<double>(mix[2 * (frames - n + i)]);
    }
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * std::numbers::pi / static_cast<double>(len);
        const std::complex<double> wl(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    const double bin_hz = static_cast<double>(glideslope::audio::mix_rate) / static_cast<double>(n);
    const auto first = static_cast<std::size_t>(std::max(1.0, std::floor(low / bin_hz)));
    const auto last = std::min(n / 2 - 2, static_cast<std::size_t>(std::ceil(high / bin_hz)));
    std::size_t best = first;
    for (std::size_t k = first; k <= last; ++k) {
        if (std::abs(a[k]) > std::abs(a[best])) {
            best = k;
        }
    }
    const double l = std::log(std::abs(a[best - 1]) + 1e-30);
    const double c = std::log(std::abs(a[best]) + 1e-30);
    const double r = std::log(std::abs(a[best + 1]) + 1e-30);
    const double denominator = l - 2.0 * c + r;
    const double offset = denominator != 0.0 ? 0.5 * (l - r) / denominator : 0.0;
    return (static_cast<double>(best) + offset) * bin_hz;
}

std::vector<CatalogueEntry> catalogue() {
    return glideslope::sim::read_catalogue(data());
}

// The sixteen aircraft the catalogue holds today: a test walking them says
// how many it walked, and fails when it is not all of them.
constexpr std::size_t aircraft_in_the_catalogue = 16;

// **Her start in the air**, as the server gives a player: 3,000 ft at her
// catalogue airspeed, gear up, trimmed.
glideslope::sim::InitialConditions in_the_air(const CatalogueEntry& e) {
    glideslope::sim::InitialConditions ic;
    ic.latitude_deg = -33.9;
    ic.longitude_deg = 151.2;
    ic.altitude_ft = 3000.0;
    ic.airspeed_kts = e.start_airspeed_kts;
    ic.engine_running = true;
    ic.gear = 0.0;
    ic.trim = true;
    return ic;
}

// The mean drive of her running engines.
double mean_drive(const Heard& h) {
    double sum = 0.0;
    int n = 0;
    for (const EngineHeard& e : h.engines) {
        if (e.running) {
            sum += e.drive;
            ++n;
        }
    }
    return n > 0 ? sum / n : 0.0;
}

} // namespace

// **Every aircraft in the catalogue has a sound set, and it is hers**: her
// file in data/sound reads; it gives a gear motor exactly where her gear
// retracts and a flap motor exactly where her flap lever works flaps; and
// her engine is a piston where her flight model's engines report their RPM
// and a turbine where they report N1. Nothing in data/sound is for an
// aircraft the catalogue has not got.
GLIDESLOPE_TEST(every_aircraft_in_the_catalogue_has_a_sound_set_that_matches_her_flight_model) {
    const std::vector<CatalogueEntry> all = catalogue();
    std::size_t matched = 0;
    std::string wrong;
    for (const CatalogueEntry& e : all) {
        const SoundSet s = glideslope::audio::read_sound_set(data(), e.id);
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        const bool turbine = aircraft.has_property("propulsion/engine[0]/n1");
        const bool piston = aircraft.has_property("propulsion/engine[0]/engine-rpm");
        const bool engine_ok = turbine != piston &&
                               (s.engine == glideslope::audio::EngineKind::turbine) == turbine;
        const bool gear_ok = s.gear.has_value() == aircraft.gear_retracts();
        const bool flaps_ok = s.flaps.has_value() == aircraft.flap_lever();
        std::printf("  %-13s %s engine%s, gear motor %s, flap motor %s\n", e.id.c_str(),
                    turbine ? "turbine" : "piston", engine_ok ? "" : " (WRONG)",
                    s.gear ? "yes" : "no", s.flaps ? "yes" : "no");
        if (engine_ok && gear_ok && flaps_ok) {
            ++matched;
        } else {
            wrong += " " + e.id + (engine_ok ? "" : "(engine)") + (gear_ok ? "" : "(gear)") +
                     (flaps_ok ? "" : "(flaps)");
        }
    }
    std::size_t files = 0;
    std::string strays;
    for (const auto& f : std::filesystem::directory_iterator(data() / "sound")) {
        ++files;
        const std::string id = f.path().stem().string();
        if (f.path().extension() != ".sound" ||
            std::none_of(all.begin(), all.end(),
                         [&](const CatalogueEntry& e) { return e.id == id; })) {
            strays += " " + f.path().filename().string();
        }
    }
    check(wrong.empty(), "every sound set matches her flight model; these do not:" + wrong);
    check(strays.empty(), "data/sound holds sound sets for catalogued aircraft only:" + strays);
    check(all.size() == aircraft_in_the_catalogue && matched == all.size() &&
              files == all.size(),
          "all " + std::to_string(aircraft_in_the_catalogue) + " aircraft have their set: " +
              std::to_string(matched) + " matched of " + std::to_string(all.size()) +
              " catalogued, " + std::to_string(files) + " files");
}

// **A sound set wrong is refused, and says where**: a line unknown, a number
// out of its range, and each line every set must give, left out.
GLIDESLOPE_TEST(a_sound_set_with_a_line_wrong_or_missing_is_refused) {
    const std::string whole = "engine piston 4 0.25\nwind 0.15 110\ntouchdown 0.4\n"
                              "stall-warning 1600 0.1\nheard-as-other 2400\n";
    (void)glideslope::audio::parse_sound_set("whole", whole);
    const std::vector<std::pair<std::string, std::string>> wrong = {
        {"an unknown line", whole + "horn 1\n"},
        {"a gain past full scale", "engine piston 4 1.5\n" + whole.substr(whole.find('\n') + 1)},
        {"a turbine with no blades", "engine turbine 5000 0.28\n" +
                                         whole.substr(whole.find('\n') + 1)},
        {"no engine", whole.substr(whole.find('\n') + 1)},
        {"no wind", "engine piston 4 0.25\ntouchdown 0.4\nstall-warning 1600 0.1\n"
                    "heard-as-other 2400\n"},
        {"no touchdown", "engine piston 4 0.25\nwind 0.15 110\nstall-warning 1600 0.1\n"
                         "heard-as-other 2400\n"},
        {"no stall warning", "engine piston 4 0.25\nwind 0.15 110\ntouchdown 0.4\n"
                             "heard-as-other 2400\n"},
        {"no drive heard as another", "engine piston 4 0.25\nwind 0.15 110\ntouchdown 0.4\n"
                                      "stall-warning 1600 0.1\n"},
    };
    std::size_t refused = 0;
    for (const auto& [what, text] : wrong) {
        try {
            (void)glideslope::audio::parse_sound_set("wrong", text);
            std::printf("  %-26s taken\n", what.c_str());
        } catch (const glideslope::audio::SoundSetError& error) {
            std::printf("  %-26s refused: %s\n", what.c_str(), error.what());
            ++refused;
        }
    }
    check(refused == wrong.size() && wrong.size() == 8,
          "all 8 wrong sets refused: " + std::to_string(refused));
}

// **Her engine heard rises in pitch with her RPM, in every aircraft**: begun
// in the air, flown 15 s on her throttle and propeller levers shut - her
// engines' slowest - and then 15 s with both full forward, the mix played on
// the dummy driver every step. Over the last 1.37 s of each, the strongest
// frequency of the mix is her set's fundamental at the drive her flight
// model reports - her firing rate from her RPM, or her fan's from her N1 -
// within 2% and a bin, and it is higher at full power than shut, by as much
// as her drive rose.
GLIDESLOPE_TEST(every_aircrafts_engine_heard_on_the_dummy_driver_rises_in_pitch_with_her_rpm) {
    const auto device = dummy_device();
    const std::vector<CatalogueEntry> all = catalogue();
    std::size_t rose = 0;
    std::string failed;
    for (const CatalogueEntry& e : all) {
        const SoundSet set = glideslope::audio::read_sound_set(data(), e.id);
        glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
        aircraft.initialize(in_the_air(e));
        glideslope::sim::Controls controls = glideslope::sim::held_until_moved(aircraft);
        Mixer mixer(set, glideslope::audio::mix_rate);
        // The fundamental heard and the one her drive says, shut and full.
        double heard_hz[2] = {0.0, 0.0};
        double said_hz[2] = {0.0, 0.0};
        bool off_the_ground = true;
        for (int lever = 0; lever < 2; ++lever) {
            controls.throttle = lever;
            controls.propeller = lever;
            std::vector<float> mix;
            Heard heard;
            double drive = 0.0;
            int measured = 0;
            for (int i = 0; i < 15 * steps_per_second; ++i) {
                aircraft.set_controls(controls);
                aircraft.step();
                heard = glideslope::audio::heard_of(aircraft, set);
                mixer.hear(heard);
                // The last 1.4 s are tapped, and her drive over them averaged.
                if (i == 15 * steps_per_second - 168) {
                    device->tap(&mix);
                }
                if (i >= 15 * steps_per_second - 168) {
                    drive += mean_drive(heard);
                    ++measured;
                }
                device->play(mixer, step_s);
            }
            device->tap(nullptr);
            off_the_ground = off_the_ground && !heard.on_surface;
            said_hz[lever] = set.fundamental_hz(drive / measured);
            heard_hz[lever] = peak_hz(mix, 5.0, 2000.0);
        }
        const double bin_hz = glideslope::audio::mix_rate / 65536.0;
        bool ok = off_the_ground;
        for (int lever = 0; lever < 2; ++lever) {
            ok = ok && std::abs(heard_hz[lever] - said_hz[lever]) <= 0.02 * said_hz[lever] + bin_hz;
        }
        ok = ok && said_hz[1] > said_hz[0] + 4.0 * bin_hz && heard_hz[1] > heard_hz[0];
        std::printf("  %-13s shut: heard %.1f Hz, her drive says %.1f; full: heard %.1f Hz, "
                    "says %.1f%s\n",
                    e.id.c_str(), heard_hz[0], said_hz[0], heard_hz[1], said_hz[1],
                    ok ? "" : "  <- WRONG");
        if (ok) {
            ++rose;
        } else {
            failed += " " + e.id;
        }
    }
    check(failed.empty(), "every engine heard at her drive's pitch, rising; not:" + failed);
    check(all.size() == aircraft_in_the_catalogue && rose == all.size(),
          std::to_string(rose) + " of " + std::to_string(all.size()) + " aircraft heard rising");
}

// **With her engines stopped, standing still, she is silent - and with them
// running she is not**: every aircraft started on the ground (a seaplane
// afloat, settled 30 s first) with her engines stopped, flown 2 s and still
// under 1 kt, is mixed
// below 1e-4 of full scale (-80 dBFS) over 1 s; started so with them
// running at idle, the same second is above 1e-2 (-40 dBFS), so that the
// measure is seen to hear an engine.
GLIDESLOPE_TEST(every_aircraft_heard_with_her_engines_stopped_on_the_ground_is_silent) {
    constexpr double silent = 1e-4;
    constexpr double heard_level = 1e-2;
    const auto device = dummy_device();
    const std::vector<CatalogueEntry> all = catalogue();
    std::size_t built = 0;
    std::string failed;
    for (const CatalogueEntry& e : all) {
        const SoundSet set = glideslope::audio::read_sound_set(data(), e.id);
        double level[2] = {0.0, 0.0};
        bool as_built = true;
        for (int running = 0; running < 2; ++running) {
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = -33.9;
            ic.longitude_deg = 151.2;
            ic.engine_running = running == 1;
            ic.gear = 1.0;
            if (e.seaplane) {
                aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
                    [](double, double) { return 0.0; }, [](double, double) { return true; }));
                ic.altitude_ft = 6.0;
            }
            aircraft.initialize(ic);
            glideslope::sim::Controls idle;
            idle.left_brake = 1.0;
            idle.right_brake = 1.0;
            idle.mixture = 1.0;
            const int settle = e.seaplane ? 30 * steps_per_second : 0;
            for (int i = 0; i < settle; ++i) {
                aircraft.set_controls(idle);
                aircraft.step();
            }
            Mixer mixer(set, glideslope::audio::mix_rate);
            std::vector<float> mix;
            Heard heard;
            for (int i = 0; i < 2 * steps_per_second; ++i) {
                aircraft.set_controls(idle);
                aircraft.step();
                heard = glideslope::audio::heard_of(aircraft, set);
                mixer.hear(heard);
                if (i == steps_per_second) {
                    device->tap(&mix);
                }
                device->play(mixer, step_s);
            }
            device->tap(nullptr);
            level[running] = rms_of(mix, 0, mix.size() / 2);
            const bool stopped = std::none_of(heard.engines.begin(), heard.engines.end(),
                                              [](const EngineHeard& x) { return x.running; });
            // Standing still stopped; idling, a seaplane afloat - with no
            // brakes on the water - drifts at a few knots.
            const bool this_built = heard.on_surface &&
                                    heard.airspeed_kts < (running == 1 ? 10.0 : 1.0) &&
                                    stopped == (running == 0) && !heard.engines.empty();
            if (!this_built) {
                std::printf("  %-13s %s not as built: %s, %.1f kt, engines %s\n", e.id.c_str(),
                            running == 1 ? "idling" : "stopped",
                            heard.on_surface ? "on the surface" : "off it", heard.airspeed_kts,
                            stopped ? "stopped" : "running");
            }
            as_built = as_built && this_built;
        }
        const bool ok = as_built && level[0] < silent && level[1] > heard_level;
        std::printf("  %-13s stopped %.2e, idling %.2e of full scale%s\n", e.id.c_str(), level[0],
                    level[1], ok ? "" : "  <- WRONG");
        if (ok) {
            ++built;
        } else {
            failed += " " + e.id;
        }
    }
    check(failed.empty(), "every aircraft silent stopped and heard idling; not:" + failed);
    check(all.size() == aircraft_in_the_catalogue && built == all.size(),
          std::to_string(built) + " of " + std::to_string(all.size()) + " aircraft heard so");
}

// **The wind heard grows louder with airspeed, in every aircraft**: her
// engines stopped and nothing else sounding, at every 20 kt from 0 to 600,
// each speed mixed 0.5 s on the dummy driver and measured over its last
// 0.25 s - louder at each speed than at the one before, and nothing at 0.
GLIDESLOPE_TEST(the_wind_heard_grows_louder_with_airspeed_in_every_aircraft) {
    const auto device = dummy_device();
    const std::vector<CatalogueEntry> all = catalogue();
    constexpr int speeds = 31;
    std::size_t walked = 0;
    std::string failed;
    for (const CatalogueEntry& e : all) {
        const SoundSet set = glideslope::audio::read_sound_set(data(), e.id);
        double before = -1.0;
        bool rising = true;
        std::string levels;
        for (int s = 0; s < speeds; ++s) {
            Heard heard;
            heard.airspeed_kts = 20.0 * s;
            Mixer mixer(set, glideslope::audio::mix_rate);
            mixer.hear(heard);
            std::vector<float> mix;
            device->tap(&mix);
            for (int i = 0; i < steps_per_second / 2; ++i) {
                device->play(mixer, step_s);
            }
            device->tap(nullptr);
            const double level = rms_of(mix, mix.size() / 4, mix.size() / 2);
            rising = rising && (s == 0 ? level == 0.0 : level > before);
            before = level;
            ++walked;
            if (s % 10 == 0) {
                levels += " " + std::to_string(20 * s) + " kt " + std::to_string(level);
            }
        }
        std::printf("  %-13s%s%s\n", e.id.c_str(), levels.c_str(), rising ? "" : "  <- WRONG");
        if (!rising) {
            failed += " " + e.id;
        }
    }
    check(failed.empty(), "the wind rose at every step of speed in every aircraft; not:" + failed);
    check(all.size() == aircraft_in_the_catalogue && walked == all.size() * speeds,
          std::to_string(walked) + " of " + std::to_string(all.size() * speeds) +
              " speeds heard");
}

// **Every event is heard from the step it happens, in every aircraft that
// has it**: her gear moving, her flaps moving, her touchdown and her stall
// warning. Each is built as two flights of Heards, a step at a time, the
// same but for the event, begun at step 60: her engines at the drive she is
// heard at by others and half power, 120 kt, 5 degrees of alpha, gear down,
// flaps up and off the ground. Then the gear runs up over 6 s, the flaps
// down over 6 s, she meets the surface sinking 8 ft/s, or her alpha goes 2
// degrees past her warning's. The two mixes are the same, sample for sample,
// before the event, and the difference after it - the event alone - is at
// least a tenth of the rest of the mix (-20 dB) over its first quarter
// second.
//
// **Not built, named**: the gear motor of an aircraft whose gear does not
// retract, and the flap motor of one with no flaps her lever works - their
// sets give none (every_aircraft_in_the_catalogue_has_a_sound_set_...).
GLIDESLOPE_TEST(every_event_is_heard_from_the_step_it_happens_in_every_aircraft) {
    const auto device = dummy_device();
    const std::vector<CatalogueEntry> all = catalogue();
    constexpr int begins = 60;
    constexpr int steps = begins + 30;
    std::size_t heard_events = 0;
    std::size_t events = 0;
    std::size_t not_built = 0;
    std::string failed;
    enum Event { gear, flaps, touchdown, stall };
    const char* names[] = {"gear", "flaps", "touchdown", "stall warning"};
    for (const CatalogueEntry& e : all) {
        const SoundSet set =
            glideslope::audio::own_sound_set(data(), e.id, e.model, e.start_airspeed_kts);
        glideslope::sim::Aircraft model(data() / "jsbsim", e.model);
        const int engines = model.engine_count();
        std::string line;
        for (int ev = gear; ev <= stall; ++ev) {
            if ((ev == gear && !set.gear) || (ev == flaps && !set.flaps)) {
                ++not_built;
                continue;
            }
            ++events;
            std::vector<float> mixes[2];
            bool flagged = false;
            for (int with = 0; with < 2; ++with) {
                Mixer mixer(set, glideslope::audio::mix_rate);
                device->tap(&mixes[with]);
                for (int i = 0; i < steps; ++i) {
                    Heard h;
                    h.engines.assign(static_cast<std::size_t>(engines),
                                     EngineHeard{true, set.heard_as_other_drive, 0.5});
                    h.airspeed_kts = 120.0;
                    h.alpha_deg = 5.0;
                    h.gear = 1.0;
                    h.flaps = 0.0;
                    const bool on = with == 1 && i >= begins;
                    const double since = (i - begins + 1) / (6.0 * steps_per_second);
                    if (on && ev == gear) {
                        h.gear = 1.0 - since;
                    } else if (on && ev == flaps) {
                        h.flaps = since;
                    } else if (on && ev == touchdown) {
                        h.on_surface = true;
                        h.sink_fps = 8.0;
                    } else if (on && ev == stall) {
                        h.alpha_deg = set.stall_warning_alpha_deg(h.flaps) + 2.0;
                    }
                    mixer.hear(h);
                    if (on && i == begins) {
                        flagged = (ev == gear && mixer.gear_moving()) ||
                                  (ev == flaps && mixer.flaps_moving()) ||
                                  (ev == touchdown && mixer.touchdowns() == 1) ||
                                  (ev == stall && mixer.stall_warning());
                    }
                    device->play(mixer, step_s);
                }
                device->tap(nullptr);
            }
            const std::size_t at = mixes[0].size() / 2 * begins / steps;
            const std::size_t quarter = glideslope::audio::mix_rate / 4;
            double before = 0.0;
            for (std::size_t i = 0; i < 2 * at; ++i) {
                before = std::max(before, std::abs(static_cast<double>(mixes[1][i] - mixes[0][i])));
            }
            std::vector<float> difference(mixes[0].size());
            for (std::size_t i = 0; i < difference.size(); ++i) {
                difference[i] = mixes[1][i] - mixes[0][i];
            }
            const double event = rms_of(difference, at, at + quarter);
            const double rest = rms_of(mixes[0], at, at + quarter);
            const bool ok = flagged && before == 0.0 && event >= 0.1 * rest;
            line += std::string(" ") + names[ev] + " " + std::to_string(event / rest) +
                    (ok ? "" : "(WRONG)");
            if (ok) {
                ++heard_events;
            } else {
                failed += " " + e.id + "'s " + names[ev];
            }
        }
        std::printf("  %-13s event over the rest:%s\n", e.id.c_str(), line.c_str());
    }
    check(failed.empty(), "every event heard from its step; not:" + failed);
    check(all.size() == aircraft_in_the_catalogue && heard_events == events &&
              events + not_built == 4 * all.size(),
          std::to_string(heard_events) + " of " + std::to_string(events) +
              " events heard; " + std::to_string(not_built) +
              " not built, for aircraft without the motor, of " +
              std::to_string(4 * all.size()));
}

// **Another aircraft is heard louder nearer and on the side she is on**:
// the listener's own aircraft silent - no engines, standing - and another of
// each of the sixteen placed ahead, behind, above, 45 and 90 degrees right
// and left, at 25 m and at every doubling from 50 m to 25.6 km. At each
// place her loudness is the same at 25 m as at 50 (her full), falls at every
// doubling past it, and is nothing past 20 km; to the right the right
// channel is louder than the left by four times and more, to the left the
// left, and ahead, behind and above the two are the same within 1%.
GLIDESLOPE_TEST(another_aircraft_is_heard_louder_nearer_and_on_the_side_she_is_on) {
    const auto device = dummy_device();
    const std::vector<CatalogueEntry> all = catalogue();
    const SoundSet listener = glideslope::audio::read_sound_set(data(), "c172p");
    struct Place {
        const char* name;
        double right, up, forward;
        int side; // 1 right, -1 left, 0 the middle
    };
    const double d45 = std::sqrt(0.5);
    const std::vector<Place> places = {
        {"ahead", 0, 0, 1, 0},          {"behind", 0, 0, -1, 0},
        {"above", 0, 1, 0, 0},          {"90 right", 1, 0, 0, 1},
        {"90 left", -1, 0, 0, -1},      {"45 right", d45, 0, d45, 1},
        {"45 left", -d45, 0, d45, -1}};
    const std::vector<double> distances = {25,   50,   100,  200,   400,  800,
                                           1600, 3200, 6400, 12800, 25600};
    std::size_t walked = 0;
    std::string failed;
    for (const CatalogueEntry& e : all) {
        const SoundSet other = glideslope::audio::read_sound_set(data(), e.id);
        for (const Place& p : places) {
            std::vector<double> levels;
            bool ok = true;
            for (const double d : distances) {
                Heard h;
                h.on_surface = true;
                h.others.push_back(OtherHeard{7, &other, p.right * d, p.up * d, p.forward * d});
                Mixer mixer(listener, glideslope::audio::mix_rate);
                mixer.hear(h);
                std::vector<float> mix;
                device->tap(&mix);
                for (int i = 0; i < steps_per_second / 4; ++i) {
                    device->play(mixer, step_s);
                }
                device->tap(nullptr);
                const std::size_t frames = mix.size() / 2;
                const double left = rms(channel(mix, 0, frames / 2, frames));
                const double right = rms(channel(mix, 1, frames / 2, frames));
                const double level = std::sqrt(left * left + right * right);
                if (d > glideslope::audio::other_heard_within_m) {
                    ok = ok && level == 0.0;
                } else if (p.side > 0) {
                    ok = ok && right > 4.0 * left;
                } else if (p.side < 0) {
                    ok = ok && left > 4.0 * right;
                } else {
                    ok = ok && std::abs(right - left) <= 0.01 * (right + left);
                }
                if (d == 50) {
                    ok = ok && std::abs(level - levels.back()) <= 0.01 * level;
                } else if (d > 50) {
                    ok = ok && level < levels.back();
                }
                levels.push_back(level);
                ++walked;
            }
            if (!ok) {
                failed += " " + e.id + " " + p.name;
            }
        }
        std::printf("  %-13s heard at every place and distance%s\n", e.id.c_str(),
                    failed.find(e.id) == std::string::npos ? "" : "  <- WRONG");
    }
    check(failed.empty(), "every other aircraft placed and faded as she is; not:" + failed);
    check(all.size() == aircraft_in_the_catalogue &&
              walked == all.size() * places.size() * distances.size(),
          std::to_string(walked) + " of " +
              std::to_string(all.size() * places.size() * distances.size()) +
              " placings heard");
}

// **Every aircraft's stall warning sounds at the angle her own flight model
// gives** (sim/lift_peak.hpp): clean and with her flaps fully down, her lift
// coefficient peaks inside her tables, and her warning's angle - where the
// coefficient first reaches its peak over her warning margin squared
// (sim::stall_warning_margin) - lies below the peak and above level
// flight's. Her set loaded to be flown (audio::own_sound_set) carries those
// two angles, and the mix sounds her horn at each, and half way between them
// at half flap, and not a tenth of a degree below. Every aircraft, both
// configurations, asserted.
GLIDESLOPE_TEST(every_aircrafts_stall_warning_sounds_at_the_angle_her_own_lift_curve_gives) {
    const std::vector<CatalogueEntry> all = catalogue();
    std::size_t read = 0;
    std::string failed;
    for (const CatalogueEntry& e : all) {
        const double margin = glideslope::sim::stall_warning_margin(data(), e.model);
        const SoundSet own =
            glideslope::audio::own_sound_set(data(), e.id, e.model, e.start_airspeed_kts);
        for (const double flaps : {0.0, 1.0}) {
            const glideslope::sim::LiftPeak p = glideslope::sim::lift_peak(
                data() / "jsbsim", e.model, e.start_airspeed_kts, flaps, margin);
            const double carried = flaps > 0.0 ? own.stall_alpha_flaps_deg
                                               : own.stall_alpha_clean_deg;
            bool ok = p.peaked && p.cl_max > 0.0 && p.warning_alpha_deg > 0.0 &&
                      p.warning_alpha_deg < p.alpha_deg && carried == p.warning_alpha_deg;
            for (const double f : {flaps, 0.5}) {
                const double at = own.stall_warning_alpha_deg(f);
                for (const double alpha : {at - 0.1, at}) {
                    Mixer mixer(own, glideslope::audio::mix_rate);
                    Heard h;
                    h.flaps = f;
                    h.alpha_deg = alpha;
                    mixer.hear(h);
                    ok = ok && mixer.stall_warning() == (alpha == at);
                }
            }
            ok = ok && own.stall_warning_alpha_deg(0.5) ==
                           0.5 * (own.stall_alpha_clean_deg + own.stall_alpha_flaps_deg);
            std::printf("  %-13s flaps %.0f: CLmax %.2f at %.2f degrees, warned from %.2f "
                        "(her warning %.3f times her stall speed)%s\n",
                        e.id.c_str(), flaps, p.cl_max, p.alpha_deg, p.warning_alpha_deg, margin,
                        ok ? "" : "  <- WRONG");
            if (ok) {
                ++read;
            } else {
                failed += " " + e.id + (flaps > 0.0 ? "(flaps down)" : "(clean)");
            }
        }
    }
    check(failed.empty(), "every stall warning at her lift curve's angle; not:" + failed);
    check(all.size() == aircraft_in_the_catalogue && read == 2 * all.size(),
          std::to_string(read) + " of " + std::to_string(2 * all.size()) + " lift curves read");
}

// **Her engine's note is the fundamental worked by hand**: a C172P's
// four-cylinder Lycoming at 2,700 RPM fires 2,700 / 60 x 4 / 2 = 90 times a
// second, and a 737-300's CFM56-3 at 100% N1 turns its fan at the 5,179 RPM
// her set gives, 86.32 Hz. Her set says so, and the mix of each, at full
// power on the dummy driver, is strongest there within a bin.
GLIDESLOPE_TEST(a_c172p_at_2700_rpm_is_heard_at_90_hz_and_a_737_300_at_full_n1_at_86_hz) {
    const auto device = dummy_device();
    struct Pinned {
        const char* id;
        double drive;
        double hz;
        std::size_t engines;
    };
    const std::vector<Pinned> pinned = {{"c172p", 2700.0, 90.0, 1},
                                        {"737-300", 100.0, 5179.0 / 60.0, 2}};
    const double bin_hz = glideslope::audio::mix_rate / 65536.0;
    std::size_t heard_right = 0;
    for (const Pinned& p : pinned) {
        const SoundSet set = glideslope::audio::read_sound_set(data(), p.id);
        Mixer mixer(set, glideslope::audio::mix_rate);
        Heard h;
        h.engines.assign(p.engines, EngineHeard{true, p.drive, 1.0});
        h.on_surface = true;
        mixer.hear(h);
        std::vector<float> mix;
        device->tap(&mix);
        for (int i = 0; i < 2 * steps_per_second; ++i) {
            device->play(mixer, step_s);
        }
        device->tap(nullptr);
        const double heard = peak_hz(mix, 5.0, 2000.0);
        const bool ok = std::abs(set.fundamental_hz(p.drive) - p.hz) < 1e-9 &&
                        std::abs(heard - p.hz) <= bin_hz;
        std::printf("  %-8s at %.0f: her set says %.3f Hz, heard %.3f, by hand %.3f%s\n", p.id,
                    p.drive, set.fundamental_hz(p.drive), heard, p.hz, ok ? "" : "  <- WRONG");
        heard_right += ok ? 1 : 0;
    }
    check(heard_right == pinned.size(), std::to_string(heard_right) + " of 2 heard as worked");
}

// **A bounce is a landing again only after a quarter second off the
// surface** (audio::touchdown_after_off_s): built a step at a time, she
// touches, leaves the surface for N steps and touches again - one thump for
// 1, 15 and 29 steps off, two for 30, 60 and 240 - and the first touch of a
// flight is a thump whenever it comes.
GLIDESLOPE_TEST(a_bounce_off_the_surface_is_heard_again_only_after_a_quarter_second_off_it) {
    const auto device = dummy_device();
    const SoundSet set = glideslope::audio::read_sound_set(data(), "c172p");
    const std::vector<std::pair<int, std::size_t>> cases = {{1, 1},  {15, 1},  {29, 1},
                                                            {30, 2}, {60, 2}, {240, 2}};
    std::size_t right = 0;
    for (const auto& [off, thumps] : cases) {
        Mixer mixer(set, glideslope::audio::mix_rate);
        for (int i = 0; i < 300 + off; ++i) {
            Heard h;
            h.on_surface = (i >= 10 && i < 20) || i >= 20 + off;
            h.sink_fps = 5.0;
            mixer.hear(h);
            device->play(mixer, step_s);
        }
        const bool ok = mixer.touchdowns() == thumps;
        std::printf("  off %3d steps: %zu thumps, %zu expected%s\n", off, mixer.touchdowns(),
                    thumps, ok ? "" : "  <- WRONG");
        right += ok ? 1 : 0;
    }
    check(right == cases.size(), std::to_string(right) + " of 6 bounces heard as they should");
}

// **Every aircraft's landing is heard from the step her flight model puts
// her on the surface**: set down at idle on level ground - a seaplane on
// water - from 30 ft at six tenths of her catalogue airspeed, the nose held
// two degrees up (six for a flying boat), in JSBSim, each step's sound mixed
// on the dummy driver, for 20 s. Flown twice, the same but for her
// touchdown's gain (nought the second time), the two mixes are the same,
// sample for sample, until the step a wheel or her hull first meets the
// surface; the difference from it - the thump - is at least a tenth of the
// rest of the mix over a quarter second. The mix counts as many landings as
// the flight has touches after a quarter second off the surface - the
// first among them; a wheel lifting for less on the rollout is not one.
GLIDESLOPE_TEST(every_aircrafts_landing_in_her_flight_model_is_heard_from_the_step_she_touches) {
    const auto device = dummy_device();
    const std::vector<CatalogueEntry> all = catalogue();
    constexpr int steps = 20 * steps_per_second;
    constexpr int quarter_steps = steps_per_second / 4;
    std::size_t heard_right = 0;
    std::string failed;
    for (const CatalogueEntry& e : all) {
        SoundSet set = glideslope::audio::read_sound_set(data(), e.id);
        std::vector<float> mixes[2];
        int touched_at = -1;
        std::size_t edges = 0;
        std::size_t landings = 0;
        std::size_t counted = 0;
        for (int run = 0; run < 2; ++run) {
            if (run == 1) {
                set.touchdown_gain = 0.0;
            }
            glideslope::sim::Aircraft aircraft(data() / "jsbsim", e.model);
            aircraft.set_terrain(std::make_shared<glideslope::sim::FunctionTerrain>(
                [](double, double) { return 0.0; },
                [&e](double, double) { return e.seaplane; }));
            glideslope::sim::InitialConditions ic;
            ic.latitude_deg = -33.9;
            ic.longitude_deg = 151.2;
            ic.altitude_ft = 30.0;
            ic.airspeed_kts = 0.6 * e.start_airspeed_kts;
            ic.engine_running = true;
            ic.gear = 1.0;
            aircraft.initialize(ic);
            glideslope::sim::TestPilot pilot(aircraft);
            glideslope::sim::Controls c;
            c.gear = 1.0;
            c.throttle = 0.0;
            Mixer mixer(set, glideslope::audio::mix_rate);
            device->tap(&mixes[run]);
            bool was_on = false;
            int off_steps = 1000000;
            for (int i = 0; i < steps; ++i) {
                c.elevator = pilot.pitch_to(e.seaplane ? 6.0 : 2.0);
                c.aileron = pilot.roll_to(0.0);
                aircraft.set_controls(c);
                aircraft.step();
                const Heard h = glideslope::audio::heard_of(aircraft, set);
                if (run == 0) {
                    if (h.on_surface && !was_on) {
                        ++edges;
                        landings += off_steps >= quarter_steps ? 1 : 0;
                        if (touched_at < 0) {
                            touched_at = i;
                        }
                    }
                    off_steps = h.on_surface ? 0 : off_steps + 1;
                }
                was_on = h.on_surface;
                mixer.hear(h);
                device->play(mixer, step_s);
            }
            device->tap(nullptr);
            if (run == 0) {
                counted = mixer.touchdowns();
            }
        }
        bool ok = touched_at > 0 && mixes[0].size() == mixes[1].size() && counted == landings &&
                  landings >= 1;
        double ratio = 0.0;
        if (ok) {
            // The step's sound is mixed after the step is heard: the thump
            // begins with the frames of the step she touched on.
            const std::size_t at = mixes[0].size() / 2 * static_cast<std::size_t>(touched_at) /
                                   static_cast<std::size_t>(steps);
            double before = 0.0;
            for (std::size_t i = 0; i < 2 * at; ++i) {
                before = std::max(before,
                                  std::abs(static_cast<double>(mixes[0][i] - mixes[1][i])));
            }
            std::vector<float> difference(mixes[0].size());
            for (std::size_t i = 0; i < difference.size(); ++i) {
                difference[i] = mixes[0][i] - mixes[1][i];
            }
            const std::size_t quarter = glideslope::audio::mix_rate / 4;
            ratio = rms_of(difference, at, at + quarter) / rms_of(mixes[1], at, at + quarter);
            ok = before == 0.0 && ratio >= 0.1;
        }
        std::printf("  %-13s touched at step %d, %zu touches, %zu landings, %zu thumps, the "
                    "first %.2f of the rest%s\n",
                    e.id.c_str(), touched_at, edges, landings, counted, ratio,
                    ok ? "" : "  <- WRONG");
        if (ok) {
            ++heard_right;
        } else {
            failed += " " + e.id;
        }
    }
    check(failed.empty(), "every landing heard from its step; not:" + failed);
    check(all.size() == aircraft_in_the_catalogue && heard_right == all.size(),
          std::to_string(heard_right) + " of " + std::to_string(all.size()) +
              " landings heard");
}

// **A mix made faster than it is played is trimmed, not dropped**: a whole
// second mixed at once on the dummy driver, with nothing queued yet, gives
// SDL the quarter second that fits (audio::most_queued_s) and counts the
// rest not queued - where it once gave nothing at all, a cut a click.
GLIDESLOPE_TEST(a_mix_made_faster_than_it_is_played_is_trimmed_to_what_fits_not_dropped) {
    const auto device = dummy_device();
    const SoundSet set = glideslope::audio::read_sound_set(data(), "c172p");
    Mixer mixer(set, glideslope::audio::mix_rate);
    Heard h;
    h.engines.assign(1, EngineHeard{true, 2700.0, 1.0});
    mixer.hear(h);
    device->play(mixer, 1.0);
    const auto fits =
        static_cast<std::size_t>(glideslope::audio::most_queued_s * glideslope::audio::mix_rate);
    std::printf("  mixed %zu frames, %zu not queued, %zu fit\n", device->mixed_frames(),
                device->not_queued_frames(), fits);
    check(device->mixed_frames() == static_cast<std::size_t>(glideslope::audio::mix_rate) &&
              device->not_queued_frames() == device->mixed_frames() - fits,
          "a second mixed at once queues the quarter second that fits");
}
