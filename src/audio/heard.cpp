#include "audio/heard.hpp"

#include <string>

namespace glideslope::audio {

namespace {

// A property, or 0 where her model has none.
double read(const sim::Aircraft& aircraft, const std::string& name) {
    return aircraft.has_property(name) ? aircraft.property(name) : 0.0;
}

} // namespace

Heard heard_of(const sim::Aircraft& aircraft, const SoundSet& sounds) {
    Heard h;
    const int engines = aircraft.engine_count();
    for (int i = 0; i < engines; ++i) {
        const std::string engine = "propulsion/engine[" + std::to_string(i) + "]/";
        const std::string drive =
            engine + (sounds.engine == EngineKind::piston ? "engine-rpm" : "n1");
        const std::string throttle = "fcs/throttle-pos-norm[" + std::to_string(i) + "]";
        h.engines.push_back({aircraft.engine_running(i), read(aircraft, drive),
                             read(aircraft, throttle)});
    }
    h.airspeed_kts = read(aircraft, "velocities/vc-kts");
    h.gear = aircraft.gear_retracts() ? read(aircraft, "gear/gear-pos-norm") : 1.0;
    h.flaps = read(aircraft, "fcs/flap-pos-norm");
    // A wheel, a float or her hull meeting the surface: any of her contacts
    // carrying weight, or her hull in the water.
    bool on = aircraft.in_water();
    for (int i = 0; !on; ++i) {
        const std::string wow = "gear/unit[" + std::to_string(i) + "]/WOW";
        if (!aircraft.has_property(wow)) {
            break;
        }
        on = read(aircraft, wow) > 0.5;
    }
    h.on_surface = on;
    h.sink_fps = -read(aircraft, "velocities/h-dot-fps");
    h.alpha_deg = read(aircraft, "aero/alpha-deg");
    return h;
}

} // namespace glideslope::audio
