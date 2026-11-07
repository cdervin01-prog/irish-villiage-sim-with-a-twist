# Irish Village Simulation with a Twist

C++20, text-only research prototype: Dennis Dineen, Gerrard and Wayne in a fictional County Cork village around 1850. The engine controls daily activity and individual state. A configurable llama.cpp HTTP server supplies short speech turns; SQLite stores observations, relationships, events and checkpoints. This is an experiment, not evidence that believable autonomy has been achieved.

## Build

Ubuntu/Debian dependencies (or equivalent development packages):

```sh
sudo apt install build-essential cmake libsqlite3-dev libcurl4-openssl-dev nlohmann-json3-dev doctest-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

There are no automatic dependency downloads or model downloads. For packages extracted into a user-owned prefix, set `CMAKE_PREFIX_PATH`, `SQLite3_INCLUDE_DIR`, `SQLite3_LIBRARY`, `CURL_INCLUDE_DIR`, `CURL_LIBRARY_RELEASE` and `DOCTEST_INCLUDE_DIR` explicitly. The session build used this method because sudo requires a password.

## Run

```sh
# Three simulated days; mock speech is visibly labelled and intentionally artificial.
./build/village --provider mock --seed 42 --steps 144 --out runs/mock-42

# Requires an already running llama.cpp chat-completions endpoint.
./build/village --provider llama --seed 42 --steps 96 --max-calls 20 \
  --endpoint http://127.0.0.1:8080/v1/chat/completions --out runs/qwen-42

# Continue the same world, including profiles, moods, relationships and RNG.
./build/village --provider mock --resume --steps 48 --out runs/mock-42

./build/village --help
```

Use a fresh output directory for each independent seed. Existing databases require `--resume`; the program never silently overwrites one. `--weather winter` selects the cold-weather calibration. `--config` and `--prompt` override the version-controlled files for a new run. `--db` can select an existing parent directory and a different SQLite path. On resume keep the provider and prompt unchanged for a comparable experiment. The real-model call budget counts attempts over the saved world's lifetime; increase `--max-calls` to permit additional speech after resuming. Budget exhaustion produces explicit silence events, never mock fallback.

Every output directory contains `village.sqlite`, `events.jsonl`, `summary.json`, and `transcript.txt`. The JSONL export is regenerated from SQLite at successful completion and includes prior invocations on resume. SQLite is authoritative if the process is interrupted. Model validation failures produce `dialogue_error` events and exit status 2 after the run; configuration/build/IO errors exit 1. A failed provider response is never substituted with invented dialogue. The simulation continues silently so the failure can be studied.

For the GPU server prepared on the owner's machine:

```sh
~/llama.cpp/build-vulkan/bin/llama-server \
  -m ~/Downloads/Qwen3.5-4B-Q4_K_M.gguf \
  --host 127.0.0.1 --port 8080 -c 4096 -ngl 99 --parallel 1 --reasoning-budget 0
```

The model is not included. Expect model output to vary across hardware/backend versions despite a fixed seed. Shared-prefix caching, deterministic batching and seed parameters do not guarantee bitwise reproducible GPU inference.

## Structure

- `src/state.cpp`: profiles, needs, utility choices, duties and weather.
- `src/simulation.cpp`: action validation, encounters, logging and measurement.
- `src/memory.cpp`: SQLite observations, relationships and full checkpoints.
- `src/dialogue.cpp`: mock provider, HTTP client, private context and output validation.
- `config/`, `prompts/`: reviewable behaviour configuration and speech contract.
- `tests/`: reproducible engine tests; no model required.
- `docs/DESIGN.md`: authoritative rules and prototype limits.
- `docs/EXPERIMENT_REPORT.md`: measured results and unresolved questions.
- `samples/`: small labelled transcripts and structured evidence, not model weights or databases.

The project uses a terminal interface only. Hallmark and Unreal are outside this experiment's scope.
