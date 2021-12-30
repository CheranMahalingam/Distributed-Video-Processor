#include "command_log.h"

namespace raft {

CommandLog::CommandLog(const std::vector<std::string>& peer_ids)
    : commit_index_(-1), last_applied_(-1) {
    for (auto peer_id:peer_ids) {
        next_index_[peer_id] = 0;
        match_index_[peer_id] = -1;
    }
}

void CommandLog::AppendLog(const rpc::LogEntry& entry) {
    logger(LogLevel::Debug) << "Appending entry to log";
    entries_.push_back(entry);
}

void CommandLog::InsertLog(int idx, const std::vector<rpc::LogEntry>& new_entries) {
    logger(LogLevel::Debug) << "Inserting entries to log, index =" << idx;
    std::vector<rpc::LogEntry> updated_log(entries_.begin(), entries_.begin() + idx);
    updated_log.insert(updated_log.begin(), new_entries.begin(), new_entries.end());
    entries_ = updated_log;
}

int CommandLog::LastLogIndex() {
    if (entries_.size() > 0) {
        return entries_.size() - 1;
    } else {
        return -1;
    }
}

int CommandLog::LastLogTerm() {
    if (entries_.size() > 0) {
        return entries_[LastLogIndex()].term();
    } else {
        return -1;
    }
}

void CommandLog::set_entries(const std::vector<rpc::LogEntry>& entries) {
    entries_ = entries;
}

void CommandLog::set_next_index(const std::string peer_id, const int new_index) {
    next_index_[peer_id] = new_index;
}

void CommandLog::set_match_index(const std::string peer_id, const int new_index) {
    match_index_[peer_id] = new_index;
}

void CommandLog::set_commit_index(const int idx) {
    commit_index_ = idx;
}

void CommandLog::increment_last_applied() {
    last_applied_++;
}

std::vector<rpc::LogEntry> CommandLog::entries() const {
    return entries_;
}

std::vector<rpc::LogEntry> CommandLog::entries(const int begin) const {
    std::vector<rpc::LogEntry> new_entries(entries_.begin() + begin, entries_.end());
    return new_entries;
}

int CommandLog::next_index(const std::string peer_id) {
    return next_index_[peer_id];
}

int CommandLog::match_index(const std::string peer_id) {
    return match_index_[peer_id];
}

int CommandLog::commit_index() const {
    return commit_index_;
}

int CommandLog::last_applied() const {
    return last_applied_;
}

}
