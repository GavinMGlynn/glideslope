### Aircraft as data, 2026-09-19 — item done (CI run 35387301607)

**What is missing first:** the catalogue holds one aircraft, the Cessna 172P;
the rest of Phase 5 adds the others, each held to its published figures
first. Nothing chooses an aircraft yet - the client flies the catalogue's
Cessna - and the catalogue says nothing of how an aircraft looks, sounds or
is started on the ground.

**The catalogue** (`assets/aircraft`, read by `sim::read_catalogue`) is a file
to an aircraft: its name, its JSBSim model, and the airspeed and throttle a
flight begun in the air starts with. Its published figures and its selftest,
if it has them, are found by its model's name. The client looks its aircraft
up there rather than naming the Cessna in code, and starts it at the
catalogue's airspeed and throttle; `glideslope_cli aircraft` lists what the
data holds.

**The tests:** a copy of the data with one `.aircraft` file more - another
aircraft on the Cessna's model, started at 80 KCAS - holds one aircraft more,
which loads and, on the autopilot, holds its start; every aircraft the data
holds does the same at 3,000 ft for a minute; and a file with a command it
does not know, a throttle past full, no start, or a model the data does not
hold is refused, by its line. **Watched to fail:** the catalogue finding any
aircraft for any name, and reading no files.

**Found on the way: a server hanging up could end the client.** The package
job's client, flying under gdb, stopped on SIGPIPE in a terrain download
(package run 35387301586). libcurl's requests are made with CURLOPT_NOSIGNAL,
as threads must, and then ignoring SIGPIPE - raised when TLS writes to a
connection the server has closed - is the program's job; nothing did, and
by default it ends the process. The platform's HTTP now ignores it when it
first loads libcurl, a test checks it is ignored once a download is made
(watched to fail), and gdb in the package job passes it through.
