### An empty answer is tried again, 2026-09-19

CI's live weather check failed on Rocky Linux (run 35421371495) when one of
the weather services, after two minutes of trying, answered 200 with nothing
in it, and the empty answer reached the JSON reader as "not a value". Nothing
glideslope fetches is ever empty - aviationweather.gov says it has no report
with a 204 - so a 200 with nothing in it is now tried again, as a server's
error is, and a test holds it so.
