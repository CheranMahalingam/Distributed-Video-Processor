#ifndef CONSENSUS_MODULE_H
#define CONSENSUS_MODULE_H

#include <boost/asio.hpp>
#include <vector>

#include "server.h"
#include "raft.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ClientContext;
using grpc::Channel;

using rpc::RaftService;
using rpc::RequestVoteRequest;
using rpc::RequestVoteResponse;
using rpc::AppendEntriesRequest;
using rpc::AppendEntriesResponse;

namespace raft {

class ConcensusModule {
public:
    enum class ElectionRole {
        Leader,
        Candidate,
        Follower,
        Dead
    };

    ConcensusModule(boost::asio::io_context& io_context, int id, std::vector<int> peer_ids);

    void ElectionCallback(int term);

    void StartElection();

    void Stop();

    void RequestVote(const rpc::RequestVoteRequest& args, rpc::RequestVoteResponse* reply);

    void AppendEntries();

    void HeartbeatCallback();

    void PromoteToLeader();

    void ResetToFollower(int term);

    void ElectionTimeout(int term);

    void HeartbeatTimeout();

    void Log();

private:
    int id_;
    std::vector<int> peer_ids_;
    int current_term_;
    int vote_;
    std::vector<int> log_;
    ElectionRole state_;
    boost::asio::io_context& io_;
    boost::asio::steady_timer election_timer_;
    boost::asio::steady_timer heartbeat_timer_;
};

}

#endif
