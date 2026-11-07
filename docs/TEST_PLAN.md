# Test plan and acceptance criteria

Run `ctest --test-dir build --output-on-failure` after building. Tests use doctest, temporary/in-memory SQLite and mock/probe providers; no model/server/network is required.

| Area | Acceptance check |
| --- | --- |
| Identity | Exactly Dennis, Gerrard and Wayne; distinct baselines; Gerrard nonbelieving and Wayne's theory private |
| Daily duties | Scheduled work persists unless severe fatigue requires rest |
| Interruption | Across 100 controlled seeds, Dennis refuses busy interruptions substantially more often than free encounters; no automatic initiator grudge |
| Natural silence | A normal 3-day mock run contains silent co-location events |
| Wayne/Gerrard | Controlled accepted reality discussion sends only each speaker's mind; Gerrard receives only Wayne's actual speech; Dennis remains unaware |
| Knowledge | Unwitnessed secret never appears in Gerrard's recall/prompt; speech observations retain witness provenance |
| Determinism | Seeds 7, 42, 99 each reproduce complete 2-day mock event logs |
| Persistence | Close/reopen SQLite midway; subsequent events, world and RNG-driven choices match uninterrupted run; six directional relationships persist |
| Mood | Summer/winter effects differ by profile; sober enjoyment has no hangover; alcohol low lasts 16 half-hour steps; anger decays faster than depleted patience |
| Social decay | Transient influence decays while smaller trust change persists |
| Model validation | Reject extra fields/world updates, excessive changes, reflection without heard speech, multiline/empty/oversize speech and malformed responses |
| Physical validation | Reject self/absent/asleep/unavailable targets and cooldown violations before calling a provider |

Experiment acceptance is more demanding than unit tests. Run mock seeds 7/42/99 in summer and winter for at least 3 days, then real Qwen seeds separately with a finite call budget. Archive provider/model identity, prompts, seeds, actions and errors. Inspect denial/initiative rates and each speaker's text. Successful build/tests establish mechanics, not believable people. Report repetition, convergence and historical/semantic problems instead of changing the experiment to manufacture an entertaining outcome.

Additional completed cases cover exact-repeat rejection, lifetime-budget continuation, transactional rollback, provider failure without mock fallback and refusing provider-type changes on resume. A closed-port HTTP integration run also exercised the real client's transport failure path. Fifteen doctest cases pass. Malformed configuration coverage and longer real-model runs remain useful extensions. Findings and actual completed checks belong in EXPERIMENT_REPORT.md.
