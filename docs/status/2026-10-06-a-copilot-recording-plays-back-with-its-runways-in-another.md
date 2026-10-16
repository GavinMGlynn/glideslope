### A copilot recording plays back with its runways in another order, 2026-10-06 — tail done

**Runways nearby are listed nearest first**, and two at nearly the same
distance swap places in a flight flown a little differently: the coast
recording broke so on 2026-10-02 and had to be asked again (see below,
"For whoever restarts engines"). Their order is
their distances', so playing back but its numbers (`copilot::Match::but_numbers`)
now disregards it too: each run of consecutive lines naming a runway
(`runway` the line's first word, as the planner gives one, or its second,
after the airport, as the copilot does) is sorted on both sides before they
are compared (`runway_order_disregarded`, `copilot/provider.cpp`). Nothing
else is reordered - a route's waypoints must come in the recording's order.
No recording changes: sorted the same on both sides, every one plays as it
did.

`a_recording_played_back_but_its_numbers_answers_the_same_runways_listed_in_another_order`
records a copilot's situation, as a JSON body, with YSSY 34L, YSSY 16R and
YSBK 29 nearby; played back with 34L and 16R swapped it is answered, and
three others are refused: the swap played back exactly, another runway in
one's place, and the route's two waypoints in the other order. Red with the
old matcher ("exchange 1 was recorded for another request").
