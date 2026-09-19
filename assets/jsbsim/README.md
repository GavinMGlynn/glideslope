# assets/jsbsim

The flight-model files glideslope's programs read at run time, copied into
`data/jsbsim/` beside them. JSBSim's layout: `aircraft/`, `engine/`.

Where each comes from, and under what licence, is in `docs/ASSETS.md`.

## The Cessna 172P

`aircraft/c172p/c172p.xml`, `engine/prop_75in2f.xml` and `engine/eng_io320.xml`.

**Do not edit these by hand.** They are made by `tools/make_c172p.py` from the
files pinned in `ext/jsbsim`, with a documented list of changes, and a test
fails if what is committed here differs from what the script makes. Change the
script and run it:

```sh
python3 tools/make_c172p.py
```

## The Cessna 182S

`aircraft/c182/c182.xml`, `engine/prop_81in2v.xml` and `engine/engIO540AB1A5.xml`.

**Do not edit these by hand** either: `tools/make_c182.py` makes them from the
pinned files, with its changes listed, and a test fails if what is committed
differs.

```sh
python3 tools/make_c182.py
```

## The Piper PA-28-180 Cherokee

`aircraft/pa28/pa28.xml`, `engine/prop_pa28_76in.xml` and `engine/engIO360C.xml`.

**Do not edit these by hand** either: `tools/make_pa28.py` makes them from the
pinned files, with its changes listed, and a test fails if what is committed
differs.

```sh
python3 tools/make_pa28.py
```

## The Piper J-3 Cub

`aircraft/j3cub/j3cub.xml`, `aircraft/j3cub/Systems/conventional-controls.xml`,
`engine/engA65-8.xml` and `engine/prop_j3cub_74in.xml`.

**Do not edit these by hand** either: `tools/make_j3cub.py` makes them from
the pinned files, with its changes listed, and a test fails if what is
committed differs.

```sh
python3 tools/make_j3cub.py
```

## The airliners

The Airbus A320 and Boeing 737-300, 747-400 and 787-8: `aircraft/a320/`,
`aircraft/737-300/`, `aircraft/747-400/` and `aircraft/787-8/`, with
`engine/CFM56-5B4.xml`, `engine/CFM56-3B1.xml`, `engine/CF6-80C2B1F.xml`,
`engine/Trent1000.xml` and `engine/direct.xml`.

**Do not edit these by hand** either: `tools/make_a320.py`,
`tools/make_737_300.py`, `tools/make_747_400.py` and `tools/make_787_8.py` make
them from the pinned files, sharing what they change alike in
`tools/airliner.py`, and a test fails if what is committed differs.

```sh
python3 tools/make_a320.py
python3 tools/make_737_300.py
python3 tools/make_747_400.py
python3 tools/make_787_8.py
```

## The fighters

The McDonnell Douglas F-15C Eagle and Lockheed Martin F-22A Raptor:
`aircraft/f15c/` and `aircraft/f22/`, with `engine/F100-PW-220.xml`,
`engine/F119-PW-100.xml` and `engine/direct.xml`.

**Do not edit these by hand** either: `tools/make_f15c.py` and
`tools/make_f22.py` make them from the pinned files, sharing their lift, drag
and engines' thrust with Mach in `tools/fighter.py`, and a test fails if what
is committed differs.

```sh
python3 tools/make_f15c.py
python3 tools/make_f22.py
```

## The Mosquito FB Mk VI

`aircraft/mosquito-fb6/mosquito-fb6.xml`, `engine/merlin25.xml` and
`engine/prop_dh_hydromatic.xml`, written for glideslope from the aircraft's
trials and Pilot's Notes; each file says what its numbers are and where they
come from.

The airframe and the engine are edited by hand. **The propeller is not:** its
tables are computed by `tools/make_mosquito_propeller.py`, and a test fails if
what is committed here differs from what the script makes:

```sh
python3 tools/make_mosquito_propeller.py
```
