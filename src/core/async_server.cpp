#include <string>

#include "async_server.h"
#include "logger.h"

using grpc::ServerBuilder;
using grpc::Status;

namespace raft {

AsyncServer::~AsyncServer() {
    server_->Shutdown();
    scq_->Shutdown();
}

void AsyncServer::Run() {
    std::string address = "localhost:3000";

    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    scq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();

    HandleRPC();
}

void AsyncServer::HandleRPC() {
    new RequestVoteData{&service_, scq_.get()};
    new AppendEntriesData{&service_, scq_.get()};
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
            log_ << "RPC call failed";
        }
    }
}

AsyncServer::CallData::CallData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq)
    : service_(service), scq_(scq) {
    Proceed();
}

AsyncServer::RequestVoteData::RequestVoteData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq) 
    : CallData{service, scq}, responder_(&ctx_) {
    tag_.id = MessageID::RequestVote;
    tag_.call = this;
    Proceed();
}

void AsyncServer::RequestVoteData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            status_ = CallStatus::Process;
            service_->RequestRequestVote(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            new RequestVoteData{service_, scq_};
            response_.set_term(0);
            response_.set_votegranted(true);
            responder_.Finish(response_, Status::OK, (void*)&tag_);
            break;
        }
        default: {
            delete this;
        }
    }
}

AsyncServer::AppendEntriesData::AppendEntriesData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq) 
    : CallData{service, scq}, responder_(&ctx_) {
    tag_.id = MessageID::AppendEntries;
    tag_.call = this;
    Proceed();
}

void AsyncServer::AppendEntriesData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            status_ = CallStatus::Process;
            service_->RequestAppendEntries(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            new AppendEntriesData{service_, scq_};
            response_.set_term(0);
            response_.set_success(true);
            responder_.Finish(response_, Status::OK, (void*)&tag_);
            break;
        }
        default: {
            delete this;
        }
    }
}

}
