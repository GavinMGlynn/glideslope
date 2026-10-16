### Reliable delivery over an unreliable channel, 2026-09-21 — item done 2026-09-22, above

Phase 6's second item. **The layer is built and its verification is met; the
messages it is for do not exist yet.**

**How it works**, in four sentences. Every reliable message is numbered, from
one, and carries its number. The sender keeps a message until it has been
acknowledged and sends it again if it has not been, no faster than once every
quarter of a second. The receiver hands messages up in number order, holding
one that arrives early until its predecessors have come and throwing away one
that arrives twice. The receiver answers with the highest number below which
nothing is missing, which acknowledges that one and every one before it at
once.

**An endpoint with nothing to say still answers.** Without that the far end
retransmits for ever at a receiver that already has everything - so an
endpoint that owes an acknowledgement and has nothing of its own to send
sends a header with message number 0, which is an acknowledgement and nothing
else.

**It is bounded at both ends.** A sender that gets 256 messages behind
refuses rather than queueing for ever, and a receiver stuck behind one
missing message holds 256 and no more. Neither is a window in the congestion
sense: these messages are few, small and occasional, and the channel below is
a game's, not a file transfer's.

**Nothing here touches a socket**, which is what lets it be tested against
every pattern of loss rather than against a network that happens to be
working.

**Verification run.** Seven tests. The item's own -
`under_every_pattern_of_loss_every_message_arrives_exactly_once_and_in_order`
- walks **all 4,096 patterns of loss over twelve datagrams in both
directions**, and in every one of them all six messages arrived exactly once
and in order. It states how many patterns there are, how many actually lost
something (4,064: an exchange of six messages is over in seven datagrams, so
the 32 patterns setting only bits 7 to 11 never touch anything), and what the
worst pattern cost in datagrams, so that a change making delivery far more
expensive shows up here rather than nowhere. Watched to fail with
retransmission taken out: "with loss pattern 1 of 4096, 0 of 6 messages
arrived and it never finished". 319 of 319 tests pass locally at
`-j4`, in 1006 s.

The other six hold the ordinary cases: nothing lost; a message delivered
twice handed up once; three messages arriving before the one they follow,
held and then handed up together; an endpoint answering with nothing to say
and then going quiet; four kinds of rubbish off the wire handed up as
nothing; and a sender refusing at 256.
