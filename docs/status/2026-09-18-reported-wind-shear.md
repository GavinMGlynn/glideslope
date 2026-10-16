### Reported wind shear, 2026-09-18 — item done (CI run 35331164089)

**What is missing first:** a report of shear is flown as a model, not as what
was measured: a METAR says only that there is shear on a runway, not how much
or where. The runway's direction is its number, magnetic taken as true - off by
the local variation, 8 degrees at Narita. Peak winds and wind shifts are read
and not flown: a peak wind 50 minutes old is not the air now.

**Read** (`world/metar.hpp`): wind shear on a runway or all of them in every
form a METAR writes it - `WS R02`, `WS RWY27`, `WS TKOF RWY20`, `WS LDG RWY09L`,
`WS ALL RWY` - and from the remarks the peak wind (`PK WND 22033/0832`, its hour
the report's when only minutes are given) and a wind shift (`WSHFT 0743`, with
`FROPA` when a front brought it).

**Flown** (`world::with_air_motion`): within 8 km of the station, a 15 kt
headwind on the named runway's approach - along the surface wind for all
runways - between 60 and 600 m above the ground, all of it from 300 to 450 m
and none by 60 m: the airspeed an aircraft loses on short final flying down
out of it.

**The test:** five recorded reports - Lisbon's `WS R02`, Narita's `WS R34R`
before its `TEMPO`, Jackson Hole's `WSHFT`, the peak winds at Clines Corners and
Albuquerque - decode to what they say, and each other form as written; and on
runway 02's approach at Lisbon the headwind is the surface wind's plus the
model's to 1e-9 at seven points, from 8.5 km out to 1.1 km, 60 to 700 m up.
**Watched to fail:** the shear blowing as a tailwind; `WS` not read.
