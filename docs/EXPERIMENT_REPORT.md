# Prototype experiment report, 2026-11-07

## Result

The C++20 application builds and runs with SQLite persistence and a real llama.cpp/Qwen endpoint. Fifteen doctest cases pass. The engine supports private observations, obligations, optional encounters, refusal, silence, individual weather responses and resumable seeded state. **The central research question remains unresolved.** The real model showed repetition, unsupported details and weak grounding. Do not treat these transcripts as validated personalities or scale the village yet.

No separate model reviewed the implementation. Qwen generated dialogue; that is not code review.

## Environment and validation

GNU C++ 13.3.0; CMake Debug build, C++20; SQLite 3.45.1; nlohmann/json 3.11.3; libcurl 8.5.0; doctest 2.4.11. Development packages were downloaded from the configured Ubuntu repositories and extracted into a user-owned prefix because sudo requires a password. No system packages or GPU drivers were changed for this prototype.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/home/dayton/.local/share/irish-village-deps/root/usr \
  -DSQLite3_INCLUDE_DIR=/home/dayton/.local/share/irish-village-deps/root/usr/include \
  -DSQLite3_LIBRARY=/usr/lib/x86_64-linux-gnu/libsqlite3.so.0 \
  -DCURL_INCLUDE_DIR=/home/dayton/.local/share/irish-village-deps/root/usr/include/x86_64-linux-gnu \
  -DCURL_LIBRARY_RELEASE=/usr/lib/x86_64-linux-gnu/libcurl.so.4 \
  -DDOCTEST_INCLUDE_DIR=/home/dayton/.local/share/irish-village-deps/root/usr/include
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

Result: successful build without emitted compiler warnings; one CTest executable, all 15 doctest cases passing, approximately 4.5 seconds. CI configuration repeats the build/tests on Ubuntu; remote CI results are not assumed here.

Tests cover controlled Wayne/Gerrard speech, isolation of Dennis's knowledge, 100-seed busy/free Dennis comparisons, ordinary silent co-location, mock determinism across three seeds, physical encounter validation, full SQLite reopen/continuation equivalence, weather/alcohol/decay, invalid model output, exact-repeat rejection, transactional rollback, lifetime call budgets, provider failures and provider-type isolation on resume. Engine reproducibility is tested within this executable and library environment, not across arbitrary C++ standard-library implementations.

An additional real HTTP failure check pointed the program at closed localhost port 1. Over 144 steps (48 plus a 96-step resume), it logged two transport failures, two subsequent budget events and zero speech. It did not fall back to mock dialogue. The initial 48-step diagnostic expected two calls but only one accepted encounter occurred; extending the run exercised both the limit and its persisted state. That was a test-harness expectation error, not a forced encounter added to the simulation.

## Experiments

All worlds start at 06:00, use the shipped profiles/prompt and run 144 half-hour steps (72 simulated hours). The mock matrix uses seeds 7/42/99 with both weather calibrations. Real Qwen runs use summer and the same three seeds, independently created databases. Real call caps were 12 for seed 7 and 10 for seeds 42/99; the observed runs finished below those caps without budget suppression. Real attempts were 11, 9 and 6 respectively. No mandatory conversations were injected into these runs.

| Provider/weather | Seed | Approaches | Declines + ignores | Silent encounters | Accepted speech turns | Model errors |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Mock/summer | 7 | 17 | 10 | 23 | 14 | 0 |
| Mock/summer | 42 | 10 | 5 | 25 | 10 | 0 |
| Mock/summer | 99 | 15 | 12 | 21 | 6 | 0 |
| Mock/winter | 7 | 9 | 4 | 28 | 10 | 0 |
| Mock/winter | 42 | 13 | 6 | 21 | 14 | 0 |
| Mock/winter | 99 | 14 | 8 | 23 | 12 | 0 |
| Qwen/summer | 7 | 16 | 10 | 26 | 10 | 1 |
| Qwen/summer | 42 | 9 | 4 | 25 | 8 | 1 |
| Qwen/summer | 99 | 15 | 12 | 21 | 6 | 0 |

The model was `Qwen3.5-4B-Q4_K_M.gguf`, served by llama.cpp build `b1-4e910a8`, Vulkan on an AMD Polaris 8-GiB card, one slot, 4096-token context, thinking disabled. Per-turn temperature is 0.7 with engine-derived seeds and a 180-token output limit. Model and prompt details are archived in `samples/manifest.json`. The earlier approximately 17-token/s standalone test was not a benchmark of this prototype's longer prompts.

Real runs 7/42 exited 2 because the application rejected exact recent repeated speech (Gerrard at tick 114; Wayne at tick 137). Seed 99 exited 0. A recorded rejection is intentional validation behaviour, but it is still a model failure and remains visible. Accepted-text exact repetition count was zero in these short runs; paraphrase repetition was plainly present and is not covered by that metric.

## What the evidence supports

- Silence is common; co-location did not force speech. Neither drama nor arguments were added to obtain conversations.
- In the summer mock runs Dennis initiated 2/1/2 approaches, Gerrard 8/5/8 and Wayne 7/4/5. In real runs the counts were Dennis 2/1/2, Gerrard 7/5/8, Wayne 7/3/5. These show the configured mechanical differences, not independent proof that personalities emerge.
- Duties persisted: each 72-hour run recorded Dennis working 54 ticks, Gerrard 48 and Wayne 42. Fatigue/rest and family time remain separate from conversation.
- Personal memories reach only their designated owners; tests prove the supported observation path does not silently copy Dennis/Wayne's secret into Gerrard's prompt. This does not prove a language model will never invent knowledge.
- Directional familiarity/trust and short-lived social influence changed following completed exchanges. State and recalled memory IDs are logged; memories are actually reintroduced into later prompts. Prompt access is not proof of faithful semantic use.
- Real runs left simulation-confidence values unchanged at Dennis 0.12, Gerrard 0.20, Wayne 0.82. Thus short-run belief convergence was absent, but meaningful belief evolution was not demonstrated.
- Weather changes individual state deterministically, but noisy encounters did not decrease monotonically in every winter seed. Three seeds are too few for population-level weather conclusions.

## Failures and uncertainty

1. **Unsupported assertions.** Qwen invented customers, crowds, a till request, a market outing, a bargain, family land stories and a wind direction. These words never mutated the C++ world, but they weaken believability and can contaminate later testimony.
2. **Repetitive phrasing.** Several speakers returned to heat, light and dream imagery. Exact-repeat rejection caught two turns; near-duplicate meaning and shared style were not detected. Dennis and Gerrard can still sound too similar.
3. **Personality drift.** Gerrard sometimes became philosophical. Wayne sometimes produced generic drinking talk or introduced his theory during an ordinary topic. The speaker contract alone is insufficient.
4. **Initiation grounding.** Seed 99 starts with Gerrard saying "Not much" despite no preceding question. The model can behave as if an unobserved dialogue occurred.
5. **Social appraisal is deliberately shallow.** It reads work pressure, openness, energy and anger, not the content's meaning. This can reward an incoherent but schema-valid exchange and tends toward mild positive effects in free time. No claim of realistic emotional appraisal is justified.
6. **Small and coarse experiment.** Only three days per seed, recent-eight memory retrieval, fixed duty windows, instantaneous boundary travel, no economy, hunger, injury, religious institution or political-event simulation. Long-term personality/memory coherence and evidence-based belief revision need more observation.
7. **Historical limits.** The Famine-era setting is acknowledged, not reconstructed. The names and personal intentions are the owner's fictionalized specification, not historical biographical evidence. Broader local social history remains unverified.
8. **Operational limits.** One process/one writer; HTTP calls can hold the tick transaction for up to 90 seconds. Lifetime call caps are explicit experimental controls. A model change at the same endpoint is not automatically detected. Provider-type changes are rejected on resume; same-type model/prompt changes remain the operator's responsibility. No database migration beyond schema 1 is supplied.

## Smallest useful next improvement

Keep three villagers and text output. Add a narrowly scoped perceived-scene record (known present people, actual responsibilities and explicit absent facts), then evaluate a revised speech prompt on the same seeds against the archived baseline. Record unsupported statements and paraphrase repetition with owner-reviewed examples before changing personality numbers. Do not create a broad autonomous planner, more villagers, graphics or an economy to mask these failures.

Acceptance for that next change should be fewer unsupported scene assertions and repeated meanings while preserving silence, duties, privacy and distinct initiation rates. Longer real-model runs and owner review of Dennis/Gerrard/Wayne remain necessary before claiming the people behave believably.
