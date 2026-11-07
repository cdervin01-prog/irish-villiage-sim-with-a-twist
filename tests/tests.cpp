#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "village.hpp"
#include <filesystem>
#include <fstream>
#include <limits>

using namespace village;
World fresh(unsigned seed = 42, const std::string& weather = "summer") {
    return load_world(std::string(VILLAGE_SOURCE_DIR) + "/config/villagers.json", seed, weather);
}
class Probe final : public DialogueProvider {
public:
    std::vector<Json> contexts;
    bool invalid = false;
    Json generate(const Json& c, unsigned) override {
        contexts.push_back(c);
        if (invalid) return {{"speech", "Invented event"}, {"belief_delta", 1}, {"world_update", "move"}};
        return {{"speech", "A turn from " + c.at("speaker").get<std::string>() + std::to_string(contexts.size())},
                {"belief_delta", c.at("may_reflect").get<bool>() ? -0.01 : 0}};
    }
    std::string label() const override { return "probe"; }
};
TEST_CASE("three independently configured people with context-sensitive choices") {
    auto w = fresh(); REQUIRE(w.villagers.size() == 3);
    auto& d = w.villagers[0]; auto& g = w.villagers[1]; auto& a = w.villagers[2];
    CHECK(g.openness > d.openness); CHECK(a.beliefs.at("simulation") > g.beliefs.at("simulation"));
    CHECK(g.beliefs.at("catholic_commitment") < 0.1);
    CHECK(willingness(d, a, true) < willingness(d, a, false));
    CHECK(willingness(d, a, false) < willingness(g, a, false));
    w.tick = 20; CHECK(choose_action(d, w) == "work");
    d.energy = 0.05; CHECK(choose_action(d, w) == "rest");
}
TEST_CASE("unwitnessed knowledge is isolated and speech is attributed testimony") {
    MemoryStore db(":memory:"); auto w = fresh();
    db.event(w.tick, "speech", {{"actor", "wayne"}, {"text", "A secret claim"}}, {"dennis", "wayne"});
    CHECK(db.recall("gerrard").empty()); REQUIRE(db.recall("dennis").size() == 1);
    auto c = dialogue_context(w.villagers[1], "Wayne", "weather", "", w, db);
    CHECK(c.dump().find("A secret claim") == std::string::npos);
    CHECK_FALSE(c.contains("world")); CHECK_FALSE(c.contains("listener_beliefs"));
    CHECK(db.recall("dennis")[0]["kind"] == "witnessed");
}
TEST_CASE("Wayne may discuss reality with Gerrard without sharing minds") {
    auto w = fresh(); MemoryStore db(":memory:"); Probe p;
    for (auto& v : w.villagers) { v.location = "village_square"; v.intention = "idle"; }
    auto& g = w.villagers[1]; g.sociability = g.openness = g.patience = g.energy = g.mood = 1;
    Simulation sim(w, db, p); sim.encounter("wayne", "gerrard", "reality");
    REQUIRE(p.contexts.size() == 2);
    CHECK(p.contexts[0]["speaker_id"] == "wayne"); CHECK(p.contexts[0]["heard_now"] == "");
    CHECK(p.contexts[1]["speaker_id"] == "gerrard"); CHECK(p.contexts[1]["heard_now"] != "");
    CHECK(db.recall("dennis").empty());
    CHECK(sim.world().villagers[1].beliefs.at("simulation") == doctest::Approx(0.19));
    CHECK(sim.world().villagers[2].beliefs.at("simulation") == doctest::Approx(0.82));
}
TEST_CASE("Dennis can refuse interruptions while working without grudges") {
    int busy_declines = 0, free_declines = 0;
    for (unsigned seed = 0; seed < 100; ++seed) {
        for (bool working : {false, true}) {
            auto w = fresh(seed); MemoryStore db(":memory:"); Probe p;
            w.villagers[0].location = w.villagers[2].location = "dineens_pub";
            w.villagers[0].intention = working ? "work" : "idle";
            w.villagers[2].intention = "visit";
            Simulation sim(w, db, p); sim.encounter("wayne", "dennis", "weather");
            for (const auto& e : db.events()) if (e["type"] == "decline" || e["type"] == "ignore") {
                if (working) ++busy_declines; else ++free_declines;
                CHECK(sim.world().villagers[2].relationships.at("dennis").trust == 0);
            }
        }
    }
    CHECK(busy_declines > free_declines + 20);
}
TEST_CASE("shared location does not force speech and responsibilities persist") {
    MemoryStore db(":memory:"); MockDialogue mock; Simulation sim(fresh(), db, mock);
    sim.run(144); auto metrics = measure(db.events());
    CHECK(metrics["event_counts"].value("silent_encounter", 0) > 0);
    for (const auto& e : db.events()) if (e["type"] == "state") {
        auto v = e["data"]["after"].get<Villager>(); int h = (e["tick"].get<int>() % 48) / 2;
        if (on_duty(v, h) && v.energy >= 0.15) CHECK(v.intention == "work");
    }
}
TEST_CASE("identical mock seeds reproduce event logs") {
    for (unsigned seed : {7, 42, 99}) {
        MemoryStore a(":memory:"), b(":memory:"); MockDialogue mock;
        Simulation x(fresh(seed), a, mock), y(fresh(seed), b, mock);
        x.run(96); y.run(96); CHECK(a.events() == b.events());
    }
}
TEST_CASE("SQLite survives reopening and resumes RNG state without knowledge leaks") {
    auto path = std::filesystem::temp_directory_path() / "irish-village-checkpoint-test.sqlite";
    std::filesystem::remove(path); MockDialogue mock;
    MemoryStore continuous(":memory:"); Simulation reference(fresh(17), continuous, mock); reference.run(96);
    {
        MemoryStore first(path.string()); Simulation part(fresh(17), first, mock); part.run(48);
        CHECK(first.count("relationships") == 6);
    }
    {
        MemoryStore second(path.string()); auto restored = fresh(999);
        REQUIRE(second.restore(restored)); CHECK(restored.tick == 60);
        Simulation rest(restored, second, mock); rest.run(48);
        CHECK(second.events() == continuous.events());
        CHECK(Json(rest.world().villagers) == Json(reference.world().villagers));
        CHECK(rest.world().dialogue_calls == reference.world().dialogue_calls);
    }
    std::filesystem::remove(path);
}
TEST_CASE("weather hangovers enjoyment and relationship effects are individual and bounded") {
    auto warm = fresh(), cold = fresh(42, "winter");
    for (int i = 0; i < 16; ++i) for (int n = 0; n < 3; ++n) {
        advance_needs(warm.villagers[n], warm); advance_needs(cold.villagers[n], cold);
    }
    CHECK(warm.villagers[0].mood > cold.villagers[0].mood);
    CHECK(warm.villagers[0].mood != warm.villagers[1].mood);
    auto v = fresh().villagers[0]; enjoy(v, 40, true);
    CHECK(v.hangover_start == 64); CHECK(v.hangover_end - v.hangover_start == 16);
    World time = fresh(); time.tick = 64;
    for (int i = 0; i < 12; ++i) advance_needs(v, time);
    CHECK(v.mood < 0.55);
    auto sober = fresh().villagers[0]; enjoy(sober, 40, false); CHECK(sober.hangover_end == -1);
    social_effect(sober, "wayne", 1); auto trust = sober.relationships.at("wayne").trust;
    auto transient = sober.relationships.at("wayne").transient;
    advance_needs(sober, time); CHECK(sober.relationships.at("wayne").transient < transient);
    CHECK(sober.relationships.at("wayne").trust == trust);
    sober.anger = 0.8; sober.patience = 0.1; advance_needs(sober, time);
    CHECK(0.8 - sober.anger > sober.patience - 0.1);
}
TEST_CASE("model output cannot mutate world state or invent unconstrained belief changes") {
    CHECK_THROWS(validate_utterance(Json{{"speech", "hi"}, {"belief_delta", 0}, {"location", "pub"}}, false));
    CHECK_THROWS(validate_utterance(Json{{"speech", "hi"}, {"belief_delta", 0.5}}, true));
    CHECK_THROWS(validate_utterance(Json{{"speech", "hi"}, {"belief_delta", 0.01}}, false));
    CHECK_THROWS(validate_utterance(Json{{"speech", "a\nb"}, {"belief_delta", 0}}, false));
    CHECK_THROWS(validate_utterance(Json{{"speech", std::string(361, 'a')}, {"belief_delta", 0}}, false));
    CHECK_THROWS(validate_utterance(Json{{"speech", ""}, {"belief_delta", 0}}, false));
    CHECK_NOTHROW(validate_utterance(Json{{"speech", "Perhaps."}, {"belief_delta", -0.02}}, true));
    auto w = fresh(); MemoryStore db(":memory:"); Probe bad; bad.invalid = true;
    w.villagers[1].sociability = w.villagers[1].openness = 1;
    for (auto& v : w.villagers) { v.location = "village_square"; v.intention = "idle"; }
    Simulation sim(w, db, bad); sim.encounter("wayne", "gerrard", "reality");
    CHECK(measure(db.events())["speech_count"] == 0);
    CHECK(sim.world().villagers[1].beliefs == w.villagers[1].beliefs);
}
TEST_CASE("physically impossible encounters and sleeping recipients are rejected") {
    MemoryStore db(":memory:"); Probe p; Simulation sim(fresh(), db, p);
    sim.encounter("wayne", "gerrard", "weather"); CHECK(p.contexts.empty());
    CHECK(measure(db.events())["event_counts"]["invalid_encounter"] == 1);
    CHECK_THROWS(sim.encounter("unknown", "gerrard", "weather"));
}
TEST_CASE("recent exact repeated speech is rejected without rewriting memories") {
    class Repeat final : public DialogueProvider {
    public:
        Json generate(const Json&, unsigned) override { return {{"speech", "The same words."}, {"belief_delta", 0}}; }
        std::string label() const override { return "repeat"; }
    } repeat;
    auto w = fresh(); MemoryStore db(":memory:");
    for (auto& v : w.villagers) { v.location = "village_square"; v.intention = "idle"; }
    auto& g = w.villagers[1]; g.sociability = g.openness = g.patience = g.mood = g.energy = 1;
    Simulation sim(w, db, repeat);
    sim.encounter("wayne", "gerrard", "weather");
    auto before = measure(db.events())["speech_count"].get<int>(); REQUIRE(before == 2);
    sim.world().tick += 2;
    sim.encounter("wayne", "gerrard", "weather");
    CHECK(measure(db.events())["speech_count"] == 2);
    CHECK(measure(db.events())["dialogue_errors"] == 1);
}
TEST_CASE("a failed checkpoint rolls back events world and random generator") {
    MemoryStore db(":memory:"); MockDialogue mock; Simulation sim(fresh(), db, mock);
    auto before = Json(sim.world().villagers); auto rng = sim.world().rng;
    db.sql("CREATE TRIGGER fail_checkpoint BEFORE INSERT ON checkpoint BEGIN SELECT RAISE(ABORT,'test failure'); END;");
    CHECK_THROWS(sim.step()); CHECK(db.count("events") == 0);
    CHECK(Json(sim.world().villagers) == before); CHECK(sim.world().rng == rng);
    CHECK(sim.world().tick == 12);
    db.sql("DROP TRIGGER fail_checkpoint"); CHECK_NOTHROW(sim.step());
}
TEST_CASE("call budget is persisted rather than reset when a world resumes") {
    auto w = fresh(); w.dialogue_calls = 12; MemoryStore db(":memory:"); db.checkpoint(w);
    auto restored = fresh(); REQUIRE(db.restore(restored)); CHECK(restored.dialogue_calls == 12);
    for (auto& v : restored.villagers) { v.location = "village_square"; v.intention = "idle"; }
    auto& g = restored.villagers[1]; g.sociability = g.openness = g.patience = g.mood = g.energy = 1;
    Probe p; Simulation sim(restored, db, p, 12); sim.encounter("wayne", "gerrard", "reality");
    CHECK(p.contexts.empty()); CHECK(measure(db.events())["event_counts"]["dialogue_budget"] == 1);
}
TEST_CASE("provider failure remains silence and does not become mock evidence") {
    class Offline final : public DialogueProvider {
    public:
        Json generate(const Json&, unsigned) override { throw std::runtime_error("offline"); }
        std::string label() const override { return "offline"; }
    } offline;
    auto w = fresh(); MemoryStore db(":memory:");
    for (auto& v : w.villagers) { v.location = "village_square"; v.intention = "idle"; }
    auto& g = w.villagers[1]; g.sociability = g.openness = g.patience = g.mood = g.energy = 1;
    Simulation sim(w, db, offline); sim.encounter("wayne", "gerrard", "weather");
    CHECK(measure(db.events())["speech_count"] == 0); CHECK(measure(db.events())["dialogue_errors"] == 1);
    CHECK(db.recall("wayne").size() == 1); // Only the witnessed approach, no fabricated utterance.
}
TEST_CASE("a resumed world cannot switch between mock and real evidence") {
    MemoryStore db(":memory:"); MockDialogue mock; Probe other;
    Simulation sim(fresh(), db, mock); sim.step();
    auto restored = fresh(); REQUIRE(db.restore(restored));
    CHECK_THROWS(Simulation(restored, db, other));
}
