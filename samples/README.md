# Captured evidence

`qwen-{7,42,99}.txt` are unedited real-model speech transcripts from three independent 144-step (72-hour) simulations. They include unsupported model claims; they are evidence to critique, not approved character dialogue. Each starts at tick 12, 06:00; `hour = (tick % 48) / 2`, day `tick / 48 + 1`.

`qwen-*.events.jsonl` preserve original event IDs and are EXCERPTS: run, approach, decline, ignore, speech, belief, dialogue_error, dialogue_budget and recall events only. Full per-tick state and social-effect records remain in the generated local `runs/` outputs and SQLite databases. They are excluded from Git to keep the review manageable. `metrics.json` contains all nine runs' aggregate metrics, including separately named mock summer/winter experiments. Zero exact repetitions among accepted speech does not mean the model never repeated: exact recent repetitions were rejected and counted as dialogue errors.

`manifest.json` identifies Qwen, llama.cpp, generation settings and current source/configuration hashes. It does not certify cross-hardware real-model determinism or a review by another model. The mock provider is a labelled mechanics fixture and has no evidential value about believable speech.
