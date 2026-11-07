#include "village.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>

namespace village {
Simulation::Simulation(World world, MemoryStore& store, DialogueProvider& provider, int max_calls)
    : world_(std::move(world)), store_(store), provider_(provider), max_calls_(max_calls) {
    if (!world_.provider.empty() && world_.provider != provider_.label())
        throw std::runtime_error("Cannot mix mock and real providers in the same saved world");
    world_.provider = provider_.label();
}
void Simulation::emit(const std::string& type, const Json& data, const std::vector<std::string>& witnesses) {
    store_.event(world_.tick, type, data, witnesses);
    if (type == "speech") std::cout << "[day " << world_.tick / 48 + 1 << ' ' << world_.hour()
        << ':' << (world_.tick % 2 ? "30" : "00") << "] " << data.at("speaker").get<std::string>()
        << ": " << data.at("text").get<std::string>() << '\n';
}
bool Simulation::speak(Villager& speaker, Villager& listener, const std::string& topic,
                       const std::string& heard, std::string& output) {
    if (provider_.label() != "mock" && world_.dialogue_calls >= max_calls_) {
        emit("dialogue_budget", {{"actor", speaker.id}}); return false;
    }
    auto context = dialogue_context(speaker, listener.name, topic, heard, world_, store_);
    auto memories = Json::array();
    for (const auto& memory : context.at("own_memories")) memories.push_back(memory.at("memory_id"));
    emit("recall", {{"actor", speaker.id}, {"memory_ids", memories}});
    ++world_.dialogue_calls;
    try {
        auto utterance = validate_utterance(provider_.generate(context, world_.rng()), context.at("may_reflect"));
        for (const auto& memory : context.at("own_memories")) {
            const auto& content = memory.at("content");
            if (memory.at("type") == "speech" && content.at("actor") == speaker.id &&
                content.at("text") == utterance.speech) throw std::runtime_error("Exact recent speech repetition");
        }
        output = utterance.speech;
        // A speech event establishes that words were said, not that their claims are true.
        emit("speech", {{"actor", speaker.id}, {"speaker", speaker.name}, {"recipient", listener.id},
            {"text", output}, {"topic", topic}, {"provider", provider_.label()}}, {speaker.id, listener.id});
        if (utterance.belief_delta != 0) {
            double old = speaker.beliefs.at("simulation");
            speaker.beliefs["simulation"] = bounded(old + utterance.belief_delta);
            emit("belief", {{"actor", speaker.id}, {"belief", "simulation"}, {"before", old},
                {"after", speaker.beliefs.at("simulation")}, {"basis", "personal reflection on heard speech"}});
        }
        return true;
    } catch (const std::exception& e) {
        emit("dialogue_error", {{"actor", speaker.id}, {"error", e.what()}, {"provider", provider_.label()}});
        return false; // Failed output never becomes speech or world state.
    }
}
void Simulation::encounter(const std::string& initiator, const std::string& recipient, const std::string& topic) {
    auto find = [&](const std::string& id) -> Villager& {
        auto it = std::find_if(world_.villagers.begin(), world_.villagers.end(), [&](const auto& v) { return v.id == id; });
        if (it == world_.villagers.end()) throw std::runtime_error("Unknown villager");
        return *it;
    };
    auto& a = find(initiator); auto& b = find(recipient);
    if (a.id == b.id || a.location != b.location || a.intention == "rest" || a.intention == "family" ||
        b.intention == "rest" || b.intention == "family" || world_.tick - a.last_social < 2 ||
        world_.tick - b.last_social < 2) {
        emit("invalid_encounter", {{"actor", a.id}, {"recipient", b.id}}); return;
    }
    a.last_social = b.last_social = world_.tick;
    emit("approach", {{"actor", a.id}, {"recipient", b.id}, {"topic", topic}, {"location", a.location}}, {a.id, b.id});
    double chance = willingness(b, a, b.intention == "work");
    if (world_.random() >= chance) {
        const std::string kind = b.intention == "work" ? "decline" : "ignore";
        emit(kind, {{"actor", b.id}, {"recipient", a.id}, {"willingness", chance}}, {a.id, b.id});
        // A busy refusal is neutral to the initiator. The interrupted person's patience can dip.
        if (b.intention == "work") {
            b.patience = bounded(b.patience - 0.02 * b.duty);
            emit("interruption", {{"actor", b.id}, {"patience", b.patience}});
        }
        return;
    }
    std::string first, reply;
    if (!speak(a, b, topic, "", first)) return;
    const bool replied = speak(b, a, topic, first, reply);
    if (!replied) return;
    for (auto* v : {&a, &b}) {
        const auto& other = v == &a ? b : a;
        const auto before = Json(*v);
        // No belief-based dislike: appraisal depends on context, energy and openness.
        const double appraisal = std::clamp(0.25 + 0.35 * v->openness + 0.20 * v->energy -
            (v->intention == "work" ? 0.65 * v->duty : 0) - 0.30 * v->anger, -1.0, 1.0);
        social_effect(*v, other.id, appraisal);
        emit("social_effect", {{"actor", v->id}, {"other", other.id}, {"appraisal", appraisal},
            {"before", before}, {"after", Json(*v)}});
    }
}
void Simulation::step() {
    // Persist each tick atomically. On failure restore the in-memory state too.
    World previous = world_;
    store_.sql("BEGIN IMMEDIATE");
    try {
        for (auto& v : world_.villagers) {
            const auto before = Json(v);
            advance_needs(v, world_);
            const std::string action = choose_action(v, world_);
            const std::string old_location = v.location;
            v.intention = action;
            if (action == "work") v.location = v.workplace;
            else if (action == "rest" || action == "family") v.location = v.home;
            else if (action == "walk" || action == "idle") v.location = "village_square";
            else if (action == "visit") {
                std::vector<std::string> open;
                for (const auto& owner : world_.villagers)
                    if (owner.id != "wayne" && owner.id != v.id && on_duty(owner, world_.hour())) open.push_back(owner.workplace);
                if (open.empty()) { v.intention = "idle"; v.location = "village_square"; }
                else {
                    v.location = open[world_.rng() % open.size()];
                    if (old_location != v.location) {
                        bool alcohol = world_.random() < v.drink_probability;
                        enjoy(v, world_.tick, alcohol);
                        emit("pub_visit", {{"actor", v.id}, {"location", v.location}, {"alcohol", alcohol}}, {v.id});
                    }
                }
            }
            if (action == "walk" && old_location != v.location) enjoy(v, world_.tick, false);
            emit("state", {{"actor", v.id}, {"action", v.intention}, {"before", before}, {"after", Json(v)}});
        }
        // Randomize initiative order. Reserve a participant for one encounter per tick.
        std::array<int, 3> order{0, 1, 2};
        for (int i = 2; i > 0; --i) std::swap(order[i], order[world_.rng() % (i + 1)]);
        std::set<std::string> occupied;
        for (int index : order) {
            auto& a = world_.villagers[index];
            if (occupied.contains(a.id) || a.intention == "work" || a.intention == "family" ||
                a.intention == "rest" || world_.tick - a.last_social < 2) continue;
            std::vector<int> nearby;
            for (int i : order) {
                const auto& b = world_.villagers[i];
                if (i != index && !occupied.contains(b.id) && a.location == b.location &&
                    b.intention != "rest" && b.intention != "family" && world_.tick - b.last_social >= 2) nearby.push_back(i);
            }
            if (nearby.empty()) continue;
            auto& b = world_.villagers[nearby[world_.rng() % nearby.size()]];
            double chance = 0.45 * willingness(a, b, false);
            if (world_.random() < chance) {
                std::string topic = "daily_life";
                double reality = a.beliefs.at("simulation") * 0.50;
                if (world_.random() < reality) topic = "reality";
                else {
                    const std::array<std::string, 4> ordinary{"work", "weather", "family", "parish_and_local_affairs"};
                    topic = ordinary[world_.rng() % ordinary.size()];
                }
                occupied.insert(a.id); occupied.insert(b.id);
                encounter(a.id, b.id, topic);
            } else {
                emit("silent_encounter", {{"actor", a.id}, {"other", b.id}, {"location", a.location}, {"initiation_chance", chance}});
                occupied.insert(a.id); occupied.insert(b.id);
            }
        }
        ++world_.tick;
        store_.checkpoint(world_);
        store_.sql("COMMIT");
    } catch (...) {
        store_.sql("ROLLBACK"); world_ = std::move(previous); throw;
    }
}
void Simulation::run(int steps) { for (int i = 0; i < steps; ++i) step(); }
Json measure(const Json& events) {
    Json report{{"event_counts", Json::object()}, {"by_actor", Json::object()},
        {"speech_count", 0}, {"exact_repeated_speech", 0}, {"dialogue_errors", 0}};
    std::map<std::string, std::set<std::string>> seen;
    for (const auto& e : events) {
        auto type = e.at("type").get<std::string>(); auto& count = report["event_counts"][type];
        count = count.is_null() ? 1 : count.get<int>() + 1;
        const auto& d = e.at("data");
        if (d.contains("actor")) {
            auto& n = report["by_actor"][d.at("actor").get<std::string>()][type];
            n = n.is_null() ? 1 : n.get<int>() + 1;
            if (type == "state") {
                auto& actor = report["by_actor"][d.at("actor").get<std::string>()];
                auto& actions = actor["actions"][d.at("action").get<std::string>()];
                actions = actions.is_null() ? 1 : actions.get<int>() + 1;
                if (!actor.contains("initial_beliefs")) actor["initial_beliefs"] = d.at("before").at("beliefs");
                actor["final_beliefs"] = d.at("after").at("beliefs");
            }
        }
        if (type == "speech") {
            report["speech_count"] = report["speech_count"].get<int>() + 1;
            auto& texts = seen[d.at("actor").get<std::string>()];
            if (!texts.insert(d.at("text").get<std::string>()).second)
                report["exact_repeated_speech"] = report["exact_repeated_speech"].get<int>() + 1;
        }
        if (type == "dialogue_error") report["dialogue_errors"] = report["dialogue_errors"].get<int>() + 1;
    }
    return report;
}
}
