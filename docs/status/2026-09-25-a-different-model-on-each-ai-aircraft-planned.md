### A different model on each AI aircraft: planned, 2026-09-25

**Asked for by the project owner**: Claude planning for some AI aircraft and
ChatGPT for others. One item is added to Phase 8, and a feature to
`FEATURES.md`. It fits the rule that the LLM plans and the controllers fly.
A model only ever produces a flight plan and autopilot modes, so changing
the model changes the decisions, never the flying.

**What each provider needs:**
- its own key, read at run time and never stored;
- tests that replay recorded answers, so CI calls no provider;
- calls off the simulation's thread.
