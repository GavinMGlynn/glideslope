### Google's tiles, through the door that had never been opened, 2026-09-22

**What was missing was a key, and nothing else.** The direct Google Maps
Platform route had been written on 2026-09-21 and never run, because no such
key existed on either machine. The project owner supplied one; it went to
`~/.config/glideslope/google-maps-key`, mode 600, which is where
`platform::google_maps_key()` already looked. Nothing needed writing.

**Mount Taranaki, 241 tiles, through the Maps Platform key alone** - drawn
with a configuration directory holding that key and no ion token at all, so
there was no route for it to fall back to. Google's attribution is on screen
with it: GOOGLE, AIRBUS, DATA SIO/NOAA/U.S. NAVY/NGA/GEBCO, VEXCEL IMAGING,
MAXAR TECHNOLOGIES, LANDSAT/COPERNICUS.

**The test was not proving what the item asked.** "Through both ways in" was
being answered by a run that used whichever secret the machine had - and the
code prefers the Google key when both are present, so on a machine with both,
the ion route would never be taken and would go untested for ever. Each way in
now gets a configuration directory of its own holding one secret, and the two
are counted: two of two drew, nought not testable here.

**A bug in that test, found by reading its own output.** The first way in
points `XDG_CONFIG_HOME` at its isolated directory; the second then asked the
environment where this machine keeps its secrets and was told *that*
directory, found nothing, and reported itself untestable - passing while
testing one route twice. It reads the real location once, before the loop,
now. The give-away was the line "no google-maps-key on this machine" printed
on a machine where the key had just been written by hand.

**Watched failing.** With the direct-key branch disabled, the ion route still
drew its 241 tiles and the other was told it "drew nothing" - the half that
had never run before today.

**A note the owner should act on.** The key was pasted into the conversation
rather than placed in a file, so it exists in that transcript. It is not in
the repository and never will be, but restricting it to the Map Tiles API or
rotating it would be prudent.
