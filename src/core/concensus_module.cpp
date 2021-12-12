#include <boost/bind.hpp>

#include "concensus_module.h"
#include "logger.h"

namespace raft {

ConcensusModule::ConcensusModule(boost::asio::io_context& io_context, int id, std::vector<int> peer_ids)
    : io_(io_context), election_timer_(io_), heartbeat_timer_(io_), current_term_(0),
        state_(ElectionRole::Follower), vote_(-1), id_(id), peer_ids_(peer_ids), log_(Logger()) {
            ElectionTimeout(0);
}

void ConcensusModule::ElectionCallback(int term) {
    Log(id_, "election timer expired");

    if (state_ != ElectionRole::Candidate && state_ != ElectionRole::Follower) {
        Log(id_, "state invalid for election");
        return;
    }

    if (current_term_ != term) {
        Log(id_, "term changed from", current_term_, "to", term);
        return;
    }

    StartElection();
}

void ConcensusModule::StartElection() {
    state_ = ElectionRole::Candidate;
    current_term_++;
    int saved_term = current_term_;
    vote_ = id_;
    Log(id_, "becomes Candidate, term:", saved_term);

    int votes_received = 1;

    for (auto peer_id:peer_ids_) {
        Log(id_, "sending RequestVote call to", peer_id);
        // Make request vote call and get reply
        int reply_term = current_term_;
        bool reply_vote_granted = true;
        Log(id_, "received RequestVote reply from", peer_id);

        // State changed when making calls
        if (state_ != ElectionRole::Candidate) {
            Log(id_, "changed state while waiting for reply");
            return;
        }

        // Another server became the leader
        if (reply_term > saved_term) {
            Log(id_, "term out of date, changed from", saved_term, "to", reply_term);
            ResetToFollower(reply_term);
            return;
        } else if (reply_term == saved_term) {
            if (reply_vote_granted) {
                votes_received++;

                if (votes_received > peer_ids_.size()/2) {
                    Log(id_, "wins election with", votes_received, "votes");
                    PromoteToLeader();
                    return;
                }
            }
        }
    }

    Log(id_, "election was unsuccessful, restarting...");
    ElectionTimeout(current_term_);
}

void ConcensusModule::Shutdown() {
    state_ = ElectionRole::Dead;
    Log(id_, "has shutdown");
}

void ConcensusModule::HeartbeatCallback() {
    if (state_ != ElectionRole::Leader) {
        Log(id_, "invalid state for sending heartbeat");
        return;
    }

    int saved_term = current_term_;

    for (auto peer_id:peer_ids_) {
        Log(id_, "sending AppendEntries call to", peer_id);
        // Make request append entries call and get reply
        int reply_term = current_term_;
        Log(id_, "received AppendEntries reply from", peer_id);
    
        if (reply_term > saved_term) {
            Log(id_, "term out of date in heartbeat reply, changed from", saved_term, "to", reply_term);
            ResetToFollower(reply_term);
            return;
        }
    }

    HeartbeatTimeout();
}

void ConcensusModule::PromoteToLeader() {
    state_ = ElectionRole::Leader;
    Log(id_, "becomes leader, term:", current_term_);

    HeartbeatTimeout();
}

void ConcensusModule::ResetToFollower(int term) {
    state_ = ElectionRole::Follower;
    current_term_ = term;
    vote_ = -1;
    Log(id_, "becomes follower, term:", current_term_);

    ElectionTimeout(term);
}

void ConcensusModule::ElectionTimeout(int term) {
    int random_timeout = std::rand() % 151 + 150;
    Log(id_, "new election timer created", random_timeout);
    election_timer_.expires_from_now(std::chrono::milliseconds(random_timeout));
    election_timer_.async_wait(boost::bind(ElectionCallback, term));
}

void ConcensusModule::HeartbeatTimeout() {
    Log(id_, "new heartbeat timer created");
    heartbeat_timer_.expires_from_now(std::chrono::milliseconds(50));
    heartbeat_timer_.async_wait(HeartbeatCallback);
}

template<typename ...Args>
void Log(Args&&... args) {
    ((log_ << std::forward<Args>(args) << " "), ...)
}

}
