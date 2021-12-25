#include "concensus_module.h"

namespace raft {

ConcensusModule::ConcensusModule(boost::asio::io_context& io_context, const std::string address, const std::vector<std::string>& peer_ids, std::unique_ptr<RpcClient> rpc)
    : address_(address), peer_ids_(peer_ids), election_timer_(io_context), heartbeat_timer_(io_context), rpc_(std::move(rpc)),
        current_term_(0), vote_(""), votes_received_(0), state_(ElectionRole::Follower) {
}

void ConcensusModule::ElectionCallback(const int term) {
    Log(LogLevel::Info) << "Election timer expired";

    if (state_ != ElectionRole::Candidate && state_ != ElectionRole::Follower) {
        Log(LogLevel::Info) << "State invalid for election";
        return;
    }

    if (current_term() != term) {
        Log(LogLevel::Info) << "Term changed from" << current_term() << "to" << term;
        return;
    }

    state_ = ElectionRole::Candidate;
    current_term_++;
    int saved_term = current_term();
    set_vote(address_);
    votes_received_ = 1;
    Log(LogLevel::Info) << "Becomes Candidate, term:" << saved_term;

    for (auto peer_id:peer_ids_) {
        Log(LogLevel::Info) << "Sending RequestVote call to" << peer_id;
        rpc_->RequestVote(peer_id, saved_term);
    }

    ElectionTimeout(current_term());
}

void ConcensusModule::HeartbeatCallback() {
    if (state_ != ElectionRole::Leader) {
        Log(LogLevel::Info) << "Invalid state for sending heartbeat";
        return;
    }

    int saved_term = current_term();

    for (auto peer_id:peer_ids_) {
        Log(LogLevel::Info) << "Sending AppendEntries call to" << peer_id;
        rpc_->AppendEntries(peer_id, saved_term);
    }

    HeartbeatTimeout();
}

void ConcensusModule::Shutdown() {
    state_ = ElectionRole::Dead;
    Log(LogLevel::Info) << "Server shutdown";
}

void ConcensusModule::PromoteToLeader() {
    state_ = ElectionRole::Leader;
    votes_received_ = 0;
    Log(LogLevel::Info) << "Becoming leader, term:" << current_term();

    HeartbeatTimeout();
}

void ConcensusModule::ResetToFollower(const int term) {
    state_ = ElectionRole::Follower;
    current_term_.store(term);
    set_vote("");
    votes_received_ = 0;
    Log(LogLevel::Info) << "Becoming follower, term:" << current_term();

    ElectionTimeout(term);
}

void ConcensusModule::ElectionTimeout(const int term) {
    int random_timeout = std::rand() % 151 + 150;
    Log(LogLevel::Info) << "Election timer created:" << random_timeout << "ms";
    election_timer_.expires_after(std::chrono::milliseconds(random_timeout));
    election_timer_.async_wait([this, term](const boost::system::error_code& err) {
        if (!err) {
            ElectionCallback(term);
        } else {
            Log(LogLevel::Error) << "Election timer cancelled with error:" << err.message();
        }
    });
}

void ConcensusModule::HeartbeatTimeout() {
    Log(LogLevel::Info) << "New heartbeat timer created";
    heartbeat_timer_.expires_after(std::chrono::milliseconds(50));
    heartbeat_timer_.async_wait([this](const boost::system::error_code& err) {
        if (!err) {
            HeartbeatCallback();
        } else {
            Log(LogLevel::Error) << "Heartbeat timer cancelled with error:" << err.message();
        }
    });
}

void ConcensusModule::set_vote(const std::string vote) {
    vote_ = vote;
}

void ConcensusModule::set_votes_received(const int votes) {
    votes_received_.store(votes);
}

int ConcensusModule::current_term() const {
    return current_term_.load();
}

ConcensusModule::ElectionRole ConcensusModule::state() const {
    return state_;
}

std::string ConcensusModule::vote() const {
    return vote_;
}

int ConcensusModule::votes_received() const {
    return votes_received_.load();
}

std::vector<std::string> ConcensusModule::peer_ids() const {
    return peer_ids_;
}

}
