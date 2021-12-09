#include "concensus_module.h"

namespace raft {

ConcensusModule::ConcensusModule(boost::asio::io_context& io_context)
    : io_(io_context), election_timer_(io_), current_term_(0), state_(ElectionRole::Follower) {}

void ConcensusModule::RunElectionTimer() {
    int random_timeout = std::rand() % 151 + 150;
    election_timer_.expires_from_now(std::chrono::milliseconds(random_timeout));
    
    if (state_ != ElectionRole::Candidate && state_ != ElectionRole::Follower) {
        return;
    }
    
    election_timer_.async_wait(RunElectionTimer);
}

void ConcensusModule::StartElection() {
    state_ = ElectionRole::Candidate;
    current_term_++;
    int saved_term = current_term_;
    vote_ = id_;

    int votes_received = 1;

    for (auto peer_id:peer_ids_) {
        if (state_ != ElectionRole::Candidate) {
            return;
        }

        if (current_term_ > saved_term) {
            ResetToFollower(current_term_);
            return;
        } else if (current_term_ == saved_term) {
            votes_received++;
            if (votes_received > 2*peer_ids_.size()) {
                PromoteToLeader();
                return;
            }
        }
    }

    election_timer_.async_wait(RunElectionTimer);
}

void ConcensusModule::SendHeartbeat() {
    int saved_term = current_term_;

    if (state_ != ElectionRole::Leader) {
        return;
    }

    for (auto peer_id:peer_ids_) {
        if (current_term_ > saved_term) {
            ResetToFollower(current_term_);
            return;
        }
    }

    election_timer_.async_wait(SendHeartbeat);
}

void ConcensusModule::PromoteToLeader() {
    state_ = ElectionRole::Leader;

    election_timer_.async_wait(SendHeartbeat);
}

void ConcensusModule::ResetToFollower(int term) {
    state_ = ElectionRole::Follower;
    current_term_ = term;
    vote_ = -1;
    election_timer_.async_wait(RunElectionTimer);
}

}
