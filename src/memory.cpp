#include "village.hpp"
#include <sstream>
#include <stdexcept>

namespace village {
namespace {
struct Statement {
    sqlite3_stmt* p = nullptr;
    explicit Statement(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &p, nullptr) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    ~Statement() { sqlite3_finalize(p); }
    void text(int i, const std::string& s) { sqlite3_bind_text(p, i, s.c_str(), -1, SQLITE_TRANSIENT); }
    void done() {
        if (sqlite3_step(p) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(p)));
    }
};
}
MemoryStore::MemoryStore(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string error = db_ ? sqlite3_errmsg(db_) : "SQLite allocation failed";
        sqlite3_close(db_); db_ = nullptr; throw std::runtime_error(error);
    }
    try {
        {
            Statement version(db_, "PRAGMA user_version");
            if (sqlite3_step(version.p) != SQLITE_ROW || sqlite3_column_int(version.p, 0) > 1)
                throw std::runtime_error("Unsupported SQLite schema version");
        }
        sql("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA user_version=1;"
            "CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY,tick INTEGER NOT NULL,type TEXT NOT NULL,data TEXT NOT NULL);"
            "CREATE TABLE IF NOT EXISTS observations(id INTEGER PRIMARY KEY,owner TEXT NOT NULL,event_id INTEGER NOT NULL REFERENCES events(id),content TEXT NOT NULL);"
            "CREATE TABLE IF NOT EXISTS checkpoint(id INTEGER PRIMARY KEY CHECK(id=1),state TEXT NOT NULL);"
            "CREATE TABLE IF NOT EXISTS relationships(owner TEXT,target TEXT,data TEXT NOT NULL,PRIMARY KEY(owner,target));"
            "CREATE INDEX IF NOT EXISTS observations_owner ON observations(owner,id);");
    } catch (...) { sqlite3_close(db_); db_ = nullptr; throw; }
}
MemoryStore::~MemoryStore() { sqlite3_close(db_); }
void MemoryStore::sql(const std::string& statement) {
    char* error = nullptr;
    if (sqlite3_exec(db_, statement.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error ? error : "SQLite error";
        sqlite3_free(error); throw std::runtime_error(message);
    }
}
void MemoryStore::event(int tick, const std::string& type, const Json& data,
                        const std::vector<std::string>& witnesses) {
    Statement s(db_, "INSERT INTO events(tick,type,data) VALUES(?,?,?)");
    sqlite3_bind_int(s.p, 1, tick); s.text(2, type); s.text(3, data.dump()); s.done();
    auto id = sqlite3_last_insert_rowid(db_);
    for (const auto& owner : witnesses) {
        Statement obs(db_, "INSERT INTO observations(owner,event_id,content) VALUES(?,?,?)");
        obs.text(1, owner); sqlite3_bind_int64(obs.p, 2, id);
        obs.text(3, Json{{"kind", "witnessed"}, {"tick", tick}, {"type", type}, {"content", data}}.dump()); obs.done();
    }
}
Json MemoryStore::recall(const std::string& id, int limit) const {
    Statement s(db_, "SELECT id,content FROM observations WHERE owner=? ORDER BY id DESC LIMIT ?");
    s.text(1, id); sqlite3_bind_int(s.p, 2, limit);
    Json result = Json::array();
    int status;
    while ((status = sqlite3_step(s.p)) == SQLITE_ROW) {
        auto item = Json::parse(reinterpret_cast<const char*>(sqlite3_column_text(s.p, 1)));
        item["memory_id"] = sqlite3_column_int64(s.p, 0); result.push_back(item);
    }
    if (status != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
    return result;
}
void MemoryStore::checkpoint(const World& w) {
    std::ostringstream rng; rng << w.rng;
    Json state{{"schema", 1}, {"tick", w.tick}, {"weather", w.weather},
               {"villagers", w.villagers}, {"rng", rng.str()}, {"dialogue_calls", w.dialogue_calls},
               {"provider", w.provider}};
    Statement s(db_, "INSERT INTO checkpoint VALUES(1,?) ON CONFLICT(id) DO UPDATE SET state=excluded.state");
    s.text(1, state.dump()); s.done();
    for (const auto& v : w.villagers) for (const auto& [target, r] : v.relationships) {
        Statement rel(db_, "INSERT INTO relationships VALUES(?,?,?) ON CONFLICT(owner,target) DO UPDATE SET data=excluded.data");
        rel.text(1, v.id); rel.text(2, target); rel.text(3, Json(r).dump()); rel.done();
    }
}
bool MemoryStore::restore(World& w) {
    Statement s(db_, "SELECT state FROM checkpoint WHERE id=1");
    int rc = sqlite3_step(s.p);
    if (rc == SQLITE_DONE) return false;
    if (rc != SQLITE_ROW) throw std::runtime_error(sqlite3_errmsg(db_));
    auto j = Json::parse(reinterpret_cast<const char*>(sqlite3_column_text(s.p, 0)));
    if (j.at("schema") != 1) throw std::runtime_error("Unsupported checkpoint schema");
    w.tick = j.at("tick"); w.weather = j.at("weather"); w.villagers = j.at("villagers").get<std::vector<Villager>>();
    w.dialogue_calls = j.at("dialogue_calls");
    w.provider = j.value("provider", "");
    std::istringstream rng(j.at("rng").get<std::string>());
    if (!(rng >> w.rng)) throw std::runtime_error("Invalid saved RNG state");
    return true;
}
Json MemoryStore::events() const {
    Statement s(db_, "SELECT id,tick,type,data FROM events ORDER BY id");
    Json rows = Json::array(); int status;
    while ((status = sqlite3_step(s.p)) == SQLITE_ROW) rows.push_back({
        {"id", sqlite3_column_int64(s.p, 0)}, {"tick", sqlite3_column_int(s.p, 1)},
        {"type", reinterpret_cast<const char*>(sqlite3_column_text(s.p, 2))},
        {"data", Json::parse(reinterpret_cast<const char*>(sqlite3_column_text(s.p, 3)))}});
    if (status != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
    return rows;
}
int MemoryStore::count(const std::string& table) const {
    if (table != "observations" && table != "events" && table != "relationships") throw std::runtime_error("Invalid count table");
    Statement s(db_, ("SELECT count(*) FROM " + table).c_str());
    if (sqlite3_step(s.p) != SQLITE_ROW) throw std::runtime_error(sqlite3_errmsg(db_));
    return sqlite3_column_int(s.p, 0);
}
}
