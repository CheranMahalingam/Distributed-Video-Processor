#ifndef CONSENSUS_MODULE_H
#define CONSENSUS_MODULE_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <grpc++/grpc++.h>
#include <vector>
#include <string>
#include <utility>
#include <thread>
#include <memory>

#include "log.h"
#include "raft.grpc.pb.h"

namespace raft {

using grpc::CompletionQueue;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::Status;

class ConcensusModule {
public:
    enum class ElectionRole {
        Leader,
        Candidate,
        Follower,
        Dead
    };

    ConcensusModule(const int id, boost::asio::io_context& io_context, const std::vector<std::string>& peer_ids);

public:
    void AsyncRpcResponseHandler();

    void ElectionTimeout(const int term);

    int current_term() const;

private:
    void ElectionCallback(const int term);

    void StartElection();

    void Shutdown();

    void HeartbeatCallback();

    void PromoteToLeader();

    void ResetToFollower(const int term);

    void HeartbeatTimeout();

    void RequestVote(const std::string peer_id, const int term);

    void HandleRequestVoteResponse(rpc::RequestVoteResponse reply);

    void AppendEntries(const std::string peer_id, const int term);

    void HandleAppendEntriesResponse(rpc::AppendEntriesResponse reply);

private:
    struct AsyncClientCall {
        rpc::RequestVoteResponse reply;
        ClientContext ctx;
        Status status;
        std::unique_ptr<ClientAsyncResponseReader<rpc::RequestVoteResponse>> response_reader;
    };

    int id_;
    std::vector<std::string> peer_ids_;
    boost::asio::io_context& io_;
    boost::asio::steady_timer election_timer_;
    boost::asio::steady_timer heartbeat_timer_;
    std::atomic<int> current_term_;
    int vote_;
    int votes_received_;
    ElectionRole state_;
    std::unordered_map<std::string, std::unique_ptr<rpc::RaftService::Stub>> stubs_;
    CompletionQueue cq_;
};

}

#endif
