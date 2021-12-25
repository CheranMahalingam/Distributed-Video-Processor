#include "node.h"

namespace raft {

Node::Node(const std::string address, const std::vector<std::string>& peer_ids, boost::asio::io_context& io_context)
    : id_(current_id), io_(io_context) {
    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    scq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();

    cm_ = std::make_shared<ConcensusModule>(id_, io_, peer_ids);
    current_id++;
}

Node::~Node() {
    server_->Shutdown();
    scq_->Shutdown();
}

void Node::Run() {
    Log(LogLevel::Info) << "Server running...";

    std::thread rpc_response_handler(&ConcensusModule::AsyncRpcResponseHandler, cm_.get());
    std::thread rpc_event_loop(&Node::HandleRPC, this);

    boost::asio::steady_timer start_client(io_);
    start_client.expires_from_now(std::chrono::seconds(10));
    start_client.wait();
    cm_->ElectionTimeout(cm_->current_term());
    io_.run();

    rpc_response_handler.join();
    rpc_event_loop.join();
}

int Node::current_id = 0;

void Node::HandleRPC() {
    new RequestVoteData{&service_, scq_.get(), cm_.get()};
    new AppendEntriesData{&service_, scq_.get(), cm_.get()};
    void* tag;
    bool ok;
    while (true) {
        if (scq_->Next(&tag, &ok) && ok) {
            auto* tag_ptr = static_cast<Tag*>(tag);
            switch (tag_ptr->id) {
                case MessageID::RequestVote: {
                    static_cast<RequestVoteData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case MessageID::AppendEntries: {
                    static_cast<AppendEntriesData*>(tag_ptr->call)->Proceed();
                    break;
                }
            }
        } else {
            Log(LogLevel::Error) << "RPC call failed";
        }
    }
}

Node::CallData::CallData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm)
    : service_(service), scq_(scq), cm_(cm), status_(CallStatus::Create) {
}

Node::RequestVoteData::RequestVoteData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm) 
    : CallData{service, scq, cm}, responder_(&ctx_) {
    tag_.id = MessageID::RequestVote;
    tag_.call = this;
    Proceed();
}

void Node::RequestVoteData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            Log(LogLevel::Info) << "Creating RequestVote reply...";
            status_ = CallStatus::Process;
            service_->RequestRequestVote(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            Log(LogLevel::Info) << "Processing RequestVote reply...";
            new RequestVoteData{service_, scq_, cm_};

            if (cm_->state() == ConcensusModule::ElectionRole::Dead) {
                status_ = CallStatus::Finish;
                responder_.Finish(response_, Status::CANCELLED, (void*)&tag_);
                break;
            }

            if (request_.term() > cm_->current_term()) {
                Log(LogLevel::Info) << "Term out of date in RequestVote RPC, changed from" << cm_->current_term() << "to" << request_.term();
                cm_->ResetToFollower(request_.term());
            }

            if (request_.term() == cm_->current_term() && (cm_->vote() == -1 || cm_->vote() == request_.candidateid())) {
                response_.set_votegranted(true);
                cm_->set_vote(request_.candidateid());
                Log(LogLevel::Info) << "Reset...";
                cm_->ElectionTimeout(request_.term());
            } else {
                response_.set_votegranted(false);
            }

            response_.set_term(cm_->current_term());
            status_ = CallStatus::Finish;
            responder_.Finish(response_, Status::OK, (void*)&tag_);
            break;
        }
        default: {
            delete this;
        }
    }
}

Node::AppendEntriesData::AppendEntriesData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm) 
    : CallData{service, scq, cm}, responder_(&ctx_) {
    tag_.id = MessageID::AppendEntries;
    tag_.call = this;
    Proceed();
}

void Node::AppendEntriesData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            Log(LogLevel::Info) << "Creating AppendEntries reply...";
            status_ = CallStatus::Process;
            service_->RequestAppendEntries(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            Log(LogLevel::Info) << "Processing AppendEntries reply...";
            new AppendEntriesData{service_, scq_, cm_};

            if (cm_->state() == ConcensusModule::ElectionRole::Dead) {
                status_ = CallStatus::Finish;
                responder_.Finish(response_, Status::CANCELLED, (void*)&tag_);
                break;
            }

            if (request_.term() > cm_->current_term()) {
                Log(LogLevel::Info) << "Term out of date in AppendEntries RPC, changed from" << cm_->current_term() << "to" << request_.term();
                cm_->ResetToFollower(request_.term());
            }

            bool success = false;
            if (request_.term() == cm_->current_term()) {
                if (cm_->state() != ConcensusModule::ElectionRole::Follower) {
                    cm_->ResetToFollower(request_.term());
                }
                Log(LogLevel::Info) << "Reset...";
                cm_->ElectionTimeout(request_.term());
                success = true;
            }

            response_.set_term(cm_->current_term());
            response_.set_success(success);
            status_ = CallStatus::Finish;
            responder_.Finish(response_, Status::OK, (void*)&tag_);
            break;
        }
        default: {
            delete this;
        }
    }
}

}
