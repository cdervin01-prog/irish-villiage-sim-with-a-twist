#include "village.hpp"
#include <curl/curl.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace village {
Utterance validate_utterance(const Json& result, bool may_reflect) {
    if (!result.is_object() || result.size() != 2 || !result.contains("speech") ||
        !result.contains("belief_delta") || !result.at("speech").is_string() ||
        !result.at("belief_delta").is_number()) throw std::runtime_error("Invalid dialogue schema");
    Utterance u{result.at("speech"), result.at("belief_delta")};
    if (u.speech.empty() || u.speech.size() > 360 || u.speech.find_first_of("\n\r") != std::string::npos ||
        !std::isfinite(u.belief_delta) || std::abs(u.belief_delta) > 0.02 ||
        (!may_reflect && u.belief_delta != 0)) throw std::runtime_error("Dialogue outside allowed bounds");
    for (unsigned char c : u.speech) if (c < 32) throw std::runtime_error("Control character in dialogue");
    return u;
}
Json dialogue_context(const Villager& speaker, const std::string& listener_name,
                     const std::string& topic, const std::string& heard, const World& w,
                     const MemoryStore& store) {
    // Never send the world, another mind, or the objective event ledger to the model.
    return {{"speaker", speaker.name}, {"speaker_id", speaker.id}, {"profile", speaker.profile},
        {"occupation", speaker.occupation}, {"mood", speaker.mood}, {"patience", speaker.patience},
        {"energy", speaker.energy}, {"anger", speaker.anger}, {"own_beliefs", speaker.beliefs},
        {"own_relationships", speaker.relationships}, {"intention", speaker.intention},
        {"listener", listener_name}, {"location", speaker.location}, {"hour", w.hour()}, {"tick", w.tick},
        {"weather", w.weather}, {"topic", topic}, {"heard_now", heard},
        {"own_memories", store.recall(speaker.id)},
        {"may_reflect", topic == "reality" && !heard.empty()}};
}
Json MockDialogue::generate(const Json& c, unsigned seed) {
    // Intentionally artificial fixtures. Mock output is not research evidence about language.
    const std::array<std::string, 5> topics = {"the weather", "the day's work", "a quiet walk", "family duties", "an ordinary meal"};
    std::string speech = "[MOCK] " + c.at("speaker").get<std::string>() + " mentions " + topics[seed % topics.size()] + ".";
    if (c.at("topic") == "reality") speech += " I am uncertain what explains our world.";
    speech += " (fixture tick " + std::to_string(c.at("tick").get<int>()) + ")";
    return {{"speech", speech}, {"belief_delta", 0.0}};
}
namespace {
size_t receive(char* data, size_t size, size_t count, void* target) {
    size_t bytes = size * count;
    auto& text = *static_cast<std::string*>(target);
    if (bytes > 1024 * 1024 || text.size() > 1024 * 1024 - bytes) return 0;
    text.append(data, bytes); return bytes;
}
}
LlamaDialogue::LlamaDialogue(std::string endpoint, const std::string& path) : endpoint_(std::move(endpoint)) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open versioned model prompt: " + path);
    std::ostringstream text; text << in.rdbuf(); prompt_ = text.str();
    static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw std::runtime_error("curl initialization failed");
    if (endpoint_.rfind("http://", 0) != 0 && endpoint_.rfind("https://", 0) != 0)
        throw std::runtime_error("Endpoint must use HTTP or HTTPS");
}
Json LlamaDialogue::generate(const Json& context, unsigned seed) {
    Json schema = {{"type", "object"}, {"additionalProperties", false},
        {"properties", {{"speech", {{"type", "string"}, {"minLength", 1}, {"maxLength", 360}}},
                        {"belief_delta", {{"type", "number"}, {"minimum", -0.02}, {"maximum", 0.02}}}}},
        {"required", {"speech", "belief_delta"}}};
    Json request = {{"messages", {{{"role", "system"}, {"content", prompt_}},
                                  {{"role", "user"}, {"content", context.dump()}}}},
        {"temperature", 0.7}, {"seed", seed}, {"max_tokens", 180},
        {"chat_template_kwargs", {{"enable_thinking", false}}},
        {"response_format", {{"type", "json_schema"}, {"json_schema", {{"name", "utterance"}, {"strict", true}, {"schema", schema}}}}}};
    std::string body = request.dump(), response;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) throw std::runtime_error("curl handle allocation failed");
    auto* raw_headers = curl_slist_append(nullptr, "Content-Type: application/json");
    if (!raw_headers) throw std::runtime_error("curl header allocation failed");
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(raw_headers, curl_slist_free_all);
    curl_easy_setopt(curl.get(), CURLOPT_URL, endpoint_.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 90L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    auto rc = curl_easy_perform(curl.get());
    if (rc != CURLE_OK) throw std::runtime_error(std::string("llama.cpp transport: ") + curl_easy_strerror(rc));
    long status = 0; curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) throw std::runtime_error("llama.cpp HTTP " + std::to_string(status));
    auto envelope = Json::parse(response);
    const auto& choice = envelope.at("choices").at(0);
    if (choice.value("finish_reason", "") != "stop") throw std::runtime_error("Incomplete model response");
    return Json::parse(choice.at("message").at("content").get<std::string>());
}
}
