# Irish Village prototype instructions

Read docs/DESIGN.md before changing behaviour. The project owner and ChatGPT define social intent; code must implement that intent without adding drama or assuming other villagers' reactions. This prototype is C++20, text only, exactly Dennis Dineen, Gerrard and Wayne. The owner's construction brief v0.1 supersedes earlier engine/graphics plans.

Use CMake, SQLite, nlohmann/json, libcurl and doctest. Do not modify llama.cpp, install unnecessary frameworks, add graphics, invent historical claims or replace the engine with a model-driven world. Keep prompts under prompts/ and profiles under config/.

Never send another villager's private state or the objective ledger to a dialogue provider. Test plausible knowledge transmission, rejected output, busy refusals, silent encounters, reproducibility and persistent state. Mock speech is a fixture, not evidence of emergent personalities. Distinguish real-model runs and record actual endpoint/model/configuration and failures. Do not claim another model reviewed code unless it did.

Run cmake --build build and ctest --test-dir build --output-on-failure. Update README.md, docs/DESIGN.md, docs/DECISIONS.md, docs/TEST_PLAN.md and CHANGELOG.md when applicable. Keep generated databases, model files, credentials and build directories out of Git. Inspect staged paths. Prepare a review PR; do not merge without owner approval.
