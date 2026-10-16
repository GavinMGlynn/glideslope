### The plan-speed refusal test asks the Mosquito past her gear-up top speed, 2026-10-10

Gear up (this PR) the Mosquito's fastest plan speed is 264 kt, not 219, so
`a_plan_file_asking_a_speed_its_aircraft_cannot_hold_clean_is_refused_and_none_in_the_data_does`
asking her 240 kt no longer tested its rule (found by PR #158's run). It now
asks 270 kt and expects "outside 123 to 264 kt": the same rule, past her own
measured top.
