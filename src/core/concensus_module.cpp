#include "concensus_module.h"

namespace raft {

ConcensusModule::ConcensusModule(
    boost::asio::io_context& io_context, 
    const std::string address, 
    const std::vector<std::string>& peer_ids,
    std::unique_ptr<RpcClient> rpc, 
    std::unique_ptr<CommandLog> log, 
    std::unique_ptr<CommitChannel> channel)
    : address_(address), 
      peer_ids_(peer_ids), 
      election_timer_(io_context), 
      heartbeat_timer_(io_context), 
      rpc_(std::move(rpc)),
      log_(std::move(log)),
      current_term_(0), 
      vote_(""), 
      votes_received_(0), 
      state_(ElectionRole::Follower), 
      channel_(std::move(channel)) {
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

        rpc::RequestVoteRequest args;
        args.set_term(saved_term);
        args.set_candidateid(address_);
        args.set_lastlogindex(log_->LastLogIndex());
        args.set_lastlogterm(log_->LastLogTerm());
        rpc_->RequestVote(peer_id, args);
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

        int next = log_->next_index(peer_id);
        int prev_log_index = next - 1;
        int prev_log_term = -1;
        if (prev_log_index >= 0) {
            prev_log_term = log_->entries()[prev_log_index].term();
        }

        rpc::AppendEntriesRequest args;
        args.set_term(saved_term);
        args.set_leaderid(address_);
        args.set_prevlogindex(prev_log_index);
        args.set_prevlogterm(prev_log_term);
        args.set_leadercommit(log_->commit_index());
        for (int i = next; i < log_->entries().size(); i++) {
            *args.add_entries() = log_->entries()[i];
        }
        rpc_->AppendEntries(peer_id, args);
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

void ConcensusModule::CommitEntry(const rpc::LogEntry& entry) {
    std::lock_guard<std::mutex> guard(channel_->queue_mutex_);
    channel_->commit_queue_.push(entry);
    channel_->commit_notifier_.notify_one();
}

void ConcensusModule::Submit(const std::string command) {
    rpc::LogEntry new_entry;
    new_entry.set_term(current_term());
    new_entry.set_command(command);
    log_->AppendLog(new_entry);
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

CommandLog& ConcensusModule::log() const {
    return *log_;
}

}
