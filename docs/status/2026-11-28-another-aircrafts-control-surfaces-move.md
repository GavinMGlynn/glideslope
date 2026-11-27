### Another aircraft's control surfaces move, 2026-11-28

Phase 12c, "Another aircraft on a server is drawn with its control surfaces
still".

**What is still not true, first.**
- **The B-2's rudder is still not drawn**, on a server or off it: the
  reason is the livery item's (2026-11-27).
- **A surface is drawn to a half degree**, which is what the wire carries;
  between two updates it is moved in a straight line, as the attitude is.
- **The velocity in a state update is coarser than it was**: sixteenths of
  a metre a second, where it was a float (below, and why).

## What changed

- **Protocol version 21 (`15`).** Each aircraft in a state update carries
  four more bytes: its left aileron, right aileron, elevator and rudder,
  each the JSBSim property's value (`fcs/left-aileron-pos-rad`,
  `fcs/right-aileron-pos-rad`, `fcs/elevator-pos-rad`,
  `fcs/rudder-pos-rad`) as a signed byte of half degrees, held at the ends.
  Those four are every property the surfaces files name; a model without
  one is sent nought for it (the C182 writes no right aileron, and her file
  draws it from the left).
- **Why the velocity moved too.** Twenty aircraft with four bytes more did
  not fit a datagram: 1,269 bytes sealed, of 1,232. An aircraft's velocity
  in a state update is now three `i16`s of sixteenths of a metre a second
  (at most 3 cm/s off, ±2,048 m/s), not three `f32`s, which gives back six
  bytes an aircraft: a full packet is 1,119 bytes, 1,149 sealed. Other
  aircraft's velocity is only used to carry them on between updates and to
  show a ridden aircraft's ground speed; a client's own motion, which its
  prediction is put right by, is untouched. The alternatives looked at - the
  angles as `i16`s, or packing the surfaces into six bits each - either
  moved more of the wire or drew to a degree.
- docs/TRANSPORT.md says all of it byte for byte; the client written from
  the document (tests/doc_client) reads the new layout.
- **The server** writes them (net::surfaces_on_the_wire); **the window
  client** moves them between updates as it moves the attitude
  (net::Interpolated, `RemoteState::surfaces_rad`), and draws each surface of
  another aircraft's model by the carried deflection its property is
  (client::carried_angles). A surfaces file naming a property a state update
  does not carry is refused as the model is read.
- Test flags for the window client: `--ride-along-person` rides along in
  the first aircraft a person flies; `--shot-ridden PREFIX` draws the shot's
  frame three times more with no HUD - the aircraft ridden in as the updates
  have it, with its surfaces still, and not drawn.

## Verification

RESULTS
