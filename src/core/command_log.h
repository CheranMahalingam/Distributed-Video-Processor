#ifndef COMMAND_LOG_H
#define COMMAND_LOG_H

#include <string>
#include <vector>
#include <unordered_map>

#include "raft.grpc.pb.h"

namespace raft {

class CommandLog {
public:
    CommandLog(const std::vector<std::string>& peer_ids);

    void AppendLog(int idx, const std::vector<rpc::LogEntry>& new_entries);

    int LastLogIndex();

    int LastLogTerm();

public:

    void set_next_index(const std::string peer_id, const int new_index);

    void set_match_index(const std::string peer_id, const int new_index);

    void set_commit_index(const int idx);

    std::vector<rpc::LogEntry> entries() const;

    std::vector<rpc::LogEntry> entries(const int begin) const;

    int next_index(const std::string peer_id);

    int match_index(const std::string peer_id);

    int commit_index() const;

private:
    std::vector<rpc::LogEntry> entries_;
    int commit_index_;
    int last_applied_;
    std::unordered_map<std::string, int> next_index_;
    std::unordered_map<std::string, int> match_index_;
};

}

#endif
