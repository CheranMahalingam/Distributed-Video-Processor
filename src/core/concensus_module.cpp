#include "concensus_module.h"
#include "log.h"

namespace raft {

ConcensusModule::ConcensusModule(const int id, boost::asio::io_context& io_context, const std::vector<std::string>& peer_ids)
    : id_(id), peer_ids_(peer_ids), io_(io_context), election_timer_(io_context), heartbeat_timer_(io_context),
        current_term_(0), vote_(-1), votes_received_(0), state_(ElectionRole::Follower) {
    for (auto peer_id:peer_ids) {
        std::shared_ptr<Channel> chan = grpc::CreateChannel(peer_id, grpc::InsecureChannelCredentials());
        stubs_[peer_id] = rpc::RaftService::NewStub(chan);
    }
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
    set_vote(id_);
    votes_received_ = 1;
    Log(LogLevel::Info) << "Becomes Candidate, term:" << saved_term;

    for (auto peer_id:peer_ids_) {
        Log(LogLevel::Info) << "Sending RequestVote call to" << peer_id;
        RequestVote(peer_id, saved_term);
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
        AppendEntries(peer_id, saved_term);
    }

    HeartbeatTimeout();
}

void ConcensusModule::Shutdown() {
    state_ = ElectionRole::Dead;
    Log(LogLevel::Info) << "Server shutdown";
}

void ConcensusModule::RequestVote(const std::string peer_id, const int term) {
    rpc::RequestVoteRequest request;
    request.set_term(term);
    request.set_candidateid(id_);
    // TODO: Update log index + term
    request.set_lastlogindex(0);
    request.set_lastlogterm(0);

    auto* call = new AsyncClientCall<rpc::RequestVoteResponse>;

    call->response_reader = stubs_[peer_id]->PrepareAsyncRequestVote(&call->ctx, request, &cq_);

    call->response_reader->StartCall();

    auto* tag = new Tag;
    tag->call = (void*)call;
    tag->id = MessageID::RequestVote;
    call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

void ConcensusModule::HandleRequestVoteResponse(rpc::RequestVoteResponse reply) {
    // State changed when making calls
    if (state_ != ElectionRole::Candidate) {
        Log(LogLevel::Info) << "Changed state while waiting for reply";
        return;
    }
    Log(LogLevel::Info) << "Received RequestVote reply";

    // Another server became the leader
    if (reply.term() > current_term()) {
        Log(LogLevel::Info) << "Term out of date, changed from" << current_term() << "to" << reply.term();
        ResetToFollower(reply.term());
        return;
    } else if (reply.term() == current_term()) {
        if (reply.votegranted()) {
            votes_received_++;

            if (votes_received_*2 > peer_ids_.size()) {
                Log(LogLevel::Info) << "Wins election with" << votes_received_ << "votes";
                PromoteToLeader();
                return;
            }
        }
    }
}

void ConcensusModule::AppendEntries(const std::string peer_id, const int term) {
    rpc::AppendEntriesRequest request;
    request.set_term(current_term());
    request.set_leaderid(id_);
    // TODO: Hydrate rpc call fields with correct values
    request.set_prevlogindex(0);
    request.set_prevlogterm(0);
    // request.set_entries("0");
    request.set_leadercommit(0);

    auto* call = new AsyncClientCall<rpc::AppendEntriesResponse>;

    call->response_reader = stubs_[peer_id]->PrepareAsyncAppendEntries(&call->ctx, request, &cq_);

    call->response_reader->StartCall();

    auto* tag = new Tag;
    tag->call = (void*)call;
    tag->id = MessageID::AppendEntries;
    call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

void ConcensusModule::HandleAppendEntriesResponse(rpc::AppendEntriesResponse reply) {
    if (!reply.success()) {
        return;
    }
    Log(LogLevel::Info) << "Received AppendEntries reply";

    if (reply.term() > current_term()) {
        Log(LogLevel::Info) << "Term out of date in heartbeat reply, changed from" << current_term() << "to" << reply.term();
        ResetToFollower(reply.term());
        return;
    }
}

void ConcensusModule::PromoteToLeader() {
    state_ = ElectionRole::Leader;
    votes_received_ = 0;
    Log(LogLevel::Info) << "Becoming leader, term:" << current_term();

    std::thread heartbeat(&ConcensusModule::HeartbeatTimeout, this);
    heartbeat.detach();
}

void ConcensusModule::ResetToFollower(const int term) {
    state_ = ElectionRole::Follower;
    current_term_.store(term);
    set_vote(-1);
    votes_received_ = 0;
    Log(LogLevel::Info) << "Becoming follower, term:" << current_term();

    std::thread election(&ConcensusModule::ElectionTimeout, this, term);
    election.detach();
}

void ConcensusModule::ElectionTimeout(const int term) {
    reset_election_timer();
    election_timer_.async_wait([this, term](const boost::system::error_code& err) {
        if (!err) {
            ElectionCallback(term);
        } else {
            Log(LogLevel::Error) << "Election timer cancelled with error:" << err.message();
        }
    });
    io_.run();
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
    io_.run();
}

void ConcensusModule::AsyncRpcResponseHandler() {
    void* tag;
    bool ok = false;

    while (cq_.Next(&tag, &ok)) {
        GPR_ASSERT(ok);

        auto* tag_ptr = static_cast<Tag*>(tag);
        switch (tag_ptr->id) {
            case MessageID::RequestVote: {
                auto* call = static_cast<AsyncClientCall<rpc::RequestVoteResponse>*>(tag_ptr->call);
                if (call->status.ok()) {
                    HandleRequestVoteResponse(call->reply);
                    // std::thread handler(&ConcensusModule::HandleRequestVoteResponse, this, call->reply);
                    // handler.detach();
                } else {
                    Log(LogLevel::Error) << "RPC RequestVote call failed unexpectedly";
                }
                delete call;
                break;
            }
            case MessageID::AppendEntries: {
                auto* call = static_cast<AsyncClientCall<rpc::AppendEntriesResponse>*>(tag_ptr->call);
                if (call->status.ok()) {
                    HandleAppendEntriesResponse(call->reply);
                    // std::thread handler(&ConcensusModule::HandleAppendEntriesResponse, this, call->reply);
                    // handler.detach();
                } else {
                    Log(LogLevel::Error) << "RPC AppendEntries call failed unexpectedly";
                }
                delete call;
                break;
            }
        }
        delete tag_ptr;
    }
}

void ConcensusModule::set_vote(const int vote) {
    vote_.store(vote);
}

void ConcensusModule::reset_election_timer() {
    int random_timeout = std::rand() % 151 + 150;
    Log(LogLevel::Info) << "Election timer created:" << random_timeout << "ms";
    election_timer_.expires_after(std::chrono::milliseconds(random_timeout));
}

int ConcensusModule::current_term() const {
    return current_term_.load();
}

ConcensusModule::ElectionRole ConcensusModule::state() const {
    return state_;
}

int ConcensusModule::vote() const {
    return vote_.load();
}

std::vector<std::string> ConcensusModule::peer_ids() const {
    return peer_ids_;
}

}
