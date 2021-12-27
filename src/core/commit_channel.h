#ifndef COMMIT_CHANNEL_H
#define COMMIT_CHANNEL_H

#include <condition_variable>
#include <queue>
#include <mutex>

#include "log.h"
#include "raft.grpc.pb.h"

namespace raft {

class CommitChannel {
public:
    CommitChannel();

    void ConsumeEvents();

private:
    void ApplyCommit(const rpc::LogEntry& commit);

public:
    std::queue<rpc::LogEntry> commit_queue_;
    std::condition_variable commit_notifier_;
    std::mutex queue_mutex_;
};

}

#endif
