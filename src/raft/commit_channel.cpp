#include "commit_channel.h"

namespace raft {

CommitChannel::CommitChannel() {
}

void CommitChannel::ConsumeEvents() {
    while (true) {
        rpc::LogEntry commit;

        std::unique_lock<std::mutex> guard(queue_mutex_);

        // pub/sub queue waits for new work
        commit_notifier_.wait(guard, [&]{ return !commit_queue_.empty(); });

        commit = commit_queue_.front();
        commit_queue_.pop();

        guard.unlock();

        ApplyCommit(commit);
    }
}

void CommitChannel::ApplyCommit(const rpc::LogEntry& commit) {
    // logger(LogLevel::Info) << "Applying commit, term =" << commit.term() << "command =" << commit.command();
}

}
