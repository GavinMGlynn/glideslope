### Jet-borne, the F-35B's stick and pedals turn her, 2026-10-21

**What is still not true first.** Only the stick law's kind is sourced, not
its figures, and the pedal law is wholly this model's choice. Nothing public
gives the figures of the F-35B's jet-borne lateral-directional control laws.

- **The stick, sourced in kind.** Her STOVL laws descend from the "unified"
  laws flown on the VAAC Harrier. John Farley's account of flying them, in
  Flight International ("Single-minded", flightglobal.com/single-minded/27936.article),
  says that flying on the wing, "if the pilot centres both the stick and
  throttle, the aircraft holds the existing speed, bank attitude and climb or
  dive angle". This law follows that, as the owner chose (2026-10-21): the
  stick asks a roll rate, and centred it holds the bank it left.
- **The stick's figures, chosen.** Its roll rate, 10 degrees a second at the
  stop, and its most bank, 20 degrees (under the 30 the verification allows),
  are this law's choices. Her bank overshoots to 22.8 on the way in.
- **The pedals, chosen.** The pedals' turn is flat, wings level. At 60 kt she
  skids, and the speed law along her new heading takes the skid up.

**What changed** (src/sim/transition.cpp, `fly_jet_borne`). These apply when
her pilot flies her in STOVL mode, jet-borne or slowing to a hover, never
while converting back:

- the stick sideways asks a roll rate, in proportion, to 10 degrees a second.
  The bank it moves is held when the stick is centred, to 20 degrees at most,
  on the ailerons and the roll posts. Whenever her law levels her - no
  pilot, converting back, or her selector out of STOVL mode - the bank held
  is put back to level, so none is held over to when the stick is hers
  again. The heading her law holds turns with the bank she has as in a
  coordinated turn: g tan(bank) over her speed along it, taking her speed as
  no slower than her translational rate command's 30 kt, so the turn stays
  bounded as she slows;
- the pedals turn the heading she holds, in proportion, at her hover's
  10 degrees a second (`fcs/stovl-turn-degps`) at 30 kt, falling with the
  square of her speed above it: 2.5 degrees a second at 60 kt. A turn falling
  only in proportion to her speed was 5 degrees a second at 60 kt, and the
  skid it left took her to 83 kt along her new heading before the next case;
- the nozzle's swivel is told the turn asked, as the hover law's is, so that
  it does not damp the turn it is asked for.

Before, her law held her heading and wings level whatever the stick and
pedals did. The hover's use of them is unchanged. The bank held is the law's
own, like her heading. A prediction is not told it: like the jet-borne
integrals, each side makes its own, so a client joining mid-bank predicts
her level. That is an open Phase 9 item in COMPLETION_PLAN.md, the server
telling her held bank as the likely fix.

**A first version, not kept.** The stick first asked a bank in proportion to
its travel (attitude command), and she levelled when it was let go. The
owner chose the VAAC law instead (2026-10-21).

**Verification** (linux-release):
`jet_borne_the_f35bs_pilots_stick_banks_and_turns_her_and_her_pedals_turn_her_heading`
hovers her at 300 ft, takes her to 60 kt on the lever, and flies every
direction: four, asserted. Each case is begun at 60 kt within 5 and wings
level within 2 degrees:

| case | begun at | result |
|---|---|---|
| stick full right, 20 s | 60.9 kt | 6.62 deg/s over the last 10 s, bank at most 22.7 |
| then centred, 10 s | bank 20.0 | bank held within 0.01 deg (bound 0.5) |
| stick full left, 20 s | 60.8 kt | -6.61 deg/s, bank at most 22.8 |
| then centred, 10 s | bank -20.0 | bank held within 0.01 deg |
| pedal full right, 10 s | 60.8 kt | heading moved 18.43 deg |
| pedal full left, 10 s | 64.5 kt | heading moved -17.78 deg |

After each stick case her pilot rolls her level with the stick against the
bank. Then **no bank is held over**: stick full right 3 s and centred (bank
22.4), the selector out of STOVL mode for 10 s (her law levels her, -0.3),
and back in it with the stick centred for 10 s: bank at most 0.33 (bound 2).
Her height stayed within 1.58 ft of what was asked throughout (15
allowed), and her wheels never touched.

Seen red three times:

- with `transition.cpp` put back to before the item, the stick right turned
  her -0.0015 degrees a second;
- with the first version's level-on-release law, centred from 20 degrees,
  her bank moved 23.6 degrees (bound 0.5);
- without the held bank put back to level when her law levels her, back in
  STOVL mode she rolled to 22.7 degrees, the bank she had been left at.
  Reachable: out of STOVL mode below her conversion speed she stays
  jet-borne, never passing the point the law begins again, and her selector
  back in hands the stick the held bank.

The F-35B's other STOVL, hover and prediction tests pass, and so does the
selftest. Its hash does not move: the selftest flies the C172P, which has no
lift system.
