#include "concensus_module.h"

namespace raft {

ConcensusModule::ConcensusModule(
    boost::asio::io_context& io_context, 
    const std::string address, 
    const std::vector<std::string>& peer_ids,
    CompletionQueue& cq,
    std::shared_ptr<file_system::ChunkManager> manager)
    : address_(address),
      peer_ids_(peer_ids),
      election_timer_(io_context),
      heartbeat_timer_(io_context),
      current_term_(0),
      state_(ElectionRole::Follower),
      vote_(""),
      votes_received_(0) {
    channel_ = std::make_unique<raft::CommitChannel>(manager);
    snapshot_ = std::make_unique<raft::Snapshot>("../raft_store/" + address + "/");
    rpc_ = std::make_unique<raft::RaftClient>(address, peer_ids, cq);
    log_ = std::make_unique<raft::CommandLog>(peer_ids);

    // After the node crashes, restore the state and log from disk
    if (!snapshot_->Empty()) {
        RestoreFromStorage();
    }
}

void ConcensusModule::ElectionCallback(const int term) {
    logger(LogLevel::Debug) << "Election timer expired";

    if (state_ != ElectionRole::Candidate && state_ != ElectionRole::Follower) {
        logger(LogLevel::Debug) << "State invalid for election";
        return;
    }

    if (current_term() != term) {
        logger(LogLevel::Debug) << "Term changed from" << term << "to" << current_term();
        return;
    }

    state_ = ElectionRole::Candidate;
    current_term_++;
    // Reference when handling responses to verify that term is not out of date
    int saved_term = current_term();
    set_vote(address_);
    votes_received_ = 1;
    logger(LogLevel::Debug) << "Becomes Candidate, term:" << saved_term;

    snapshot_->PersistState(current_term_, vote_);

    for (auto peer_id:peer_ids_) {
        logger(LogLevel::Debug) << "Sending RequestVote call to" << peer_id;

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
        logger(LogLevel::Debug) << "Invalid state for sending heartbeat";
        return;
    }

    // Reference when handling responses to verify that term is not out of date
    int saved_term = current_term();

    for (auto peer_id:peer_ids_) {
        logger(LogLevel::Debug) << "Sending AppendEntries call to" << peer_id;

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

    // Submit(RandomString());

    HeartbeatTimeout();
}

void ConcensusModule::Shutdown() {
    state_ = ElectionRole::Dead;
    logger(LogLevel::Debug) << "Server shutdown";
}

void ConcensusModule::PromoteToLeader() {
    state_ = ElectionRole::Leader;
    votes_received_ = 0;
    logger(LogLevel::Debug) << "Becoming leader, term:" << current_term();

    HeartbeatTimeout();
}

void ConcensusModule::ResetToFollower(const int term) {
    state_ = ElectionRole::Follower;
    current_term_.store(term);
    set_vote("");
    votes_received_ = 0;
    logger(LogLevel::Debug) << "Becoming follower, term:" << current_term();

    snapshot_->PersistState(current_term_, vote_);

    ElectionTimeout(term);
}

void ConcensusModule::ElectionTimeout(const int term) {
    int random_timeout = std::rand() % 151 + 150;
    logger(LogLevel::Debug) << "Election timer created:" << random_timeout << "ms";
    // Create random timeout to avoid election with split votes
    election_timer_.expires_after(std::chrono::milliseconds(random_timeout));
    election_timer_.async_wait([this, term](const boost::system::error_code& err) {
        if (!err) {
            ElectionCallback(term);
        } else {
            logger(LogLevel::Info) << "Election timer cancelled with error:" << err.message();
        }
    });
}

void ConcensusModule::HeartbeatTimeout() {
    logger(LogLevel::Debug) << "New heartbeat timer created";
    heartbeat_timer_.expires_after(std::chrono::milliseconds(50));
    heartbeat_timer_.async_wait([this](const boost::system::error_code& err) {
        if (!err) {
            HeartbeatCallback();
        } else {
            logger(LogLevel::Info) << "Heartbeat timer cancelled with error:" << err.message();
        }
    });
}

void ConcensusModule::CommitEntry(const rpc::LogEntry& entry) {
    // Notify commit queue thread to apply the new command
    std::unique_lock<std::mutex> guard(channel_->queue_mutex_);
    channel_->commit_queue_.push(entry);
    channel_->commit_notifier_.notify_one();
}

void ConcensusModule::Submit(rpc::LogEntry& entry) {
    if (state_ == ElectionRole::Leader) {
        log_->AppendLog(entry);
        PersistLogToStorage({entry}, true);
    }
}

void ConcensusModule::PersistLogToStorage(const std::vector<rpc::LogEntry>& entries, bool append) {
    snapshot_->PersistLog(entries, append);
}

void ConcensusModule::RestoreFromStorage() {
    auto [term, vote] = snapshot_->RestoreState();
    current_term_ = term;
    vote_ = vote;

    auto entries = snapshot_->RestoreLog();
    log_->set_entries(entries);

    // next_index should be updated to indicate where to append in log for each node
    for (auto peer_id:peer_ids_) {
        log_->set_next_index(peer_id, log_->entries().size());
    }
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
