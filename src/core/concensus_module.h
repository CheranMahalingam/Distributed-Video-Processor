#ifndef CONSENSUS_MODULE_H
#define CONSENSUS_MODULE_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <grpc++/grpc++.h>
#include <vector>
#include <string>
#include <utility>
#include <tuple>

#include "node.h"
#include "logger.h"
#include "raft.grpc.pb.h"

namespace raft {

class ConcensusModule {
public:
    enum class ElectionRole {
        Leader,
        Candidate,
        Follower,
        Dead
    };

    ConcensusModule(const int id, boost::asio::io_context& io_context, const std::vector<std::string>& peer_ids, Logger& logger);

    void ElectionCallback(const int term);

    void StartElection();

    void Shutdown();

    std::tuple<int, bool> RequestVote(const std::string peer_id, const int term);

    std::tuple<int, bool> AppendEntries(const std::string peer_id, const int term);

    void HeartbeatCallback();

    void PromoteToLeader();

    void ResetToFollower(const int term);

    void ElectionTimeout(const int term);

    void HeartbeatTimeout();

    template<typename ...Args>
    void Log(Args&&... args);

private:
    int id_;
    std::vector<std::string> peer_ids_;
    int current_term_;
    int vote_;
    std::vector<int> log_;
    ElectionRole state_;
    boost::asio::io_context& io_;
    boost::asio::steady_timer election_timer_;
    boost::asio::steady_timer heartbeat_timer_;
    Logger& log_;
    std::unordered_map<std::string, std::unique_ptr<rpc::RaftService::Stub>> stubs_;
};

}

#endif
