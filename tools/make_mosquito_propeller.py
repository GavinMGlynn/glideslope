#!/usr/bin/env python3
"""Writes the Mosquito FB Mk VI's propeller, assets/jsbsim/engine/prop_dh_hydromatic.xml.

The propeller is a de Havilland Hydromatic, type A.5/147: three metal blades,
12 ft across, constant speed and fully feathering, driven through the Merlin
25's 0.42:1 reduction gear (A&AEE report 767,e, 12th part; the Rolls-Royce
Merlin 24/25 altitude performance sheet, AVIA 6/5817).

JSBSim wants its thrust and power coefficients as tables over the advance
ratio J = V / (n D) and the blade angle at three-quarters of the radius. No
measured charts for this propeller have been found, so they are computed here
by blade-element momentum theory - each ring of the disc taking the momentum
its blade elements put into the air, with Prandtl's tip loss - for a blade of
these estimated proportions:

  chord        9% of the diameter from 30% of the radius out, narrowing to
               7% at the tip and 7.7% at the root: an activity factor of about
               140 a blade, a wide "paddle" blade (the 1944 Pilot's Notes,
               para. 51, speak of "paddle bladed propellers"). Narrower blades
               could not take the Merlin's take-off power at 3,000 rpm
               without the governor turning them past their stall, where
               more power made less thrust.
  twist        a helix of pitch 1.6 diameters, 32 degrees root to tip
  sections     a cambered section like the Clark Y: lift slope 0.9 of thin
               aerofoil theory, zero lift at -4 degrees, stalling at a lift
               of 1.25 (and -0.8), a flat plate far past the stall; profile
               drag 0.0085 rising with lift, a stall gentler than a wing's,
               and the drag rise of a 6% thick section above Mach 0.72

The main tables are computed without compressibility. JSBSim scales them by
the helical Mach number of the blade tips, and those tables, CT_MACH and
CP_MACH, are computed by the same method at the propeller's full speed (1,260
rpm, 3,000 at the engine) and an advance ratio of 2 - the aircraft at full
speed - with the speed of sound set to give each tip Mach number, as the ratio
of each coefficient to its value without compressibility. So the tips lose
more as the air cools with height, as they did.

The thrust is then taken 5% lower (ct_factor): what a nacelle and spinner
cost a propeller in blockage and scrubbing, which the blade elements alone do
not see - estimates run from 3% to 8%. With it, the propeller's efficiency at
the aircraft's top speed is about 0.82, what the propellers of its day gave.

    python3 tools/make_mosquito_propeller.py           write it
    python3 tools/make_mosquito_propeller.py --check   exit 1 if it differs

A test fails if what is committed differs from what this script makes.
"""

import math
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "jsbsim" / "engine" / "prop_dh_hydromatic.xml"

BLADES = 3
DIAMETER_FT = 12.0
RADIUS_FT = DIAMETER_FT / 2.0
HUB = 0.2                # the blade starts at 20% of the radius
DESIGN_PITCH = 1.6       # the twist's helix, in diameters
FULL_RPM = 3000.0 / 2.381  # the propeller's, at 3,000 engine rpm
STATIONS = 40
INSTALLATION = 0.95      # thrust after the nacelle's and spinner's losses
MACH_ADVANCE = 2.0       # where the tip Mach tables are computed
MACH_BLADE_ANGLE = 45
TIP_MACH = [0.5, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 1.0, 1.1]

ADVANCE = [round(0.2 * i, 1) for i in range(0, 31)]           # J = 0 .. 6
BLADE_ANGLES = [15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 87]


def chord_over_diameter(x):
    if x < 0.3:
        return 0.077 + (0.09 - 0.077) * (x - HUB) / (0.3 - HUB)
    if x > 0.8:
        return 0.09 - (0.09 - 0.07) * (x - 0.8) / 0.2
    return 0.09


def twist_deg(x):
    """The blade angle at x less the angle at 0.75."""
    at = lambda r: math.degrees(math.atan(DESIGN_PITCH / (math.pi * r)))
    return at(x) - at(0.75)


def section(alpha, mach):
    """Lift and drag coefficients of the section at angle of attack alpha
    (radians) and Mach number mach."""
    alpha0 = math.radians(-4.0)
    slope = 0.9 * 2.0 * math.pi
    m = min(mach, 0.8)
    compressible = 1.0 / math.sqrt(1.0 - m * m)
    cl_linear = slope * (alpha - alpha0) * compressible
    cl_max, cl_min = 1.25, -0.8
    # A flat plate, far past the stall.
    cl_plate = 2.0 * math.sin(alpha) * math.cos(alpha)
    cd_plate = 2.0 * math.sin(alpha) ** 2
    if cl_min <= cl_linear <= cl_max:
        cl = cl_linear
        cd = 0.0085 + 0.006 * (cl - 0.3) ** 2
    else:
        # Past the stall: the lift falls from its peak towards the flat
        # plate's over twenty-five degrees - a turning blade's stall is
        # gentler than a wing's - and the drag rises towards the plate's.
        stall = alpha0 + (cl_max if cl_linear > 0 else cl_min) / (slope * compressible)
        beyond = min(abs(alpha - stall) / math.radians(25.0), 1.0)
        peak = cl_max if cl_linear > 0 else cl_min
        cl = peak * (1.0 - beyond) + cl_plate * beyond
        cd = (0.0085 + 0.006 * (peak - 0.3) ** 2) * (1.0 - beyond) + max(cd_plate, 0.02) * beyond
    if mach > 0.72:
        cd += 20.0 * (mach - 0.72) ** 4 + 0.1 * (mach - 0.72) ** 2
    return cl, cd


def coefficients(j, beta75, sound_fps=None):
    """Thrust and power coefficients at advance ratio j and blade angle beta75
    (degrees at 0.75 radius); with a speed of sound, compressible."""
    n = FULL_RPM / 60.0
    omega = 2.0 * math.pi * n
    v = j * n * DIAMETER_FT
    rho = 1.0  # the coefficients do not depend on it
    thrust = 0.0
    torque = 0.0
    dx = (1.0 - HUB) / STATIONS
    for k in range(STATIONS):
        x = HUB + (k + 0.5) * dx
        r = x * RADIUS_FT
        dr = dx * RADIUS_FT
        c = chord_over_diameter(x) * DIAMETER_FT
        beta = math.radians(beta75 + twist_deg(x))
        vx, vt = 0.0, 0.0
        dT = dQ = 0.0
        for _ in range(400):
            wx = v + vx
            wt = omega * r - vt
            phi = math.atan2(wx, wt)
            w2 = wx * wx + wt * wt
            mach = math.sqrt(w2) / sound_fps if sound_fps else 0.0
            cl, cd = section(beta - phi, mach)
            cn = cl * math.cos(phi) - cd * math.sin(phi)
            ct = cl * math.sin(phi) + cd * math.cos(phi)
            dT = 0.5 * rho * w2 * BLADES * c * cn          # per unit radius
            dQ = 0.5 * rho * w2 * BLADES * c * ct * r
            s = abs(math.sin(phi)) if abs(math.sin(phi)) > 1e-4 else 1e-4
            f = 2.0 / math.pi * math.acos(
                min(1.0, math.exp(-BLADES * (1.0 - x) / (2.0 * x * s))))
            f = max(f, 0.05)
            # Axial momentum: dT = 4 pi r rho (v + vx) vx F.
            disc = v * v + dT / (math.pi * r * rho * f)
            vx_new = (-v + math.sqrt(disc)) / 2.0 if disc > 0 else -v / 2.0
            # Angular momentum: dQ = 4 pi r^2 rho (v + vx) vt F.
            axial = max(v + vx_new, 1.0)
            vt_new = dQ / (4.0 * math.pi * r * r * rho * axial * f)
            vt_new = max(min(vt_new, 0.5 * omega * r), -0.5 * omega * r)
            if abs(vx_new - vx) < 1e-6 and abs(vt_new - vt) < 1e-6:
                break
            vx += 0.3 * (vx_new - vx)
            vt += 0.3 * (vt_new - vt)
        thrust += dT * dr
        torque += dQ * dr
    power = torque * omega
    ct = thrust / (rho * n * n * DIAMETER_FT ** 4)
    cp = power / (rho * n ** 3 * DIAMETER_FT ** 5)
    return ct, cp


def mach_table(name, index):
    """The ratio of a coefficient to its incompressible value, by the helical
    Mach number of the tips."""
    n = FULL_RPM / 60.0
    tip = math.hypot(math.pi * n * DIAMETER_FT, MACH_ADVANCE * n * DIAMETER_FT)
    base = coefficients(MACH_ADVANCE, MACH_BLADE_ANGLE)[index]
    rows = ["  <table name=\"%s\" type=\"internal\">" % name, "    <tableData>"]
    for m in TIP_MACH:
        ratio = coefficients(MACH_ADVANCE, MACH_BLADE_ANGLE, tip / m)[index] / base
        rows.append("    %5.2f %8.4f" % (m, ratio))
    rows += ["    </tableData>", "  </table>"]
    return "\n".join(rows)


def table(name, index):
    rows = ["  <table name=\"%s\" type=\"internal\">" % name, "    <tableData>"]
    rows.append("         " + "".join("%9d" % b for b in BLADE_ANGLES))
    for j in ADVANCE:
        values = [coefficients(j, b)[index] for b in BLADE_ANGLES]
        rows.append("    %4.1f " % j + "".join("%9.4f" % v for v in values))
    rows += ["    </tableData>", "  </table>"]
    return "\n".join(rows)


HEADER = """<?xml version="1.0"?>
<!--
  The de Havilland Hydromatic propeller of the Mosquito FB Mk VI, type
  A.5/147: three metal blades, 12 ft across, constant speed and fully
  feathering, driven through the Merlin 25's 0.42:1 reduction gear.

  Made by tools/make_mosquito_propeller.py, which computes the tables by
  blade-element momentum theory for a blade of estimated proportions and says
  what they are. Do not edit it by hand.

  The governor holds 1,800 to 3,000 engine rpm over the rpm lever's travel.
  The fine-pitch stop, 25 degrees, is estimated: above it the governor holds
  the rpm whenever the engine has the power, and at it a propeller with too
  little power - an engine throttled back, or failed and windmilling - turns
  slower. Feathering turns the blades to 87 degrees, edge on to the air, and
  the propeller stops.
-->
<propeller name="DH Hydromatic A.5/147">
  <ixx unit="SLUG*FT2"> 30 </ixx>
  <diameter unit="IN"> 144 </diameter>
  <numblades> 3 </numblades>
  <gearratio> 2.381 </gearratio>
  <minpitch> 25 </minpitch>
  <maxpitch> 87 </maxpitch>
  <minrpm> 756 </minrpm>
  <maxrpm> 1260 </maxrpm>
  <ct_factor> %.2f </ct_factor>

""" % INSTALLATION


def propeller():
    return (HEADER + table("C_THRUST", 0) + "\n\n" + table("C_POWER", 1) + "\n\n" +
            mach_table("CT_MACH", 0) + "\n\n" + mach_table("CP_MACH", 1) +
            "\n</propeller>\n")


def main():
    text = propeller()
    if "--check" in sys.argv[1:]:
        if not OUT.exists() or OUT.read_text() != text:
            print(f"{OUT.relative_to(ROOT)} is not what tools/make_mosquito_propeller.py "
                  "makes; run: python3 tools/make_mosquito_propeller.py", file=sys.stderr)
            return 1
        return 0
    OUT.write_text(text)
    print(f"wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
