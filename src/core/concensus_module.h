#ifndef CONSENSUS_MODULE_H
#define CONSENSUS_MODULE_H

#include <boost/asio.hpp>
#include <vector>

namespace raft {

class ConcensusModule {
public:
    enum class ElectionRole {
        Leader,
        Candidate,
        Follower,
        Dead
    };

    ConcensusModule(boost::asio::io_context& io_context);

    ~ConcensusModule();

    void RunElectionTimer();

    void StartElection();

    void Stop();

    void RequestVote();

    void AppendEntries();

    void SendHeartbeat();

    void PromoteToLeader();

    void ResetToFollower(int term);

private:
    int id_;
    std::vector<int> peer_ids_;
    int current_term_;
    int vote_;
    std::vector<int> log_;
    ElectionRole state_;
    boost::asio::io_context& io_;
    boost::asio::steady_timer election_timer_;
};

}

#endif
