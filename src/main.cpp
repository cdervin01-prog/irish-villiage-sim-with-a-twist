#include "village.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    using namespace village;
    try {
        std::map<std::string, std::string> args;
        for (int i = 1; i < argc; ++i) {
            std::string key = argv[i];
            if (key == "--help") {
                std::cout << "village [--provider mock|llama] [--seed N] [--steps N] [--weather summer|winter]\n"
                    "        [--db PATH] [--out DIR] [--resume] [--max-calls N] [--config PATH]\n"
                    "        [--endpoint URL] [--prompt PATH]\n"; return 0;
            }
            if (key == "--resume") { args[key] = "true"; continue; }
            if (key != "--provider" && key != "--seed" && key != "--steps" && key != "--weather" &&
                key != "--db" && key != "--out" && key != "--max-calls" && key != "--config" &&
                key != "--endpoint" && key != "--prompt") throw std::runtime_error("Unknown option: " + key);
            if (++i >= argc || args.contains(key)) throw std::runtime_error("Missing value or duplicate option: " + key);
            args[key] = argv[i];
        }
        auto arg = [&](const std::string& key, const std::string& fallback) { return args.contains(key) ? args.at(key) : fallback; };
        auto integer = [&](const std::string& key, const std::string& fallback) {
            auto value = arg(key, fallback); size_t used = 0; int n = std::stoi(value, &used);
            if (used != value.size() || n < 0) throw std::runtime_error("Expected nonnegative integer: " + key);
            return n;
        };
        auto out = arg("--out", "runs/default");
        auto db = arg("--db", out + "/village.sqlite");
        std::filesystem::create_directories(out);
        if (std::filesystem::exists(db) && !args.contains("--resume"))
            throw std::runtime_error("Database exists. Use --resume or a fresh --out directory.");
        const auto provider_name = arg("--provider", "mock");
        if (provider_name != "mock" && provider_name != "llama") throw std::runtime_error("Provider must be mock or llama");
        auto config = arg("--config", std::string(VILLAGE_SOURCE_DIR) + "/config/villagers.json");
        auto prompt = arg("--prompt", std::string(VILLAGE_SOURCE_DIR) + "/prompts/dialogue-v1.txt");
        auto endpoint = arg("--endpoint", "http://127.0.0.1:8080/v1/chat/completions");
        unsigned seed = integer("--seed", "42");
        int steps = integer("--steps", "144"), max_calls = integer("--max-calls", "40");
        auto world = load_world(config, seed, arg("--weather", "summer"));
        MemoryStore store(db);
        if (args.contains("--resume") && !store.restore(world)) throw std::runtime_error("No checkpoint to resume");
        if (args.contains("--resume") && (args.contains("--seed") || args.contains("--weather") || args.contains("--config")))
            throw std::runtime_error("Resume restores seed, weather and profiles; do not override them");
        std::unique_ptr<DialogueProvider> provider;
        if (provider_name == "mock") provider = std::make_unique<MockDialogue>();
        else provider = std::make_unique<LlamaDialogue>(endpoint, prompt);
        int initial_tick = world.tick;
        std::cout << "Provider: " << provider->label() << "; seed: " << seed << "; weather: " << world.weather << '\n';
        Simulation sim(std::move(world), store, *provider, max_calls);
        store.event(sim.world().tick, "run", {{"provider", provider->label()}, {"seed_argument", seed},
            {"resume", args.contains("--resume")}, {"steps", steps}, {"max_calls", max_calls}});
        sim.run(steps);
        auto rows = store.events();
        std::ofstream log(out + "/events.jsonl"), transcript(out + "/transcript.txt");
        if (!log || !transcript) throw std::runtime_error("Cannot write outputs");
        transcript << "Provider: " << provider->label() << "; starting tick: " << initial_tick << '\n';
        for (const auto& e : rows) {
            log << e.dump() << '\n';
            if (e.at("type") == "speech") transcript << "[tick " << e.at("tick") << "] "
                << e.at("data").at("speaker").get<std::string>() << ": "
                << e.at("data").at("text").get<std::string>() << '\n';
        }
        auto report = measure(rows); report["final_villagers"] = sim.world().villagers;
        report["provider_this_invocation"] = provider->label(); report["final_tick"] = sim.world().tick;
        std::ofstream summary(out + "/summary.json");
        if (!summary) throw std::runtime_error("Cannot write summary");
        summary << report.dump(2) << '\n';
        std::cout << report["event_counts"].dump() << '\n';
        return report["dialogue_errors"].get<int>() > 0 ? 2 : 0;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
