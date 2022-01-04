#include "chunk_server.h"

namespace file_system {

ChunkServer::ChunkServer(
    const std::string address,
    std::shared_ptr<raft::ConcensusModule> cm,
    std::shared_ptr<ChunkManager> manager)
    : cm_(cm), manager_(manager)  {
    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    scq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();
}

ChunkServer::~ChunkServer() {
    server_->Shutdown();
    scq_->Shutdown();
}

void ChunkServer::HandleRPC() {
    new UploadData{&service_, scq_.get(), cm_.get(), manager_.get()};
    new DownloadData{&service_, scq_.get(), cm_.get(), manager_.get()};
    new DeleteData{&service_, scq_.get(), cm_.get(), manager_.get()};
    void* tag;
    bool ok;
    while (true) {
        if (scq_->Next(&tag, &ok) && ok) {
            auto* tag_ptr = static_cast<Tag*>(tag);
            switch (tag_ptr->id) {
                case VideoMessageID::Upload: {
                    static_cast<UploadData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case VideoMessageID::Download: {
                    static_cast<DownloadData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case VideoMessageID::Delete: {
                    static_cast<DeleteData*>(tag_ptr->call)->Proceed();
                    break;
                }
            }
        } else {
            logger(LogLevel::Error) << "RPC call failed";
        }
    }
}

ChunkServer::CallData::CallData(
    storage::VideoService::AsyncService* service,
    ServerCompletionQueue* scq,
    raft::ConcensusModule* cm,
    ChunkManager* manager)
    : service_(service), scq_(scq), cm_(cm), manager_(manager), status_(CallStatus::Create) {
}

ChunkServer::UploadData::UploadData(
    storage::VideoService::AsyncService* service,
    ServerCompletionQueue* scq,
    raft::ConcensusModule* cm,
    ChunkManager* manager)
    : CallData{service, scq, cm, manager}, responder_(&ctx_) {
    tag_.id = VideoMessageID::Upload;
    tag_.call = this;
    Proceed();
}

void ChunkServer::UploadData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            logger(LogLevel::Debug) << "Creating UploadVideo reply...";
            status_ = CallStatus::Process;
            service_->RequestUploadVideo(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            logger(LogLevel::Debug) << "Processing UploadVideo reply...";
            new UploadData{service_, scq_, cm_, manager_};

            auto new_chunks = manager_->CreateChunks(request_.id(), request_.version(), request_.data());
            for (auto &chunk:new_chunks) {
                manager_->WriteToChunk(chunk);
            }

            response_.set_success(true);
            status_ = CallStatus::Finish;
            responder_.Finish(response_, Status::OK, (void*)&tag_);
        }
        default: {
            delete this;
        }
    }
}

ChunkServer::DownloadData::DownloadData(
    storage::VideoService::AsyncService* service,
    ServerCompletionQueue* scq,
    raft::ConcensusModule* cm,
    ChunkManager* manager)
    : CallData{service, scq, cm, manager}, responder_(&ctx_) {
    tag_.id = VideoMessageID::Download;
    tag_.call = this;
    Proceed();
}

void ChunkServer::DownloadData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            logger(LogLevel::Debug) << "Creating DownloadVideo reply...";
            status_ = CallStatus::Process;
            service_->RequestDownloadVideo(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            logger(LogLevel::Debug) << "Processing DownloadVideo reply...";
            new DownloadData{service_, scq_, cm_, manager_};

            bool last = false;
            int sequence = 0;
            std::string data = "";
            while (!last) {
                auto chunk = manager_->ReadFromChunk(request_.id(), request_.version(), sequence);
                data += chunk.data();
                last = chunk.metadata().last();
                sequence++;
            }

            response_.set_success(true);
            response_.set_data(data);
            status_ = CallStatus::Finish;
            responder_.Finish(response_, Status::OK, (void*)&tag_);
        }
        default: {
            delete this;
        }
    }
}

ChunkServer::DeleteData::DeleteData(
    storage::VideoService::AsyncService* service,
    ServerCompletionQueue* scq,
    raft::ConcensusModule* cm,
    ChunkManager* manager)
    : CallData{service, scq, cm, manager}, responder_(&ctx_) {
    tag_.id = VideoMessageID::Delete;
    tag_.call = this;
    Proceed();
}

void ChunkServer::DeleteData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            logger(LogLevel::Debug) << "Creating DeleteVideo reply...";
            status_ = CallStatus::Process;
            service_->RequestDeleteVideo(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            logger(LogLevel::Debug) << "Processing DeleteVideo reply...";
            new DeleteData{service_, scq_, cm_, manager_};

            manager_->DeleteChunks(request_.id(), request_.version());

            response_.set_success(true);
            status_ = CallStatus::Finish;
            responder_.Finish(response_, Status::OK, (void*)&tag_);
        }
        default: {
            delete this;
        }
    }
}

}
