# Assets

**What this file is.** Where every piece of third-party data this project uses
comes from, which version, and under what terms — aircraft models, terrain,
imagery, weather data, fonts and sound. The code licence (GPL-3.0-or-later) does
not cover any of it; each source's own terms do.

**Thirty-four things are used: Cessna 172P and 182S, Piper PA-28-180 and J-3
Cub, Airbus A320, Boeing 737-300, 747-400 and 787-8, McDonnell Douglas F-15C,
Lockheed Martin F-22A and Short S.23 flight models derived from JSBSim's, those
eleven aircraft's published figures - handbooks for the light aircraft,
airport-planning documents and type certificates for the airliners, the
Air Force's and the Department of Defense's for the fighters, Flight
magazine's for the S.23 - the Airbus
A380's documents and the Boeing 747's published derivatives and the
Learjet 35A's flight manual and NASA's measurements of the Learjet 23, and
what is published of the F-35B and the B-2A (for flight models written
here), the Mosquito FB Mk VI's trials
and Pilot's Notes (for a flight model written here), the Copernicus DEM, the
EGM2008 geoid grid, EOX's Sentinel-2 cloudless imagery, METARs from
aviationweather.gov and winds aloft from Open-Meteo** - the DEM, the geoid and the imagery fetched
as they are needed, the weather when it is asked for, with one recorded
response of each weather service committed for the tests, and **eight
aircraft visual models from FlightGear's aircraft**, converted from their
pinned sources and committed. No font or sound is used or fetched yet.

## The rule

An asset is recorded here **in the commit that first uses it**, with its source,
its pinned version (a commit, a dataset release, or a URL plus SHA-256), its
licence, and the attribution text it requires. Terms are quoted from the source
at that point, not paraphrased from memory.

## In use

### The Cessna 172P flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/c172p/c172p.xml`, `engine/prop_75in2f.xml`, `engine/eng_io320.xml` |
| Changes | Made by `tools/make_c172p.py`, whose docstring lists each change and the published figure it answers: the propeller's power and thrust coefficients, zero-lift drag and drag with angle of attack, the lift curve up to the stall, and flap lift. The engine file is unchanged. |
| In the repository | `assets/jsbsim/`, as the script makes it; a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |
| Where it goes | Copied at configure time into `data/jsbsim/` beside the programs, in the build tree and in every package |

The model file's own header says: "This model was created using publicly
available data, publicly available technical reports, textbooks, and guesses.
It contains no proprietary or restricted data. If this model has been validated
at all, it would be only to the extent that it seems to "fly right", and that it
possibly complies with published, publicly known, performance data (maximum
speed, endurance, etc.). Thus, this model is meant for educational and
entertainment purposes only."

### The Cessna 172P's published figures

| | |
| --- | --- |
| Source | Cessna Model 172P Pilot's Operating Handbook, 12 May 1981 (the Island Enterprises reprint): section 1 specifications, section 2 powerplant limitations, figures 3-1, 5-1, 5-3, 5-5, 5-6 and 5-8 |
| In the repository | `assets/figures/c172p.xml`: individual numbers, each with its section or figure, not the handbook's text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Cessna 182S flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/c182/c182.xml`, `engine/prop_81in2v.xml`, `engine/engIO540AB1A5.xml` |
| Changes | Made by `tools/make_c182.py`, whose docstring lists each change and why: the handbook's empty weight, seats and tanks; flaps to 38 degrees; main wheels that no longer castor; the lift curve for the stalls; the elevator's and incidence drag; a stopped engine's friction; the propeller's thrust at low advance ratio. The engine file is unchanged. |
| In the repository | `assets/jsbsim/`, as the script makes it; a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Cessna 182S's published figures

| | |
| --- | --- |
| Source | Cessna Model 182S Skylane Information Manual, P/N 182SIM, 1997 (the Pilot's Operating Handbook of 3 February 1997 with revision 4 of 1 November 2001), page ii and figures 3-1, 5-1, 5-4, 5-6, 5-7 and 5-9, as copied at <http://tssflyingclub.org/documents/C182S_POH.pdf>; FAA type certificate data sheet 3A13, revision 66, section XIII, for the static rpm and the flaps' travel |
| In the repository | `assets/figures/c182.xml`: individual numbers, each with its page or figure, not the handbook's text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Piper PA-28-180 Cherokee flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/pa28/pa28.xml`, `engine/engIO360C.xml`, and `engine/prop_75in2f.xml` for its propeller |
| Changes | Made by `tools/make_pa28.py`, whose docstring lists each change and why: the handbook's empty weight, seats and tanks; flaps at 10, 25 and 40 degrees; main wheels that no longer castor; a fixed-pitch 76 in propeller in place of the model's constant-speed one, with its power and thrust at low advance ratio; the 172's gear springs and dampers; the stabilator's moment and lift; the lift curve and flap lift for the stalls; induced drag with ground effect; zero-lift and gear drag. The engine file is unchanged. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/pa28/pa28.xml`, `engine/engIO360C.xml`, `engine/prop_pa28_76in.xml`); a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Piper PA-28-180 Cherokee's published figures

| | |
| --- | --- |
| Source | Piper Cherokee 180 "E" Owner's Handbook, P/N 753 806, issued October 1969, revised January 1974, sections I, III and V, as copied at <https://www.coyoteflight.com/resources/Aircraft_Manuals/Piper_PA-28-180E.pdf>; the Airplane Flight Manual, Model PA-28-180, FAA approved 3 August 1962, revision 4, for the calibrated stalling speeds, as copied at <https://www.nehemiahaviation.com/files/pa28flightmanual.pdf>; FAA type certificate data sheet 2A13, revision 64, section III, for the static rpm |
| In the repository | `assets/figures/pa28.xml`: individual numbers, each with its section, not the handbook's text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Piper J-3 Cub flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/J3Cub/J3Cub.xml`, with its `Engines/Continental A-65-8.xml`, `Engines/CM7445 MCCauley.xml` and `Systems/Conventional Controls.xml` |
| Changes | Made by `tools/make_j3cub.py`, whose docstring lists each change and why: the files renamed, without spaces; the booklet's empty weight and the type certificate's seats, baggage and tank; the type certificate's elevator travel; the airframe's drag at zero lift and a light aircraft's induced drag; the engine's rpm limit; and the propeller's diameter and tables. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/j3cub/j3cub.xml`, `aircraft/j3cub/Systems/conventional-controls.xml`, `engine/engA65-8.xml`, `engine/prop_j3cub_74in.xml`); a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Piper J-3 Cub's published figures

| | |
| --- | --- |
| Source | Piper's Owner's Manual for the J3C-65 (undated; a Wag-Aero reproduction), pages 11, 12, 41 and 47, as copied at <https://stpeteair.org/wp-content/uploads/cub_owners_manual.pdf>; Piper's booklet "How to Fly a Piper Cub" (1945), page 12, as copied at <https://home.adelphi.edu/~allendon/fly_a_cub.pdf>; FAA Aircraft Specification A-691, revision 34, section IV and propeller item 2, for the static rpm, the stations and the elevator's travel |
| In the repository | `assets/figures/j3cub.xml`: individual numbers, each with its page, not the manual's text |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Airbus A320 flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/A320/A320.xml`, `engine/CFM56_5.xml`, `engine/direct.xml` |
| Changes | Made by `tools/make_a320.py` with `tools/airliner.py`, whose docstrings list each change and why: made the A320-214, with the CFM56-5B4's 27,000 lb; weights, tanks and payload; the pitch stiffness; the drag, rebuilt as induced and flap drag, with a Mach drag rise and a windmilling engine's drag; the gear's drag; ground effect and the lift with the take-off flaps; ground spoilers that take the lift off on the runway, as JSBSim's 737's do; the engines' thrust with height and speed. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/a320/a320.xml`, `engine/CFM56-5B4.xml`, `engine/direct.xml`); a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Airbus A320's published figures

| | |
| --- | --- |
| Source | Airbus, A320 Aircraft Characteristics - Airport and Maintenance Planning, June 2024 edition, figure 3-3-1-991-005-A01, as published at <https://www.aircraft.airbus.com/sites/g/files/jlcbta126/files/2025-01/AC_A320_0624.pdf>; Airbus, Getting to Grips with Aircraft Performance, January 2002, page 155, as copied at <https://skybrary.aero/sites/default/files/bookshelf/2263.pdf>; FAA type certificate data sheet A28NM, revision 42, as copied at <https://downloads.regulations.gov/FAA-2021-0799-0001/attachment_3.pdf>, for the Mmo and ceiling; FAA type certificate data sheet E37NE, for the CFM56-5B4's rating |
| In the repository | `assets/figures/a320.xml`: individual numbers, each with its table or figure, not the documents' text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Boeing 737-300 flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/737/737.xml`, `engine/CFM56.xml`, `engine/direct.xml` |
| Changes | Made by `tools/make_737_300.py` with `tools/airliner.py`, whose docstrings list each change and why: no network sockets; weights, tanks and payload; flaps in degrees, the leading edge devices and the flaps' drag; a Mach drag rise and a windmilling engine's drag; braking friction; the engines' thrust with height and speed, and no bleed. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/737-300/737-300.xml`, `engine/CFM56-3B1.xml`, `engine/direct.xml`); a test fails if they differ |
| Licence | The airframe file's own header names the GPL (`licenseName="GPL (General Public License)"`, with no version), and the modified file remains under it - this project's licence, GPL-3.0-or-later, whose text ships as `LICENSE`. The engine files are LGPL-2.1, as the JSBSim repository; they remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Boeing 737-300's published figures

| | |
| --- | --- |
| Source | Boeing, 737 Airplane Characteristics for Airport Planning, D6-58325-6 revision E, November 2023, table 2.1.6 and figure 3.3.11, as published at <https://www.boeing.com/content/dam/boeing/v2/airports/acaps/737CL_REV_E.pdf>; FAA type certificate data sheet A16WE, as copied at <http://www.b737.org.uk/a16we.pdf>; the cruise Mach from EUROCONTROL's Aircraft Performance Database, <https://learningzone.eurocontrol.int/ilp/customs/ATCPFDB/details.aspx?ICAO=B733>, a secondary source, Boeing publishing none |
| In the repository | `assets/figures/737-300.xml`: individual numbers, each with its table or figure, not the documents' text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Boeing 747-400 flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/B747/B747.xml`, `engine/GE-CF6-80C2-B1F.xml`, `engine/direct.xml` |
| Changes | Made by `tools/make_747_400.py` with `tools/airliner.py`, whose docstrings list each change and why: weights, tanks and payload; the nose gear's spring; a Mach drag rise and a windmilling engine's drag; braking friction; the engines' thrust with height and speed. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/747-400/747-400.xml`, `engine/CF6-80C2B1F.xml`, `engine/direct.xml`); a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Boeing 747-400's published figures

| | |
| --- | --- |
| Source | Boeing, 747-400 Airplane Characteristics for Airport Planning, D6-58326-1 revision F, December 2024, table 2.1.1 and figures 3.2.1 and 3.3.1, as published at <https://www.boeing.com/content/dam/boeing/v2/airports/acaps/747-400_Rev_F.pdf>; FAA type certificate data sheet A20WE, revision 58, as published at <https://www.boeing.com/content/dam/boeing/v2/airports/7478-airport-comp/A20WE.pdf> |
| In the repository | `assets/figures/747-400.xml`: individual numbers, each with its table or figure, not the documents' text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Boeing 787-8 flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/787-8/787-8.xml`, `engine/trent_1000.xml`, `engine/direct.xml` |
| Changes | Made by `tools/make_787_8.py` with `tools/airliner.py`, whose docstrings list each change and why: the lift, with a wing of its aspect ratio's slope and the slats; the span, the induced, zero-lift and flap drag; the rudder's command; a Mach drag rise and a windmilling engine's drag; braking friction; the engines' thrust with height and speed, and no bleed. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/787-8/787-8.xml`, `engine/Trent1000.xml`, `engine/direct.xml`); a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The Boeing 787-8's published figures

| | |
| --- | --- |
| Source | Boeing, 787 Airplane Characteristics for Airport Planning, D6-58333 revision O, February 2023, table 2.1.1 and figures 3.2.1 and 3.3.1, as published at <https://www.boeing.com/content/dam/boeing/v2/airports/acaps/787.pdf>; FAA type certificate data sheet T00021SE, revision 32, from the FAA's regulatory and guidance library as archived by the Internet Archive; Boeing's own 787-8 page, as archived, for the cruise Mach |
| In the repository | `assets/figures/787-8.xml`: individual numbers, each with its table or figure, not the documents' text or charts |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The McDonnell Douglas F-15C Eagle flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/f15/f15.xml`, `engine/F100-PW-229.xml`, `engine/direct.xml` |
| Changes | Made by `tools/make_f15c.py` with `tools/fighter.py` and `tools/airliner.py`, whose docstrings list each change and why: the F-15C's weights, fuel and stores; lift and drag across the Mach range, and the drag past the flow separating; the F100-PW-220's thrust, its afterburner lit above 0.99, and its thrust with speed and height, fitted to the Standard Aircraft Characteristics' charts. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/f15c/f15c.xml`, `engine/F100-PW-220.xml`, `engine/direct.xml`); a test fails if they differ |
| Licence | LGPL-2.1, as the JSBSim repository - the model files name no licence of their own; the modified files remain under it, and its text ships as `licenses/JSBSim.txt` |

### The McDonnell Douglas F-15C Eagle's published figures

| | |
| --- | --- |
| Source | United States Air Force, Standard Aircraft Characteristics, F-15C Eagle (220 engine), AFG 2 volume 1 addendum 61, February 1992 (performance basis: the contractor's June 1986 status), pages 4 to 6 (69 to 74 of 228), as scanned in the collection at <http://alternatewars.com/SAC/F-15C_Eagle_SAC_-_February_1992.pdf>, read through the Internet Archive. A work of the United States government |
| In the repository | `assets/figures/f15c.xml`: individual numbers, each with its page, not the document's text or charts. `tools/make_f15c.py` names the page 6 chart its thrust and drag are fitted to, not the numbers read from it |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Lockheed Martin F-22A Raptor flight model, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/f22/f22.xml`, `engine/F119-PW-1.xml`, `engine/direct.xml` |
| Changes | Made by `tools/make_f22.py` with `tools/fighter.py` and `tools/airliner.py`, whose docstrings list each change and why: control surfaces that move; military power at 0.99; the F-22's weights, fuel and missiles; lift, drag and the tail's moment across the Mach range; the pitch loop's gains scheduled on the tail's moment, and the roll stick shaped; the F119's thrust, and its thrust with speed and height. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/f22/f22.xml`, `engine/F119-PW-100.xml`, `engine/direct.xml`); a test fails if they differ |
| Licence | The airframe file's own header names the GPL (`licenseName="GPL (General Public License)"`, with no version), and the modified file remains under it - this project's licence, GPL-3.0-or-later, whose text ships as `LICENSE`. The engine files are LGPL-2.1, as the JSBSim repository; they remain under it, and its text ships as `licenses/JSBSim.txt` |

The airframe file's header says: "This model was created using data that is,
or has been, publically available by means of technical reports, textbooks,
image graphs or published code. This aircraft description file is in no way
related to the manufacturer of the real aircraft."

### The Lockheed Martin F-22A Raptor's published figures

| | |
| --- | --- |
| Source | Department of Defense, Selected Acquisition Report (RCS: DD-A&T(Q&A)823-265), F-22, as of 31 December 2010, its performance characteristics' "Demonstrated Performance", from the Washington Headquarters Services' FOIA reading room (<https://www.esd.whs.mil/Portals/54/Documents/FOID/Reading%20Room/Selected_Acquisition_Reports/FY_2010_SARS/F-22-SAR-25_DEC_2010.pdf>), read through the Internet Archive; the United States Air Force's F-22 Raptor, AIM-120 AMRAAM and AIM-9 Sidewinder fact sheets (<https://www.af.mil/About-Us/Fact-Sheets/Display/Article/104506/f-22-raptor/>, `.../104576/aim-120-amraam/`, `.../104557/aim-9-sidewinder/`), read through the Internet Archive; Lockheed Martin's release of 18 November 2002 on the F/A-22's clearance to Mach 2, as reproduced at <https://www.f-16.net/f-22-news-article1660.html>. Works of the United States government, and one company release, from which only facts are taken |
| In the repository | `assets/figures/f22.xml`: individual numbers, each with its source, not the documents' text |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

### The Short S.23 Empire flying boat, derived from JSBSim's

| | |
| --- | --- |
| Source | `ext/jsbsim`, JSBSim-Team/jsbsim at `v1.3.1` (`3b25f25`): `aircraft/Short_S23/Short_S23.xml` and its `Systems/` (`datcom_aero.xml`, `electrical.xml`, `engines.xml`, `flaps.xml`, `fuel-system.xml`, `Short_S23-hydrodynamics.xml`, `take-off-ap.xml`); `engine/eng_PegasusXc.xml` and `engine/prop_deHavilland5000.xml`; and JSBSim's shared `systems/hydrodynamics.xml`, `systems/hydrodynamic-planing-floats.xml` and `systems/sperry-a2-autopilot.xml` - Anders Gidenstam's model of the S.23 and its hull's and floats' hydrodynamics |
| Changes | Made by `tools/make_short_s23.py`, whose docstring lists each change and why: the model named `short_s23`; keel and float skids that meet land and not water; the drag's factor, 1.23 to 1.9; the airscrews' pitch levers the way round glideslope's propeller control is; a flap selector for glideslope's flap control; and eighteen declarations JSBSim warned of, removed. The shared systems, the engine and the airscrew are copied unchanged. |
| In the repository | `assets/jsbsim/`, as the script makes it (`aircraft/short_s23/`, `engine/eng_PegasusXc.xml`, `engine/prop_deHavilland5000.xml`, `systems/`); a test fails if they differ |
| Licence | The airframe, its systems but the aerodynamics, the engine, the airscrew and `sperry-a2-autopilot.xml` each name the GPL, "version 2 of the License, or (at your option) any later version" (the airscrew's, "GPL license version 2 or later"), Copyright (C) 2008 - 2023 Anders Gidenstam; they are used under this project's licence, GPL-3.0-or-later, whose text ships as `LICENSE`, and the modified files remain under it. `hydrodynamics.xml` and `hydrodynamic-planing-floats.xml` name the LGPL, "version 2.1 of the License, or (at your option) any later version", and remain under it; its text ships as `licenses/JSBSim.txt`. `datcom_aero.xml`, generated by Datcom+, names no licence of its own and is used as the JSBSim repository distributes it, under its LGPL-2.1; it keeps the United States Air Force's disclaimer for public release in its header. |

### The Short S.23's published figures

| | |
| --- | --- |
| Source | Flight magazine, as scanned by the Internet Archive (`sim_flight-international_*`): 29 October 1936 (Vol 30 No 1453), "The Aircraft Engineer" supplement p. 440f, the standard boat's specification panel and general arrangement, SHA-256 `332d6bb839f885d5c7032bd690e2393732f92b97b98d8c784921be2ecb2afbfd`; 10 December 1936 (No 1459), p. 619, SHA-256 `f905a346ae576c1150b25043366f37c9cec9d8cfbbe41ae3659b99cefcfb8e6d`; 17 December 1936 (No 1460), pp. 647-649, Arthur Gouge's paper to the Royal Aeronautical Society, part 1, SHA-256 `20268f77f97549b38b59dca42d51e3ecd5d52f2d98bc5a413075e4a347f3a7f5`; 24 December 1936 (No 1461), p. 680, its conclusion and Table II, SHA-256 `611d34fc28e99a53311580a1adb30dd0d09c95280c6af75091e5f3093728578f`; and 9 July 1936 (No 1437), the height over the water line, SHA-256 `76d1b7564cfe5d2553ee6aba92735656e034736edfac980d34df396f446adefe`. Each at `https://archive.org/details/sim_flight-international_<date>_<volume>_<number>` |
| In the repository | `assets/figures/short_s23.xml`: individual numbers, each with its page and column, and the draught scaled from the general arrangement - not the magazine's text, drawings or scans |
| Use | The checks the flight model is held to; see `docs/PROJECT_STATUS.md` |

Flight's pages are its publisher's. **Only facts are taken** - numbers, each
cited where it is used, and a few words quoted with each figure to say where
its number is - and a draught measured off its drawing; none of its text,
drawings or scans is in the repository. Nothing suggests Short Brothers,
Flight or its publisher endorses this model.

### The Airbus A380-841: its documents, and the Boeing 747's derivatives

The flight model - `assets/jsbsim/aircraft/a380/a380.xml` - is written for
this project by `tools/make_a380.py`, whose docstring names the source of each
number, and is under its licence, GPL-3.0-or-later. Its engine,
`assets/jsbsim/engine/Trent970.xml`, is JSBSim's `engine/TRENT-900.xml` (at
`v1.3.1`, `3b25f25`) made a Trent 970-84 by the same script, and remains under
JSBSim's LGPL-2.1, whose text ships as `licenses/JSBSim.txt`. A test fails if
the committed files differ from what the script writes. The figures it is held
to are `assets/figures/a380.xml`.

| Document | Copy consulted | What is taken from it |
| --- | --- | --- |
| Airbus, A380 Aircraft Characteristics - Airport and Maintenance Planning, revision 20, 1 December 2025 | <https://www.aircraft.airbus.com/sites/g/files/jlcbta126/files/2025-12/AC_A380_20251201.pdf>, SHA-256 `0973c3655f544f92b9a48ec83cc11f9ece7185b7efecb076919611421c59de09` | Dimensions, the gear's layout, ground clearances, fuel, the payload-range chart (for the operating empty weight), the take-off field length and the final approach speed |
| EASA type certificate data sheet A.110, issue 17, 5 August 2026 | <https://www.easa.europa.eu/en/downloads/7309/en>, SHA-256 `fca8e2e6640e0276eeaeec1c22486bf3a9d31a22ae83d585c06429b1dc9cff3c` | The certification basis, JAR 25 change 15; the weight variants and engines |
| FAA type certificate data sheet A58NM, revision 11, 25 July 2024 | The FAA's Dynamic Regulatory System, SHA-256 `329a98798893eeb41bbe8a3e784996186b7da3a59510083d8e03df0aa21888ea` | The datum, the control surfaces' travel, the maximum operating altitude |
| EASA type certificate data sheet E.012, RB211 Trent 900, issue 12, 16 March 2026 | <https://www.easa.europa.eu/en/downloads/7779/en>, SHA-256 `cc9b0e050db0adf2a4edcfc156b75045a2e947c21ae49169347c8c206974cdb5` | The Trent 970-84's take-off thrust and fan |
| ICAO Aircraft Engine Emissions Databank, version 32, March 2026 | <https://www.easa.europa.eu/en/downloads/131424/en>, SHA-256 `57a9ff572458ad3a3141afc1aea932b5faa5796d279f0ac74600b27869302530` | The Trent 970-84's bypass ratio |
| Airbus, A380 Facts and Figures, February 2022 | <https://www.airbus.com/sites/g/files/jlcbta136/files/2025-01/airbus-a380-facts-and-figures-february-2022.pdf>, read through the Internet Archive, SHA-256 `ec80c978d299ecd2260c3dbdd22652759ae3ae7ef128fd3a4454549ddf0f2a40` | The wing's area and sweep, the cruise Mach, the 575 t variants' zero-fuel weight |
| Airbus, A380-800 specifications page, as archived 17 January 2013 | <http://www.airbus.com/aircraftfamilies/passengeraircraft/a380family/a380-800/specifications/>, read through the Internet Archive | Mmo |
| Airbus, A380-800 Flight Deck and Systems Briefing for Pilots, STL 945.1380/05 issue 3, May 2009 | Only as text at <https://pdfcoffee.com/a380-800-flight-deck-systems-briefing-for-pilots-3-pdf-free.html>, a third party's copy whose integrity cannot be checked; the document says it is for information only | The slats' and flaps' settings for each configuration |
| NASA CR-2144, Heffley and Jewell, Aircraft Handling Qualities Data, December 1972, section IX, the Boeing 747 | <https://ntrs.nasa.gov/api/citations/19730003312/downloads/19730003312.pdf>, SHA-256 `f2976b2d9a3f62471de276c019f314af58b83c92ebd61f915779bea4d784349a`. A work of the United States government | Every stability and control derivative (table IX-2) and the inertias (table IX-3), carried to the A380 |

**Only facts are taken** - numbers, each cited where it is used - not the
documents' text or charts, none of which is in the repository. Airbus's
documents say their curves are "for information only"; this model is not
Airbus's, and nothing suggests Airbus endorses it.

### The Gates Learjet 35A: its flight manual, and NASA's Learjet 23

The flight model - `assets/jsbsim/aircraft/learjet35a/learjet35a.xml` - is
written for this project by `tools/make_learjet35a.py`, whose docstring names
the source of each number, and is under its licence, GPL-3.0-or-later. Its
engine, `assets/jsbsim/engine/TFE731-2.xml`, is JSBSim's `engine/Tay-620.xml`
(at `v1.3.1`, `3b25f25`) made a TFE731-2 by the same script, and remains under
JSBSim's LGPL-2.1, whose text ships as `licenses/JSBSim.txt`. A test fails if
the committed files differ from what the script writes. The figures it is held
to are `assets/figures/learjet35a.xml`.

| Document | Copy consulted | What is taken from it |
| --- | --- | --- |
| Gates Learjet 35A/36A Airplane Flight Manual, FM-108, change 23, FAA approved, with the FC-530 autopilot | A scan at <https://archive.org/details/learjet-35-36-afm-fc-530>, uploaded by a user, SHA-256 `e3e34e6b721854f12c7334b4e8911552eb857360d40bcafade87100919cd3a44` | The limitations, the weight and balance data - stations, chord, gear, fuel - and the performance charts: the stall speeds and take-off field length |
| FAA type certificate data sheet A10CE, revision 67, 19 February 2015 | The FAA's regulatory library, read through the Internet Archive, SHA-256 `107c11ef1988b089d5fbf561da9a27938675f5d403d364cb9245733c98ed5114` | The engines' thrust, the controls' travel, the mean aerodynamic chord, the maximum operating altitude |
| NTSB, operational factors group chair's factual report, WPR22FA068 (Learjet 35A N880Z) | <https://data.ntsb.gov/Docket/Document/docBLOB?ID=16325896&FileExtension=pdf&FileName=WPR22FA068+Factual+Report-Final-Rel.pdf>, SHA-256 `b3f302502c0bfba3c81518b0b9ff334d0d8dbf87c2fcd0ee6448694161ea373b` | A 35A's basic empty weight; the span between the tip tanks' centres |
| NASA TN D-6573, Soderman and Aiken, Full-Scale Wind-Tunnel Tests of a Small Unpowered Jet Aircraft with a T-Tail, November 1971 | <https://ntrs.nasa.gov/api/citations/19720002382/downloads/19720002382.pdf>, SHA-256 `683ec716deb22871c4721482a8162b5bb108ee82c9417ad3bea559755fe28025`. A work of the United States government | The Learjet 23's planform, flaps' lift, spoilers' drag, and lateral and directional derivatives |
| NASA TN D-7647, Parameter Estimation Techniques and Applications in Aircraft Flight Testing, 1974: Wingrove, estimation of longitudinal aerodynamic coefficients | <https://ntrs.nasa.gov/api/citations/19740017456/downloads/19740017456.pdf>, SHA-256 `383dd911fbdd48cf7285634d54ce926bcf93ec086a72456bfb05c93a41cf50fd`. A work of the United States government | A Lear Jet's longitudinal coefficients, identified from flight |
| Ross and Neal (Gates Learjet), Learjet Model 25 Drag Analysis, NASA/Industry/University General Aviation Drag Reduction Workshop, 1975 | <https://ntrs.nasa.gov/api/citations/19760003936/downloads/19760003936.pdf>, SHA-256 `4588391bf60ec5933df769d6ae486036739e3635dae709fba912a2413965f387` | The drag at zero lift and due to lift |
| Miller, Outside Loop Control in Asymmetrical Trimmed Flight Conditions, AFIT thesis, 2004 (DTIC ADA424733), table 1 | <https://archive.org/download/DTIC_ADA424733/DTIC_ADA424733.pdf>, SHA-256 `5d4639e0aeaae92db92cbd0b44623a2c96744680f1f83033a4993874cdcabb43` | A Learjet 25 model's inertias and side force - of low confidence, its table contradicting itself |
| United States Air Force, C-21 fact sheet | <https://www.af.mil/About-Us/Fact-Sheets/Display/Article/104522/c-21/>, read through the Internet Archive | The C-21A's speed at 41,000 ft |

**Only facts are taken** - numbers, each cited where it is used - not the
documents' text or charts, none of which is in the repository. The flight
manual's copy is a user's upload; its numbers agree with the type
certificate's where both give them.

### The Lockheed Martin F-35B and Northrop Grumman B-2A: what is published

The flight models - `assets/jsbsim/aircraft/f35b/f35b.xml` and
`assets/jsbsim/aircraft/b2/b2.xml` - are written for this project by
`tools/make_f35b.py` and `tools/make_b2.py`, whose docstrings name the source
of each number and every estimate, and are under its licence,
GPL-3.0-or-later. Their engines, `assets/jsbsim/engine/F135-PW-600.xml` and
`assets/jsbsim/engine/F118-GE-100.xml`, are JSBSim's `engine/F100-PW-229.xml`
(at `v1.3.1`, `3b25f25`) made each by the same scripts, and remain under
JSBSim's LGPL-2.1, whose text ships as `licenses/JSBSim.txt`. A test fails if
the committed files differ from what the scripts write. Their figures are
`assets/figures/f35b.xml` and `assets/figures/b2.xml`. Much of both aircraft's
performance, and all of their aerodynamics, is not public; what the models
estimate is listed in `docs/PROJECT_STATUS.md`. **The F-35B's lift fan is not
modelled**, so none of its STOVL performance is claimed, and **no service
ceiling is claimed for it**, because Lockheed Martin publishes none for any
F-35.

| Document | Copy consulted | What is taken from it |
| --- | --- | --- |
| Lockheed Martin, F-35B product card, ©2023, 23-08442_002, PIRA AER2023060207 | <https://www.f35.com/content/dam/lockheed-martin/aero/f35/documents/F-35B%20Product%20Card.pdf>, SHA-256 `cd353921c5351f37b51228fcbc2830a6967760f5084b1a2421f396e2b8571637` | Length 51.2 ft, span 35 ft, wing area 460 sq ft, internal fuel 13,100 lb, the F135-PW-600 at 38,000 lb Max and 26,000 lb Mil, Mach 1.6, range >900 n.mi, 7.0 g |
| Lockheed Martin, F-35 Lightning II Program Status and Fast Facts, April 2020, FG19-24749_004 | <https://www.lockheedmartin.com/content/dam/lockheed-martin/aero/documents/F-35/FG19-24749_004%20F35FastFacts4_2020.pdf>, SHA-256 `fc7ad7c6443ffc357de5d30e61787ee73d002ae29b7acb77986bf00903480a03` | Empty weight 32,300 lb and height 14.3 ft, which the product card does not give; maximum weight "60,000 lb class". It is the newest Fast Facts carrying the specification table: the June 2023, November 2024 and March 2025 editions have dropped it. |
| United States Air Force, B-2 Spirit fact sheet, current as of December 2015 | <https://www.af.mil/About-Us/Fact-Sheets/Display/Article/104482/b-2-spirit/>, read through the Internet Archive, SHA-256 `cabe940c317105cc8d358dcee047675d2150ed9db9f034cb853ebd3f8da22c42` | Its size, weights, engines' thrust, speed, ceiling and range |
| NASA CR-2144, Heffley and Jewell, Aircraft Handling Qualities Data, December 1972, section IV, the F-4C | As in the A380's entry above | The F-35B's moments' derivatives and inertias - the F-4C's, the nearest published fighter's |

**Only facts are taken** - numbers and a few words quoted with each figure -
not the documents' text. Nothing suggests Lockheed Martin, Northrop Grumman,
Pratt & Whitney, General Electric or the Air Force endorses these models.

### The Mosquito FB Mk VI: its trials, Pilot's Notes and engine curves

The flight model - `assets/jsbsim/aircraft/mosquito-fb6/`,
`assets/jsbsim/engine/merlin25.xml` and
`assets/jsbsim/engine/prop_dh_hydromatic.xml` - is written for this project
and is under its licence, GPL-3.0-or-later; no other simulator's Mosquito is
used. Its numbers come from these documents, and the figures it is held to
(`assets/figures/mosquito-fb6.xml`) quote them by paragraph or figure.

| Document | Copy consulted | What is taken from it |
| --- | --- | --- |
| A&AEE Boscombe Down report 767,e, 12th part, 1 February 1944: Mosquito FB Mk VI HX809, level speed performance at normal and increased boost rating | Scan at <http://www.wwiiaircraftperformance.org/mosquito/hx809.pdf> and `hx809-level.jpg` beside it, "courtesy Neil Stirling" | Level speeds and full-throttle heights at +18 lb/sq in; the loading; the propellers' type and diameter |
| A&AEE report 767,e, 7th part, 19 September 1943: Mosquito FB Mk VI HJ679, brief performance trials | Scan at <http://www.wwiiaircraftperformance.org/mosquito/hj679.pdf> and `hj679-climb.jpg` | Climb rates, time to 20,000 ft, the boost obtained, the loadings |
| Rolls-Royce, Merlin 24, 25, 26, 27, 224, 225 altitude performance (AVIA 6/5817, fig. 1) | Scan at <http://www.wwiiaircraftperformance.org/mosquito/merlin25-powercurve.jpg> | The engine's power with height at +18 and 3,000 rpm; the reduction gear, 0.42:1 |
| A.P. 2019E, Pilot's Notes for the Mosquito FB 6, 1950 edition | A transcription distributed by Zeno's Warbird Video Drive-In, <http://www.zenoswarbirdvideos.com> | Engine limits, the supercharger's gear change heights, fuel tanks, stalling speeds, safety speeds, single-engine performance, take-off handling |
| A.P. 2019E, Pilot's Notes, 1944 edition | Page scans at <https://www.fs2000.org/2004/02/15/pilots-notes-for-mosquito/> | The swing on take-off; stalling speeds at full load |
| Aircraft Data Sheet, Mosquito B Mk IV, card 3(b), 1 May 1944 | Scan at <http://www.wwiiaircraftperformance.org/mosquito/Mosquito_MkIV_ads.jpg> | Span, wing area, length; the take-off distance over 50 ft |
| Air Fighting Development Unit report 74, tactical trials, Mosquito VI | Scan at <http://www.wwiiaircraftperformance.org/mosquito/Mosquito-VI-tactical.pdf> | Loadings, for the empty weight's estimate |

All are Crown copyright documents of 1943 to 1950. **Only facts are taken** -
numbers, each cited where it is used, and a few words quoted with each figure
to say where its number is - not their text, charts or scans, none of which is
in the repository.

### Aircraft visual models, from FlightGear's aircraft

Fourteen of the sixteen aircraft ship a visual model. The geometry is AC3D
(`.ac`), and for the A380 also 3D Studio (`.3ds`), placed by FlightGear model
XML; `tools/make_models.py` fetches the pinned files listed in
`assets/models/sources.txt`, each by URL and SHA-256, flattens each aircraft's
exterior into one mesh in the body frame and writes
`assets/models/<model>.mesh`. A test fails if what is committed differs from
what the script makes, and another fails if a model ships without the entry
below or an entry names a model that does not ship.

Eight of the fourteen come from a FlightGear directory that states its terms.
The other six state none anywhere in their own directory, and ship on
FGAddon's project-wide requirement that its content is GPL. That is a policy
and not a grant by the author; the project owner decided on 2026-09-20 that
those six ship on it, and each entry below says so and says what was looked
at.

No mesh carries a texture, so no livery ships and a surface takes the flat
diffuse colour of its AC3D material; none carries an animation, so control
surfaces, gear and propellers are welded where the model has them. Each
model's origin is its FlightGear aircraft's, which is not always its flight
model's, so a model is drawn at its flight model's visual reference point
moved by the offset in `assets/models/alignment.txt`, which
`tools/align_models.py` measures by putting the model's undercarriage on the
flight model's. That file also records what the fit leaves over, because a
flight model and a visual model of the same aeroplane do not always agree,
and each aircraft is held to its own figures.

**The meshes are also measured, not only drawn.** A mesh is the airframe, so
it is where the points an aeroplane comes down on with its wheels up are taken
from: the belly, the nose, the wing tips and the tail. `tools/ground.py`
measures them and writes them into the flight models of the aeroplanes that
had none, and holds each mesh to within 1.5% of its aeroplane's published
length and span, because a height measured off a mesh is only worth what the
mesh's scale is worth. It takes only the height *above the mesh's own tyres*,
a difference within one mesh, so it does not depend on the alignment above -
which for three aircraft is out by between half a metre and two and a half.

### Visual model: c172p - FlightGear's c172p

| | |
| --- | --- |
| Source | The c172p team's own repository, <https://github.com/c172p-team/c172p>: `Models/c172p.xml` and the `Models/c172-common.ac` it places, pitched -3 degrees as that file asks |
| Revision | commit `84477612bba340ab98004a10f8b28a81c18e6169`, 2026-09-02 |
| Licence | GPL-2.0: the GNU GPL v2 text verbatim in `LICENSE`, and GitHub reads the repository as GPL-2.0 |
| In the repository | `assets/models/c172p.mesh`, 45,451 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: c182 - FlightGear's c182s

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/c182s`: `Models/c182s.xml` and the `Models/c182s.ac` it places |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL-2.0: the GNU GPL v2 text verbatim in `LICENSE` |
| In the repository | `assets/models/c182.mesh`, 30,169 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: pa28 - FlightGear's PA28-161 Warrior II

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/PA28`: `Models/PA28-161-180.xml`, which is an include of `Models/PA28-161-160.xml`, and the `Models/PA28-161.ac` it places |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL-2.0: the GNU GPL v2 text verbatim in `LICENSE` |
| In the repository | `assets/models/pa28.mesh`, 91,464 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: j3cub - FlightGear's J3Cub

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/J3Cub`: `Models/J3Cub.xml` and the `Models/J3Cub.ac` it places |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL-3.0: the GNU GPL v3 text verbatim in `copying.txt`, and `readme.txt` says "License: GPL (see file \"COPYING.txt\" for details)" |
| In the repository | `assets/models/j3cub.mesh`, 51,387 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: 737-300 - FlightGear's 737-300

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/737-300`: `Models/737-300.xml` and the `Models/737-300.ac` it places |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL-3.0: the GNU GPL v3 text verbatim in `LICENSE.md`, and `README.md` says "This is the 737-300 in Progress and under GNU GPL v3.0" |
| In the repository | `assets/models/737-300.mesh`, 48,318 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: 747-400 - FlightGear's 747-400

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/747-400`: `Models/747-400.xml`, and the `747-400_fuselage.ac`, `747-400_gear.ac`, `747-400_wings.ac` and four placements of `GE_CF6-80C2B1F.ac` it composes |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL-2.0: the GNU GPL v2 text verbatim in `COPYING` |
| In the repository | `assets/models/747-400.mesh`, 24,382 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: 787-8 - FlightGear's 787-8

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/787-8`: `Models/787-8.xml` and the `Models/787-8.ac` it places |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL-2.0: the GNU GPL v2 text verbatim in `COPYING` |
| In the repository | `assets/models/787-8.mesh`, 28,759 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: a320 - FlightGear's A320-family, the A320-200 with CFM56 engines

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/A320-family`: `Models/A320-200-CFM.xml`, through `A320-common.xml` and `Fuselage/fuselage.xml` to `Fuselage/res/A320-216.ac`, and `Fuselage/a320.cfm.xml` to `Fuselage/res/CFM56.ac` |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL-2.0: the GNU GPL v2 text verbatim in `LICENSE`; the model files carry "Copyright (c) 2026 Josh Davidson (Octal450)" |
| In the repository | `assets/models/a320.mesh`, 107,449 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: a380 - FlightGear's A380

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/A380`: `XML/A380.xml` and the `Models/a380.ac` it places, with the wing (`XML/Wings/wings.xml`), the horizontal tailplane (`XML/htp.xml`), the belly fairing, the four pylons and the four engines, which are 3D Studio rather than AC3D |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL, on FGAddon's project-wide requirement that its content is GPL - a policy, not a grant stated by the author, on which the project owner decided on 2026-09-20 that this model ships. What was looked at: no `COPYING`, `LICENSE`, `README` or licence file of any kind at any level of the directory, and `A380-set.xml` names the authors "Ampere.K, I.Cunningham, F.Dalvi, S.Hamilton, et al" and states no terms. |
| In the repository | `assets/models/a380.mesh`, 17,278 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: b2 - FlightGear's B-2 Spirit

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/B-2`: `Models/b2-spirit.xml` and the `Models/spirit.ac` it places |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL, on FGAddon's project-wide requirement that its content is GPL - a policy, not a grant stated by the author, on which the project owner decided on 2026-09-20 that this model ships. What was looked at: no licence file at any level, `B-2-set.xml` names the author "Markus Zojer" and states no terms, and `readme-spirit.txt` is a flying guide. |
| In the repository | `assets/models/b2.mesh`, 5,612 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: f35b - FlightGear's F-35B

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/F-35B`: `Models/F-35B.xml` and the `Models/F-35B.ac`, `Models/Engine.ac` and `Models/Gear.ac` it places |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-22 |
| Licence | GPL-3.0, the GNU GPL v3 text verbatim in `License.txt` |
| In the repository | `assets/models/f35b.mesh`, 27,643 triangles, as `tools/make_models.py` makes it; a test fails if it differs. Its `antennas` object is left out: it is a nose air-data boom reaching 1.5 m past the radome, which would make the model 10% longer than the published aeroplane. Without it the mesh is 15.64 m against a published 51.2 ft. |

### Visual model: f15c - FlightGear's F-15C

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/F-15`: `Models/F-15C.xml`, which is an include of `Models/f15c.xml`, and the `Models/f15c.ac` it places. Its missiles, bombs, rails and drop tanks are a payload FlightGear picks, not the airframe, and are left out, as is the boarding ladder |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL, on FGAddon's project-wide requirement that its content is GPL - a policy, not a grant stated by the author, on which the project owner decided on 2026-09-20 that this model ships. What was looked at: no licence file at any level, and `README.txt` is a feature list crediting Richard Harrison that states no terms. |
| In the repository | `assets/models/f15c.mesh`, 162,347 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: f22 - FlightGear's F-22 Raptor

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/Lockheed-Martin-FA-22A-Raptor`: `Models/F-22-JSBSIM-Model-File.xml` and the `Models/Lockheed-Martin-FA-22A-Raptor.ac` it places |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL, on FGAddon's project-wide requirement that its content is GPL - a policy, not a grant stated by the author, on which the project owner decided on 2026-09-20 that this model ships. What was looked at: no licence file at any level, and `f22-jsbsim-set.xml` names the author "Fabrizio Fracaroli" and states no terms. |
| In the repository | `assets/models/f22.mesh`, 29,949 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: mosquito-fb6 - FlightGear's Mosquito FB Mk VI

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/mosquito`: `Models/Mosquito-FB6.xml` and the `Models/mosquitofb6.ac` it places, with the propeller blades from `Models/pdisk.ac` - the airframe carries hubs and no blades - and without the two discs FlightGear blurs them into |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL, on FGAddon's project-wide requirement that its content is GPL - a policy, not a grant stated by the author, on which the project owner decided on 2026-09-20 that this model ships. What was looked at: no licence file at any level, and `mosquito-fbVI-set.xml` names the authors "Ludovic Brenta, Detlef Faber." and states no terms. |
| In the repository | `assets/models/mosquito-fb6.mesh`, 12,506 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Visual model: short_s23 - FlightGear's Short Empire

| | |
| --- | --- |
| Source | FGAddon, `Aircraft/Short_Empire`: `Models/Short_Empire.xml` and the `Models/Short_Empire.ac` it places, with the four propellers, the four Pegasus Xc engines and their cowling gills, and without the discs the propellers blur into |
| Revision | Subversion r21588 of <https://svn.code.sf.net/p/flightgear/fgaddon/trunk>, 2026-09-19 |
| Licence | GPL, on FGAddon's project-wide requirement that its content is GPL - a policy, not a grant stated by the author, on which the project owner decided on 2026-09-20 that this model ships. What was looked at: no licence file at any level. `Short_Empire-set.xml` and `Models/Short_Empire.xml` each carry "Copyright (C) 2007 - 2025 Anders Gidenstam ... This file is licensed under the GPL license version 2 or later", which is a grant, but it is on those two files and not on the geometry; `AUTHORS` credits the propeller models to the Boeing 314 and the engine model to the Lockheed-Vega, without terms. |
| In the repository | `assets/models/short_s23.mesh`, 85,604 triangles, as `tools/make_models.py` makes it; a test fails if it differs |

### Aircraft with no visual model

One aircraft ships none: FlightGear has no model of it.

### No visual model: learjet35a

| | |
| --- | --- |
| Why | FlightGear has no Learjet of any mark in FGAddon, so there is nothing to take. |

### The Copernicus DEM, GLO-30 Public

| | |
| --- | --- |
| Source | The Copernicus DEM GLO-30 Public, as Cloud Optimized GeoTIFFs in the AWS Open Data bucket `copernicus-dem-30m` (<https://copernicus-dem-30m.s3.amazonaws.com/readme.html>) |
| Version | The bucket names no release. Its objects are dated 2022-05-09; each tile's metadata gives its creation as 2019-10-19 and its heights as "WGS 84 Geoid EGM08". What is used is pinned file by file, by SHA-256 |
| Pinned | `tests/data/downloads/files.txt`: `Copernicus_DSM_COG_10_S34_00_E151_00_DEM.tif` (33-34 S, 151-152 E), 20,882,213 bytes, SHA-256 `6e20871096986cd00fc3903ea95670a0d83860236a0e4f1360ac9ac67def485d`; and its water body mask, `AUXFILES/Copernicus_DSM_COG_10_S34_00_E151_00_WBM.tif`, 158,882 bytes, SHA-256 `bc3da27a02dfd81e8572bf6bf997eba64b4669403c29269a50e0ad9de326b654` |
| Used | Each tile's heights, and its water body mask - the auxiliary file published beside it, on the same grid - for where the ground is water |
| In the repository | No tile or mask: the tests fetch the tile and its mask into the build tree, or the directory `GLIDESLOPE_DOWNLOADS` names. `assets/dem/coverage.txt` says which 1-degree cells have a tile at 30 m, only at 90 m, or none; `tools/make_dem_coverage.py` makes it from both buckets' `tileList.txt` (30 m: 1,110,900 bytes, SHA-256 `10604e3052c98a09e9216f1a8f0a555a04148419757575f783d4937fd44316dc`; 90 m: 1,111,950 bytes, SHA-256 `e5a5efe088e70506bc1007d22006bdcb09b0ec03177b62f9652363c13f49ed97`), and a test checks it still matches them |
| Licence | "Licence for Copernicus DEM instance COP-DEM-GLO-30-F Global 30m Full, Free & Open", published beside each tile as `INFO/eula_F.pdf` (SHA-256 `32049914c37f14e7d53b48d13d74a49e77c030236e2acc7a0df426f9344feba2`). It grants, free of charge, worldwide and without limit in time, "(a) reproduction; (b) distribution; (c) communication to the General Public; (d) adaptation, modification and combination with other data and information." |

Its Article 6, quoted:

> (a) When communicating to the General Public or distributing the Copernicus
> WorldDEM-30, the User shall inform the General Public of the source by using
> the following notice: © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH
> 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights
> reserved.
>
> (b) Where the Copernicus WorldDEM-30 data have been adapted or modified, the
> User shall provide the following notice: "produced using Copernicus
> WorldDEM-30 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH
> 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights
> reserved".
>
> (c) Users exercising the right of distribution or communication to the General
> Public of the Copernicus WorldDEM-30, modified or not, must ensure that the
> Subsequent Users understand that neither the Licensor nor any other legal
> entities in charge of the Copernicus programme or the delivery of Copernicus
> data and information under the Copernicus programme may be held liable with
> regard to any aspect of the Copernicus WorldDEM-30. The following sentence or
> its translation in any language shall be added by such Users in a licence or
> any legal warning or notice covering their distribution or communication to
> the General Public of the Copernicus WorldDEM-30: "The organisations in charge
> of the Copernicus programme by law or by delegation do not incur any liability
> for any use of the Copernicus WorldDEM-30".
>
> (d) User shall make sure not to convey the impression to the General Public
> that the user's activities are officially endorsed by the Provider, the
> Licensor or any other legal entities in charge of the Copernicus programme or
> the delivery of Copernicus data and information under the Copernicus
> programme.

**Its stated accuracy**, from the Copernicus DEM Product Handbook (version 5.0,
29 November 2022, <https://dataspace.copernicus.eu/sites/default/files/media/files/2024-06/geo1988-copernicusdem-spe-002_producthandbook_i5.0.pdf>,
SHA-256 `b5a0b027bddef7122e39de368dba9cf10adce8631a2e3026d246b4be98c3f0c4`),
for the EEA-10, GLO-30 and GLO-90 instances, quoted:

> Absolute Vertical Accuracy 1) 2) 3) < 4m (90% linear error)
>
> 3) Due to the global coverage of the TanDEM-X DEM / WorldDEM / Copernicus DEM,
> all accuracy statistics and values stated in this document are calculated as
> an arithmetic mean. Local deviations can occur.

**Its water body mask**, from the same handbook (section 1.2.5.4 and its
table 8, page 21), quoted:

> The Water Body Mask (WBM) shows all DEM pixels, which are classified as water
> and edited according to the categories Ocean, Lake or River. Table 8 shows the
> meaning of the pixel values.

Its values are 0, no water; 1, ocean; 2, lake; 3, river (`world::Water`).
Its format (section 1.2.5, "8 Bit unsigned integer, GeoTIFF") is read by
`world/geotiff.hpp` as the heights are; a test holds it to an independent
decoder's reading of the pinned mask. The mask is not shown: it says only
where the ground is water, and so where a landplane ditches.

**What that asks of glideslope, and where it is done.** The client draws
terrain made from the DEM - adapted, so notice (b) - and `glideslope_cli height`
prints heights from it. Both show notice (b) wherever the data are shown: the
client along the bottom of every frame that has terrain in it, as its other
credits are, and the CLI after the heights (`world::copernicus_dem_notice`).
The client's font has capitals, digits and a little punctuation and no
copyright sign, so on screen the notice is in capitals with "(C)" for "©"; a
test reads it back out of the frame. (c)'s sentence is in `README.md`, which
every package carries, with the notice. Nothing says or suggests the Copernicus
programme endorses this project, as (d) asks.

### The EGM2008 geoid, GeographicLib's 5-minute grid

| | |
| --- | --- |
| Source | GeographicLib's geoid distribution, `egm2008-5.zip` from <https://sourceforge.net/projects/geographiclib/files/geoids-distrib/>, listed on <https://geographiclib.sourceforge.io/C++/doc/geoid.html> |
| What it is | NGA's Earth Gravitational Model 2008 evaluated on a 5-arc-minute grid, 4320 by 2161 points, quantised to 3 mm; its header says "WGS84 EGM2008, 5-minute grid", dated 2009-08-29, with a maximum bilinear interpolation error of 0.478 m (RMS 0.012 m) |
| Pinned | `tests/data/downloads/files.txt`: `egm2008-5.zip`, 16,773,259 bytes, SHA-256 `408f05e0c04a9f2e17b9ea2d27123f936e9dea60128bb3411a272f8ddbe318dd` |
| In the repository | Nothing: fetched, as the DEM is |
| Use | Converting the DEM's heights above the geoid to heights above the WGS84 ellipsoid (`world/geoid.hpp`) |
| Licence | **Public domain**, as the model's. GeographicLib's geoid page and the zip state no terms of their own for the grids (of the library, GeographicLib's documentation says: "It is licensed under the MIT License; see LICENSE.txt."). The grid is NGA's EGM2008 evaluated by GeographicLib, and PROJ's data package, which redistributes NGA's EGM2008 as a grid GeographicLib produced, records it so; see below |

PROJ-data's `us_nga/us_nga_README.txt`
(<https://github.com/OSGeo/PROJ-data/blob/master/us_nga/us_nga_README.txt>),
quoted:

> ### Vertical grid: EGM2008 geoid model
>
> *Source*: [NGA](http://earth-info.nga.mil/GandG/wgs84/gravitymod/egm2008/egm08_wgs84.html)
> *Format*: GeoTIFF converted from GTX
> *License*: Public Domain
>
> 2.5 minute worldwide geoid undulation grid that transforms physical heights
> to WGS84 ellipsoidal heights.
>
> This file has been produced by [GeographicLib](https://geographiclib.sourceforge.io/html/gravity.html)
> using the EGM2008 gravity model

NGA's own page could not be read when this was recorded; the statement above
is PROJ's, about the same model.

### Surveyed heights the DEM is tested against

| | |
| --- | --- |
| In the repository | `tests/data/dem/surveyed.txt`: twelve runway ends, five coastal waters and five summits, each with its position, height and source |
| Runway ends | The FAA's airport data - surveyed runway-end positions and NAVD 88 elevations - as published by AirNav (`https://www.airnav.com/airport/<ID>`), read 2026-09-18, for KDEN, KLAS, KBOS, PAJN, PANC and PABR. Individual numbers, not AirNav's pages |
| Summits | The US National Geodetic Survey's datasheets for triangulation stations GT1811, KL0637, FQ0624, GM0779 and CD0994 (`https://geodesy.noaa.gov/api/nde/pid?pid=<PID>`), read 2026-09-18: adjusted NAD 83 positions and NAVD 88 heights. Works of the United States government |
| Coastal waters | Positions chosen off coasts, whose height is sea level by definition |

### Imagery: EOX's Sentinel-2 cloudless, 2016

| | |
| --- | --- |
| Source | EOX IT Services GmbH's Web Map Tile Service, `https://tiles.maps.eox.at/wmts/1.0.0/`, layer `s2cloudless` ("Sentinel-2 cloudless layer for 2016 by EOX - 4326"), tile matrix set `WGS84` - latitude and longitude, two tiles across at level 0 - JPEG tiles of 256 pixels, read to level 13, about 10 m a pixel (`gfx::open_imagery()`) |
| What it is | A cloud-free mosaic of the whole Earth made from the Copernicus Sentinel-2 satellites' images of 2016 |
| Version | The 2016 layer, which EOX no longer changes: its tiles are dated 2 August 2017 (`Last-Modified`) |
| In the repository | Nothing. The client fetches tiles as the view needs them and keeps them in `cesium-cache.sqlite` in the cache directory for as long as the service's `Cache-Control: max-age=604800`, a week, allows; the terrain test's reference keeps the tiles it reads in the downloads directory |
| Licence | **CC BY 4.0.** The service's own capabilities document (`WMTSCapabilities.xml`, read 2026-09-18) says of the layer: "EOxCloudless https://cloudless.eox.at by EOX IT Services GmbH (Contains modified Copernicus Sentinel data 2016) released under Creative Commons Attribution 4.0 International License." EOX's announcement (<https://eox.at/2017/08/sentinel-2-global-cloudless-mosaic/>) says the same, and that the service endpoints may be used directly in an application. The later years' layers are CC BY-NC-SA 4.0 and are not used |
| Credit | **"Sentinel-2 cloudless - https://s2maps.eu by EOX IT Services GmbH (Contains modified Copernicus Sentinel data 2016)"**, the text EOX asks for, on every frame with imagery in it, in capitals as the client's font has them. Changes made: the tiles are resampled onto the terrain and lit by its slope |

EOX's announcement, quoted:

> Sentinel-2 cloudless by EOX IT Services GmbH is now licensed under a
> Creative Commons Attribution 4.0 International License.

The Copernicus Sentinel data the mosaic is made from are named in the credit,
as EOX's text has them.

### METARs from aviationweather.gov

| | |
| --- | --- |
| Source | The Aviation Weather Center's Data API, of the US National Weather Service (NOAA): `https://aviationweather.gov/api/data/metar?ids=<ICAO>&format=json`, documented at <https://aviationweather.gov/data/api/> |
| Version | None: a METAR is the latest observation. What the program reads is each report's raw text (`rawOb`), with the station's position and elevation |
| In the repository | `tests/data/weather/aviationweather-metars-2026-09-17T1600Z.json`: one response, fetched 2026-09-17 at about 16:08 UTC, for CYYZ, EGLL, KBOS, KDEN, NZCH, PABR, SCCI, UUEE, YSSY and ZBAA, unmodified. `tests/data/weather/aviationweather-gusts-2026-09-18T0800Z.json`: the three reports with gusts - EGPH, KABQ and KMWN - of one response for 69 stations fetched 2026-09-18 at about 08:25 UTC, each report as the response gave it. `tests/data/weather/aviationweather-shear-2026-09-18.json`: five reports found searching 2026-09-18's responses for wind shear and its remarks - LPPT's `WS R02` and RJAA's `WS R34R`, from a week's history of 49 stations, and KJAC's `WSHFT`, KCQC's and KABQ's `PK WND`, from 1,357 reports over eight regions - each as the response gave it |
| Use | `world/weather.hpp`: the surface wind, temperature and QNH a flight's weather starts from; `glideslope_cli weather STATION`; `glideslope --weather STATION` |
| Licence | **Public domain**, as National Weather Service information. The API page's "Disclaimer" link is <https://www.weather.gov/disclaimer>; see below |
| Credit | Not required. `glideslope_cli weather` names the source; nothing presents the data as official NWS material |

The National Weather Service's disclaimer, read 2026-09-17, quoted:

> The information on National Weather Service (NWS) Web pages are in the public
> domain, unless specifically noted otherwise, and may be used without charge
> for any lawful purpose so long as you do not: 1) claim it is your own (e.g.,
> by claiming copyright for NWS information -- see below), 2) use it in a
> manner that implies an endorsement or affiliation with NOAA/NWS, or 3) modify
> its content and then present it as official government material.

The Data API page's own guidelines, quoted, which the program keeps to - it
sends its own User-Agent, and asks for one station when a flight starts and
every fifteen minutes after:

> Set a custom user agent to prevent automated filtering inadvertently blocking
> valid traffic. Consider product update frequency. For example most METARs
> update once per hour and TCF is issued every other hour. Wait between
> consecutive requests — maximum 100 requests per minute. Exceeding request
> limits will result in access being blocked.

### Winds aloft from Open-Meteo

| | |
| --- | --- |
| Source | Open-Meteo's free forecast API, `https://api.open-meteo.com/v1/forecast`, asking for wind speed, wind direction, temperature and geopotential height on 19 pressure levels from 1000 to 30 hPa, and wind speed and direction 10, 80, 120 and 180 m above the ground, hourly, for one day (`world::open_meteo_url`) |
| Version | None: the forecast for the hour asked for. Open-Meteo combines national weather services' models, chosen per place |
| In the repository | `tests/data/weather/open-meteo-sydney-2026-09-17.json`: one response for -33.9461, 151.1772 (the grid point -33.919155, 151.1596), 2026-09-17 00:00 to 23:00 UTC, fetched 2026-09-17, unmodified. `tests/data/weather/open-meteo-sydney-2026-09-18.json`: the same request with the winds 10, 80, 120 and 180 m above the ground added, 2026-09-18 00:00 to 23:00 UTC, fetched 2026-09-18, unmodified |
| Use | `world/winds_aloft.hpp`: the wind and temperature above the surface; `glideslope_cli weather STATION`; `glideslope --weather STATION` |
| Licence | **CC BY 4.0**, with Open-Meteo's terms for the free API: non-commercial use only, and under 10,000 calls a day. This project is free software with no subscriptions or advertising; a server run for profit would need Open-Meteo's commercial API |
| Credit | **"Weather data by Open-Meteo.com"**, with a link to <https://open-meteo.com/>. `glideslope_cli weather` prints it with the link and the licence; the client's HUD shows it along the bottom whenever the flight is in reported weather (`world::open_meteo_credit`); this entry credits the recorded response. Changes made: the levels' geopotential heights are converted to geometric heights, and winds from speed and direction to north and east components |

Open-Meteo's terms (<https://open-meteo.com/en/terms>), read 2026-09-17, quoted:

> By using the Free API for non-commercial use you agree to following terms:
> Less than 10'000 API calls per day, 5'000 per hour and 600 per minute. You
> may only use the free API services for non-commercial purposes. You accept to
> the CC-BY 4.0 licence, as specified in the licence conditions.

Its licence page (<https://open-meteo.com/en/licence>), quoted:

> API data are offered under Attribution 4.0 International (CC BY 4.0) [...]
> Attribution: You must give appropriate credit, provide a link to the licence,
> and indicate if changes were made. You may do so in any reasonable manner,
> but not in any way that suggests the licensor endorses you or your use. You
> must include a link next to any location Open-Meteo data are displayed, for
> example: `<a href="https://open-meteo.com/">Weather data by Open-Meteo.com</a>`

A HUD cannot hold a link; the client shows the credit's text on screen and
prints it with the link when the flight's weather is fetched, as
`glideslope_cli weather` does.

## Written here, from no outside source

Some of what ships is this project's own writing rather than anyone else's
data. It is recorded here so that the answer to "where did this come from"
is never silence.

### Figures measured from the flight models, where nothing is published

Sixteen figures in `assets/figures/*.xml` carry `from="measured"`, added
2026-09-23. **They are not from any source and no source is named for them.**
They are what this project's own flight models do, measured once and written
down, so that the aeroplanes whose manuals are not public can be taught
something beyond turns - a lesson's reference speed has to come from
somewhere, and inventing one would mean nothing.

| | |
| --- | --- |
| Source | **None.** Each number is a measurement of this project's flight model, taken by the same flights in `src/sim/figures.cpp` that check the published figures |
| What they assert | That the model still does what it did when the number was taken - **not** that the aeroplane does it. A published figure holds the model to the world; a measured figure holds the model to itself |
| Stall speeds, in the landing configuration | 737-300 105.72, 787-8 113.71, A320 113.33, A380 104.86, B-2A 95.42, F-15C 151.06, F-35B 122.24, Short S.23 66.32 KCAS. Each was taken from three entry speeds a comfortable margin above the stall - 160, 200 and 240 knots, and 120 and 160 for the Short S.23 - and varies by at most 1.2 knots between them, so the number is the model's and not the entry's |
| Rates of climb, and the speeds they are flown at | 737-300 4,438 ft/min at 260 kt, 787-8 5,457 at 300, A320 5,804 at 300, A380 4,773 at 270, B-2A 8,271 at 230, F-15C 24,569 at 200, F-35B 20,390 at 200, Learjet 35A 8,097 at 240. The Learjet is the one of these whose stall speed *is* published, so only its rate of climb is measured |
| How the climbing speed was chosen | The rate was swept from 150 or 200 knots upwards and the speed taken is **the lowest that still climbs within ninety-five per cent of the best rate**, never below 1.3 times the measured stall speed, **and never below a speed the aeroplane can actually be climbed at**. The peak itself is no use: a jet's climb rate goes on rising to speeds it is never operated at - the A320's peaks at 400 knots - and a fighter's is flat within a few per cent over hundreds of knots, so for the F-15C and the F-35B the floor is what chose the speed |
| Why "can actually be climbed at" is a separate condition | The sweep measures the rate at **full throttle**, and a fighter will climb at full throttle from almost any speed. A lesson does not: the autopilot holds a speed *and* a rate of climb, and near the stall it runs out of nose before it runs out of thrust. The F-35B at 160 knots - its 1.3-times-stall floor - pinned the nose at the fifteen degrees it is allowed, sat at nineteen degrees of alpha and sank at 2,000 ft/min. It was the only one of the eight this caught, and its speed is 200 rather than 160 because of it |
| The weight each was measured at | **The stall speed and the rate of climb of a given aeroplane are measured at the same loading** - `landing` for the airliners, `light` for the B-2A, `clean` for the F-15C, `combat` for the F-35B, `standard` for the Short S.23, `landing` for the Learjet 35A - so that a lesson's two speeds describe the same aeroplane on the same flight. A published climbing speed is usually given at a take-off weight and a published stall speed at a landing weight, which is right for a handbook and wrong for a lesson that names both in one exercise. A heavier aeroplane climbs best a little faster than these numbers say |
| Licence | This project's own measurements, GPL-3.0-or-later with the rest |

**The Boeing 747-400 and the Lockheed Martin F-22A have no measured figures
at all, and so are taught only turns.** They have no measured climbing speed
either, and that follows from the stall speed rather than being a second
problem: the climbing speed is floored at 1.3 times the stall speed, so
without one there is nothing to anchor it to. The 747-400's ninety-five per
cent cut lands at 360 knots, within five of its maximum operating speed,
which is not a speed anything climbs at - the floor is what would have stopped
that, and it has no floor. **An aeroplane's reference speeds hang together, and
these two have none of them.**

The stall speed is what neither will give. Measured the way the other eight
were, the same aeroplane came out between 146 and 160 knots (the 747-400) and
between 128 and 149 (the F-22A), depending only on the speed the deceleration
was started from. A number that moves by twenty knots with the approach to it
is not a measurement of anything, and is not written down. The other eight
move by at most 1.2 knots over the same range.

**Making the deceleration stop only on a sustained rise was tried, and made it
worse.** The flight ends when the speed passes three knots above the slowest
seen, and the thought was that a single sample there could be a phugoid rather
than the stall. Requiring two seconds of it changed nothing at all for the
eight that already held still - the same numbers to a hundredth of a knot,
which is what a no-op looks like - and widened the 747-400's spread to 21.8
knots and the F-22A's to 84.3, one run mushing all the way down to 57. The
change was reverted. What those two do near the stall is not a late break in
the measurement; it is the models themselves.

### The aircraft checklists

`assets/aircraft/*.checklist`: a checklist for each of the nine phases of
flight for every aircraft in the roster. **Six of the sixteen are written
from the aeroplane's own handbook; ten are not**, and which is which is
in each file's own header as well as here.

#### The six written from a handbook

| | |
| --- | --- |
| Source | The same handbooks recorded above for these aeroplanes' published figures, read for their normal-procedures sections: the **Cessna 172P** POH and FAA Approved Airplane Flight Manual, 1981 Model 172P, section 4, pages 4-6 to 4-10, as copied at <https://tx435.cap.gov/media/cms/C172PPOHwoSupplements_0A69C5AA130B9.pdf> and <https://www.lsvr.de/de/wp-content/uploads/2019/10/POH-Cessna-172-P-D-EKRM.pdf> (the second read only to settle two figures the first had scanned badly); the **Cessna 182S** POH, section 4, pages 4-11 to 4-17, at <http://tssflyingclub.org/documents/C182S_POH.pdf>; and the **Piper PA-28-180 Cherokee "E"** Owner's Handbook, section III, pages 17 to 23, at <https://www.coyoteflight.com/resources/Aircraft_Manuals/Piper_PA-28-180E.pdf> |
| Source, the other three | The **de Havilland Mosquito FB 6**'s Pilot's Notes, A.P. 2019E-P.N., January 1950, 2nd edition - the same Notes recorded above for its figures - read for its own "PILOT'S CHECK LIST", items 38 to 128 on pages 6 to 10, and Part II paragraphs 37 to 44 for the handling that goes with them, as scanned at <https://www.zenoswarbirdvideos.com/Images/Mosquito/MosquitoFB6Manual.pdf>; and the **McDonnell Douglas F-15C**'s flight manual, T.O. 1F-15A-1 for the F-15A/B/C/D from block 7 up, section II, Normal Procedures, pages 2-7 to 2-12, as scanned at <https://archive.org/details/f-15-manual>; and the **Piper J-3 Cub**'s Owner's Manual for the J3C-65 - again the manual already recorded above for its figures - read for its "Flying Hints", pages 37 to 42, sections A to H, at <https://stpeteair.org/wp-content/uploads/cub_owners_manual.pdf> |
| In the repository | `assets/aircraft/c172p.checklist`, `c182.checklist`, `pa28.checklist`, `mosquito-fb6.checklist`, `f15c.checklist`, `j3cub.checklist`: the items and their figures, in this project's own words and in the handbook's order. None of the handbooks' text, drawings or scans is here |
| Conversions | The PA-28's and the J-3 Cub's handbooks are in miles an hour and their files say so item by item; the Cessnas' speeds are indicated and the simulation reads calibrated, so each band is opened a knot at each end |
| Licence | The words are this project's, GPL-3.0-or-later. Only facts are taken from the handbooks - what to do, in what order, at what figure |

#### The ten that are not

| | |
| --- | --- |
| Source | **None.** They are written for this project, in its own words, following the ordinary practice for each type in the order it is flown. No manual was read for them |
| Where the numbers come from | This project's own published-figure files, `assets/figures/<id>.xml`, whose sources are recorded above; and the flight models themselves wherever the model is what decides - a lever's gate, a flap's travel, an engine's idle |

**Why those ten, one by one**, so that the list accounts for every
aeroplane in it rather than for most of them:

- **no flight manual is public for the B-2A, the F-22A or the F-35B** at all;
- the **737-300**'s, **747-400**'s, **787-8**'s, **A320**'s, **A380**'s and
  **Learjet 35A**'s operating manuals are their operators' and are not
  published;
- the **Short S.23**'s were searched for on 2026-09-22 and none was found:
  the type went out of service in the 1940s and what survives publicly is
  history rather than Imperial Airways' own notes.

**An earlier version of this section named thirteen and explained ten.** The
Mosquito and the F-15C were in the list with no reason given, and both had
manuals that could be had - the Mosquito's were already recorded a few
sections above this one, for its figures. **And the reason given for the J-3
Cub was not a reason**: its manual has no text layer, which stops a text
search and stops nothing else. Its pages were read as pages. All three are now
written from their manuals.

**What none of them is**: a substitute for an aircraft's real checklist, or
airworthy guidance. They exist to teach the simulator's aeroplanes.

## Planned sources

These are named in `REQUIREMENTS.md`. Their entries are filled in when they are
first used.

| Source | For | Phase | Terms known now |
| --- | --- | --- | --- |
| Further aircraft: flight models, from JSBSim's or written here from published data | Flight dynamics | 5 | Recorded per model, as above |
| OpenStreetMap | Buildings | Tail | ODbL; source of the building data not yet chosen |
| FlightGear aircraft | Visual models | 5 | Mostly GPL; checked per model |
| Cesium ion | Optional visual terrain and imagery | 5b | The user's own account and terms |
| Google Photorealistic 3D Tiles | Optional visual scenery | 5b | The user's own key or Cesium ion token, and Google's terms |

Commercial providers are reached only with the user's own credentials. Nothing
here is cached beyond what a provider's terms allow, and whichever provider is
drawing is credited on screen.
