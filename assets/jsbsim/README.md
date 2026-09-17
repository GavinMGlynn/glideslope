# assets/jsbsim

The flight-model files glideslope's programs read at run time, copied into
`data/jsbsim/` beside them. JSBSim's layout: `aircraft/`, `engine/`.

**Do not edit these by hand.** They are made by `tools/make_c172p.py` from the
files pinned in `ext/jsbsim`, with a documented list of changes, and a test
fails if what is committed here differs from what the script makes. Change the
script and run it:

```sh
python3 tools/make_c172p.py
```

Where the originals come from and under what licence is in `docs/ASSETS.md`.
