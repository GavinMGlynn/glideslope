# Features

**What this file is.** The menu, at the altitude of "what would the player
notice". It is written so that a feature can be argued about, kept or dropped
without anybody having to read code, and there is no implementation in it on
purpose — no data structures, no formats, no function names.
`REQUIREMENTS.md` is where the design decisions live; `COMPLETION_PLAN.md` is
where a feature turns into work with a verification attached;
`PROJECT_STATUS.md` is where it turns into a claim about what actually runs.

Each entry carries one of:

`CORE` — the simulator is not itself without this ·
`WANTED` — decided in, not yet scheduled ·
`CANDIDATE` — a good idea nobody has committed to ·
`DONE` — built, and ticked in `COMPLETION_PLAN.md` with its verification ·
`OUT` — deliberately rejected, with the reason, so it does not get re-proposed

An entry is `DONE` only when every item of the plan it rests on is ticked. One
still tagged `CORE` or `WANTED` may be partly built; if so, it says what is not
there yet.

---

## The idea

Fly a real aircraft, in real wind, over the real Earth, with friends — and hand
the controls to an AI pilot whenever you like, then take them back.

---

## Flying

- **Real flight dynamics.** `CORE`
  Six degrees of freedom, from a flight model that already flies real aircraft
  to their published numbers. An aircraft climbs, stalls, glides and turns the
  way its handbook says it should.
  *Not yet:* the Cessna 172, the Cessna 182 and the Cub do not climb to their
  handbooks' ceilings, the F-15 does not stall as her manual describes, and
  the F-35 cannot hover, land vertically or take off short.

- **Wind and turbulence.** `DONE`
  The air moves. Crosswinds push you off the centreline and rough air bumps you
  around.

- **Live weather.** `CORE`
  The wind and conditions come from real reports for where you are flying, so
  the weather over an airfield is the weather there today.
  *Not yet:* a flight flies the weather of the airfield it starts from,
  wherever it goes, and on a server each player flies their own copy of it.

- **Wind that shears and gusts.** `DONE`
  The wind changes with height and from moment to moment, as real wind does. It
  slows and swings round near the ground, so an approach is flown down through
  shear, and on a gusty day the gusts the report gives are the gusts you fly
  in. When an airfield reports wind shear, it is there.

- **Hazardous air.** `WANTED`
  Air that rises and sinks where the weather and the ground say it should:
  thermals over sunlit ground, waves and rotor in the lee of a ridge, and the
  microburst on short final that a pilot has to fly out of.
  *Not yet:* thermals rise over the sea as strongly as over a sunlit field,
  and the air behind a ridge has no rotor and no waves trapped beneath a
  stable layer.

- **Weather you can see.** `WANTED`
  Cloud, rain and haze from the same reports, drawn where they are, so you
  break out of cloud at the base the report gives.
  *Not yet:* cloud is a flat sheet, not a volume, and does not drift with the
  wind; a storm cloud is no tower; the sky jumps rather than blends when a new
  report arrives; and rain falls only close by.

- **A choice of aircraft.** `CORE`
  Chosen at the start of a flight, from a Piper Cub to an A380: light aircraft
  (the Cub, a Cessna 172 and 182, a Piper Cherokee), a flying boat that lands on
  the sea and on lakes, a Second World War Mosquito, a Learjet, the airliners
  (A320, A380, 737, 747, 787), the F-15, F-22 and F-35 fighters, and a B-2
  bomber. Each flies to its published numbers and looks like the aircraft it
  flies like.
  *Not yet:* on a server a player flies the aircraft the server gives them,
  not one they choose; the Learjet is not drawn at all; and no aircraft wears
  a livery or moves its control surfaces.

- **Joysticks, HOTAS and yokes.** `DONE`
  Proper flight controls on every platform, not only a keyboard.

- **A head-up display.** `CORE`
  Airspeed, altitude, heading, vertical speed and attitude at a glance.
  *Not yet:* the HUD's horizon line does not lie on the horizon drawn behind
  it.

- **Views of the aircraft.** `DONE`
  Fly from the cockpit, or step outside and watch the aircraft from ahead,
  behind, either side or above, or orbit it freely - switched with a key at
  any moment, as flight simulators let you.

## The world

- **Anywhere on Earth.** `CORE`
  Take off from any airfield and fly anywhere; nothing is fenced into a region.
  *Not yet:* the ground is drawn only around where a flight starts; fly far
  enough and there is sky beneath you.

- **Real terrain and imagery with no account.** `DONE`
  The default scenery works the moment the simulator starts, with no sign-up and
  no key.

- **The ground is the same for everyone.** `CORE`
  In a shared session, every player's aircraft touches down on the same ground,
  wherever each player's scenery comes from.

- **Runways you can roll on.** `CORE`
  Every runway the open airport data places, anywhere in the world, is as
  smooth to take off from and land on as a runway is, following its real
  slope from end to end, however rough the terrain data around it.
  *Not yet:* where two runways meet at different slopes, each still pulls the
  other's surface up to two-thirds of a metre off its line.

- **Richer scenery from Cesium ion.** `DONE`
  Players with their own Cesium ion token can fly over Cesium World Terrain and
  imagery instead.

- **Photorealistic cities from Google.** `DONE`
  Players with their own Google Maps Platform key, or a Cesium ion token, can
  fly over Google's photorealistic 3D scenery.

- **Credit where it is due.** `DONE`
  Whichever scenery is showing, the people who made it are credited on screen.

## Learning to fly

- **Checklists for every aircraft.** `DONE`
  Each aircraft comes with its own checklists - before start, taxi, take-off,
  climb, cruise, descent, approach, landing and after landing - on screen when
  you want them. What the aircraft can tell has been done ticks itself off, and
  what it cannot is yours to confirm.

- **Lessons that teach you to fly.** `CORE`
  Take-off, the circuit, climbs, turns, stalls and landing, taught for each
  kind of aircraft from a light trainer to an airliner: what to do at each
  stage, as you fly it, and afterwards what to do differently. A debrief, not
  a score.
  *Not yet:* the Learjet's take-off lesson cannot catch an early rotation or a
  tail strike.

- **An instructor who shows you first.** `WANTED`
  The AI pilot flies the lesson while you watch, then hands you the controls,
  and takes them back if you ask.
  *Not yet:* in the stall lesson, several aircraft's demonstrated recovery
  loses more height than the lesson allows or pulls harder than 2 g.

## AI pilots

- **Hand over the controls, and take them back.** `CORE`
  Any aircraft, at any moment, in either direction, without a jolt. In the
  windowed client, M chooses what plans your aircraft once you hand it over
  - no model, Claude or ChatGPT - at any time in flight; a model you have no
  key for is refused, said on the spot, and the AI simply holds your
  aircraft's course.
  *Not yet:* an aircraft landed by hand and taken back on its landing roll is
  never brought to a stop, and in the windowed client a hand-over pressed
  during a take-over can go to the wrong aircraft.

- **See who is flying, and what they are doing with the controls.** `DONE`
  The screen always says whether you or the AI pilot has the aircraft, and
  what the AI is doing. A small panel shows where the stick, rudder, throttle,
  flaps and gear are - yours, or the AI's while it flies.

- **Ride along in any AI aircraft, then take it over.** `WANTED`
  Step into the cockpit of any aircraft an AI pilot is flying and watch it
  fly: the view from its seat, its instruments, and its controls moving as
  the AI flies. Take the controls when you like, without a jolt; the aircraft
  you were flying is handed to the AI pilot, so the sky stays as full. Never
  another player's aircraft, and a server may forbid it.
  *Not yet:* once, in testing, a take-over of another player's aircraft was
  not refused, and why is not known; and on a slow machine taking over can
  jolt.

- **An autopilot.** `DONE`
  Hold a heading, an altitude, an airspeed or a climb rate.

- **Flight plans.** `DONE`
  Follow a route of waypoints from start to finish.

- **AI traffic.** `WANTED`
  Other aircraft flown by AI pilots share the sky, and keep flying when every
  player has gone home.
  AI aircraft keep 500 ft or 1.5 nm apart, climbing or descending out of one
  another's way.
  *Not yet:* an aircraft you hand to the AI is given the same room but nobody
  has measured it, nothing stops you flying into an AI aircraft, and on the
  Sydney route the Learjet and the Mosquito come down in Botany Bay.

- **A copilot you talk to.** `WANTED`
  Say "take off, climb to 3,000 ft and orbit the CBD" and the copilot turns it
  into a plan and flies it; later it stays with you, changing the plan and the
  autopilot's settings as the flight goes, and brings the aircraft down to a
  runway. With your own key, which stays on your machine.

- **A different AI on each aircraft.** `WANTED`
  Choose which language model plans for each AI aircraft - Claude on one,
  ChatGPT on another, or none, flying its plan as it was given - and watch
  them decide differently: which way they go, how they handle a failure, what
  they do when told something new. The flying itself is always the
  simulator's own AI pilot; the model only decides where to go. Whoever
  chooses brings the key: a server's own AI aircraft are planned with its
  operator's, and an aircraft you hand over with yours.
  In the windowed client, M chooses the model for your hand-over in flight.

- **Pilots that learned to fly.** `CANDIDATE`
  AI trained to land or fly aerobatics rather than programmed to. A stretch
  goal.
  *Not yet:* only the Cessna 172 has a learnt landing, and only the
  command-line program offers it, not a session.

## Flying together

- **Up to four players.** `CORE`
  Fly in the same sky as three friends, each seeing the others' aircraft move
  smoothly.
  *Not yet:* a fault that once showed a fifth player's aircraft is fixed, but
  not shown to be gone on Windows.

- **The same air for everyone.** `CORE`
  In a shared session everyone flies through the same wind, gusts and shear: a
  gust that lifts one aircraft's wing lifts the aircraft alongside it too.
  *Not yet:* on a server the air does not rise over hills and ridges, as it
  does when you fly alone.

- **Controls that answer immediately.** `CORE`
  Your aircraft responds on the frame you move the stick, even with the server
  on the other side of the world.
  *Not yet:* the windowed client does not smooth the server's corrections, and
  after a long frame, or behind a slow server, it can be put right by metres.

- **Crashes happen, and cost a flight, not the session.** `DONE`
  Aircraft that meet in the air, or come down harder than their undercarriage
  can take, are wrecked - everyone sees it - and a few seconds later fly again
  from where they started.

- **Leaving does not crash the aircraft.** `DONE`
  When a player drops out, their aircraft either leaves the sky or an AI pilot
  takes it over, whichever the session chose.

- **A public server to join.** `WANTED`
  One option to fly with whoever else is online, with no address to type.
  *Not yet:* no public server is running.

- **Run your own server.** `DONE`
  A server anyone can run, in one of two ways, chosen when it starts. In a
  terminal - the default, and what a server in the cloud with no screen runs -
  it shows who is connected, how well and how much they are sending, and what
  is flying, or just a log of what happens for a machine nobody watches. In a
  window, when asked for, it shows the same things with a list of who came and
  went and a button beside each player to drop them; closing the window stops
  the server.

## The platform

- **Linux, Windows and macOS.** `DONE`
  Both families of Linux, 64-bit Windows, and Apple silicon Macs.

- **A download that runs.** `DONE`
  One file per platform that unpacks and runs in place.

- **A Mac download that opens without a warning.** `WANTED`
  Signed and notarised, so macOS does not refuse to open it.

- **One Linux download for every distribution.** `WANTED`
  An AppImage or Flatpak rather than a build per distribution.

---

## Deliberately not

- **Scores, leaderboards and lap times.** `OUT`
  This is a simulator to fly, not a competition. Everything that would follow
  from scoring — replays that prove a result, a server that checks it — is
  weight this project does not need to carry.

- **Rollback and a deterministic simulation.** `OUT`
  The flight model is floating point and runs on different machines. Making it
  bit-identical everywhere would cost more than everything it buys, when nothing
  is being scored.

- **Peer-to-peer play.** `OUT`
  Every player talks to the server and to nobody else, so there is no need to
  punch through home routers or relay traffic.

- **A language model flying the aircraft.** `OUT`
  The copilot plans; the autopilot flies. A model's output never moves a control
  surface directly.

- **Keys or tokens shipped with the simulator.** `OUT`
  Commercial scenery is opt-in, with the player's own key, under that provider's
  terms.

- **OpenGL.** `OUT`
  Deprecated on macOS and stuck at an old version there.

- **32-bit builds.** `OUT`
  Every target platform is 64-bit, and none of the dependencies want otherwise.

- **Deciding who sees which aircraft.** `OUT`
  With at most four players, every player receives every aircraft.

## Open questions

- **Where the free buildings come from.** The default scenery needs a source
  of buildings that needs no account. (The free imagery is settled: a
  cloud-free satellite mosaic of the whole Earth, 10 m a pixel.)
