#pragma once
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <array>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace village {
using Json = nlohmann::json;
double bounded(double x);
struct Relationship {
    double trust = 0, familiarity = 0, transient = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Relationship, trust, familiarity, transient)
struct Villager {
    std::string id, name, occupation, profile, home, workplace, location, intention = "rest";
    double sociability = 0.5, openness = 0.5, duty = 0.8, baseline_patience = 0.6;
    double weather_sensitivity = 0.1, mood = 0.65, patience = 0.6, energy = 0.8, anger = 0;
    double enjoyment = 0, drink_probability = 0.2;
    int hangover_start = -1, hangover_end = -1, last_social = -100;
    std::vector<int> work_hours;
    std::map<std::string, double> beliefs;
    std::map<std::string, Relationship> relationships;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Villager, id, name, occupation, profile, home, workplace,
    location, intention, sociability, openness, duty, baseline_patience, weather_sensitivity,
    mood, patience, energy, anger, enjoyment, drink_probability, hangover_start, hangover_end,
    last_social, work_hours, beliefs, relationships)
struct World {
    int tick = 12; // Half-hour steps, initially 06:00.
    int dialogue_calls = 0;
    std::string provider;
    std::string weather = "summer";
    std::vector<Villager> villagers;
    std::mt19937 rng;
    double random();
    int hour() const { return (tick % 48) / 2; }
};
World load_world(const std::string& path, unsigned seed, const std::string& weather);
bool on_duty(const Villager& v, int hour);
void advance_needs(Villager& v, const World& w);
void enjoy(Villager& v, int tick, bool alcohol);
double willingness(const Villager& v, const Villager& other, bool working);
void social_effect(Villager& v, const std::string& other, double appraisal);
std::string choose_action(Villager& v, World& w);

class MemoryStore {
    sqlite3* db_ = nullptr;
public:
    explicit MemoryStore(const std::string& path);
    ~MemoryStore();
    MemoryStore(const MemoryStore&) = delete;
    MemoryStore& operator=(const MemoryStore&) = delete;
    void sql(const std::string& statement);
    void event(int tick, const std::string& type, const Json& data,
               const std::vector<std::string>& witnesses = {});
    Json recall(const std::string& id, int limit = 8) const;
    void checkpoint(const World& w);
    bool restore(World& w);
    Json events() const;
    int count(const std::string& table) const;
};
struct Utterance {
    std::string speech;
    double belief_delta = 0;
};
Utterance validate_utterance(const Json& result, bool may_reflect);
Json dialogue_context(const Villager& speaker, const std::string& listener_name,
                      const std::string& topic, const std::string& heard, const World& w,
                      const MemoryStore& store);
class DialogueProvider {
public:
    virtual ~DialogueProvider() = default;
    virtual Json generate(const Json& context, unsigned seed) = 0;
    virtual std::string label() const = 0;
};
class MockDialogue final : public DialogueProvider {
public:
    Json generate(const Json& context, unsigned seed) override;
    std::string label() const override { return "mock"; }
};
class LlamaDialogue final : public DialogueProvider {
    std::string endpoint_, prompt_;
public:
    LlamaDialogue(std::string endpoint, const std::string& prompt_path);
    Json generate(const Json& context, unsigned seed) override;
    std::string label() const override { return "llama.cpp"; }
};
class Simulation {
    World world_;
    MemoryStore& store_;
    DialogueProvider& provider_;
    int max_calls_;
    void emit(const std::string& type, const Json& data,
              const std::vector<std::string>& witnesses = {});
    bool speak(Villager& speaker, Villager& listener, const std::string& topic,
               const std::string& heard, std::string& output);
public:
    Simulation(World world, MemoryStore& store, DialogueProvider& provider, int max_calls = 40);
    World& world() { return world_; }
    void step();
    void run(int steps);
    void encounter(const std::string& initiator, const std::string& recipient,
                   const std::string& topic);
};
Json measure(const Json& events);
}
