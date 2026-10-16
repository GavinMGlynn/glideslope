### A runway reported contaminated is slippery: the METAR's runway-state group read onto the runway it names, the list never waited for, and every client told; rain that has stopped still wets, 2026-10-14 — item done

**What is still missing, first.** **The copilot and planner are not told**:
they judge each runway dry or wet (`world::WetRunways`), and a runway
reported icy is judged as the weather alone says - no aircraft has a
published landing distance on snow, slush or ice (a new Phase 10b item).
**How long a runway stays wet is an estimate**, half an hour
(`world::still_wet_after_rain_s`): no study of runways' drying was found.
**A measured friction coefficient (0366's 00 to 90) is read and not used**:
AC 25-32's matrix correlates codes with braking action, not with a
measured coefficient. **Until the runway list has come, a state lies on no
runway**: OurAirports' runways are fetched off the stepping thread and the
frame loop the first time a report with a state is flown (from the cache in
a moment; from the network, the first time ever, seconds; where they
cannot be had, asked for again an hour later, each failure said), and
meanwhile the runway is as the
weather alone says - wet in rain, dry otherwise - on the server and in what
it tells its clients alike. **The reach is small, and said so**: ICAO's
Amendment 80 to Annex 3 kept the group in METAR only until 3 November 2021,
the GRF's start, after which runway conditions go by SNOWTAM. Asked for six
hours of 30 stations on 2026-10-13, the ten Russian ones (UUEE, UUDD, UUWW,
ULLI, UNNT, USSS, UWWW, URSS, UHWW, UIII) sent the group in every report,
and none of the twenty others - Kazakhstan, Belarus, Ukraine, Uzbekistan,
Armenia, Kyrgyzstan, the Nordic and Baltic States, Poland, Czechia, Austria,
Slovakia, Hungary - in any. US METARs never carried it (FMH-1 has no such
group). **A long METAR can lose its group**: `frontend::fit_to_send` cuts a
report past 256 bytes word by word from its end, and the runway-state
groups come after the QNH, so they are the first cut after any trend; the
server flies the report as cut, so both ends agree, but on a dry runway.
**Extent 5 is taken whole**: a deposit over 26 to 50 per cent of a runway
gives the whole runway its code, as AC 25-32 counts more than 25 per cent
as contaminated; where it lies along the runway is not said, so a wheel on
a clear half brakes as on the deposit - the more slippery reading.

**What it does.** `world::parse_metar` reads the state of the runway
(WMO-No. 306 FM 15 regulation 15.13.6): the newer `R24/290195`,
`R24L/CLRD//`, `R/SNOCLO` and `SNOCLO`, and the older eight figures
`24290195`, in which 50 added to the number is the right parallel (74 is
24R) and 88 is all runways, 99 the last report repeated; and recent weather
(`RERA`, `RESHSN`; regulation 15.13.2). A group shaped as a state but not one
- a character no table has, a reserved code (extent 0, 3, 4, 6-8; depth 91;
friction 96-98), a runway not 01-36 (or 51-86 in the eight figures), 88 or
99 with a letter, half a depth, CLRD with neither // nor figures - is put in
`Metar::runway_states_refused` with why, and the rest of the report read.

`world::runway_condition_of(state, temperature)` gives the FAA code
(AC 25-32 table 2, the RCAM):

| Deposit (0919) | 3 mm or less, or depth // | deeper | depth 99 (closed) |
| --- | --- | --- | --- |
| 0 clear and dry | 6 | 6 | 6 |
| 1 damp, 3 rime or frost | 5 | 5 | 5 |
| 2 wet, water patches | 5 | 2 | 1 |
| 4 dry snow, 5 wet snow | 5 | 3 | 1 |
| 6 slush | 5 | 2 | 1 |
| 7 ice | 1 | 1 | 1 |
| 8 compacted snow, 9 frozen ruts (estimate: as compacted) | 4 at -15 C and colder, 3 warmer or no temperature | same | same |
| / not reported | none | none | none |

Extent 1 or 2 (25 per cent or less covered) is 6, as AC 25-32 6.1 calls it
dry; 5, 9 or / the deposit's. A braking action (0366's 91 poor to 95 good)
is the matrix's column - 1 to 5 - and is taken where more slippery than the
deposit's, or alone. CLRD is 6; SNOCLO 1 everywhere (code 0, nil, is not
flown). Depth 99, the runway not operational with its depth not said, is 1.

**On the runway it names.** `WeatherReport::contaminated` holds each
runway end a state names (`world::contaminated_runways`: `world::names`,
either end - 24 is 06's runway, 24R 06L's, a number alone its left parallel
too - at most 16) with its code. `world::with_air_motion`, within 8 km of
the station, gives an aircraft the code where it is on one
(`world::on_runway`: along the end's length and within 30 m of its
centreline, half Annex 14's widest, 60 m) and that is more slippery than
the weather's.

**Never waited for** (from the review of #187: the list was read on the
stepping thread, and a source held for its 30 s of retries put the server
3,598 steps behind). `frontend::StationRunways` fetches the list with
`std::async` the first time `lay` is asked with a report that has a state,
and `lay` only looks whether it has come: the server asks before every step
(`Fleet::lay_runways_when_had`), the single-player client every tick
(`Flight::lay_runways_when_had`), and once it has, it lays both the report
flown and the one it blends from (`ReportedWeather::lay_runways`) and the
server says so ("runways laid at ..."). Let go while fetching, it gives the
fetch up (`world::FetchesGivenUp`) and waits only for that. **A failed fetch
is tried again an hour later** (`StationRunways::default_retry_after`), not
never - a server runs for days - and each failure is said. **An end no
client would read is never laid** (`world::contaminated_runways`): off the
Earth, heading outside 0 to 360, of no length or over 20,000 m
(`world::longest_runway_laid_m`, the reader's `net::most_runway_m`), or not
a number - so the server never sends what every client refuses.

**The clients are told, not left to fetch** (`RUNWAY_CONDITIONS`, kind
`0C`, protocol version `0B`; `docs/TRANSPORT.md`). After every `WEATHER`,
and again when the server's runways are laid, the server sends the runways
the weather now and the one it blends from lay a condition on, and until
when the runways near the station stay wet (`ReportedWeather::wet_until_s`),
for the weather that took over when it says. A client lays them on its copy
of the server's air (`HeardAir::conditions_heard`, from both clients) and
drops them if they are for another weather. So a client never fetches the
runway list - one that could not is no different - and a client joining
after the rain, told only the dry report, flies the wet runway the server
still flies. At its limits the message is 1,076 bytes.

**Still wet after the rain.** Recent rain, drizzle, snow or other
precipitation wets the runway as falling rain does, and
`world::ReportedWeather` keeps the runways near the station wet for
`still_wet_after_rain_s` (1,800 s) after a report that wetted them gives way
- from the blend's halfway, where the runway turns. **Sources**: none found
for the drying time; it is stated as an estimate.

**Sources pinned** in `docs/ASSETS.md`, "The runway's condition":
WMO-No. 306 Volume I.1, 2011 edition updated 2013 (SHA-256
2ed7574aa9014eb702e477317f70adad43cd2152582942579524d482ee736eb9) and its 1995
edition for the eight figures (ba60b39a6f9f9ca77e6f71eb5f11a674d47f9e50936d3c32f695e411f2a393e3);
ICAO State letter AN 10/1.1 ... 20/73 of 30 July 2020, Amendment 80 to
Annex 3, which keeps the group in METAR only until 3 November 2021, the GRF's
start (00be218b222aef9774657a5324237912099aa6874cb1721b2e5e8c1f604cb63f);
AC 25-32 as before; the 50-added convention from a decoding guide, the
European ANP FASID itself not found.

**Verification.**
- `every_runway_state_deposit_and_depth_maps_to_its_runway_condition_code`:
  all 11 deposits at all 100 depths (1,100 groups), each read back and given
  the table's code; compacted snow and frozen ruts at -15 and -14 C; every
  extent with every deposit (55); every braking action on every deposit (55);
  CLRD and SNOCLO in both forms. A measured coefficient is named and walked
  once. **Seen red**: deep slush given 3 - "R24/2904// is code 2, not 3".
- `a_runway_states_eight_figures_and_its_newer_group_name_the_same_runways`:
  24, 74 (24R), 88, 99, R24L/, R88/; a runway visual range not a state; and
  which ends a state names.
- `a_malformed_runway_state_group_is_refused_with_its_reason`: ten ways,
  each refused with its reason and the QNH after it read. **Seen red**:
  the extent's reserved codes let through - "R24/230195 is no state".
- `a_runway_is_still_wet_after_the_rain_stops`: RERA, REDZ, RESHSN, RETSRA,
  REFZRA wet, REFG not; wet 1,799 s after a dry report, dry at 1,801 s, dry
  8.1 km off. **Seen red**: no time kept - "wet 29 min 59 s after the rain
  stopped"; and `a_runway_turning_wet_or_dry_in_a_blend_turns_halfway_through_it`
  (now turning back to dry 1,800 s after halfway) red with it.
- `a_metar_reporting_slush_or_ice_on_a_runway_makes_it_slippery_to_an_aircraft_braking_on_it`:
  a C172P run to 40 kt on runway 24 and braked: dry code 6, 77.8 m; slush
  20 mm (R24/6920//) code 2, 291.6 m; ice (R24/7901//, and 24790195) code 1,
  550.0 m; ice on 15 and slush over a quarter of 24 (R24/6220//) code 6,
  77.8 m; each within 4.4 m of the centreline. **Seen red**: the state never
  laid on the runway - "the codes she stopped on: ...".
- `a_server_keeps_stepping_at_its_rate_while_the_runways_a_reported_state_lies_on_are_slow_to_come`
  (cmake/server_runways_slow.cmake): a server whose list comes from where
  nothing listens (30 s of retries) flies ice reported on 16R from 0.5 s, on
  the stepping thread, for 3 s: it ended 0 steps behind, after 4 s. **Seen red
  on the code before**: "the server ended 3598 steps behind".
- `the_runways_a_reported_state_lies_on_are_laid_when_had_and_never_waited_for`:
  a source held by the test is asked 1,000 times and lays nothing, the
  runway dry; let go, laid once and icy; a failing source says why once and
  is asked once. **Seen red**: a failed fetch begun again - "asked 2 times".
- `a_client_joining_after_the_rain_flies_the_wet_runway_the_server_does`:
  rain then from 600 s a dry report; a client joining at 1,200 s through the
  wire's bytes has the server's code 5 at 1,200 and 2,399 s and 6 at 2,401 s;
  told the weather alone it was dry. A client with no runways of its own is
  told the server's ice on 24 and flies code 1 on it, 6 off it. **Seen red**:
  the wet-until left out of the message - "the server's runway is 5 and the
  joining client's 6".
- `real_metars_carrying_a_runways_state_are_read_and_each_given_its_code`:
  ULLI, USSS, UUEE and UUWW at 15:00 UTC on 2026-10-13
  (aviationweather.gov, recorded and pinned in docs/ASSETS.md): six groups,
  none refused - USSS's `R26L/CLRD60` code 6, the rest wet, code 5. **Seen
  red**: CLRD with a friction's figures refused - "USSS: nothing refused".
- The message's tests walk the new kind: read back, refused as every other
  kind (144 pairs), truncated, trailing, every byte changed, filled to its
  limits (1,076 bytes), through 8,192 loss patterns, and its six
  floating-point fields each refusing NaN, infinity and its range (38 fields,
  228 refusals, 171 readings, 19 out of range); `docs/THREATS.md` names it.
- `a_runway_end_no_client_would_read_is_never_laid`: of a 3,000 m end and
  five no reader takes (25,000 m, no length, heading 361, latitude 91, a NaN
  heading), only the 3,000 m one is laid. **Seen red**: the length's top
  removed - "only the 3,000 m runway is laid, of 6: 2".
- `runways_that_could_not_be_had_are_asked_for_again_after_the_wait_and_each_failure_said`:
  a source failing three times, waited out at once, is tried four times,
  each failure said as itself, and laid at the fourth. **Seen red**: a
  failure never tried again - the test never finished (killed at 60 s).
- `every_refusal_the_document_names_for_runway_conditions_is_refused`:
  sixteen cases refused (codes 0 and 6, each side of the poles and 180
  degrees, headings -0.5 and 360.5, lengths 0 and 20,000.5, a moment or
  wet-until below nought, a wet flag of 2, a time with the flag 00, 17
  runways now or before, a trailing byte) and three edges read. **Seen
  red**: the code's top removed - "code 6 is refused"; the heading's top
  removed - `every_floating_point_field_of_every_message_refuses_a_nan_and_an_infinity`
  "runway_conditions with the largest finite double at byte 35 is outside
  its range and must be refused".
- 81 tests matching weather, METAR, wet, the runway state, same air, the
  station, messages, the protocol version and the documents - the server
  and client weather sessions and the client written from the document
  among them - pass in linux-debug. **The selftest hash does not move**:
  `6e988ba216e5ee07` in linux-debug and `182dd6c996e0ee4c` in linux-release,
  each the baseline recorded for that build type before this branch.
