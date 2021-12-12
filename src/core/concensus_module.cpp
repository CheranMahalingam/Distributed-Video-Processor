#include <boost/bind.hpp>

#include "concensus_module.h"

namespace raft {

ConcensusModule::ConcensusModule(boost::asio::io_context& io_context, int id, std::vector<int> peer_ids)
    : io_(io_context), election_timer_(io_), heartbeat_timer_(io_), current_term_(0),
        state_(ElectionRole::Follower), vote_(-1), id_(id), peer_ids_(peer_ids) {
            ElectionTimeout(0);
}

void ConcensusModule::ElectionCallback(int term) {
    if (state_ != ElectionRole::Candidate && state_ != ElectionRole::Follower) {
        return;
    }

    if (current_term_ != term) {
        return;
    }

    StartElection();
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
    ElectionTimeout(current_term_);
}

void ConcensusModule::HeartbeatCallback() {
    if (state_ != ElectionRole::Leader) {
        return;
    }

    int saved_term = current_term_;

    for (auto peer_id:peer_ids_) {
        // Make request append entries call and get reply
        int reply_term = current_term_;
    
        if (reply_term > saved_term) {
            ResetToFollower(reply_term);
            return;
        }
    }

    HeartbeatTimeout();
}

void ConcensusModule::PromoteToLeader() {
    state_ = ElectionRole::Leader;

    HeartbeatTimeout();
}

void ConcensusModule::ResetToFollower(int term) {
    state_ = ElectionRole::Follower;
    current_term_ = term;
    vote_ = -1;

    ElectionTimeout(term);
}

void ConcensusModule::ElectionTimeout(int term) {
    int random_timeout = std::rand() % 151 + 150;
    election_timer_.expires_from_now(std::chrono::milliseconds(random_timeout));
    election_timer_.async_wait(boost::bind(ElectionCallback, term));
}

void ConcensusModule::HeartbeatTimeout() {
    heartbeat_timer_.expires_from_now(std::chrono::milliseconds(50));
    heartbeat_timer_.async_wait(HeartbeatCallback);
}

}
