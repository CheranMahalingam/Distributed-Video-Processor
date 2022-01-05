#include "server_impl.h"

ServerImpl::ServerImpl(
    const std::string address,
    std::shared_ptr<raft::ConcensusModule> cm,
    std::shared_ptr<file_system::ChunkManager> manager)
    : cm_(cm), manager_(manager) {
    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    scq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();

    raft_server_ = std::make_unique<raft::RaftServer>();
    chunk_server_ = std::make_unique<file_system::ChunkServer>();
}

ServerImpl::~ServerImpl() {
    server_->Shutdown();
    scq_->Shutdown();
}

void ServerImpl::HandleRPC() {
    new file_system::ChunkServer::UploadData{&service_, scq_.get(), cm_.get(), manager_.get()};
    new file_system::ChunkServer::DownloadData{&service_, scq_.get(), cm_.get(), manager_.get()};
    new file_system::ChunkServer::DeleteData{&service_, scq_.get(), cm_.get(), manager_.get()};
    new raft::RaftServer::RequestVoteData{&service_, scq_.get(), cm_.get()};
    new raft::RaftServer::AppendEntriesData{&service_, scq_.get(), cm_.get()};
    void* tag;
    bool ok;
    while (true) {
        if (scq_->Next(&tag, &ok) && ok) {
            auto* tag_ptr = static_cast<Tag*>(tag);
            switch (tag_ptr->id) {
                case RpcCommandID::UploadVideo: {
                    static_cast<file_system::ChunkServer::UploadData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case RpcCommandID::DownloadVideo: {
                    static_cast<file_system::ChunkServer::DownloadData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case RpcCommandID::DeleteVideo: {
                    static_cast<file_system::ChunkServer::DeleteData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case RpcCommandID::RequestVote: {
                    static_cast<raft::RaftServer::RequestVoteData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case RpcCommandID::AppendEntries: {
                    static_cast<raft::RaftServer::AppendEntriesData*>(tag_ptr->call)->Proceed();
                    break;
                }
            }
        } else {
            logger(LogLevel::Error) << "RPC call failed";
        }
    }
}
