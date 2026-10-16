### The owner's decisions of 2026-10-02 recorded, and `FEATURES.md` tagged from the plan, 2026-10-02 — documents only

Nothing in the code changed, nothing in the plan was ticked or unticked, and
the selftest's hash is untouched. The project owner decided four things,
and each is now written where it belongs:
- **The transport's hash is BLAKE2b, as built.** `REQUIREMENTS.md` named
  `Noise_IK_25519_ChaChaPoly_BLAKE2s`; libsodium has no BLAKE2s, and BLAKE2b
  is its native hash, a hash Noise defines, and what every client and
  server already speaks. Changing it would move the protocol's version for
  no gain in security. `REQUIREMENTS.md` 6.7 now names BLAKE2b, section 9
  moves the question from Open to Closed 2026-10-02, and `TRANSPORT.md` and
  `THREATS.md` say the owner decided it. The header comment of
  `src/net/handshake.hpp` still says the owner has not ruled; it is code,
  and this change made none, so it is left for the next change there.
- **The F-15C's stall is held to the behaviour her manual describes**, since
  it publishes no stall speed: T.O. 1F-15A-1 section VI, "1 G stalls" -
  at full aft stick her angle of attack settles at 45 units or more, at
  100 knots or less. She settles at 42.4 degrees and 116.6 knots (the
  pitching-moment entry below), so what is left is the speed, and a source
  for turning the manual's units into degrees. The plan item "A published
  stall speed for the F-15C" is rewritten as "The F-15C's stall, held to
  her flight manual's", with that behaviour as its verification.
- **An operator's drop lasts for the server's run**, as built and as
  `TRANSPORT.md` describes: a server started again has forgotten whom it
  dropped. `REQUIREMENTS.md` 6.6 says so, and section 9 records it as
  decided; a ban list that outlasts a run could be a later feature. No
  plan item or tail asks for a drop that outlasts the run.
- **`FEATURES.md` is tagged from the plan.** An entry is `DONE` where every
  plan item it rests on is ticked with its verification, and "Nothing is
  `DONE` yet" is gone. Sixteen entries are `DONE`: wind and turbulence;
  wind that shears and gusts; views of the aircraft; real terrain and
  imagery with no account; Cesium ion; Google's photorealistic scenery;
  credit on screen; checklists; who is flying and the controls on screen;
  an autopilot; flight plans; crashes; leaving without a crash; running
  your own server; Linux, Windows and macOS; and a download that runs.
  Twenty-two keep their tag with a "Not yet" saying in a player's words
  what an open tail leaves missing - among them the same air for everyone,
  which is not true on a server until the weather travels. The Mac
  signing and one Linux download, not begun, are left as they were.
