# Authoritative simulation rules, v0.1

Source: the owner's CODEX CONSTRUCTION BRIEF, Version 0.1, supplied in this session. This document records its implementation contract; numeric profile values and schedules are illustrative calibrations requiring owner review.

## Question and scope

Can three autonomous villagers maintain distinct personalities, private beliefs, memories and behaviour over repeated encounters? Start with exactly Dennis Dineen, Gerrard and Wayne. C++20 terminal prototype, no graphics or 3D, no elaborate economy, scripted drama or additional villagers. Do not expand scope until this experiment is informative. A convincing line of dialogue is not success.

## People

Dennis runs Dineens. He is polite, respectful, quietly authoritative, selective about conversation, tolerant of heavy drinkers and not harshly judgmental. He speaks more willingly with people he respects. Temper follows accumulated pressure; anger can recede faster than patience. No automatic grudge from an isolated incident. Humour can defuse tension. If an actual fight later exists, collecting glasses and directing it outside is appropriate. He is not permanently angry, violent, suspicious or politically obsessed and may be indifferent.

Gerrard runs another pub. Travel made him open to different ways of living. He does not personally believe in Catholicism, without rejecting believers. He can disagree without rejecting a person, welcome different personalities, become irritated and attend to his family and work. He is not perpetually philosophical, agreeable or drunk.

Wayne frequently returns to the possibility of an artificial reality, interprets some experiences through that belief and may persuade others. He also works, rests, has other interests and may revise his confidence. Other people are not preprogrammed to find him boring, foolish or interesting. No endlessly repeated speech.

## Engine ownership and privacy

Every person owns a profile, stable numeric traits, mood, energy, patience, anger, private beliefs/confidences, directional trust/familiarity/transient social influence, intentions, duty hours, cooldowns and SQLite observations. Actions are work, rest, family duties, walk, visit, idle and contextually chosen approach/ignore/decline/speech. Moving away counts as leaving; speaking to someone establishes listening only when they accepted the encounter. No location alone forces conversation.

Thirty-minute ticks start at 06:00. Night and severe fatigue favour rest. Duties are a high-priority constraint; an owner on duty works at their pub. Otherwise a small utility selector combines needs, social preferences, stickiness and seeded variation. This is deliberately coarse, not a complete economic or physiological model. A shuffled initiative order avoids a permanent Dennis-first advantage. Each person participates in at most one encounter per tick with an hour cooldown between approaches. Visiting a pub selects only a currently scheduled open pub, not one's own. Travel takes one tick boundary; no pathfinding.

Only the engine moves people and updates the database. A dialogue provider gets the speaker's state, local time/weather, listener name, selected topic, words actually heard and up to eight most recent personal observations. It does not get another profile, another private belief or the global ledger. Some public facts are initially known by configuration (names/occupations and the broad setting); actual personal experiences must be observed.

`events` records objective engine events, not objective truth of model claims. `observations` records explicitly named witnesses with event IDs and provenance. In particular, hearing Wayne make a claim is knowledge that he said it, not proof of his claim. Private mood/belief/relationship events have no external witnesses. Model speech cannot create a transaction, change a location, update another person or prove a rumour. Persistent memories influence later prompts and directional relationships influence future willingness. Retrieval is recent-only in v0.1; relevance and semantic contradiction checks remain limitations.

## Social and physical calibration

Initial feel-good baseline is 0.65. Enjoyment can move it toward 0.80. Alcohol may set a next-day 08:00-16:00 low target near 0.40. These are gradual targets, not hard universal percentages. A sober pub visit or walk can be enjoyable without hangover. Alcohol is sampled only on entering a pub, not every tick. Good summer and miserable winter weather have different weights per person relative to their baseline. No day-night weather forecast model is implied.

Positive encounters can add roughly 0.10 transient willingness and 0.05 lasting trust at maximum appraisal, weighted by individual openness; negatives can subtract them. Transient influence decays each half-hour; lasting trust does not decay in this first version. Appraisal depends on work pressure, openness, energy and anger, not a hard-coded opinion of Wayne. Busy refusals are neutral to the initiator; the interrupted person's patience may dip slightly. No automatic escalation into fights or grudges. Successful exchanges alone do not guarantee friendship. Mood is not a direct outward-behaviour percentage; traits and context mediate willingness and speech. The appraisal formula is a provisional social abstraction, not a validated psychological model or semantic reading of the words.

## Model boundary

Output must be an exact object with `speech` and `belief_delta`. One turn, <=360 UTF-8 bytes, no control characters/newlines, finite delta in [-0.02,+0.02]. Reflection is allowed only on the responding person's own simulation confidence after actually hearing a reality-topic statement. Zero is allowed and encouraged; no compulsory agreement. No arbitrary belief keys. Repeated exact recent self-speech is rejected. JSON-schema generation assists but never replaces application validation. Transport, HTTP, truncated JSON and schema errors produce recorded silence, not mock fallback. Semantically invented claims, nuanced personality violations and paraphrase repetition require transcript review; v0.1 cannot prove their absence.

## Setting

Fictional County Cork, around 1850; personalities inspired by the owner's recollections of the 1980s. Wayne's artificial-reality belief is an explicit speculative twist, not a historical assertion. English dialogue is a presentation abstraction, not a claim about local language use. Avoid exaggerated accents, twentieth-century organizations and invented political facts. Catholic practice, Irish identity, family, work, local affairs and gossip can matter differently to each individual without dominating every conversation.

The date overlaps the Great Famine of 1845-1852. This is documented by the [National Archives of Ireland](https://nationalarchives.ie/help-with-research/research-guides/famine-records-distress-papers-and-the-relief-commission/). The prototype does not simulate local relief, land tenure, hunger or mortality, and must not claim to reconstruct ordinary life in a real Cork village. Historical validation is incomplete. Church/family influence is represented in private profile and dialogue context, not a full institution model. Do not add specific political events or regional customs without reliable evidence.

## Reproducibility and acceptance

Same executable, configuration and seed with mock dialogue must reproduce events. Persist complete profiles, relationships, needs, memories, time, RNG and dialogue-attempt count. Checkpoint with each tick in a SQLite transaction; recover the in-memory world on failure. Across model/backend versions real speech is not guaranteed deterministic. Database schema is version 1; automatic migration is not implemented.

Measure actions, initiative, refusals/ignores, silent encounters, speech, recalled memory IDs, state changes, belief reflection, relationship changes, model errors and exact repetitions. Run several seeds and inspect both counts and transcripts. Report personality convergence, repetition, agreement, leaked knowledge, unmotivated emotion and abandoned responsibilities as open questions when not measured. No unsupported success claims. Mock fixtures test mechanics only. No other model review is implied.
