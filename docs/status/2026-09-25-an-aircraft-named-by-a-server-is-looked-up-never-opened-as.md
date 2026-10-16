### An aircraft named by a server is looked up, never opened as a path, 2026-09-25 — tail done

**Found while bringing `THREATS.md` up to date.** A client loads the model an
`AIRCRAFT` message names, and both clients joined what the server sent to a
path as it stood:
- the client with the window, the mesh in `models/`;
- `glideslope_cli`, the flight model's directory in `jsbsim/`.

A hostile server could have named `../../` anything. Now the id is looked up
among the catalogue's (`sim::known_aircraft`), and the model is the
catalogue's for it, never the wire's. An id that is not in the catalogue is
drawn as nothing and flown as nothing.

**Verified** by `an_aircraft_named_from_the_wire_is_one_in_the_catalogue_or_none`.
All sixteen aircraft's ids are known, each with its own model. Eight strangers
are named by none: `../` climbing out, an absolute path, a wrong case, a
trailing space and a name that is not there. With the lookup made to answer
whatever it is asked, the test goes red.
