### A player asks for a loading as she joins: a player's F-35B converted, hovered and landed vertically through a server, 2026-10-16 — item done

**What is still not true first.** **The windowed client's `--loading` is
compiled, not run**: it asks for the loading with `--aircraft`, flies alone
at it, loads a server's aircraft as its `AIRCRAFT` says, and makes her
flight again on a take-over of the same aeroplane loaded otherwise - none of
it run by a test here; only `glideslope_cli` flies a loading through a
server. **Out of the hover again is not flown through a server**: the test
converts, hovers and lands her; a pilot's flight back to wing-borne is the
in-process `a_pilots_stovl_selector_and_stick_fly_the_f35b_to_a_hover_onto_the_ground_up_and_back_to_wing_borne`.
**The jet-borne and hover laws' integrals are still each side's own**
(COMPLETION_PLAN's open "drifts from the server's flight-control laws"):
through the hover and the landing they did not move the error (below), but
nothing puts them right. **The server's own words on the loading are not
read by the test**: its standard output is the relay's input
(`--until-input-ends`); what it gave is held by the client's `AIRCRAFT` and
by the server's word having her hover at all.

**The design** (REQUIREMENTS.md, decisions closed 2026-10-16). A loading
is content: the ones her figures file names (`assets/figures/f35b.xml`'s
`vertical_landing`, 37,530 lb, under her 39,837 lb hover limit). The
player asks for one by name with the aeroplane, in the handshake's payload
- the aeroplane is already asked for there (REQUIREMENTS 4.2), and her
loading is part of that pick, where a session file would have chosen it
for her. Protocol version 14 (`0E`):

- **The initiation's payload**: after the id's length and bytes, either
  nothing or a loading's length (1 to 32) and its name, the id's letters;
  anything else makes the payload none. `net::write_asked_aircraft(id,
  loading)`, `net::read_asked_aircraft` now returns `net::AskedAircraft`.
- **`AIRCRAFT`** ends with a third text, her loading's name, empty for her
  model's own: so every client - the one that asked, one that takes her over
  later - predicts her as heavy as the server flies her.
- **The server** reads every model's loadings with the catalogue, once
  (never per initiation, never joined to a path: THREATS.md), gives the one
  asked for where it gives the aeroplane asked for and her figures name it,
  says so (`slot 0 asked for the vertical_landing loading, 37530 lb, and
  flies it`), and otherwise flies her model's own, said so. A wreck flies
  again at her loading, her tanks refilled to it - one at her model's own
  flies again on the fuel she wrecked with, as before. **A server now
  reads every catalogue aeroplane's figures as it starts**, so a figures
  file that does not read stops it even if nobody flies her; the figures
  tests hold every one readable.
- **The clients**: `glideslope_cli connect --aircraft f35b --loading
  vertical_landing`, and the windowed client's `--loading` (refused
  without `--aircraft`, as the command line's is); each loads her
  prediction by name (`sim::named_loading`, her figures file) before it puts
  her anywhere. `glideslope_cli` says what its prediction weighed as it
  began, and an aircraft's loading where it is told one.

**Verification** (linux-release):

- `a_players_f35b_at_her_vertical_landing_loading_converts_hovers_and_lands_vertically_through_a_server_and_the_client_agrees`
  (tests/cmake/server_stovl_vertical_landing.cmake, multi-process,
  RUN_SERIAL): server_stovl_landing.cmake's server, plan, 100 ms relay and
  pilot, the client asking for her `vertical_landing` loading and staying
  until its prediction is down after a hover and the server's word has her
  at rest. Told `aircraft 0 is f35b, loaded vertical_landing`; predicted at
  37,530 lb; selected with input 150, converted at 63.7 s, hovering at
  164.4 s, down at 339.5 s, the server's word hovering and at rest, lowest 6
  ft; 8,469 updates compared, the median 0 mm, the 95th percentile 2 mm, the
  worst 1.357 m (a step of the clocks' difference), none too large to hide
  once joined; her pitch within 0.109 degrees. Held: told the loading, under
  37,530 lb and over 37,000, converted, hovered and down, the server's word
  hovering and at rest, no wreck, at least 3,000 compared, the median under
  60 mm, the 95th percentile under 3.4 m, the worst under 5.6 m, nine in ten
  joined, the pitch within half a degree - server_stovl_landing.cmake's
  bounds and reasons. 343 s. Again through ctest: converted at 69.1 s,
  hovering at 170.4 s, down at 332.2 s; 8,280 compared, the median 0 mm,
  the 95th percentile 2 mm, the worst 0.523 m, her pitch within 0.105
  degrees; 336 s. **Seen red** with the server not loading her (a
  deliberate bug in `give`, the client still told the loading): the
  server's F-35B, at her model's own weight, a wreck - "struck the ground
  with its airframe" - her predicted pitch 14.3 degrees off and a
  correction of 42 km; reverted, green. In the `-march=x86-64-v2` build
  (no fused multiply-adds, as CI): converted at 68.8 s, hovering at 170.1 s,
  down at 331.8 s; 8,269 compared, the median 1 mm, the 95th percentile 3
  mm, the worst 1.415 m, her pitch within 0.115 degrees; 344 s. A client
  that does not load her is caught by the weight it says (45,400 lb against
  37,000 to 37,530); that half was not run red on its own.
- `the_aeroplane_asked_for_reads_back_and_a_malformed_ask_reads_as_none`:
  53 cases, counted - every id with five loadings and none read back,
  four loading names and a loading of no aeroplane not written, and a
  loading's length of 0, too few bytes, a byte trailing, a capital, a path
  and 33 bytes each making the whole payload none.
- `every_loading_a_figures_file_names_is_found_by_its_name_and_weighs_its_total`:
  41 loadings of 16 models, each found by name as the file has it and,
  loaded, weighing its total within half a per cent; the three unnamed
  loadings, an unheard-of name, an empty name and a model with no figures
  file found by none.
- `a_player_who_asks_for_an_aeroplane_when_joining_flies_it_and_every_client_is_told_so`:
  the PA-28 asked for at a loading her figures do not name flies at her
  model's own, said so; an unknown aeroplane asked for at the F-35B's
  loading flies the plan's Cessna, and no server gives a loading.
- The messages test's `AIRCRAFT` round trip and largest message carry a
  loading; the fuzzer has a seed with one (22 seeds); the protocol test
  refuses `0B`, `0C` and `0D` by name; the gearstick refusal is
  `474c44530e0401`; the doc client and the version-with-the-ground test say
  `0E`.
- The whole suite here before the rebase onto #212 to #215: 1,328 tests,
  none failed, 24 skipped (live keys, Windows' file locks, Vulkan), 46
  minutes at -j4. On CI run 38803123702 the new test passed on all seven
  presets (290 to 344 s).
- **Its bounds are server_stovl_landing.cmake's as #214 set them from
  every platform's CI** (#214, the median's 60 mm by #219: CI's unbroken
  medians reached 28 mm on windows-clang, and the pitch check catches a
  wandered law): the median under 60 mm, the 95th percentile under
  3.4 m.
- The STOVL selector held wing-borne at her model's own weight,
  `a_players_stovl_selector_through_a_server_holds_a_heavy_f35b_wing_borne_and_the_client_agrees`,
  still green beside it (124 s).

**The item's whole verification** - she is flown into a hover and out
again, and a pilot and an AI plan each convert her: a pilot through a
server, above; a pilot into a hover, onto the ground, up and back to
wing-borne,
`a_pilots_stovl_selector_and_stick_fly_the_f35b_to_a_hover_onto_the_ground_up_and_back_to_wing_borne`;
an AI plan converting, hovering and landing her on a point,
`an_ai_plan_flies_the_f35b_to_a_point_converts_hovers_and_lands_vertically_on_it`,
and a plan given her hovering converting her back and flown (2026-10-14).
