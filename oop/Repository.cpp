#include "Repository.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "MiniGitError.hpp"

std::vector<std::string> CommitView::parentIds() const {
    std::vector<std::string> parents;
    for (std::size_t i = 0; i < commit_->parent_count; ++i) parents.emplace_back(commit_->parent_ids[i]);
    return parents;
}

Repository::Repository() {
    throwIfError(mg_graph_init(&graph_), "Repository");
    std::random_device rd;
    std::seed_seq seed{rd(), rd(),
                       static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())};
    rng_.seed(seed);
}

Repository::~Repository() { mg_graph_destroy(&graph_); }

void Repository::init() {
    mg_graph_clear(&graph_);
    head_.clear();
}

std::string Repository::generateHash() {
    static const char kChars[] = "0123456789abcdef";
    std::uniform_int_distribution<int> pick(0, 15);
    std::string hash;
    do {                                   // retry in the (rare) case of a collision
        hash.clear();
        for (int i = 0; i < 8; ++i) hash += kChars[pick(rng_)];
    } while (hasCommit(hash));
    return hash;
}

std::string Repository::getCurrentTime() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::ostringstream text;
    text << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S");
    return text.str();
}

std::string Repository::createCommit(const Patch& deltas, const std::string& parentHash, const std::string& message) {
    const std::string id = generateHash();
    const std::string time = getCurrentTime();

    const char* parents[1] = {parentHash.c_str()};
    const std::size_t parentCount = parentHash.empty() ? 0 : 1;

    throwIfError(mg_graph_add_commit(&graph_, id.c_str(), parentCount ? parents : nullptr, parentCount,
                                     message.c_str(), time.c_str(), &deltas.raw()),
                 "createCommit (parent '" + parentHash + "')");
    head_ = id;
    return id;
}

CommitView Repository::getCommit(const std::string& hash) const {
    const mg_commit* commit = mg_graph_get(&graph_, hash.c_str());
    if (!commit) throw MiniGitError(MG_ERR_NOT_FOUND, "commit '" + hash + "'");
    return CommitView(commit);
}

bool Repository::hasCommit(const std::string& hash) const {
    return mg_graph_get(&graph_, hash.c_str()) != nullptr;
}

Dataset Repository::checkout(const std::string& hash) const {
    Dataset rows;
    throwIfError(mg_graph_rebuild(&graph_, hash.c_str(), &rows.raw()), "checkout '" + hash + "'");
    return rows;
}

std::size_t Repository::commitCount() const { return mg_graph_count(&graph_); }

void Repository::serializeToDisk() {}
void Repository::deserializeFromDisk() {}
