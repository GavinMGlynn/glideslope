#!/usr/bin/env python3
"""make_short_s23.py - glideslope's Short S.23 Empire flying boat, made from
JSBSim's Short_S23.

JSBSim's Short_S23, as pinned in ext/jsbsim, is Anders Gidenstam's model of
the Short S.23 Empire flying boat of 1936, with four Bristol Pegasus Xc
engines and de Havilland two-pitch airscrews, and its hull's and wing floats'
hydrodynamics - buoyancy, planing lift and water drag - computed from its 3d
model. Its figures are Short Brothers' and Flight's, in assets/figures/
short_s23.xml.

This script writes the model into assets/jsbsim/ from the pinned files with
the changes below and nothing else. The committed output is checked against
it by a test; to change the aircraft, change this script and run it.

    python3 tools/make_short_s23.py           write the files
    python3 tools/make_short_s23.py --check   exit 1 if what is committed differs

The changes, and what each is for:

  Files and names       The model is short_s23. Its own systems are in
                        aircraft/short_s23/Systems/, as the pinned model's
                        are; the three of JSBSim's shared systems it uses -
                        hydrodynamics, hydrodynamic-planing-floats and
                        sperry-a2-autopilot - are in systems/, and its engine
                        and airscrew in engine/, all unchanged.

  Airframe (aircraft/short_s23/short_s23.xml)
    Keel skids, added   The model meets the ground only at its bow, tail and
                        wings, as the hull's hydrodynamics are what meet the
                        water; on land its hull sank into the ground until
                        its wings touched, the centre of gravity 7 ft under
                        it. Three points on the keel - the forebody's, level
                        from 4.2 m aft of the bow to the main step at 10.5 m,
                        and the afterbody's at 17.1 m - are contacts JSBSim
                        takes as wheels, which water does not bear, so that
                        they meet land and not water, with a hull's sliding
                        friction and no rolling. Their heights are scaled
                        from the side and front views in Flight's general
                        arrangement (29 October 1936, p. 440f): the floats'
                        keels 4.3 ft above the hull's, and the model's floats'
                        1.71 ft below its hydrodynamic reference point, put
                        the forebody keel 6.0 ft (1.83 m) below it; the
                        afterbody's keel rises 4.1 ft to 17.1 m aft. The wing
                        floats' keels, where the model's hydrodynamics has
                        them - 31.99 ft out, 1.71 ft below the reference
                        point and their steps 4.72 ft aft of it - are skids
                        too, so that on land it heels onto a float as it
                        does afloat, not onto a wing tip.
    Six declarations, removed
                        The co-pilot's and the Sperry autopilot's control
                        commands, declared again where the systems that
                        drive them declare them, which JSBSim warns of at
                        every load.

  Aerodynamics (aircraft/short_s23/Systems/datcom_aero.xml)
    The drag, times 1.23 -> times 1.9
                        The model's DATCOM drag, which it multiplied by 1.23
                        "adjusted for climb time", flew it at 225 mph level at
                        5,500 ft where its maximum is 200, and climbed it at
                        1,418 ft/min at sea level at the Pegasus's normal
                        boost where it climbs at 950 (Flight's specification,
                        29 October 1936); and so fast that its airscrews,
                        designed for 172 knots, turned the engines to 2,822
                        rpm and 945 hp, where the Pegasus gives 830 hp at
                        2,475. Times 1.9, it flies 203 mph and climbs at 977
                        ft/min; its take-offs from water lengthen by 4 to 10%,
                        to 32.6 s at 45,000 lb against Gouge's 30.5, and 26.4
                        s at 40,500 against his 24.

    Ground effect's two factors
                        Each a <product> of one table, which JSBSim warns of
                        at every load; the table alone.

  Engines (aircraft/short_s23/Systems/engines.xml)
    The controls' twelve declarations, removed
                        Each engine's throttle, airscrew and mixture command
                        declared again, which JSBSim's flight controls already
                        declare, and which it warns of at every load.
    The airscrews' pitch levers
                        The model's lever put the airscrew in coarse pitch at
                        1 and in fine at 0; glideslope's propeller control is
                        an rpm lever, 1 its highest rpm, which for a two-pitch
                        airscrew is fine pitch. With the model's sense, every
                        take-off was made in coarse pitch at 620 hp an engine
                        where the Pegasus gives 910, and never left the water.

  Flaps (aircraft/short_s23/Systems/flaps.xml)
    The flap selector, added
                        The model's flaps are moved by an electric motor, out
                        to 25 degrees in a minute and in in a minute and a
                        half, switched on and set running by switches nothing
                        in glideslope works. A selector now runs the motor out
                        or in until the flaps are where glideslope's flap
                        control puts them, and the motor's power switch starts
                        on; the motor, and its time, are the model's.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PINNED = ROOT / "ext" / "jsbsim"
OUT = ROOT / "assets" / "jsbsim"

PINNED_S23 = PINNED / "aircraft" / "Short_S23"
MODEL = "short_s23"
UNCHANGED_SYSTEMS = ["electrical", "fuel-system", "Short_S23-hydrodynamics", "take-off-ap"]
DRAG_FACTOR = 1.9
SHARED_SYSTEMS = ["hydrodynamics", "hydrodynamic-planing-floats", "sperry-a2-autopilot"]
ENGINE_FILES = ["eng_PegasusXc", "prop_deHavilland5000"]

# The keel, in the model's frame (metres: x aft of the bow, z up), scaled from
# Flight's general arrangement; see above.
FOREBODY_KEEL_Z = -1.83
KEEL = [("KEEL_FORWARD", 4.2, 0.0, FOREBODY_KEEL_Z), ("KEEL_STEP", 10.5, 0.0, FOREBODY_KEEL_Z),
        ("KEEL_AFT", 17.1, 0.0, FOREBODY_KEEL_Z + 1.26)]
# The wing floats' keels at their steps, from the model's hydrodynamics
# (Short_S23.xml's hydrodynamic-planing-floats, and its reference point at
# 341.73 in aft of the bow): feet converted to metres.
FLOAT_X = 341.73 * 0.0254 + 4.72 * 0.3048
FLOAT_Y = 31.99 * 0.3048
FLOAT_Z = -1.71 * 0.3048
FLOATS = [("LEFT_FLOAT_KEEL", FLOAT_X, -FLOAT_Y, FLOAT_Z), ("RIGHT_FLOAT_KEEL", FLOAT_X, FLOAT_Y, FLOAT_Z)]
# Stiff enough that the heaviest boat, 45,000 lb, settles half a foot on its
# keel; a hull's sliding friction.
KEEL_SPRING_LBS_FT = 45000
KEEL_DAMPING_LBS_FT_SEC = 9000
KEEL_FRICTION = 0.5


def replace_once(text, pattern, replacement, what):
    new, n = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if n != 1:
        raise SystemExit(f"make_short_s23: could not find {what} - has the pinned model changed?")
    return new


def without_sockets(text):
    if re.search(r"<input\s+port=|type=\"SOCKET\"", re.sub(r"<!--.*?-->", "", text, flags=re.S)):
        raise SystemExit("make_short_s23: the model opens a network socket")
    return text


def keel_skid(name, x, y, z):
    return (f"  <contact type=\"BOGEY\" name=\"{name}\">\n"
            f"   <location unit=\"M\">\n"
            f"    <x> {x:.2f} </x>\n"
            f"    <y> {y:.2f} </y>\n"
            f"    <z> {z:.2f} </z>\n"
            f"   </location>\n"
            f"   <static_friction>  {KEEL_FRICTION:.2f} </static_friction>\n"
            f"   <dynamic_friction> {KEEL_FRICTION:.2f} </dynamic_friction>\n"
            f"   <rolling_friction> {KEEL_FRICTION:.2f} </rolling_friction>\n"
            f"   <spring_coeff unit=\"LBS/FT\">      {KEEL_SPRING_LBS_FT} </spring_coeff>\n"
            f"   <damping_coeff unit=\"LBS/FT/SEC\"> {KEEL_DAMPING_LBS_FT_SEC} </damping_coeff>\n"
            f"   <max_steer unit=\"DEG\"> 0 </max_steer>\n"
            f"   <brake_group> NONE </brake_group>\n"
            f"   <retractable> 0 </retractable>\n"
            f"  </contact>\n")


def airframe():
    text = (PINNED_S23 / "Short_S23.xml").read_text()
    text = replace_once(
        text, r"(<fileheader>)",
        r"\1\n  <!-- glideslope: this is JSBSim's Short_S23 with the changes listed in\n"
        r"       tools/make_short_s23.py, which made it. Do not edit it by hand. -->",
        "the file header")
    without_sockets(text)
    for owner in ("fcs/copilot", "sperry-autopilot"):
        text = replace_once(
            text, rf"\n(  <property value=\"0\.0\">{owner}/(aileron|elevator|rudder)-cmd-norm</property>\n){{3}}",
            "\n", f"the {owner} commands' declarations")
    skids = ("  <!-- glideslope: the keel, which meets land and not water; see\n"
             "       tools/make_short_s23.py. -->\n"
             + "".join(keel_skid(name, x, y, z) for name, x, y, z in KEEL + FLOATS))
    text = replace_once(text, r"(\n </ground_reactions>)", lambda m: "\n" + skids.rstrip("\n") + m.group(1),
                        "the end of the ground reactions")
    return text


def aerodynamics():
    text = (PINNED_S23 / "Systems" / "datcom_aero.xml").read_text()
    for what in ("lift", "drag"):
        text = replace_once(
            text,
            rf"(<function name=\"aero/function/ground-effect-factor-{what}\">\s*<description>[^<]*</description>\n)"
            r"\s*<product>\n(\s*<table>.*?</table>\n)\s*</product>\n",
            r"\1\2", f"ground effect's {what} factor")
    return replace_once(
        text, r"<value>1\.23</value> <!-- Adjusted for climb time\. -->",
        f"<value>{DRAG_FACTOR}</value> <!-- glideslope: 1.23 in the pinned model; see "
        "tools/make_short_s23.py. -->",
        "the drag's factor")


def engines():
    text = (PINNED_S23 / "Systems" / "engines.xml").read_text()
    for control in ("throttle", "advance", "mixture"):
        text = replace_once(
            text, rf"\n( <property value=\"0\.0\">fcs/{control}-cmd-norm\[\d\]</property>\n){{4}}", "",
            f"the {control} commands' declarations")
    for i in range(4):
        text = replace_once(
            text,
            rf"(<switch name=\"fcs/propeller/blade-angle-deg\[{i}\]\">\s*<default value=\")20\.0(\"/>\s*"
            rf"<test value=\")28\.0(\">\s*fcs/advance-cmd-norm\[{i}\]) GT 0\.5",
            r"\g<1>28.0\g<2>20.0\g<3> GT 0.5",
            f"engine {i}'s pitch lever")
    return replace_once(
        text, r"( <documentation>\n   The propeller pitch control only had the settings FINE and COARSE.\n)",
        r"\1   glideslope: the lever in FINE at 1, glideslope's highest rpm, and COARSE\n"
        r"   at 0; see tools/make_short_s23.py.\n",
        "the pitch control's documentation")


SELECTOR = """ <channel name="The flap selector">
  <!-- glideslope: the pilot's flap control, fcs/flap-cmd-norm, runs the motor
       out or in until the flaps are where it puts them; see
       tools/make_short_s23.py. -->
  <summer name="fcs/flap/selector-error-norm">
   <input>fcs/flap-cmd-norm</input>
   <input>-fcs/flap-pos-norm</input>
  </summer>
  <switch name="fcs/flap/selector-motor-cmd-norm">
   <default value="0.0"/>
   <test value="1.0">
    fcs/flap/selector-error-norm GT 0.01
   </test>
   <test value="-1.0">
    fcs/flap/selector-error-norm LT -0.01
   </test>
   <output>fcs/flap-motor-cmd-norm</output>
  </switch>
 </channel>

"""


def flaps():
    text = (PINNED_S23 / "Systems" / "flaps.xml").read_text()
    text = replace_once(text, r"<property value=\"0\.0\">fcs/flap-power-cmd-norm</property>",
                        "<property value=\"1.0\">fcs/flap-power-cmd-norm</property>",
                        "the flap motor's power switch")
    return replace_once(text, r"( <channel name=\"Flap indicator lights\">)",
                        lambda m: SELECTOR + m.group(1), "the flap indicator lights")


def outputs():
    out = {
        OUT / "aircraft" / MODEL / f"{MODEL}.xml": airframe(),
        OUT / "aircraft" / MODEL / "Systems" / "datcom_aero.xml": aerodynamics(),
        OUT / "aircraft" / MODEL / "Systems" / "engines.xml": engines(),
        OUT / "aircraft" / MODEL / "Systems" / "flaps.xml": flaps(),
    }
    for name in UNCHANGED_SYSTEMS:
        out[OUT / "aircraft" / MODEL / "Systems" / f"{name}.xml"] = \
            (PINNED_S23 / "Systems" / f"{name}.xml").read_text()
    for name in SHARED_SYSTEMS:
        out[OUT / "systems" / f"{name}.xml"] = (PINNED / "systems" / f"{name}.xml").read_text()
    for name in ENGINE_FILES:
        out[OUT / "engine" / f"{name}.xml"] = (PINNED / "engine" / f"{name}.xml").read_text()
    return out


def main():
    check = "--check" in sys.argv[1:]
    stale = []
    for path, text in outputs().items():
        if check:
            if not path.exists() or path.read_text() != text:
                stale.append(path.relative_to(ROOT))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            print(f"wrote {path.relative_to(ROOT)}")
    if stale:
        print("assets/jsbsim/ is not what tools/make_short_s23.py makes:", file=sys.stderr)
        for s in stale:
            print(f"  {s}", file=sys.stderr)
        print("run: python3 tools/make_short_s23.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
