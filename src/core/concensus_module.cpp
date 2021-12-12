#include <boost/bind.hpp>

#include "concensus_module.h"

namespace raft {

ConcensusModule::ConcensusModule(boost::asio::io_context& io_context, int id, std::vector<int> peer_ids)
    : io_(io_context), election_timer_(io_), heartbeat_timer_(io_), current_term_(0),
        state_(ElectionRole::Follower), vote_(-1), id_(id), peer_ids_(peer_ids) {
            RunElectionTimer(0);
}

void ConcensusModule::RunElectionTimer(int term) {
    int timeout_duration = ElectionTimeout();
    election_timer_.expires_from_now(std::chrono::milliseconds(timeout_duration));

    if (state_ != ElectionRole::Candidate && state_ != ElectionRole::Follower) {
        return;
    }

    if (current_term_ != term) {
        return;
    }

    StartElection();

    //election_timer_.async_wait(boost::bind(RunElectionTimer, current_term_));
}

void ConcensusModule::StartElection() {
    state_ = ElectionRole::Candidate;
    current_term_++;
    int saved_term = current_term_;
    vote_ = id_;

    int votes_received = 1;

    for (auto peer_id:peer_ids_) {
        // Make request vote call and get reply
        int reply_term = current_term_;
        bool reply_vote_granted = true;

        // State changed when making calls
        if (state_ != ElectionRole::Candidate) {
            return;
        }

        // Another server became the leader
        if (reply_term > saved_term) {
            ResetToFollower(reply_term);
            return;
        } else if (reply_term == saved_term) {
            if (reply_vote_granted) {
                votes_received++;

                if (votes_received > peer_ids_.size()/2) {
                    PromoteToLeader();
                    return;
                }
            }
        }
    }

    // If election was unsuccessful restart election process
    election_timer_.async_wait(boost::bind(RunElectionTimer, current_term_));
}

void ConcensusModule::SendHeartbeat() {
    int saved_term = current_term_;

    for (auto peer_id:peer_ids_) {
        // Make requesst append entries call and get reply
        int reply_term = current_term_;
    
        if (reply_term > saved_term) {
            ResetToFollower(reply_term);
            return;
        }
    }

    heartbeat_timer_.async_wait(SendHeartbeat);
}

void ConcensusModule::PromoteToLeader() {
    state_ = ElectionRole::Leader;

    heartbeat_timer_.async_wait(SendHeartbeat);
}

void ConcensusModule::ResetToFollower(int term) {
    state_ = ElectionRole::Follower;
    current_term_ = term;
    vote_ = -1;
    election_timer_.async_wait(boost::bind(RunElectionTimer, current_term_));
}

int ConcensusModule::ElectionTimeout() {
    return std::rand() % 151 + 150;
}

}
