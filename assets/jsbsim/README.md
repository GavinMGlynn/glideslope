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
