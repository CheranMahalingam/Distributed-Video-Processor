#include "concensus_module.h"
#include "log.h"

namespace raft {

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::Status;

ConcensusModule::ConcensusModule(const int id, boost::asio::io_context& io_context, const std::vector<std::string>& peer_ids)
    : id_(id), peer_ids_(peer_ids), io_(io_context), election_timer_(io_context), heartbeat_timer_(io_context),
        current_term_(0), vote_(-1), state_(ElectionRole::Follower) {
    std::unordered_map<std::string, std::unique_ptr<rpc::RaftService::Stub>> stubs_;
    for (auto peer_id:peer_ids) {
        std::shared_ptr<Channel> chan = grpc::CreateChannel(peer_id, grpc::InsecureChannelCredentials());
        stubs_[peer_id] = rpc::RaftService::NewStub(chan);
    }

    ElectionTimeout(0);
}

void ConcensusModule::ElectionCallback(const int term) {
    // log_ << id_ << "election timer expired";

    if (state_ != ElectionRole::Candidate && state_ != ElectionRole::Follower) {
        // log_ << id_ << "state invalid for election";
        return;
    }

    if (current_term_ != term) {
        // log_ << id_ << "term changed from" << current_term_ << "to" << term;
        return;
    }

    StartElection();
}

void ConcensusModule::HeartbeatCallback() {
    if (state_ != ElectionRole::Leader) {
        // log_ << id_ << "invalid state for sending heartbeat";
        return;
    }

    int saved_term = current_term_;

    for (auto peer_id:peer_ids_) {
        // log_ << id_ << "sending AppendEntries call to" << peer_id;
        // Make request append entries call and get reply
        auto [reply_term, reply_success] = AppendEntries(peer_id, saved_term);
        if (!reply_success) {
            return;
        }
        // log_ << id_ << "received AppendEntries reply from" << peer_id;
    
        if (reply_term > saved_term) {
            // log_ << id_ << "term out of date in heartbeat reply, changed from" << saved_term << "to" << reply_term;
            ResetToFollower(reply_term);
            return;
        }
    }

    HeartbeatTimeout();
}

void ConcensusModule::StartElection() {
    state_ = ElectionRole::Candidate;
    current_term_++;
    int saved_term = current_term_;
    vote_ = id_;
    // log_ << id_ << "becomes Candidate, term:" << saved_term;

    int votes_received = 1;

    for (auto peer_id:peer_ids_) {
        // log_ << id_ << "sending RequestVote call to" << peer_id;

        // Make request vote call and get reply
        auto [reply_term, reply_vote_granted] = RequestVote(peer_id, saved_term);
        if (reply_term == -1 && !reply_vote_granted) {
            return;
        }
        // log_ << id_ << "received RequestVote reply from" << peer_id;

        // State changed when making calls
        if (state_ != ElectionRole::Candidate) {
            Log() << id_ << "changed state while waiting for reply";
            return;
        }

        // Another server became the leader
        if (reply_term > saved_term) {
            // log_ << id_ << "term out of date, changed from" << saved_term << "to" << reply_term;
            ResetToFollower(reply_term);
            return;
        } else if (reply_term == saved_term) {
            if (reply_vote_granted) {
                votes_received++;

                if (votes_received*2 > peer_ids_.size()) {
                    // log_ << id_ << "wins election with" << votes_received << "votes";
                    PromoteToLeader();
                    return;
                }
            }
        }
    }

    // log_ << id_ << "election was unsuccessful, restarting...";
    ElectionTimeout(current_term_);
}

void ConcensusModule::Shutdown() {
    state_ = ElectionRole::Dead;
    // log_ << id_ << "has shutdown";
}

std::tuple<int, bool> ConcensusModule::RequestVote(const std::string peer_id, const int term) {
    rpc::RequestVoteRequest request;
    request.set_term(term);
    request.set_candidateid(id_);
    // TODO: Update log index + term
    request.set_lastlogindex(0);
    request.set_lastlogterm(0);

    rpc::RequestVoteResponse response;
    ClientContext ctx;
    CompletionQueue cq;
    Status status;
    std::unique_ptr<ClientAsyncResponseReader<rpc::RequestVoteResponse>> rpc;
    rpc = stubs_[peer_id]->PrepareAsyncRequestVote(&ctx, request, &cq);

    rpc->StartCall();

    rpc->Finish(&response, &status, (void*)1);
    void* tag;
    bool ok = false;
    if (cq.Next(&tag, &ok) && ok && tag == (void*)1) {
        if (status.ok()) {
            return std::make_tuple(response.term(), response.votegranted());
        } else {
            // log_ << id_ << "rpc RequestVote call failed with error" << status.error_code() << status.error_message();
            return std::make_tuple(-1, false);
        }
    } else {
        // log_ << id_ << "rpc RequestVote call failed unexpectedly";
        return std::make_tuple(-1, false);
    }
}

std::tuple<int, bool> ConcensusModule::AppendEntries(const std::string peer_id, const int term) {
    rpc::AppendEntriesRequest request;
    request.set_term(term);
    request.set_leaderid(id_);
    // TODO: Update with correct values
    request.set_prevlogindex(0);
    request.set_prevlogterm(0);
    //request.set_entries(15);
    request.set_leadercommit(0);

    rpc::AppendEntriesResponse response;
    ClientContext ctx;
    CompletionQueue cq;
    Status status;
    std::unique_ptr<ClientAsyncResponseReader<rpc::AppendEntriesResponse>> rpc;
    rpc = stubs_[peer_id]->PrepareAsyncAppendEntries(&ctx, request, &cq);

    rpc->StartCall();

    rpc->Finish(&response, &status, (void*)1);
    void* tag;
    bool ok = false;
    if (cq.Next(&tag, &ok) && ok && tag == (void*)1) {
        if (status.ok()) {
            return std::make_tuple(response.term(), response.success());
        } else {
            // log_ << id_ << "rpc AppendEntries call failed with error" << status.error_code() << status.error_message();
            return std::make_tuple(-1, false);
        }
    } else {
        // log_ << id_ << "rpc AppendEntries call failed unexpectedly";
        return std::make_tuple(-1, false);
    }
}

void ConcensusModule::PromoteToLeader() {
    state_ = ElectionRole::Leader;
    // log_ << id_ << "becomes leader, term:" << current_term_;

    HeartbeatTimeout();
}

void ConcensusModule::ResetToFollower(const int term) {
    state_ = ElectionRole::Follower;
    current_term_ = term;
    vote_ = -1;
    // log_ << id_ << "becomes follower, term:" << current_term_;

    ElectionTimeout(term);
}

void ConcensusModule::ElectionTimeout(const int term) {
    int random_timeout = std::rand() % 151 + 150;
    // log_ << id_ << "new election timer created" << random_timeout;
    election_timer_.expires_from_now(std::chrono::milliseconds(random_timeout));
    election_timer_.async_wait(boost::bind(&ConcensusModule::ElectionCallback, this, term));
}

void ConcensusModule::HeartbeatTimeout() {
    // log_ << id_ << "new heartbeat timer created";
    heartbeat_timer_.expires_from_now(std::chrono::milliseconds(50));
    heartbeat_timer_.async_wait(boost::bind(&ConcensusModule::HeartbeatCallback, this));
}

// template<typename ...Args>
// void ConcensusModule::Log(Args&&... args) {
//     ((log_ << std::forward<Args>(args) << " "), ...);
// }

int ConcensusModule::current_term() const {
    return current_term_;
}

}
