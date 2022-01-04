#include "chunk_server.h"

namespace file_system {

ChunkServer::ChunkServer() {
}

ChunkServer::~ChunkServer() {
}

ChunkServer::UploadData::UploadData(
    server::VideoProcessorService::AsyncService* service,
    ServerCompletionQueue* scq,
    raft::ConcensusModule* cm,
    ChunkManager* manager)
    : CallData{service, scq, cm}, manager_(manager), responder_(&ctx_) {
    tag_.id = RpcCommandID::UploadVideo;
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
            break;
        }
        default: {
            delete this;
        }
    }
}

ChunkServer::DownloadData::DownloadData(
    server::VideoProcessorService::AsyncService* service,
    ServerCompletionQueue* scq,
    raft::ConcensusModule* cm,
    ChunkManager* manager)
    : CallData{service, scq, cm}, manager_(manager), responder_(&ctx_) {
    tag_.id = RpcCommandID::DownloadVideo;
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
            break;
        }
        default: {
            delete this;
        }
    }
}

ChunkServer::DeleteData::DeleteData(
    server::VideoProcessorService::AsyncService* service,
    ServerCompletionQueue* scq,
    raft::ConcensusModule* cm,
    ChunkManager* manager)
    : CallData{service, scq, cm}, manager_(manager), responder_(&ctx_) {
    tag_.id = RpcCommandID::DeleteVideo;
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
            break;
        }
        default: {
            delete this;
        }
    }
}

}
