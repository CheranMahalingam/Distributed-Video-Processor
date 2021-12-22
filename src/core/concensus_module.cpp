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
    vote_ = id_;
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
        // Make request append entries call and get reply
        // auto [reply_term, reply_success] = AppendEntries(peer_id, saved_term);
        // if (!reply_success) {
        //     return;
        // }
        // Log(LogLevel::Info) << "Received AppendEntries reply from" << peer_id;
    
        // if (reply_term > saved_term) {
        //     Log(LogLevel::Info) << "Term out of date in heartbeat reply, changed from" << saved_term << "to" << reply_term;
        //     ResetToFollower(reply_term);
        //     return;
        // }
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

    AsyncClientCall* call = new AsyncClientCall;

    call->response_reader = stubs_[peer_id]->PrepareAsyncRequestVote(&call->ctx, request, &cq_);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
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

    HeartbeatTimeout();
}

void ConcensusModule::ResetToFollower(const int term) {
    state_ = ElectionRole::Follower;
    current_term_.store(term);
    vote_ = -1;
    votes_received_ = 0;
    Log(LogLevel::Info) << "Becoming follower, term:" << current_term();

    ElectionTimeout(term);
}

void ConcensusModule::ElectionTimeout(const int term) {
    int random_timeout = std::rand() % 151 + 150;
    Log(LogLevel::Info) << "New election timer created" << random_timeout << "ms";
    election_timer_.expires_after(std::chrono::milliseconds(random_timeout));
    election_timer_.async_wait([this, term](const boost::system::error_code& err) {
        if (!err) {
            ElectionCallback(term);
        } else {
            Log(LogLevel::Error) << "Election timer cancelled with error:" << err.value() << err.message();
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
            Log(LogLevel::Error) << "Heartbeat timer cancelled with error:" << err.value() << err.message();
            HeartbeatTimeout();
        }
    });
    io_.run();
}

void ConcensusModule::AsyncRpcResponseHandler() {
    void* got_tag;
    bool ok = false;

    while (cq_.Next(&got_tag, &ok)) {
        AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);

        GPR_ASSERT(ok);

        if (call->status.ok()) {
            HandleRequestVoteResponse(call->reply);
        } else {
            Log(LogLevel::Error) << "RPC RequestVote call failed unexpectedly";
        }

        delete call;
    }
}

int ConcensusModule::current_term() const {
    return current_term_.load();
}

}
