### #194's STOVL selector through a server held to what the clocks' difference allows, 2026-10-15 — a red PR's test bounded as its sibling is

**What is still not true first.** **The clocks' difference is still known
only to a step**: on a busy machine some of the server's words are placed a
step off, or two, and the prediction error on those is that step's travel.
That is a limit of the estimate (the least of the last fifty words', in
sim::Prediction), named in server_paced_prediction.cmake since 2026-10-08,
not fixed here. **A constant misplacement is not caught by this test**:
placed three steps off on every word, the median stayed 17 mm - each replay
starts from the misplaced step, so the client is compared with its own
record of the same shift.
**And the median it holds was not the prediction alone**: a steady part of
it, to 108 mm, was the STOVL law's height asked never told to the client -
found and fixed in the entry above (2026-10-16).

**What was wrong.** #194's multi-process test,
`a_players_stovl_selector_through_a_server_holds_a_heavy_f35b_wing_borne_and_the_client_agrees`,
failed on every CI run for two days on its 95th-percentile prediction
error: macOS 0.890 and 0.901 m, Windows 1.22 and 1.28 m with the worst 2.99
and 3.56 m (runs 38517430777, 38531751565), against a bound of 10 cm set
from runs on an idle machine (1 to 44 mm). It is reproduced here with twice
as many busy loops as cores beside it: the 95th percentile 0.868 m, the
worst 1.505 m, the median 1 mm, and the client's own "the clocks'
difference ... 1 at worst". At her 200 KCAS a step is 0.86 m: one word in
twenty a step off puts the 95th percentile there. Its sibling,
server_paced_prediction.cmake, measures the same error through a server
and bounds it by this limit - a metre for 95 in a hundred at the C172P's
0.42 m a step, three at worst, five centimetres at the median - and runs
serially; this test was ten times stricter and ran beside others.

**The change.** The bounds are put in steps of her travel, as the sibling's
are: the 95th percentile within two steps' travel at 200 KCAS (1.7 m), the
worst within four at the 330 kt she dives to (5.6 m, over CI's 3.56), and
the median under 5 cm, unchanged - the median carries the prediction
itself. And the test runs serially (RUN_SERIAL), as its sibling does. No
code changed.

**Verification.** Passes in linux-release idle and with the busy loops
beside it (that run the 95th percentile 9 mm, the worst 1.375 m - which
runs place a word a step off varies). **Seen red** with the prediction
flown on 0.8 of the stick the client sent the server (a deliberate bug in
glideslope_cli): the median 85 mm, over 5 cm; reverted, green.
