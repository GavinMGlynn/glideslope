### AI 1 holds its 4,000 ft layer round the CBD: its height counted from when it gets there, 2026-10-06 — fix

`each_ai_aircraft_is_planned_by_the_model_its_server_gives_it_and_one_with_no_key_is_refused`
was red on main with "AI 1 round CBD at 3839 to 4000 ft". **The cause is the
separation work (#93) alone**, not the engines (#100): the server built at
5c9c85b, before #100, gives 3844 to 4000 ft, and #93's own last CI run failed
the same way. The layers put AI 1 at 4,000 ft instead of 3,000; at the
autopilot's 700 ft a minute it reaches the CBD 600 ft short (3,441 ft as it
starts circling) and climbs on in the orbit, and the server counted heights
from the quarter turn, when it was at about 3,840 ft. Once there it holds it:
level (within 10 ft) from 0.52 turns, then 3,990 to 4,000 ft. #100's
full-rich-below-3,000-ft moved the figure 5 ft (3,844 with the 172P leaned
from the ground, 3,839 without); the leaner leans and holds above 3,000 ft.

**The fix**: the server's orbit summary counts the height round an orbit from
the turn the aircraft first comes within 10 ft of it, and says that turn
("level from 0.52 turns at 3990 to 4000 ft"), or "never level at its height".
The test keeps 50 ft from then on, and adds that each is level within its
first turn (a turn of AI 1's 1,447 m circle is over three minutes, twice the
climb) and fails on "never level". Verified in linux-release: green (AI 1 level
from 0.52 turns, 3,990 to 4,000 ft; AI 2 from 0.25, 2,995 to 3,000); counting
from the quarter turn again (a 10-ft band made 10^9) red with "3839 to 4000";
a band of 0 red with "never reached its height"; a first-turn limit of 0.4 red
with "not level until 0.52 turns". The kept-apart, separation and leaner tests
(35) pass.
