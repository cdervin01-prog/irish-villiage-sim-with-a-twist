#include "village.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace village {
double bounded(double x) { return std::clamp(x, 0.0, 1.0); }
double World::random() { return static_cast<double>(rng()) / 4294967296.0; }
World load_world(const std::string& path, unsigned seed, const std::string& weather) {
    if (weather != "summer" && weather != "winter") throw std::runtime_error("Weather must be summer or winter");
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open character configuration: " + path);
    auto config = Json::parse(input);
    World w;
    w.rng.seed(seed);
    w.weather = weather;
    for (const auto& p : config.at("villagers")) {
        Villager v;
        v.id = p.at("id"); v.name = p.at("name"); v.occupation = p.at("occupation");
        v.profile = p.at("profile"); v.home = p.at("home"); v.workplace = p.at("workplace");
        v.location = v.home; v.work_hours = p.at("work_hours").get<std::vector<int>>();
        for (int h : v.work_hours) if (h < 0 || h > 23) throw std::runtime_error("Invalid work hour");
        const auto& t = p.at("traits");
        v.sociability = t.at("sociability"); v.openness = t.at("openness");
        v.duty = t.at("duty"); v.baseline_patience = t.at("patience");
        v.weather_sensitivity = t.at("weather_sensitivity"); v.drink_probability = t.at("drink_probability");
        for (double x : {v.sociability, v.openness, v.duty, v.baseline_patience,
                         v.weather_sensitivity, v.drink_probability})
            if (!std::isfinite(x) || x < 0 || x > 1) throw std::runtime_error("Invalid trait range");
        v.beliefs = p.at("beliefs").get<std::map<std::string, double>>();
        for (const auto& [key, value] : v.beliefs)
            if (!std::isfinite(value) || value < 0 || value > 1) throw std::runtime_error("Invalid belief range: " + key);
        v.patience = v.baseline_patience;
        w.villagers.push_back(v);
    }
    if (w.villagers.size() != 3 || w.villagers[0].id != "dennis" ||
        w.villagers[1].id != "gerrard" || w.villagers[2].id != "wayne")
        throw std::runtime_error("Prototype requires Dennis, Gerrard and Wayne, in stable ID order");
    for (auto& v : w.villagers)
        for (const auto& other : w.villagers)
            if (v.id != other.id) v.relationships[other.id] = {};
    return w;
}
bool on_duty(const Villager& v, int hour) {
    return std::find(v.work_hours.begin(), v.work_hours.end(), hour) != v.work_hours.end();
}
void advance_needs(Villager& v, const World& w) {
    const double weather = (w.weather == "summer" ? 1 : -1) * v.weather_sensitivity;
    double target = 0.65 + weather + v.enjoyment;
    if (w.tick >= v.hangover_start && w.tick < v.hangover_end) target = 0.40 + weather;
    v.mood = bounded(v.mood + 0.18 * (target - v.mood));
    v.enjoyment *= 0.78;
    v.patience = bounded(v.patience + 0.07 * (v.baseline_patience + weather - v.patience));
    v.anger *= 0.55; // Anger recovers faster than patience.
    v.energy = bounded(v.energy + (v.intention == "rest" ? 0.10 : -0.018));
    for (auto& [id, r] : v.relationships) { (void)id; r.transient *= 0.88; }
}
void enjoy(Villager& v, int tick, bool alcohol) {
    v.enjoyment = alcohol ? 0.15 : 0.10;
    v.mood = bounded(v.mood + (0.80 - v.mood) * (alcohol ? 0.65 : 0.4));
    if (alcohol) {
        v.hangover_start = ((tick / 48) + 1) * 48 + 16; // Next day, 08:00.
        v.hangover_end = v.hangover_start + 16; // Eight hours.
    }
}
double willingness(const Villager& v, const Villager& other, bool working) {
    const auto& r = v.relationships.at(other.id);
    return bounded(0.08 + 0.42 * v.sociability + 0.14 * v.openness +
        0.12 * v.mood + 0.12 * v.patience + 0.10 * v.energy +
        0.15 * r.trust + r.transient - 0.30 * v.anger - (working ? 0.40 * v.duty : 0));
}
void social_effect(Villager& v, const std::string& other, double appraisal) {
    auto& r = v.relationships.at(other);
    const double individual = 0.6 + 0.4 * v.openness;
    r.transient = std::clamp(r.transient + 0.10 * appraisal * individual, -0.3, 0.3);
    r.trust = std::clamp(r.trust + 0.05 * appraisal * individual, -1.0, 1.0);
    r.familiarity = bounded(r.familiarity + 0.02);
    v.mood = bounded(v.mood + 0.025 * appraisal * individual);
    v.patience = bounded(v.patience + 0.025 * appraisal * individual);
    if (appraisal < 0) v.anger = bounded(v.anger - 0.08 * appraisal);
}
std::string choose_action(Villager& v, World& w) {
    const int h = w.hour();
    if (h >= 23 || h < 7 || v.energy < 0.15) return "rest";
    if (on_duty(v, h)) return "work";
    std::vector<std::pair<std::string, double>> scores = {
        {"rest", 0.15 + (1 - v.energy) * 0.7},
        {"walk", 0.30 + v.mood * 0.15},
        {"family", 0.20 + v.duty * 0.20},
        {"visit", h >= 16 && h < 22 ? 0.28 + v.sociability * 0.25 : -10.0},
        {"idle", 0.20}
    };
    std::string best;
    double high = -100;
    for (const auto& [action, score] : scores) {
        double value = score + (w.random() - 0.5) * 0.6 + (action == v.intention ? 0.08 : 0);
        if (value > high) { high = value; best = action; }
    }
    return best;
}
}
