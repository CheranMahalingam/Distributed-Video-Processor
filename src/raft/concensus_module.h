#ifndef CONSENSUS_MODULE_H
#define CONSENSUS_MODULE_H

#include <boost/asio.hpp>
#include <grpc++/grpc++.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <tuple>
#include <random>

#include "raft_client.h"
#include "command_log.h"
#include "commit_channel.h"
#include "snapshot.h"
#include "log.h"
#include "raft.grpc.pb.h"

namespace raft {

using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::Status;
using grpc::CompletionQueue;

class ConcensusModule {
public:
    enum class ElectionRole {
        Leader,
        Candidate,
        Follower,
        Dead
    };

    ConcensusModule(
        boost::asio::io_context& io_context, 
        const std::string address,
        const std::vector<std::string>& peer_ids,
        CompletionQueue& cq);

    void ElectionTimeout(const int term);

    void ResetToFollower(const int term);

    void PromoteToLeader();

    void CommitEntry(const rpc::LogEntry& entry);

    void Submit(const std::string command);

    void PersistLogToStorage(const std::vector<rpc::LogEntry>& entries, bool append);

public:
    void set_vote(const std::string peer_id);

    void set_votes_received(const int votes);

    int current_term() const;

    ElectionRole state() const;

    std::string vote() const;

    int votes_received() const;

    std::vector<std::string> peer_ids() const;

private:
    void ElectionCallback(const int term);

    void StartElection();

    void Shutdown();

    void HeartbeatCallback();

    void HeartbeatTimeout();

    void RestoreFromStorage();

    std::string RandomString();

public:
    std::unique_ptr<CommitChannel> channel_;
    std::unique_ptr<CommandLog> log_;

private:
    std::string address_;
    std::vector<std::string> peer_ids_;
    std::unique_ptr<RaftClient> rpc_;
    std::unique_ptr<Snapshot> snapshot_;
    boost::asio::steady_timer election_timer_;
    boost::asio::steady_timer heartbeat_timer_;
    std::atomic<int> current_term_;
    std::string vote_;
    std::atomic<int> votes_received_;
    ElectionRole state_;
};

}

#endif
