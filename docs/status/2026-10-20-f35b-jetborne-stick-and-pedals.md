### Jet-borne, the F-35B's stick and pedals turn her, 2026-10-20

**What is still not true first.** The shapes of both laws are this
model's choices, not the F-35B's published ones. Nothing public gives her
unified control laws' jet-borne lateral-directional figures; Lockheed's and
BAE's descriptions (Flight International on the VAAC Harrier's "unified"
laws, which hers derive from) say that on the wing a centred stick holds
the bank she has - a roll-rate command with attitude hold - where this law
asks a bank in proportion to the stick and levels her when it is let go.
Her bank at full stick, 20 degrees, is a choice under the 30 the
verification allows; it overshoots to 23.6. The pedals' turn is flat, wings
level: at 60 kt she skids, and the speed law along her new heading takes the
skid up.

**What changed** (src/sim/transition.cpp, `fly_jet_borne`). Flown by her
pilot in STOVL mode, jet-borne or slowing to a hover, never converting back:

- the stick sideways asks a bank, in proportion, to 20 degrees, on the
  ailerons and the roll posts; the heading her law holds turns with the bank
  she has, g tan(bank) over her speed along it (no slower than her
  translational rate command's 30 kt, so the turn stays bounded as she
  slows) - a coordinated turn;
- the pedals turn the heading she holds, in proportion, to her hover's
  10 degrees a second (`fcs/stovl-turn-degps`) at 30 kt, falling with the
  square of her speed above it: 2.5 degrees a second at 60 kt. In
  proportion to the speed alone it was 5 at 60 kt, and the skid it left took
  her to 83 kt along her new heading before the next case;
- the nozzle's swivel is told the turn asked, as the hover law's is, so
  that it does not damp the turn it is asked for.

Before, her law held her heading and wings level whatever the stick and
pedals did. The hover's use of them is unchanged.

**Verification** (linux-release):
`jet_borne_the_f35bs_pilots_stick_banks_and_turns_her_and_her_pedals_turn_her_heading`
hovers her at 300 ft, takes her to 60 kt on the lever and flies every
direction - four, asserted - each begun at 60 kt within 5 and wings level:

| case | begun at | result |
|---|---|---|
| stick full right, 20 s | 60.9 kt | 6.61 deg/s over the last 10 s, bank at most 23.6 |
| stick full left, 20 s | 60.9 kt | -6.60 deg/s, bank at most 23.6 |
| pedal full right, 10 s | 60.9 kt | heading moved 18.58 deg |
| pedal full left, 10 s | 64.5 kt | heading moved -17.61 deg |

Her height within 2.34 ft of what was asked throughout (15 allowed), her
wheels never touching. Seen red with `transition.cpp` put back: the stick
right turned her -0.0015 degrees a second.

The F-35B's other STOVL, hover and prediction tests pass, as does the
selftest; its hash does not move, the selftest flying the C172P, which has
no lift system.
